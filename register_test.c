#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <stdarg.h>
#include <unistd.h>
#include "machines.h"
#include "util.h"
#include <time.h>

int main(int argc, char **argv) {

   mach_set_ctx(mach_make_ctx());

   int rc = mach_open();
   if (rc != MACH_OKAY) {
      printf("mach_open error %d\n", rc);
      exit(rc);
   }

   mach_set_spec_provider(NULL, specProvider, MACH_FREE_FOR_PROVIDER);
   rc = mach_set_spec_cache_limit(32);
   if (rc != MACH_OKAY) {
      printf("warning: failed to set spec cache size\n");
   }

   rc = evalFiles(argc, argv);
   if (MACH_OKAY != rc) {
      exit(rc);
   }

   size_t dst_limit = 128 * 1024;
   char *dst = (char *)malloc(dst_limit);

   char *js_code =
      "print('Testing dynamic functions:');\n"
      "print('add(3, 4) = ' + (typeof add === 'function' ? add(3, 4) : 'not found'));\n"
      "print('multiply(3, 4) = ' + (typeof multiply === 'function' ? multiply(3, 4) : 'not found'));\n"
      "print('uppercase(\"winning!\") = ' + (typeof uppercase === 'function' ? uppercase('winning!') : 'not found'));\n";

   rc = mach_eval(js_code, dst, dst_limit);
   if (rc != 0) {
      printf("mach_eval() failed, rc: %d\n", rc);
   }

#ifndef OSX
   const char *filename = "/tmp/test.txt";
   FILE *file;

   // Make sure the file exist before starting the monitoring test
   file = fopen(filename, "w");
   if (file == NULL) {
      printf("Error opening file for writing");
      return EXIT_FAILURE;
   }

   fprintf(file, "Initial content written at: %s", ctime(&(time_t) { time(NULL) }));
   fflush(file);
   fclose(file);

   printf("\nStarting file monitoring...\n");
   rc = mach_eval("file_monitor('/tmp/test.txt')", dst, dst_limit);
   if (rc != 0) {
      printf("mach_eval() failed, rc: %d\n", rc);
   }

   // Open the file and write some additional data to it
   file = fopen(filename, "a");
   for (int i = 0; i < 5; i++) {
      sleep(2);
      fprintf(file, "Writing time at: %s", ctime(&(time_t) { time(NULL) }));
      fflush(file);
   }

   fflush(file);
   sleep(2);

   printf("Stop file monitoring...\n");
   rc = mach_eval("file_monitor('stop')", dst, dst_limit);

   sleep(3);

   fclose(file);

#endif

   free(dst);
   mach_close();

   free(mach_get_ctx());
}
