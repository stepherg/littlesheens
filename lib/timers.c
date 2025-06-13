#include <duktape.h>
#include <register.h>
#include <pthread.h>
#include <unistd.h>
#include <errno.h>
#include <string.h>
#include <stdlib.h>
#include <stdio.h>
#include <stddef.h>
#include <time.h>

// Timer structure for managing JavaScript timeouts and intervals.
typedef struct {
   duk_context *ctx;         // Shared Duktape context, protected by duk_mutex.
   int id;                   // Timer ID (1-based for JavaScript).
   volatile int active;      // 0 if cleared or finished, volatile for thread visibility.
   int repeat;               // 1 for setInterval, 0 for setTimeout.
   long delay_ms;            // Delay in milliseconds.
   volatile int in_use;      // 1 if slot is allocated.
   pthread_cond_t cond;      // Condition variable for timed wait/signal.
   pthread_t thread_id;      // Thread ID for the timer thread.
} duk_timer_t;

// Global mutexes: one for the timers array, one for Duktape context access.
static pthread_mutex_t timer_mutex;
pthread_mutex_t duk_mutex;
#define MAX_TIMERS 100
static duk_timer_t timers[MAX_TIMERS];

// Maximum delay to prevent integer overflow (approximately 24 days).
#define MAX_DELAY_MS (0x7FFFFFFF / 1000)

// Initialize all timer slots to a zeroed state.
static void init_timers() {
   for (int i = 0; i < MAX_TIMERS; i++) {
      memset((void *)&timers[i], 0, sizeof(duk_timer_t));
   }
}

// Allocate a timer slot from the global array.
static duk_timer_t *alloc_timer() {
   pthread_mutex_lock(&timer_mutex);
   int slot = -1;
   for (int i = 0; i < MAX_TIMERS; i++) {
      if (!timers[i].in_use) {
         slot = i;
         if (pthread_cond_init(&timers[i].cond, NULL) != 0) {
            fprintf(stderr, "ERROR: Failed to initialize condition variable for slot %d (%s)\n", i, strerror(errno));
            pthread_mutex_unlock(&timer_mutex);
            return NULL;
         }
         break;
      }
   }

   if (slot == -1) {
      fprintf(stderr, "ERROR: No free timer slots available (MAX_TIMERS=%d)\n", MAX_TIMERS);
      pthread_mutex_unlock(&timer_mutex);
      return NULL;
   }

   duk_timer_t *timer = &timers[slot];
   timer->id = slot + 1;
   timer->in_use = 1;
   timer->active = 0;
   timer->repeat = 0;
   timer->delay_ms = 0;
   timer->ctx = NULL;
   pthread_mutex_unlock(&timer_mutex);
   return timer;
}

// Free a timer slot and reset its state.
static void free_timer(int id) {
   if (id < 1 || id > MAX_TIMERS) {
      return; // Silently ignore invalid IDs to avoid redundant logging.
   }
   int slot_idx = id - 1;
   pthread_mutex_lock(&timer_mutex);
   if (!timers[slot_idx].in_use) {
      pthread_mutex_unlock(&timer_mutex);
      return;
   }

   if (pthread_cond_destroy(&timers[slot_idx].cond) != 0) {
      fprintf(stderr, "ERROR: Failed to destroy condition variable for timer ID %d (%s)\n", id, strerror(errno));
   }

   memset((void *)&timers[slot_idx], 0, sizeof(duk_timer_t));
   pthread_mutex_unlock(&timer_mutex);
}

// Count active timers for cleanup.
static int get_timer_count() {
   pthread_mutex_lock(&timer_mutex);
   int cnt = 0;
   for (int i = 0; i < MAX_TIMERS; i++) {
      if (timers[i].in_use) {
         cnt++;
      }
   }
   pthread_mutex_unlock(&timer_mutex);
   return cnt;
}

// Timer thread to execute callbacks after a delay.
static void *timer_thread(void *arg) {
   duk_timer_t *timer = (duk_timer_t *)arg;
   struct timespec timeout, now;
   int rc;

   duk_context *ctx = timer->ctx;

   while (timer->active) {
      clock_gettime(CLOCK_REALTIME, &now);
      long current_ms = now.tv_sec * 1000 + now.tv_nsec / 1000000;
      long target_ms = current_ms + timer->delay_ms;

      timeout.tv_sec = target_ms / 1000;
      timeout.tv_nsec = (target_ms % 1000) * 1000000;

      if (timeout.tv_nsec >= 1000000000) {
         timeout.tv_sec += timeout.tv_nsec / 1000000000;
         timeout.tv_nsec %= 1000000000;
      }

      pthread_mutex_lock(&timer_mutex);
      rc = pthread_cond_timedwait(&timer->cond, &timer_mutex, &timeout);

      if (!timer->active) {
         pthread_mutex_unlock(&timer_mutex);
         break;
      }

      if (rc != 0 && rc != ETIMEDOUT) {
         fprintf(stderr, "WARNING: pthread_cond_timedwait for Timer %d returned error %d (%s)\n",
            timer->id, rc, strerror(rc));
      }

      pthread_mutex_lock(&duk_mutex); // Protect Duktape operations.
      duk_push_global_stash(ctx);
      duk_get_prop_string(ctx, -1, "timers");

      if (!duk_has_prop_index(ctx, -1, timer->id)) {
         duk_pop_2(ctx);
         pthread_mutex_unlock(&duk_mutex);
         pthread_mutex_unlock(&timer_mutex);
         timer->active = 0;
         break;
      }

      duk_get_prop_index(ctx, -1, timer->id);
      duk_idx_t timer_obj_idx = duk_get_top_index(ctx);
      duk_get_prop_string(ctx, timer_obj_idx, "callback");

      if (!duk_is_function(ctx, -1)) {
         duk_pop_3(ctx);
         pthread_mutex_unlock(&duk_mutex);
         pthread_mutex_unlock(&timer_mutex);
         timer->active = 0;
         break;
      }

      duk_get_prop_string(ctx, timer_obj_idx, "args");
      int nargs = 0;
      if (duk_is_array(ctx, -1)) {
         nargs = duk_get_length(ctx, -1);
         duk_idx_t args_idx = duk_push_array(ctx);
         for (int i = 0; i < nargs; i++) {
            duk_get_prop_index(ctx, -2, i);
            duk_put_prop_index(ctx, args_idx, i);
         }
         for (int i = 0; i < nargs; i++) {
            duk_get_prop_index(ctx, args_idx, i);
         }
         duk_remove(ctx, args_idx);
      }
      duk_remove(ctx, -1 - nargs);
      duk_remove(ctx, timer_obj_idx);

      if (duk_pcall(ctx, nargs) != DUK_EXEC_SUCCESS) {
         fprintf(stderr, "ERROR: Timer %d callback execution error: %s\n", timer->id, duk_safe_to_string(ctx, -1));
      }
      duk_pop(ctx);
      duk_pop_2(ctx);
      pthread_mutex_unlock(&duk_mutex);

      if (!timer->repeat) {
         timer->active = 0;
         pthread_mutex_lock(&duk_mutex);
         duk_push_global_stash(ctx);
         duk_get_prop_string(ctx, -1, "timers");
         if (duk_has_prop_index(ctx, -1, timer->id)) {
            duk_del_prop_index(ctx, -1, timer->id);
         }
         duk_pop_2(ctx);
         pthread_mutex_unlock(&duk_mutex);
         pthread_mutex_unlock(&timer_mutex);
         break;
      }
      pthread_mutex_unlock(&timer_mutex);
   }

   free_timer(timer->id);
   return NULL;
}

// JavaScript setTimeout implementation.
static duk_ret_t duk_set_timeout(duk_context *ctx) {
   int nargs = duk_get_top(ctx);
   if (!duk_is_function(ctx, 0)) {
      return duk_error(ctx, DUK_ERR_TYPE_ERROR, "Callback must be a function");
   }

   int delay = duk_get_int_default(ctx, 1, 0);
   if (delay < 0) {
      return duk_error(ctx, DUK_ERR_RANGE_ERROR, "Delay must be non-negative");
   }
   if (delay > MAX_DELAY_MS) {
      return duk_error(ctx, DUK_ERR_RANGE_ERROR, "Delay exceeds maximum (%d ms)", MAX_DELAY_MS);
   }
   delay = delay == 0 ? 10 : delay;

   duk_timer_t *timer = alloc_timer();
   if (!timer) {
      return duk_error(ctx, DUK_ERR_ERROR, "Failed to allocate timer slot");
   }

   timer->ctx = ctx;
   timer->active = 1;
   timer->repeat = 0;
   timer->delay_ms = delay;

   pthread_mutex_lock(&duk_mutex); // Protect Duktape operations.
   duk_push_global_stash(ctx);
   if (!duk_has_prop_string(ctx, -1, "timers")) {
      duk_push_object(ctx);
      duk_put_prop_string(ctx, -2, "timers");
   }
   duk_get_prop_string(ctx, -1, "timers");
   duk_push_object(ctx);
   duk_dup(ctx, 0);
   duk_put_prop_string(ctx, -2, "callback");
   duk_push_array(ctx);
   for (int i = 2; i < nargs; i++) {
      duk_dup(ctx, i);
      duk_put_prop_index(ctx, -2, i - 2);
   }
   duk_put_prop_string(ctx, -2, "args");
   duk_put_prop_index(ctx, -2, timer->id);
   duk_pop_n(ctx, 2);
   pthread_mutex_unlock(&duk_mutex);

   if (pthread_create(&timer->thread_id, NULL, timer_thread, timer) != 0) {
      pthread_mutex_lock(&timer_mutex);
      timer->active = 0;
      pthread_mutex_unlock(&timer_mutex);
      pthread_mutex_lock(&duk_mutex);
      duk_push_global_stash(ctx);
      duk_get_prop_string(ctx, -1, "timers");
      duk_del_prop_index(ctx, -1, timer->id);
      duk_pop_2(ctx);
      pthread_mutex_unlock(&duk_mutex);
      free_timer(timer->id);
      return duk_error(ctx, DUK_ERR_ERROR, "Failed to create timer thread");
   }
   pthread_detach(timer->thread_id);

   duk_push_int(ctx, timer->id);
   return 1;
}

// JavaScript setInterval implementation.
static duk_ret_t duk_set_interval(duk_context *ctx) {
   int nargs = duk_get_top(ctx);
   if (!duk_is_function(ctx, 0)) {
      return duk_error(ctx, DUK_ERR_TYPE_ERROR, "Callback must be a function");
   }

   int delay = duk_get_int_default(ctx, 1, 0);
   if (delay < 0) {
      return duk_error(ctx, DUK_ERR_RANGE_ERROR, "Delay must be non-negative");
   }
   if (delay > MAX_DELAY_MS) {
      return duk_error(ctx, DUK_ERR_RANGE_ERROR, "Delay exceeds maximum (%d ms)", MAX_DELAY_MS);
   }
   delay = delay == 0 ? 10 : delay;

   duk_timer_t *timer = alloc_timer();
   if (!timer) {
      return duk_error(ctx, DUK_ERR_ERROR, "Failed to allocate timer slot");
   }

   timer->ctx = ctx;
   timer->active = 1;
   timer->repeat = 1;
   timer->delay_ms = delay;

   pthread_mutex_lock(&duk_mutex); // Protect Duktape operations.
   duk_push_global_stash(ctx);
   if (!duk_has_prop_string(ctx, -1, "timers")) {
      duk_push_object(ctx);
      duk_put_prop_string(ctx, -2, "timers");
   }
   duk_get_prop_string(ctx, -1, "timers");
   duk_push_object(ctx);
   duk_dup(ctx, 0);
   duk_put_prop_string(ctx, -2, "callback");
   duk_push_array(ctx);
   for (int i = 2; i < nargs; i++) {
      duk_dup(ctx, i);
      duk_put_prop_index(ctx, -2, i - 2);
   }
   duk_put_prop_string(ctx, -2, "args");
   duk_put_prop_index(ctx, -2, timer->id);
   duk_pop_n(ctx, 2);
   pthread_mutex_unlock(&duk_mutex);

   if (pthread_create(&timer->thread_id, NULL, timer_thread, timer) != 0) {
      pthread_mutex_lock(&timer_mutex);
      timer->active = 0;
      pthread_mutex_unlock(&timer_mutex);
      pthread_mutex_lock(&duk_mutex);
      duk_push_global_stash(ctx);
      duk_get_prop_string(ctx, -1, "timers");
      duk_del_prop_index(ctx, -1, timer->id);
      duk_pop_2(ctx);
      pthread_mutex_unlock(&duk_mutex);
      free_timer(timer->id);
      return duk_error(ctx, DUK_ERR_ERROR, "Failed to create timer thread");
   }
   pthread_detach(timer->thread_id);

   duk_push_int(ctx, timer->id);
   return 1;
}

// JavaScript clearTimeout/clearInterval implementation.
static duk_ret_t duk_clear_timer(duk_context *ctx) {
   int timer_id = duk_require_int(ctx, 0);
   if (timer_id < 1 || timer_id > MAX_TIMERS) {
      return duk_error(ctx, DUK_ERR_ERROR, "Invalid timerId: %d. Must be between 1 and %d", timer_id, MAX_TIMERS);
   }

   int slot_idx = timer_id - 1;
   pthread_mutex_lock(&timer_mutex);
   if (!timers[slot_idx].in_use) {
      pthread_mutex_unlock(&timer_mutex);
      return 0;
   }

   timers[slot_idx].active = 0;
   if (pthread_cond_signal(&timers[slot_idx].cond) != 0) {
      fprintf(stderr, "ERROR: Failed to signal condition variable for timer ID %d (%s)\n", timer_id, strerror(errno));
   }
   pthread_mutex_unlock(&timer_mutex);

   pthread_mutex_lock(&duk_mutex); // Protect Duktape operations.
   duk_push_global_stash(ctx);
   duk_get_prop_string(ctx, -1, "timers");
   if (duk_has_prop_index(ctx, -1, timer_id)) {
      duk_del_prop_index(ctx, -1, timer_id);
   }
   duk_pop_2(ctx);
   pthread_mutex_unlock(&duk_mutex);
   return 0;
}

// Function list for Duktape global object.
static const duk_function_list_entry timer_functions[] = {
    {"setTimeout", duk_set_timeout, DUK_VARARGS},
    {"setInterval", duk_set_interval, DUK_VARARGS},
    {"clearTimeout", duk_clear_timer, 1},
    {"clearInterval", duk_clear_timer, 1},
    {NULL, NULL, 0}
};

// Register timer functions with Duktape.
duk_idx_t register_function(duk_context *ctx) {
   for (const duk_function_list_entry *f = timer_functions; f->key != NULL; f++) {
      duk_push_c_function(ctx, f->value, f->nargs);
      duk_put_global_string(ctx, f->key);
   }

   init_timers();
   pthread_mutexattr_t mutex_attr;
   pthread_mutexattr_init(&mutex_attr);
   pthread_mutexattr_settype(&mutex_attr, PTHREAD_MUTEX_RECURSIVE);
   pthread_mutex_init(&timer_mutex, &mutex_attr);
   pthread_mutex_init(&duk_mutex, &mutex_attr); // Initialize Duktape mutex.
   pthread_mutexattr_destroy(&mutex_attr);
   return 1;
}

// Cleanup timers and mutexes on shutdown.
void close_function(duk_context *ctx) {
   while (get_timer_count() > 0) {
      sleep(1);
   }
   pthread_mutex_destroy(&timer_mutex);
   pthread_mutex_destroy(&duk_mutex);
}
