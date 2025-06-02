#include <duktape.h>
#include <register.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <sys/stat.h>
#include <unistd.h>
#include <fcntl.h>
#include <dirent.h>

// Helper: Throw error with errno message
static void fs_throw_error(duk_context *ctx, const char *operation, const char *path)
{
   const char *err_msg = strerror(errno);
   duk_push_error_object(ctx, DUK_ERR_ERROR, "%s failed for '%s': %s", operation, path, err_msg);
   duk_throw(ctx);
}

// fs.readFileSync(path[, options])
static duk_ret_t duk_fs_read_file_sync(duk_context *ctx)
{
   if (!duk_is_string(ctx, 0))
   {
      duk_error(ctx, DUK_ERR_TYPE_ERROR, "path must be a string");
   }
   const char *path = duk_get_string(ctx, 0);
   const char *encoding = "utf8"; // Default encoding

   // Check options
//   if (duk_is_object(ctx, 1))
//   {
      duk_get_prop_string(ctx, 1, "encoding");
      if (duk_is_string(ctx, -1))
      {
         encoding = duk_get_string(ctx, -1);
      }
      duk_pop(ctx);
//   }
//   else if (!duk_is_undefined(ctx, 1))
//   {
//      duk_error(ctx, DUK_ERR_TYPE_ERROR, "options must be an object");
//   }

   // Open file
   int fd = open(path, O_RDONLY);
   if (fd < 0)
   {
      fs_throw_error(ctx, "readFileSync", path);
   }

   // Get file size
   struct stat st;
   if (fstat(fd, &st) < 0)
   {
      close(fd);
      fs_throw_error(ctx, "readFileSync", path);
   }
   off_t size = st.st_size;

   // Allocate buffer
   char *buffer = (char *)malloc(size + 1);
   if (!buffer)
   {
      close(fd);
      duk_error(ctx, DUK_ERR_ERROR, "memory allocation failed");
   }

   // Read file
   ssize_t total_read = 0;
   while (total_read < size)
   {
      ssize_t bytes_read = read(fd, buffer + total_read, size - total_read);
      if (bytes_read < 0)
      {
         free(buffer);
         close(fd);
         fs_throw_error(ctx, "readFileSync", path);
      }
      total_read += bytes_read;
   }
   buffer[size] = '\0'; // Null-terminate for string

   close(fd);

   // Push result
   if (strcmp(encoding, "utf8") == 0)
   {
      duk_push_string(ctx, buffer);
   }
   else
   {
      // For non-utf8, return as buffer (simplified, treat as string)
      duk_push_string(ctx, buffer);
   }
   free(buffer);
   return 1;
}

// fs.writeFileSync(path, data[, options])
static duk_ret_t duk_fs_write_file_sync(duk_context *ctx)
{
   if (!duk_is_string(ctx, 0))
   {
      duk_error(ctx, DUK_ERR_TYPE_ERROR, "path must be a string");
   }
   const char *path = duk_get_string(ctx, 0);
   const char *data;
   duk_size_t data_len;

   // Get data
   if (duk_is_string(ctx, 1))
   {
      data = duk_get_lstring(ctx, 1, &data_len);
   }
   else
   {
      duk_error(ctx, DUK_ERR_TYPE_ERROR, "data must be a string");
   }

   const char *encoding = "utf8"; // Default encoding
   // Check options
   if (duk_is_object(ctx, 2))
   {
      duk_get_prop_string(ctx, 2, "encoding");
      if (duk_is_string(ctx, -1))
      {
         encoding = duk_get_string(ctx, -1);
      }
      duk_pop(ctx);
   }
   else if (!duk_is_undefined(ctx, 2))
   {
      duk_error(ctx, DUK_ERR_TYPE_ERROR, "options must be an object");
   }

   // Validate encoding
   if (strcmp(encoding, "utf8") != 0)
   {
      duk_error(ctx, DUK_ERR_TYPE_ERROR, "only utf8 encoding is supported");
   }

   // Open file
   int fd = open(path, O_WRONLY | O_CREAT | O_TRUNC, 0644);
   if (fd < 0)
   {
      fs_throw_error(ctx, "writeFileSync", path);
   }

   // Write data
   ssize_t total_written = 0;
   while (total_written < data_len)
   {
      ssize_t bytes_written = write(fd, data + total_written, data_len - total_written);
      if (bytes_written < 0)
      {
         close(fd);
         fs_throw_error(ctx, "writeFileSync", path);
      }
      total_written += bytes_written;
   }

   close(fd);
   duk_push_undefined(ctx);
   return 1;
}

// fs.readdirSync(path)
static duk_ret_t duk_fs_readdir_sync(duk_context *ctx)
{
   const char *path = duk_require_string(ctx, 0);

   DIR *dir = opendir(path);
   if (!dir)
   {
      fs_throw_error(ctx, "opendir", path);
   }

   duk_push_array(ctx);
   struct dirent *entry;
   duk_idx_t idx = 0;
   errno = 0; // Reset errno for readdir
   while ((entry = readdir(dir)) != NULL)
   {
      // Skip "." and ".."
      if (strcmp(entry->d_name, ".") == 0 || strcmp(entry->d_name, "..") == 0)
      {
         continue;
      }
      duk_push_string(ctx, entry->d_name);
      duk_put_prop_index(ctx, -2, idx++);
   }
   if (errno)
   {
      closedir(dir);
      fs_throw_error(ctx, "readdir", path);
   }

   closedir(dir);
   return 1;
}

// Helper: isFile method
static duk_ret_t duk_is_file(duk_context *ctx)
{
   duk_push_this(ctx);
   duk_get_prop_string(ctx, -1, "\xFF"
                                "stat_data");
   struct stat *st = (struct stat *)duk_require_pointer(ctx, -1);
   //printf("%s() st: %p\n", __FUNCTION__, st);
   //printf("%s() st_size: %ld\n", __FUNCTION__, st->st_size);

   duk_pop_2(ctx);
   duk_push_boolean(ctx, S_ISREG(st->st_mode));
   return 1;
}

// Helper: isDirectory method
static duk_ret_t duk_is_directory(duk_context *ctx)
{
   duk_push_this(ctx);
   duk_get_prop_string(ctx, -1, "\xFF"
                                "stat_data");
   struct stat *st = (struct stat *)duk_require_pointer(ctx, -1);
   duk_pop_2(ctx);
   duk_push_boolean(ctx, S_ISDIR(st->st_mode));
   return 1;
}

// fs.statSync(path)
static duk_ret_t duk_fs_stat_sync(duk_context *ctx)
{
   const char *path = duk_require_string(ctx, 0);
   static struct stat st;

   if (stat(path, &st) != 0)
   {
      fs_throw_error(ctx, "stat", path);
   }

   duk_push_object(ctx);

   // Properties
   duk_push_number(ctx, st.st_mode);
   duk_put_prop_string(ctx, -2, "mode");
   duk_push_number(ctx, st.st_size);
   duk_put_prop_string(ctx, -2, "size");
   duk_push_number(ctx, st.st_mtime);
   duk_put_prop_string(ctx, -2, "mtime");

   // Methods
   duk_push_c_function(ctx, duk_is_file, 0);
   duk_put_prop_string(ctx, -2, "isFile");
   duk_push_c_function(ctx, duk_is_directory, 0);
   duk_put_prop_string(ctx, -2, "isDirectory");

   // Store stat struct in object for methods
   // printf("%s() st: %p\n", __FUNCTION__, &st);

   duk_push_pointer(ctx, (void *)&st);
   duk_put_prop_string(ctx, -2, "\xFF"
                                "stat_data");

   return 1;
}

// FS module functions
static const duk_function_list_entry fs_functions[] = {
    {"readFileSync", duk_fs_read_file_sync, DUK_VARARGS},
    {"writeFileSync", duk_fs_write_file_sync, DUK_VARARGS},
    {"readdirSync", duk_fs_readdir_sync, 1},
    {"statSync", duk_fs_stat_sync, 1},
    {NULL, NULL, 0}};

// Module registration
duk_idx_t register_function(duk_context *ctx)
{
#if 0   
   for (duk_function_list_entry *f = fs_functions; f->key != NULL; f++)
   {
      duk_push_c_function(ctx, f->value, f->nargs);
      duk_put_global_string(ctx, f->key);
   }
#endif

   duk_push_string(ctx, "fs");
   duk_push_object(ctx);
   duk_put_function_list(ctx, -1, fs_functions);
   duk_put_global_string(ctx, "fs");
   //duk_put_prop(ctx, -3);

   //duk_put_prop_string(ctx, -2, "fs");

   //   duk_push_string(ctx, "fs");
   //   duk_push_object(ctx);
   //   duk_put_function_list(ctx, -1, fs_functions);
   //   duk_put_prop(ctx, -3);

   return 1;
}
