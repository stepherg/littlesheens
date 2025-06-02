#include <duktape.h>
#include <register.h>
#include <curl/curl.h>
#include <string.h>
#include <stdlib.h>
#include <errno.h>

// Response data structure
typedef struct
{
   char *body;
   size_t body_size;
   char *headers;
   size_t headers_size;
   long status_code;
   duk_context *ctx;
} response_t;

// Write callback for body
static size_t write_callback(void *data, size_t size, size_t nmemb, void *userp)
{
   size_t realsize = size * nmemb;
   response_t *response = (response_t *)userp;

   char *new_body = realloc(response->body, response->body_size + realsize + 1);
   if (!new_body)
   {
      return 0; // Signal failure
   }
   response->body = new_body;
   memcpy(response->body + response->body_size, data, realsize);
   response->body_size += realsize;
   response->body[response->body_size] = 0;

   return realsize;
}

// Header callback
static size_t header_callback(char *buffer, size_t size, size_t nmemb, void *userp)
{
   size_t realsize = size * nmemb;
   response_t *response = (response_t *)userp;

   char *new_headers = realloc(response->headers, response->headers_size + realsize + 1);
   if (!new_headers)
   {
      return 0; // Signal failure
   }
   response->headers = new_headers;
   memcpy(response->headers + response->headers_size, buffer, realsize);
   response->headers_size += realsize;
   response->headers[response->headers_size] = 0;

   return realsize;
}

// Helper: Throw error
static void request_throw_error(duk_context *ctx, const char *operation, const char *msg)
{
   duk_push_error_object(ctx, DUK_ERR_ERROR, "%s: %s", operation, msg);
   duk_throw(ctx);
}

// Helper: Parse headers from options
static struct curl_slist *parse_headers(duk_context *ctx, duk_idx_t options_idx)
{
   struct curl_slist *headers = NULL;
   if (duk_is_object(ctx, options_idx))
   {
      duk_get_prop_string(ctx, options_idx, "headers");
      if (duk_is_object(ctx, -1))
      {
         duk_enum(ctx, -1, DUK_ENUM_OWN_PROPERTIES_ONLY);
         while (duk_next(ctx, -1, 1))
         {
            const char *key = duk_get_string(ctx, -2);
            const char *value = duk_get_string(ctx, -1);
            if (key && value)
            {
               char *header = malloc(strlen(key) + strlen(value) + 3); // key: value\0
               sprintf(header, "%s: %s", key, value);
               headers = curl_slist_append(headers, header);
               free(header);
            }
            duk_pop_2(ctx);
         }
         duk_pop(ctx); // enum
      }
      duk_pop(ctx); // headers
   }
   return headers;
}

// Helper: Perform request
static void perform_request(duk_context *ctx, const char *url, duk_idx_t options_idx, int is_post, response_t *response)
{
   CURL *curl = curl_easy_init();
   if (!curl)
   {
      request_throw_error(ctx, is_post ? "postSync" : "getSync", "curl initialization failed");
   }

   response->body = NULL;
   response->body_size = 0;
   response->headers = NULL;
   response->headers_size = 0;
   response->status_code = 0;
   response->ctx = ctx;

   // Set URL
   curl_easy_setopt(curl, CURLOPT_URL, url);

   // Set callbacks
   curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, write_callback);
   curl_easy_setopt(curl, CURLOPT_WRITEDATA, response);
   curl_easy_setopt(curl, CURLOPT_HEADERFUNCTION, header_callback);
   curl_easy_setopt(curl, CURLOPT_HEADERDATA, response);

   // Set options
   long timeout = 0;
   struct curl_slist *headers = NULL;
   char *post_body = NULL;

   if (duk_is_object(ctx, options_idx))
   {
      // Timeout
      duk_get_prop_string(ctx, options_idx, "timeout");
      if (duk_is_number(ctx, -1))
      {
         timeout = duk_get_int(ctx, -1);
         curl_easy_setopt(curl, CURLOPT_TIMEOUT_MS, timeout);
      }
      duk_pop(ctx);

      // Headers
      headers = parse_headers(ctx, options_idx);
      if (headers)
      {
         curl_easy_setopt(curl, CURLOPT_HTTPHEADER, headers);
      }

      // Body (for POST)
      if (is_post)
      {
         duk_get_prop_string(ctx, options_idx, "body");
         if (duk_is_string(ctx, -1))
         {
            post_body = strdup(duk_get_string(ctx, -1));
            curl_easy_setopt(curl, CURLOPT_POSTFIELDS, post_body);
         }
         else if (duk_is_object(ctx, -1))
         {
            duk_json_encode(ctx, -1); // Stringify object
            post_body = strdup(duk_get_string(ctx, -1));
            curl_easy_setopt(curl, CURLOPT_POSTFIELDS, post_body);
         }
         duk_pop(ctx);
      }
   }

   // Set POST if needed
   if (is_post)
   {
      curl_easy_setopt(curl, CURLOPT_POST, 1L);
   }

   // Perform request
   CURLcode res = curl_easy_perform(curl);
   if (res != CURLE_OK)
   {
      curl_slist_free_all(headers);
      free(post_body);
      free(response->body);
      free(response->headers);
      curl_easy_cleanup(curl);
      request_throw_error(ctx, is_post ? "postSync" : "getSync", curl_easy_strerror(res));
   }

   // Get status code
   curl_easy_getinfo(curl, CURLINFO_RESPONSE_CODE, &response->status_code);

   // Clean up
   curl_slist_free_all(headers);
   free(post_body);
   curl_easy_cleanup(curl);
}

// request.getSync(url[, options])
static duk_ret_t duk_request_get_sync(duk_context *ctx)
{
   if (!duk_is_string(ctx, 0))
   {
      request_throw_error(ctx, "getSync", "url must be a string");
   }
   const char *url = duk_get_string(ctx, 0);
   duk_idx_t options_idx = duk_is_object(ctx, 1) ? 1 : -1;

   response_t response = {0};
   perform_request(ctx, url, options_idx, 0, &response);

   // Push response object
   duk_push_object(ctx);
   duk_push_int(ctx, response.status_code);
   duk_put_prop_string(ctx, -2, "statusCode");
   duk_push_string(ctx, response.body ? response.body : "");
   duk_put_prop_string(ctx, -2, "body");

   // Parse headers into object
   duk_push_object(ctx);
   if (response.headers)
   {
      char *header = strtok(response.headers, "\r\n");
      while (header)
      {
         char *colon = strchr(header, ':');
         if (colon)
         {
            *colon = '\0';
            const char *key = header;
            const char *value = colon + 1;
            while (*value == ' ')
               value++;
            duk_push_string(ctx, value);
            duk_put_prop_string(ctx, -2, key);
         }
         header = strtok(NULL, "\r\n");
      }
   }
   duk_put_prop_string(ctx, -2, "headers");

   // Clean up
   free(response.body);
   free(response.headers);
   return 1;
}

// request.postSync(url, options)
static duk_ret_t duk_request_post_sync(duk_context *ctx)
{
   if (!duk_is_string(ctx, 0))
   {
      request_throw_error(ctx, "postSync", "url must be a string");
   }
   const char *url = duk_get_string(ctx, 0);
   if (!duk_is_object(ctx, 1))
   {
      request_throw_error(ctx, "postSync", "options must be an object");
   }
   duk_idx_t options_idx = 1;

   response_t response = {0};
   perform_request(ctx, url, options_idx, 1, &response);

   // Push response object
   duk_push_object(ctx);
   duk_push_int(ctx, response.status_code);
   duk_put_prop_string(ctx, -2, "statusCode");
   duk_push_string(ctx, response.body ? response.body : "");
   duk_put_prop_string(ctx, -2, "body");

   // Parse headers into object
   duk_push_object(ctx);
   if (response.headers)
   {
      char *header = strtok(response.headers, "\r\n");
      while (header)
      {
         char *colon = strchr(header, ':');
         if (colon)
         {
            *colon = '\0';
            const char *key = header;
            const char *value = colon + 1;
            while (*value == ' ')
               value++;
            duk_push_string(ctx, value);
            duk_put_prop_string(ctx, -2, key);
         }
         header = strtok(NULL, "\r\n");
      }
   }
   duk_put_prop_string(ctx, -2, "headers");

   // Clean up
   free(response.body);
   free(response.headers);
   return 1;
}

// Request module functions
static const duk_function_list_entry request_functions[] = {
    {"getSync", duk_request_get_sync, DUK_VARARGS},
    {"postSync", duk_request_post_sync, 2},
    {NULL, NULL, 0}};

// Module registration
duk_idx_t register_function(duk_context *ctx)
{
   duk_push_string(ctx, "request");
   curl_global_init(CURL_GLOBAL_ALL); // Initialize libcurl
   duk_push_object(ctx);
   duk_put_function_list(ctx, -1, request_functions);
   duk_put_global_string(ctx, "request");

   //   duk_put_prop(ctx, -3);
   return 1;
}

// Cleanup (call on shutdown)
void duk_request_cleanup(void)
{
   curl_global_cleanup();
}