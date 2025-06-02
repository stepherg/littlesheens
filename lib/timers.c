#include <duktape.h>
#include <register.h>
#include <pthread.h>
#include <sys/timerfd.h>
#include <poll.h>
#include <unistd.h>
#include <errno.h>
#include <string.h>
#include <stdlib.h>
#include <stdio.h>
#include <stddef.h>
#include <time.h>
#include <sys/sysinfo.h>

// Timer structure
typedef struct
{
   duk_context *ctx;
   int id;
   int active;    // 0 if cleared
   int repeat;    // 1 for setInterval, 0 for setTimeout
   long delay_ms; // Delay in milliseconds
   int inUse;
} duk_timer_t;

static pthread_mutex_t timer_mutex;
#define MAX_TIMERS 100
static duk_timer_t timers[MAX_TIMERS];

static void init_timers()
{
   for (int i = 0; i < MAX_TIMERS; i++)
   {
      memset((void *)&timers[i], 0, sizeof(duk_timer_t));
   }
}

static duk_timer_t *alloc_timer()
{
   pthread_mutex_lock(&timer_mutex);

   int slot = -1;
   for (int i = 0; i < MAX_TIMERS; i++)
   {
      if (!timers[i].inUse)
      {
         slot = i;
         break;
      }
   }

   if (slot == -1)
   {
      fprintf(stderr, "Error: No free timers\n");
      pthread_mutex_unlock(&timer_mutex);
      return NULL;
   }

   timers[slot].id = slot + 1;
   timers[slot].inUse = 1;
   pthread_mutex_unlock(&timer_mutex);
   return &timers[slot];
}

static void free_timer(int id)
{
   pthread_mutex_lock(&timer_mutex);
   memset((void *)&timers[id - 1], 0, sizeof(duk_timer_t));
   pthread_mutex_unlock(&timer_mutex);
}

// Helper: Sleep for milliseconds
static void ms_sleep(long ms)
{
   struct timespec ts;
   ts.tv_sec = ms / 1000;
   ts.tv_nsec = (ms % 1000) * 1000000;
   nanosleep(&ts, NULL);
}

// Timer thread function
static void *timer_thread(void *arg)
{
   duk_timer_t *timer = (duk_timer_t *)arg;

   while (1)
   {
      ms_sleep(timer->delay_ms);
      pthread_mutex_lock(&timer_mutex);
      if (!timer->active)
      {
         pthread_mutex_unlock(&timer_mutex);
         break;
      }

      // Call the callback
      duk_context *ctx = timer->ctx;
      duk_push_global_stash(ctx);
      duk_get_prop_string(ctx, -1, "timers");
      duk_get_prop_index(ctx, -1, timer->id);
      if (duk_is_function(ctx, -1))
      {
         if (duk_pcall(ctx, 0) != DUK_EXEC_SUCCESS)
         {
            fprintf(stderr, "Timer callback error: %s\n", duk_safe_to_string(ctx, -1));
         }
      }
      duk_pop_3(ctx);

      if (!timer->repeat)
      {
         // For setTimeout, clear and exit
         timer->active = 0;
         duk_push_global_stash(ctx);
         duk_get_prop_string(ctx, -1, "timers");
         duk_del_prop_index(ctx, -1, timer->id);
         duk_pop_2(ctx);
         pthread_mutex_unlock(&timer_mutex);
         break;
      }
      pthread_mutex_unlock(&timer_mutex);
   }

   free_timer(timer->id);
   return NULL;
}

// setTimeout(callback, delay)
static duk_ret_t duk_set_timeout(duk_context *ctx)
{
   if (!duk_is_function(ctx, 0))
   {
      duk_error(ctx, DUK_ERR_TYPE_ERROR, "callback must be a function");
   }
   int delay = duk_get_int_default(ctx, 1, 0);
   if (delay < 0)
   {
      duk_error(ctx, DUK_ERR_RANGE_ERROR, "delay must be non-negative");
   }

   duk_timer_t *timer = alloc_timer();
   if (!timer)
   {
      duk_error(ctx, DUK_ERR_ERROR, "memory allocation failed");
   }

   // A zero delay causes a crash in Duktape, so set a minimum of 10ms
   delay = delay == 0 ? 10 : delay;

   timer->ctx = ctx;
   timer->active = 1;
   timer->repeat = 0;
   timer->delay_ms = delay;

   // Store callback in global stash
   duk_push_global_stash(ctx);
   if (!duk_has_prop_string(ctx, -1, "timers"))
   {
      duk_push_object(ctx);
      duk_put_prop_string(ctx, -2, "timers");
   }
   duk_get_prop_string(ctx, -1, "timers");
   duk_dup(ctx, 0); // Copy callback
   duk_put_prop_index(ctx, -2, timer->id);
   duk_pop(ctx); // Pop stash

   // Start timer thread
   pthread_t thread;
   if (pthread_create(&thread, NULL, timer_thread, timer) != 0)
   {
      duk_push_global_stash(ctx);
      duk_get_prop_string(ctx, -1, "timers");
      duk_del_prop_index(ctx, -1, timer->id);
      duk_pop_2(ctx);
      free_timer(timer->id);
      duk_error(ctx, DUK_ERR_ERROR, "failed to create timer thread: %s", strerror(errno));
   }
   pthread_detach(thread);

   duk_push_int(ctx, timer->id);
   return 1;
}

// setInterval(callback, delay)
static duk_ret_t duk_set_interval(duk_context *ctx)
{
   if (!duk_is_function(ctx, 0))
   {
      duk_error(ctx, DUK_ERR_TYPE_ERROR, "callback must be a function");
   }
   int delay = duk_get_int_default(ctx, 1, 0);
   if (delay < 0)
   {
      duk_error(ctx, DUK_ERR_RANGE_ERROR, "delay must be non-negative");
   }

   duk_timer_t *timer = alloc_timer();
   if (!timer)
   {
      duk_error(ctx, DUK_ERR_ERROR, "memory allocation failed");
   }

   timer->ctx = ctx;
   timer->active = 1;
   timer->repeat = 1;
   timer->delay_ms = delay;

   // Store callback
   duk_push_global_stash(ctx);
   if (!duk_has_prop_string(ctx, -1, "timers"))
   {
      duk_push_object(ctx);
      duk_put_prop_string(ctx, -2, "timers");
   }
   duk_get_prop_string(ctx, -1, "timers");
   duk_dup(ctx, 0);
   duk_put_prop_index(ctx, -2, timer->id);
   duk_pop(ctx);

   // Start timer thread
   pthread_t thread;
   if (pthread_create(&thread, NULL, timer_thread, timer) != 0)
   {
      duk_push_global_stash(ctx);
      duk_get_prop_string(ctx, -1, "timers");
      duk_del_prop_index(ctx, -1, timer->id);
      duk_pop_2(ctx);
      free_timer(timer->id);
      duk_error(ctx, DUK_ERR_ERROR, "failed to create timer thread: %s", strerror(errno));
   }
   pthread_detach(thread);

   duk_push_int(ctx, timer->id);
   return 1;
}

// clearTimeout(timerId) / clearInterval(timerId)
static duk_ret_t duk_clear_timer(duk_context *ctx)
{
   int timer_id = duk_require_int(ctx, 0);

   if (timer_id < 1 || timer_id > MAX_TIMERS)
   {
      duk_error(ctx, DUK_ERR_ERROR, "invalid timerId: %d", timer_id);
   }
   pthread_mutex_lock(&timer_mutex);
   duk_push_global_stash(ctx);
   duk_get_prop_string(ctx, -1, "timers");
   if (duk_has_prop_index(ctx, -1, timer_id))
   {
      duk_del_prop_index(ctx, -1, timer_id);
   }
   duk_pop_2(ctx);
   timers[timer_id-1].active = 0;
   pthread_mutex_unlock(&timer_mutex);

   return 0;
}

// Global timer functions
static const duk_function_list_entry timer_functions[] = {
    {"setTimeout", duk_set_timeout, 2},
    {"setInterval", duk_set_interval, 2},
    {"clearTimeout", duk_clear_timer, 1},
    {"clearInterval", duk_clear_timer, 1},
    {NULL, NULL, 0}};

duk_idx_t register_function(duk_context *ctx)
{
   for (const duk_function_list_entry *f = timer_functions; f->key != NULL; f++)
   {
      duk_push_c_function(ctx, f->value, f->nargs);
      duk_put_global_string(ctx, f->key);
   }

   init_timers();
   pthread_mutexattr_t mutex_attr;
   pthread_mutexattr_init(&mutex_attr);
   pthread_mutexattr_settype(&mutex_attr, PTHREAD_MUTEX_RECURSIVE);
   pthread_mutex_init(&timer_mutex, &mutex_attr);
   pthread_mutexattr_destroy(&mutex_attr);

   return 1;
}