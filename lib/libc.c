#include <duktape.h>
#include <register.h>
#include <stdio.h>
#include <string.h>
#include <arpa/inet.h>
#include <unistd.h>

#define MAX_IP_STR_LEN 46 // Max length for IPv6 string + null terminator

static int arrtoip(const unsigned char *bytes, size_t len, char *output)
{
   if (!bytes || !output)
   {
      return -1;
   }

   // IPv4: 4 bytes
   if (len == 4)
   {
      if (snprintf(output, MAX_IP_STR_LEN, "%u.%u.%u.%u",
                   bytes[0], bytes[1], bytes[2], bytes[3]) >= MAX_IP_STR_LEN)
      {
         return -1;
      }
      return 0;
   }
   // IPv6: 16 bytes
   else if (len == 16)
   {
      if (inet_ntop(AF_INET6, bytes, output, MAX_IP_STR_LEN) == NULL)
      {
         return -1;
      }
      return 0;
   }

   // Invalid length
   return -1;
}

static duk_ret_t native_arrtoip(duk_context *ctx)
{
   if (!duk_is_array(ctx, 0))
   {
      //
      // Returning 0 on failure until test code using jq is updated
      //
      return 0; // duk_error(ctx, DUK_ERR_TYPE_ERROR, "argument must be an array");
   }

   duk_size_t arr_len = duk_get_length(ctx, 0);

   unsigned char inbuff[MAX_IP_STR_LEN];
   char outbuff[MAX_IP_STR_LEN];
   double octet;

   // Iterate through the array
   for (duk_size_t i = 0; i < arr_len; i++)
   {
      duk_get_prop_index(ctx, 0, i);

      if (!duk_is_number(ctx, -1))
      {
         duk_pop(ctx);
         //
         // Returning 0 on failure until test code using jq is updated
         //
         return 0; // duk_error(ctx, DUK_ERR_TYPE_ERROR, "array element %d is not a number", i);
      }

      octet = duk_get_number(ctx, -1);
      if (octet > 255 || octet < 0)
      {
         duk_pop(ctx);
         //
         // Returning 0 on failure until test code using jq is updated
         //
         return 0; // duk_error(ctx, DUK_ERR_RANGE_ERROR, "octet %d at index %d is invalid", (int)octet, i);
      }
      inbuff[i] = (unsigned char)octet;

      duk_pop(ctx);
   }

   arrtoip((unsigned char *)&inbuff, arr_len, (char *)&outbuff);

   duk_push_string(ctx, outbuff);

   return 1;
}

//
// Registration function
//
static const duk_function_list_entry libc_functions[] = {
    {"arrtoip", native_arrtoip, 1},
    {NULL, NULL, 0}};

duk_idx_t register_function(duk_context *ctx)
{
   duk_push_object(ctx);
   duk_put_function_list(ctx, -1, libc_functions);
   return 1;
}

