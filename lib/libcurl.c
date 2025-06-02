#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <curl/curl.h>
#include <duktape.h>
#include <register.h>

// Structure to hold response data from curl
struct ResponseData
{
   char *data;
   size_t size;
};

// Callback function to handle data received from curl
static size_t write_callback(void *contents, size_t size, size_t nmemb, void *userp)
{
   size_t realsize = size * nmemb;
   struct ResponseData *mem = (struct ResponseData *)userp;

   char *ptr = realloc(mem->data, mem->size + realsize + 1);
   if (!ptr)
   {
      printf("Not enough memory (realloc returned NULL)\n");
      return 0;
   }

   mem->data = ptr;
   memcpy(&(mem->data[mem->size]), contents, realsize);
   mem->size += realsize;
   mem->data[mem->size] = 0; // Null-terminate

   return realsize;
}

// Helper function to perform HTTP request
static CURLcode perform_http_request(const char *url, const char *method, const char *post_data, struct ResponseData *response)
{
   CURL *curl;
   CURLcode res;

   curl = curl_easy_init();
   if (!curl)
   {
      return CURLE_FAILED_INIT;
   }

   // Initialize response data
   response->data = malloc(1);
   response->size = 0;
   response->data[0] = 0;

   // Set URL and common options
   curl_easy_setopt(curl, CURLOPT_URL, url);
   curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, write_callback);
   curl_easy_setopt(curl, CURLOPT_WRITEDATA, (void *)response);
   curl_easy_setopt(curl, CURLOPT_USERAGENT, "libcurl-duktape/1.0");

   // Set HTTP method
   if (strcmp(method, "POST") == 0)
   {
      curl_easy_setopt(curl, CURLOPT_POST, 1L);
      if (post_data)
      {
         curl_easy_setopt(curl, CURLOPT_POSTFIELDS, post_data);
      }
   }
   else if (strcmp(method, "PUT") == 0)
   {
      curl_easy_setopt(curl, CURLOPT_CUSTOMREQUEST, "PUT");
      if (post_data)
      {
         curl_easy_setopt(curl, CURLOPT_POSTFIELDS, post_data);
      }
   }
   else if (strcmp(method, "DELETE") == 0)
   {
      curl_easy_setopt(curl, CURLOPT_CUSTOMREQUEST, "DELETE");
   }
   else
   { // Default to GET
      curl_easy_setopt(curl, CURLOPT_HTTPGET, 1L);
   }

   // Perform the request
   res = curl_easy_perform(curl);

   // Cleanup curl
   curl_easy_cleanup(curl);

   return res;
}

// Duktape binding for HTTP GET
static duk_ret_t duk_curl_get(duk_context *ctx)
{
   const char *url = duk_require_string(ctx, 0); // First argument: URL

   struct ResponseData response = {0};
   CURLcode res = perform_http_request(url, "GET", NULL, &response);

   if (res != CURLE_OK)
   {
      duk_push_string(ctx, curl_easy_strerror(res));
      free(response.data);
      return duk_throw(ctx); // Throw error in JS
   }

   duk_push_string(ctx, response.data); // Push response to stack
   free(response.data);
   return 1; // Return 1 value
}

// Duktape binding for HTTP POST
static duk_ret_t duk_curl_post(duk_context *ctx)
{
   const char *url = duk_require_string(ctx, 0);                             // First argument: URL
   const char *data = duk_is_string(ctx, 1) ? duk_get_string(ctx, 1) : NULL; // Second argument: optional data

   struct ResponseData response = {0};
   CURLcode res = perform_http_request(url, "POST", data, &response);

   if (res != CURLE_OK)
   {
      duk_push_string(ctx, curl_easy_strerror(res));
      free(response.data);
      return duk_throw(ctx);
   }

   duk_push_string(ctx, response.data);
   free(response.data);
   return 1;
}

// Duktape binding for HTTP PUT
static duk_ret_t duk_curl_put(duk_context *ctx)
{
   const char *url = duk_require_string(ctx, 0);
   const char *data = duk_is_string(ctx, 1) ? duk_get_string(ctx, 1) : NULL;

   struct ResponseData response = {0};
   CURLcode res = perform_http_request(url, "PUT", data, &response);

   if (res != CURLE_OK)
   {
      duk_push_string(ctx, curl_easy_strerror(res));
      free(response.data);
      return duk_throw(ctx);
   }

   duk_push_string(ctx, response.data);
   free(response.data);
   return 1;
}

// Duktape binding for HTTP DELETE
static duk_ret_t duk_curl_delete(duk_context *ctx)
{
   const char *url = duk_require_string(ctx, 0);

   struct ResponseData response = {0};
   CURLcode res = perform_http_request(url, "DELETE", NULL, &response);

   if (res != CURLE_OK)
   {
      duk_push_string(ctx, curl_easy_strerror(res));
      free(response.data);
      return duk_throw(ctx);
   }

   duk_push_string(ctx, response.data);
   free(response.data);
   return 1;
}

static const duk_function_list_entry curl_functions[] = {
    {"httpGet", duk_curl_get, 1},
    {"httpPost", duk_curl_post, 2},
    {"httpPut", duk_curl_put, 2},
    {"httpDelete", duk_curl_delete, 1},
    {NULL, NULL, 0}};

duk_idx_t register_function(duk_context *ctx)
{
   duk_push_object(ctx);
   duk_put_function_list(ctx, -1, curl_functions);
   return 1;
}
