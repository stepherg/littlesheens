#include <duktape.h>
#include <register.h>
#include <string.h>
#include <stdlib.h>
#include <unistd.h>
#include <errno.h>
#include <libgen.h>

// Helper: Throw error with message
static void path_throw_error(duk_context *ctx, const char *operation, const char *msg)
{
   duk_push_error_object(ctx, DUK_ERR_ERROR, "%s: %s", operation, msg);
   duk_throw(ctx);
}

// Helper: Normalize path (remove ./, //, simplify ../)
static char *normalize_path(const char *path)
{
   char *result = strdup(path);
   if (!result)
      return NULL;

   char *out = result;
   char *in = result;
   char *last_slash = NULL;
   int segments = 0;

   while (*in)
   {
      if (in[0] == '/' && in[1] == '/')
      {
         in++; // Skip duplicate slashes
         continue;
      }
      if (in[0] == '.' && (in[1] == '/' || in[1] == '\0'))
      {
         in += (in[1] == '/' ? 2 : 1); // Skip ./
         continue;
      }
      if (in[0] == '.' && in[1] == '.' && (in[2] == '/' || in[2] == '\0'))
      {
         if (segments > 0)
         {
            out = last_slash ? last_slash : result;
            segments--;
            in += (in[2] == '/' ? 3 : 2); // Skip ../
            continue;
         }
         in += (in[2] == '/' ? 3 : 2); // Skip ../ if at root
         continue;
      }
      if (*in == '/')
      {
         last_slash = out;
         segments++;
      }
      *out++ = *in++;
   }
   *out = '\0';

   // Ensure non-empty result
   if (result[0] == '\0')
   {
      free(result);
      result = strdup(".");
   }
   return result;
}

// path.join(...paths)
static duk_ret_t duk_path_join(duk_context *ctx)
{
   duk_idx_t nargs = duk_get_top(ctx);
   if (nargs == 0)
   {
      duk_push_string(ctx, ".");
      return 1;
   }

   // Collect path segments
   char *segments[64]; // Limit for simplicity
   int nsegments = 0;
   for (duk_idx_t i = 0; i < nargs && nsegments < 64; i++)
   {
      if (!duk_is_string(ctx, i))
      {
         path_throw_error(ctx, "join", "all arguments must be strings");
      }
      segments[nsegments++] = (char *)duk_get_string(ctx, i);
   }

   // Calculate total length
   size_t total_len = 0;
   for (int i = 0; i < nsegments; i++)
   {
      total_len += strlen(segments[i]) + 1; // +1 for /
   }

   // Join segments
   char *result = (char *)malloc(total_len);
   if (!result)
   {
      path_throw_error(ctx, "join", "memory allocation failed");
   }
   result[0] = '\0';

   for (int i = 0; i < nsegments; i++)
   {
      if (i > 0 && segments[i][0] != '/' && result[strlen(result) - 1] != '/')
      {
         strcat(result, "/");
      }
      strcat(result, segments[i]);
   }

   // Normalize
   char *normalized = normalize_path(result);
   free(result);
   if (!normalized)
   {
      path_throw_error(ctx, "join", "memory allocation failed");
   }

   duk_push_string(ctx, normalized);
   free(normalized);
   return 1;
}

// path.resolve(...paths)
static duk_ret_t duk_path_resolve(duk_context *ctx)
{
   duk_idx_t nargs = duk_get_top(ctx);
   if (nargs == 0)
   {
      char cwd[PATH_MAX];
      if (getcwd(cwd, sizeof(cwd)))
      {
         duk_push_string(ctx, cwd);
         return 1;
      }
      path_throw_error(ctx, "resolve", strerror(errno));
   }

   // Join segments first
   duk_push_string(ctx, "."); // Start with relative path
   for (duk_idx_t i = 0; i < nargs; i++)
   {
      if (!duk_is_string(ctx, i))
      {
         path_throw_error(ctx, "resolve", "all arguments must be strings");
      }
      duk_dup(ctx, i);
   }
   duk_call(ctx, nargs); // Call join internally
   const char *joined = duk_get_string(ctx, -1);

   // Resolve to absolute
   char resolved[PATH_MAX];
   if (realpath(joined, resolved))
   {
      duk_push_string(ctx, resolved);
   }
   else
   {
      // If realpath fails (e.g., file doesn’t exist), construct absolute path
      if (joined[0] == '/')
      {
         char *normalized = normalize_path(joined);
         duk_push_string(ctx, normalized);
         free(normalized);
      }
      else
      {
         char cwd[PATH_MAX];
         if (!getcwd(cwd, sizeof(cwd)))
         {
            duk_pop(ctx);
            path_throw_error(ctx, "resolve", strerror(errno));
         }
         char *tmp = malloc(strlen(cwd) + strlen(joined) + 2);
         sprintf(tmp, "%s/%s", cwd, joined);
         char *normalized = normalize_path(tmp);
         free(tmp);
         duk_push_string(ctx, normalized);
         free(normalized);
      }
   }
   duk_remove(ctx, -2); // Remove joined path
   return 1;
}

// path.basename(path[, ext])
static duk_ret_t duk_path_basename(duk_context *ctx)
{
   if (!duk_is_string(ctx, 0))
   {
      path_throw_error(ctx, "basename", "path must be a string");
   }
   const char *path = duk_get_string(ctx, 0);
   const char *ext = duk_is_string(ctx, 1) ? duk_get_string(ctx, 1) : NULL;

   // Use POSIX basename (make a copy to avoid modifying input)
   char *path_copy = strdup(path);
   if (!path_copy)
   {
      path_throw_error(ctx, "basename", "memory allocation failed");
   }
   char *base = basename(path_copy);

   // Handle extension
   if (ext && strlen(base) > strlen(ext))
   {
      size_t base_len = strlen(base);
      size_t ext_len = strlen(ext);
      if (strcmp(base + base_len - ext_len, ext) == 0)
      {
         char *result = strdup(base);
         result[base_len - ext_len] = '\0';
         duk_push_string(ctx, result);
         free(result);
      }
      else
      {
         duk_push_string(ctx, base);
      }
   }
   else
   {
      duk_push_string(ctx, base);
   }
   free(path_copy);
   return 1;
}

// path.dirname(path)
static duk_ret_t duk_path_dirname(duk_context *ctx)
{
   if (!duk_is_string(ctx, 0))
   {
      path_throw_error(ctx, "dirname", "path must be a string");
   }
   const char *path = duk_get_string(ctx, 0);

   // Use POSIX dirname (make a copy to avoid modifying input)
   char *path_copy = strdup(path);
   if (!path_copy)
   {
      path_throw_error(ctx, "dirname", "memory allocation failed");
   }
   char *dir = dirname(path_copy);
   duk_push_string(ctx, dir);
   free(path_copy);
   return 1;
}

// path.extname(path)
static duk_ret_t duk_path_extname(duk_context *ctx)
{
   if (!duk_is_string(ctx, 0))
   {
      path_throw_error(ctx, "extname", "path must be a string");
   }
   const char *path = duk_get_string(ctx, 0);

   // Find last dot after last slash
   const char *dot = strrchr(path, '.');
   const char *slash = strrchr(path, '/');
   if (dot && (!slash || dot > slash) && dot[1] != '\0')
   {
      duk_push_string(ctx, dot);
   }
   else
   {
      duk_push_string(ctx, "");
   }
   return 1;
}

// Path module functions
static const duk_function_list_entry path_functions[] = {
    {"join", duk_path_join, DUK_VARARGS},
    {"resolve", duk_path_resolve, DUK_VARARGS},
    {"basename", duk_path_basename, DUK_VARARGS},
    {"dirname", duk_path_dirname, 1},
    {"extname", duk_path_extname, 1},
    {NULL, NULL, 0}};

// Module registration
duk_idx_t register_function(duk_context *ctx)
{
   duk_push_string(ctx, "path");
   duk_push_object(ctx);
   duk_put_function_list(ctx, -1, path_functions);
   duk_put_global_string(ctx, "path");

   //   duk_put_prop(ctx, -3);
   return 1;
}

