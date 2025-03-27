#include <stdio.h>
#include <stdlib.h>
#include <sys/stat.h>
#include <sys/inotify.h>
#include <unistd.h>
#include <string.h>
#include <errno.h>
#include "util.h"
#include <pthread.h>
#include <duktape.h>
#include <register.h>

#define EVENT_SIZE (sizeof(struct inotify_event))
#define BUF_LEN (1024 * (EVENT_SIZE + 16))

typedef struct {
   duk_context *ctx;
   char filename[512];
} tFileMonitor;

// Global variables for cleanup
static int keep_running = 1;
static int inotify_fd = -1;

void call_js_callback(duk_context *ctx, const char *event, off_t size) {
   duk_push_global_object(ctx);
   duk_get_prop_string(ctx, -1, "onFileChange");
   if (duk_is_function(ctx, -1)) {
      duk_push_string(ctx, event);
      duk_push_int(ctx, (duk_int_t)size);
      if (duk_pcall(ctx, 2) != DUK_EXEC_SUCCESS) {
         fprintf(stderr, "Error in JavaScript: %s\n", duk_safe_to_string(ctx, -1));
      }
   } else {
      fprintf(stderr, "Error: onFileChange is not a function\n");
   }
   duk_pop(ctx);
}

void *file_monitor_thread(void *arg) {
   tFileMonitor *fm = (tFileMonitor *)arg;
   int wd;
   char buffer[BUF_LEN];
   struct stat file_stat;
   off_t last_size = -1;

   // Just pausing the thread at startup to make the caller has time
   // to return and register the onFileChange function before we try to call it.
   sleep(1);

   // Get initial file size
   if (stat(fm->filename, &file_stat) == -1) {
      fprintf(stderr, "%s(%d) Error getting initial file stats for %s\n", __FUNCTION__, __LINE__, fm->filename);
      return NULL;
   }
   last_size = file_stat.st_size;
   call_js_callback(fm->ctx, "initial", last_size);

   // Initialize inotify
   inotify_fd = inotify_init();
   if (inotify_fd < 0) {
      fprintf(stderr, "%s(%d) Error initializing inotify\n", __FUNCTION__, __LINE__);
      return NULL;
   }

   // Add watch to the file
   wd = inotify_add_watch(inotify_fd, fm->filename, IN_MODIFY | IN_DELETE_SELF | IN_MOVE_SELF);
   if (wd < 0) {
      fprintf(stderr, "%s(%d) Error adding watch to file %s\n", __FUNCTION__, __LINE__, fm->filename);
      close(inotify_fd);
      return NULL;
   }

   while (keep_running) {
      // Set up for non-blocking read with a timeout
      fd_set set;
      struct timeval timeout;
      FD_ZERO(&set);
      FD_SET(inotify_fd, &set);
      timeout.tv_sec = 1; // 1-second timeout to check keep_running
      timeout.tv_usec = 0;

      int rv = select(inotify_fd + 1, &set, NULL, NULL, &timeout);
      if (rv == -1) {
         fprintf(stderr, "%s(%d) Error in select\n", __FUNCTION__, __LINE__);
         break;
      } else if (rv == 0) {
         continue; // Timeout, loop again to check keep_running
      }

      // Read events
      ssize_t len = read(inotify_fd, buffer, BUF_LEN);
      if (len < 0) {
         fprintf(stderr, "%s(%d) Error reading inotify events\n", __FUNCTION__, __LINE__);
         break;
      }

      ssize_t i = 0;
      while (i < len) {
         struct inotify_event *event = (struct inotify_event *)&buffer[i];
         if (event->mask & (IN_MODIFY | IN_DELETE_SELF | IN_MOVE_SELF)) {
            if (stat(fm->filename, &file_stat) == -1) {
               if (errno == ENOENT) {
                  call_js_callback(fm->ctx, "deleted", -1);
                  last_size = -1;
                  keep_running = 0;
                  break;
               } else {
                  fprintf(stderr, "%s(%d) Error getting file stats after event\n", __FUNCTION__, __LINE__);
               }
            } else {
               if (file_stat.st_size != last_size) {
                  call_js_callback(fm->ctx, "modified", file_stat.st_size);
                  last_size = file_stat.st_size;
               }
            }
         }
         i += EVENT_SIZE + event->len;
      }
   }

   // Cleanup inotify resources
   inotify_rm_watch(inotify_fd, wd);
   close(inotify_fd);
   inotify_fd = -1;

   fprintf(stderr, "%s(%d) Exiting...\n", __FUNCTION__, __LINE__);

   return NULL;
}

duk_ret_t file_monitor(duk_context *ctx) {
   const char *filename = duk_require_string(ctx, 0);
   if (filename) {
      if (strncmp(filename, "stop", 4) == 0) {
         keep_running = 0;
      } else {
         keep_running = 1;
         pthread_t monitor_thread;
         static tFileMonitor fm;
         strcpy(fm.filename, filename);
         fm.ctx = ctx;
         pthread_create(&monitor_thread, NULL, file_monitor_thread, &fm);
      }
   } else {
      keep_running = 0;
   }
   return 1;
}

DukFunctionRegistration *register_functions(void) {
   DukFunctionRegistration *funcs = malloc(2 * sizeof(DukFunctionRegistration));
   funcs[0] = (DukFunctionRegistration){ "file_monitor", file_monitor, 1 };
   funcs[1] = (DukFunctionRegistration){ NULL, NULL, 0 };
   return funcs;
}
