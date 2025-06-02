#include <stdio.h>
#include <string.h>
#include <rbus.h>
#include <rbuscore.h>
#include <duktape.h>
#include <register.h>

// Global Duktape context for handlers
static duk_context *global_ctx = NULL;

// Structure to hold rbus handle in Duktape context stash
typedef struct
{
   rbusHandle_t handle;
} rbus_context_t;

typedef struct
{
   duk_context *ctx;
   char *key; // Unique key for storing the JS callback
} async_data_t;

// Helper to get rbus handle from stash
static rbusHandle_t get_rbus_handle(duk_context *ctx)
{
   duk_push_global_stash(ctx);
   if (duk_has_prop_string(ctx, -1, "rbus_handle"))
   {
      duk_get_prop_string(ctx, -1, "rbus_handle");
      rbus_context_t *rbus_ctx = (rbus_context_t *)duk_get_pointer(ctx, -1);
      duk_pop_2(ctx);
      return rbus_ctx->handle;
   }
   duk_pop(ctx);
   return NULL;
}

// Helper to store rbus handle in stash
static void store_rbus_handle(duk_context *ctx, rbusHandle_t handle)
{
   rbus_context_t *rbus_ctx = malloc(sizeof(rbus_context_t));
   rbus_ctx->handle = handle;
   duk_push_global_stash(ctx);
   duk_push_pointer(ctx, rbus_ctx);
   duk_put_prop_string(ctx, -2, "rbus_handle");
   duk_pop(ctx);
}

static void rbusObject_to_jsObject(duk_context *ctx, rbusObject_t obj)
{
   duk_push_object(ctx); // Create a new JS object
   rbusProperty_t prop = rbusObject_GetProperties(obj);
   while (prop)
   {
      const char *name = rbusProperty_GetName(prop);
      rbusValue_t val = rbusProperty_GetValue(prop);
      switch (rbusValue_GetType(val))
      {
      case RBUS_STRING:
         duk_push_string(ctx, rbusValue_GetString(val, NULL));
         break;
      case RBUS_DOUBLE:
         duk_push_number(ctx, rbusValue_GetDouble(val));
         break;
      case RBUS_BOOLEAN:
         duk_push_boolean(ctx, rbusValue_GetBoolean(val));
         break;
      default:
         duk_push_null(ctx); // Handle unsupported types
         break;
      }
      duk_put_prop_string(ctx, -2, name); // Set property on object
      prop = rbusProperty_GetNext(prop);
   }
}

static int event_callback(const char *object_name, const char *event_name, rbusMessage message, void *user_data)
{
   duk_context *ctx = (duk_context *)user_data;
   duk_push_global_stash(ctx);
   duk_get_prop_string(ctx, -1, event_name); // Retrieve callback
   if (duk_is_function(ctx, -1))
   {
      // Push arguments
      duk_push_string(ctx, event_name);
      const char *payload = NULL;

      if (rbusMessage_GetString(message, &payload) == RT_OK)
      {
         duk_push_string(ctx, payload);
      }
      else
      {
         duk_push_null(ctx);
      }

      // Call callback with 2 arguments
      if (duk_pcall(ctx, 2) != DUK_EXEC_SUCCESS)
      {
         fprintf(stderr, "Callback execution failed: %s\n", duk_safe_to_string(ctx, -1));
         duk_pop(ctx);
      }
      else
      {
         duk_pop(ctx);
      }
   }
   else
   {
      duk_pop(ctx); // Pop non-function
   }
   duk_pop(ctx); // Pop stash

   return 0;
}

static void event_handler(rbusHandle_t handle, rbusEvent_t const *event, rbusEventSubscription_t *subscription)
{
   duk_context *ctx = (duk_context *)subscription->userData;
   duk_push_global_stash(ctx);
   duk_get_prop_string(ctx, -1, event->name); // Retrieve callback
   if (duk_is_function(ctx, -1))
   {
      // Push arguments
      duk_push_string(ctx, event->name); // eventName
      if (event->data)
      {
         rbusObject_to_jsObject(ctx, event->data); // Event data
      }
      else
      {
         duk_push_null(ctx);
      }

      // Call callback with 2 arguments
      if (duk_pcall(ctx, 2) != DUK_EXEC_SUCCESS)
      {
         fprintf(stderr, "Callback execution failed: %s\n", duk_safe_to_string(ctx, -1));
         duk_pop(ctx);
      }
      else
      {
         duk_pop(ctx);
      }
   }
   else
   {
      duk_pop(ctx); // Pop non-function
   }
   duk_pop(ctx); // Pop stash
}

static duk_ret_t duk_rbus_open(duk_context *ctx, const char *componentName)
{
   rbusError_t err;

#if 1
   err = rbus_openBrokerConnection(componentName);
   if (err != RBUS_ERROR_SUCCESS)
   {
      duk_error(ctx, DUK_ERR_ERROR, "rbus_open failed: %d", err);
   }

#else

   rbusHandle_t handle;
   err = rbus_open(&handle, componentName);
   if (err != RBUS_ERROR_SUCCESS)
   {
      duk_error(ctx, DUK_ERR_ERROR, "rbus_open failed: %d", err);
   }
   store_rbus_handle(ctx, handle);

#endif

   global_ctx = ctx; // Set global context

   duk_push_true(ctx);
   return 1;
}

static duk_ret_t duk_rbus_close(duk_context *ctx)
{
#if 1
   rbus_closeBrokerConnection();
#else
   rbusHandle_t handle = get_rbus_handle(ctx);
   if (!handle)
   {
      duk_error(ctx, DUK_ERR_ERROR, "No rbus handle found");
   }
   rbusError_t err = rbus_close(handle);
   if (err != RBUS_ERROR_SUCCESS)
   {
      duk_error(ctx, DUK_ERR_ERROR, "rbus_close failed: %d", err);
   }
   duk_push_global_stash(ctx);
   duk_get_prop_string(ctx, -1, "rbus_handle");
   rbus_context_t *rbus_ctx = (rbus_context_t *)duk_get_pointer(ctx, -1);
   free(rbus_ctx);
   duk_del_prop_string(ctx, -2, "rbus_handle");
   duk_pop(ctx);
   duk_push_true(ctx);
#endif
   return 1;
}

static duk_ret_t duk_rbus_set_value(duk_context *ctx)
{
   if (!duk_is_string(ctx, 0))
   {
      duk_error(ctx, DUK_ERR_TYPE_ERROR, "propertyName must be a string");
   }
   rbusHandle_t handle = get_rbus_handle(ctx);
   if (!handle)
   {
      duk_error(ctx, DUK_ERR_ERROR, "No rbus handle found");
   }
   const char *propertyName = duk_get_string(ctx, 0);
   rbusValue_t value;
   rbusValue_Init(&value);

   if (duk_is_string(ctx, 1))
   {
      rbusValue_SetString(value, duk_get_string(ctx, 1));
   }
   else if (duk_is_number(ctx, 1))
   {
      rbusValue_SetDouble(value, duk_get_number(ctx, 1));
   }
   else if (duk_is_boolean(ctx, 1))
   {
      rbusValue_SetBoolean(value, duk_get_boolean(ctx, 1));
   }
   else
   {
      rbusValue_Release(value);
      duk_error(ctx, DUK_ERR_TYPE_ERROR, "Unsupported value type");
   }

   rbusError_t err = rbus_set(handle, propertyName, value, NULL);
   rbusValue_Release(value);
   if (err != RBUS_ERROR_SUCCESS)
   {
      duk_error(ctx, DUK_ERR_ERROR, "rbus_set failed: %d", err);
   }
   duk_push_true(ctx);
   return 1;
}

static duk_ret_t duk_rbus_get_value(duk_context *ctx)
{
   if (!duk_is_string(ctx, 0))
   {
      duk_error(ctx, DUK_ERR_TYPE_ERROR, "propertyName must be a string");
   }
   rbusHandle_t handle = get_rbus_handle(ctx);
   if (!handle)
   {
      duk_error(ctx, DUK_ERR_ERROR, "No rbus handle found");
   }
   const char *propertyName = duk_get_string(ctx, 0);
   rbusValue_t value;
   rbusError_t err = rbus_get(handle, propertyName, &value);
   if (err != RBUS_ERROR_SUCCESS)
   {
      duk_error(ctx, DUK_ERR_ERROR, "rbus_get failed: %d", err);
   }
   switch (rbusValue_GetType(value))
   {
   case RBUS_STRING:
      duk_push_string(ctx, rbusValue_GetString(value, NULL));
      break;
   case RBUS_DOUBLE:
      duk_push_number(ctx, rbusValue_GetDouble(value));
      break;
   case RBUS_BOOLEAN:
      duk_push_boolean(ctx, rbusValue_GetBoolean(value));
      break;
   default:
      rbusValue_Release(value);
      duk_error(ctx, DUK_ERR_TYPE_ERROR, "Unsupported value type");
   }
   rbusValue_Release(value);
   return 1;
}

static rbusObject_t jsObject_to_rbusObject(duk_context *ctx, duk_idx_t idx)
{
   rbusObject_t obj;
   rbusObject_Init(&obj, NULL);                      // Initialize an unnamed object
   duk_enum(ctx, idx, DUK_ENUM_OWN_PROPERTIES_ONLY); // Iterate over object properties
   while (duk_next(ctx, -1, 1))
   { // Get key-value pairs
      const char *key = duk_get_string(ctx, -2);
      if (duk_is_string(ctx, -1))
      {
         rbusObject_SetPropertyString(obj, key, duk_get_string(ctx, -1));
      }
      else if (duk_is_number(ctx, -1))
      {
         rbusObject_SetPropertyDouble(obj, key, duk_get_number(ctx, -1));
      }
      else if (duk_is_boolean(ctx, -1))
      {
         rbusObject_SetPropertyBoolean(obj, key, duk_get_boolean(ctx, -1));
      } // Add more types as needed
      duk_pop_2(ctx); // Remove key and value
   }
   duk_pop(ctx); // Remove enumerator
   return obj;
}

static duk_ret_t duk_rbus_invoke_method(duk_context *ctx)
{
   // Retrieve the RBus handle from the stash (assumed to be stored there)
   rbusHandle_t handle = get_rbus_handle(ctx); // Assume this helper exists
   if (!handle)
   {
      duk_error(ctx, DUK_ERR_ERROR, "No rbus handle found");
   }

   // Get arguments from JavaScript
   const char *methodName = duk_require_string(ctx, 0);    // Full method name
   rbusObject_t inParams = jsObject_to_rbusObject(ctx, 1); // Convert params to rbusObject_t
   rbusObject_t outParams;

   // Invoke the method
   rbusError_t err = rbusMethod_Invoke(handle, methodName, inParams, &outParams);
   rbusObject_Release(inParams); // Clean up input parameters

   // Handle result
   if (err != RBUS_ERROR_SUCCESS)
   {
      duk_error(ctx, DUK_ERR_ERROR, "rbusMethod_Invoke failed: %d", err);
   }

   rbusObject_to_jsObject(ctx, outParams); // Convert response to JS object
   rbusObject_Release(outParams);          // Clean up output parameters
   return 1;                               // Return one value (the result object)
}

// static void async_method_callback(rbusHandle_t handle, rbusError_t error, rbusObject_t response, void *userData)
static void async_method_callback(rbusHandle_t handle, char const *methodName, rbusError_t error, rbusObject_t params)
{
#if 0   
   async_data_t *data = (async_data_t *)userData;
   duk_context *ctx = data->ctx;

   // Retrieve the JavaScript callback from the stash
   duk_push_global_stash(ctx);
   duk_get_prop_string(ctx, -1, data->key);
   if (duk_is_function(ctx, -1))
   {
      // Push error and result in (err, result) style
      duk_push_int(ctx, error); // Error code (0 if success)
      if (error == RBUS_ERROR_SUCCESS)
      {
         rbusObject_to_jsObject(ctx, response); // Convert response to JS object
      }
      else
      {
         duk_push_null(ctx); // No result on error
      }
      duk_call(ctx, 2); // Call callback with 2 args: (err, result)
   }

   // Clean up
   duk_pop(ctx);                            // Remove callback
   duk_del_prop_string(ctx, -1, data->key); // Remove callback from stash
   duk_pop(ctx);                            // Remove stash
   free(data->key);
   free(data);
#endif
}

static duk_ret_t duk_rbus_invoke_method_async(duk_context *ctx)
{
   rbusHandle_t handle = get_rbus_handle(ctx);
   if (!handle)
   {
      duk_error(ctx, DUK_ERR_ERROR, "No rbus handle found");
   }

   // Get arguments
   const char *methodName = duk_require_string(ctx, 0);
   rbusObject_t inParams = jsObject_to_rbusObject(ctx, 1);
   duk_require_function(ctx, 2); // Ensure callback is a function

   // Generate a unique key for the callback
   static int counter = 0;
   char key[32];
   snprintf(key, sizeof(key), "async_callback_%d", counter++);

   // Store the callback in the stash
   duk_push_global_stash(ctx);
   duk_dup(ctx, 2); // Duplicate the callback
   duk_put_prop_string(ctx, -2, key);
   duk_pop(ctx);

   // Prepare user data
   async_data_t *data = malloc(sizeof(async_data_t));
   data->ctx = ctx;
   data->key = strdup(key);

   // Invoke asynchronously
   rbusError_t rbusMethod_InvokeAsync(
       rbusHandle_t handle,
       char const *methodName,
       rbusObject_t inParams,
       rbusMethodAsyncRespHandler_t callback,
       int timeout);

   rbusError_t err = rbusMethod_InvokeAsync(handle, methodName, inParams, async_method_callback, 0);
   rbusObject_Release(inParams);

   if (err != RBUS_ERROR_SUCCESS)
   {
      free(data->key);
      free(data);
      duk_error(ctx, DUK_ERR_ERROR, "rbusMethod_InvokeAsync failed: %d", err);
   }

   duk_push_true(ctx); // Return true to indicate success
   return 1;
}

static int callback(const char *destination, const char *method, rbusMessage message, void *user_data, rbusMessage *response, const rtMessageHeader *hdr)
{
   (void)user_data;
   (void)response;
   (void)destination;
   (void)method;
   (void)hdr;
   printf("Received message in base callback.\n");
   char *buff = NULL;
   uint32_t buff_length = 0;

   rbusMessage_ToDebugString(message, &buff, &buff_length);
   printf("%s\n", buff);
   free(buff);

   return 0;
}

static duk_ret_t duk_rbus_register_obj(duk_context *ctx)
{
   if (!duk_is_string(ctx, 0))
   {
      duk_error(ctx, DUK_ERR_TYPE_ERROR, "objectName must be a string");
   }
   const char *objectName = duk_get_string(ctx, 0);
   rbusError_t err = rbus_registerObj(objectName, callback, NULL);

   if (err != RBUS_ERROR_SUCCESS)
   {
      duk_error(ctx, DUK_ERR_ERROR, "rbus_registerObj failed: %d", err);
   }
   duk_push_true(ctx);
   return 1;
}

static duk_ret_t duk_rbus_unregister_obj(duk_context *ctx)
{
   if (!duk_is_string(ctx, 0))
   {
      duk_error(ctx, DUK_ERR_TYPE_ERROR, "objectName must be a string");
   }
   const char *objectName = duk_get_string(ctx, 0);
   rbusError_t err = rbus_unregisterObj(objectName);
   if (err != RBUS_ERROR_SUCCESS)
   {
      duk_error(ctx, DUK_ERR_ERROR, "rbus_unregisterObj failed: %d", err);
   }
   duk_push_true(ctx);
   return 1;
}

static duk_ret_t duk_rbus_add_element(duk_context *ctx)
{
   if (!duk_is_string(ctx, 0) || !duk_is_string(ctx, 1))
   {
      duk_error(ctx, DUK_ERR_TYPE_ERROR, "objectName and elementName must be strings");
   }
   const char *objectName = duk_get_string(ctx, 0);
   const char *elementName = duk_get_string(ctx, 1);
   rbusError_t err = rbus_addElement(objectName, elementName);
   if (err != RBUS_ERROR_SUCCESS)
   {
      duk_error(ctx, DUK_ERR_ERROR, "rbus_addElement failed: %d", err);
   }
   duk_push_true(ctx);
   return 1;
}

static duk_ret_t duk_rbus_remove_element(duk_context *ctx)
{
   if (!duk_is_string(ctx, 0) || !duk_is_string(ctx, 1))
   {
      duk_error(ctx, DUK_ERR_TYPE_ERROR, "objectName and elementName must be strings");
   }
   const char *objectName = duk_get_string(ctx, 0);
   const char *elementName = duk_get_string(ctx, 1);
   rbusError_t err = rbus_removeElement(objectName, elementName);
   if (err != RBUS_ERROR_SUCCESS)
   {
      duk_error(ctx, DUK_ERR_ERROR, "rbus_removeElement failed: %d", err);
   }
   duk_push_true(ctx);
   return 1;
}

static duk_ret_t duk_rbus_table_add_row(duk_context *ctx)
{
   if (!duk_is_string(ctx, 0) || !duk_is_string(ctx, 1))
   {
      duk_error(ctx, DUK_ERR_TYPE_ERROR, "tableName and rowName must be strings");
   }
   rbusHandle_t handle = get_rbus_handle(ctx);
   if (!handle)
   {
      duk_error(ctx, DUK_ERR_ERROR, "No rbus handle found");
   }
   const char *tableName = duk_get_string(ctx, 0);
   const char *rowName = duk_get_string(ctx, 1);
   u_int32_t instNum;

   rbusError_t err = rbusTable_addRow(handle, tableName, rowName, &instNum);
   if (err != RBUS_ERROR_SUCCESS)
   {
      duk_error(ctx, DUK_ERR_ERROR, "rbusTable_addRow failed: %d", err);
   }
   duk_push_int(ctx, instNum);
   return 1;
}

static duk_ret_t duk_rbus_table_remove_row(duk_context *ctx)
{
   if (!duk_is_string(ctx, 0) || !duk_is_string(ctx, 1))
   {
      duk_error(ctx, DUK_ERR_TYPE_ERROR, "tableName and rowName must be strings");
   }
   rbusHandle_t handle = get_rbus_handle(ctx);
   if (!handle)
   {
      duk_error(ctx, DUK_ERR_ERROR, "No rbus handle found");
   }
   const char *rowName = duk_get_string(ctx, 0);
   rbusError_t err = rbusTable_removeRow(handle, rowName);
   if (err != RBUS_ERROR_SUCCESS)
   {
      duk_error(ctx, DUK_ERR_ERROR, "rbusTable_removeRow failed: %d", err);
   }
   duk_push_true(ctx);
   return 1;
}

int sub1_callback(const char *object, const char *event, const char *listener, int added, const rbusMessage filter, void *data)
{
   (void)filter;
   printf("Received sub_callback object=%s event=%s listerner=%s added=%d data=%p\n", object, event, listener, added, data);
   fflush(stdout);

   duk_context *ctx = (duk_context *)data;
   duk_push_global_stash(ctx);
   duk_get_prop_string(ctx, -1, event);
   if (duk_is_function(ctx, -1))
   {
      duk_push_string(ctx, event);
#if 0      
      rbusProperty_t prop = rbusObject_GetProperty(event->data, "value");
      if (prop)
      {
         rbusValue_t value = rbusProperty_GetValue(prop);
         if (value && rbusValue_GetType(value) == RBUS_STRING)
         {
            duk_push_string(ctx, rbusValue_GetString(value, NULL));
         }
         else
         {
            duk_push_null(ctx);
         }
      }
      else
      {
         duk_push_null(ctx);
      }
#endif
      duk_call(ctx, 2);
   }
   duk_pop_2(ctx); // Pop function and stash

   return 0;
}

static duk_ret_t duk_rbus_register_event(duk_context *ctx)
{
   // if (!duk_is_string(ctx, 0) || !duk_is_string(ctx, 1) || !duk_is_function(ctx, 2))
   if (!duk_is_string(ctx, 0) || !duk_is_string(ctx, 1))
   {
      duk_error(ctx, DUK_ERR_TYPE_ERROR, "eventName must be a string and callback must be a function");
   }
   const char *objectName = duk_get_string(ctx, 0);
   const char *eventName = duk_get_string(ctx, 1);

#if 0   
   // Store callback in stash
   duk_push_global_stash(ctx);
   duk_dup(ctx, 2);
   duk_put_prop_string(ctx, -2, eventName);
   duk_pop(ctx);
#endif

   if (!duk_is_string(ctx, 0))
   {
      duk_error(ctx, DUK_ERR_TYPE_ERROR, "eventName must be a string");
   }
   // rbusError_t err = rbus_registerEvent(objectName, eventName, sub1_callback, ctx);
   rbusError_t err = rbus_registerEvent(objectName, eventName, NULL, ctx);
   if (err != RBUS_ERROR_SUCCESS)
   {
      duk_error(ctx, DUK_ERR_ERROR, "rbus_registerEvent failed: %d", err);
   }
   duk_push_true(ctx);
   return 1;
}

static duk_ret_t duk_rbus_unregister_event(duk_context *ctx)
{
   if (!duk_is_string(ctx, 0))
   {
      duk_error(ctx, DUK_ERR_TYPE_ERROR, "eventName must be a string");
   }

   const char *objectName = duk_get_string(ctx, 0);
   const char *eventName = duk_get_string(ctx, 1);
   rbusError_t err = rbus_unregisterEvent(objectName, eventName);
   if (err != RBUS_ERROR_SUCCESS)
   {
      duk_error(ctx, DUK_ERR_ERROR, "rbus_unregisterEvent failed: %d", err);
   }
   duk_push_true(ctx);
   return 1;
}

static void handlerAsyncSub(
    rbusHandle_t handle,
    rbusEventSubscription_t *subscription,
    rbusError_t error)
{
   printf("handlerAsyncSub event=%s\n", subscription->eventName);

   (void)(handle);
}

static duk_ret_t duk_rbus_subscribe(duk_context *ctx)
{
   if (!duk_is_string(ctx, 0) || !duk_is_function(ctx, 1))
   {
      duk_error(ctx, DUK_ERR_TYPE_ERROR, "eventName must be a string, callback must be a function");
   }
   rbusHandle_t handle = get_rbus_handle(ctx); // Retrieve handle from stash
   if (!handle)
   {
      duk_error(ctx, DUK_ERR_ERROR, "No rbus handle found");
   }
   const char *eventName = duk_get_string(ctx, 0);

   // Store callback in stash
   duk_push_global_stash(ctx);
   duk_dup(ctx, 1); // Duplicate callback
   duk_put_prop_string(ctx, -2, eventName);
   duk_pop(ctx);

   // rbusError_t err = rbusEvent_Subscribe(handle, eventName, event_handler, ctx, 0);
   rbusError_t err = rbusEvent_SubscribeAsync(handle, eventName, event_handler, handlerAsyncSub, ctx, 0);
   if (err != RBUS_ERROR_SUCCESS)
   {
      duk_error(ctx, DUK_ERR_ERROR, "rbusEvent_Subscribe failed: %d", err);
   }
   duk_push_true(ctx);
   return 1;
}

static duk_ret_t duk_rbus_subscribe_to_event(duk_context *ctx)
{
   if (!duk_is_string(ctx, 0) || !duk_is_string(ctx, 1) || !duk_is_function(ctx, 2))
   {
      duk_error(ctx, DUK_ERR_TYPE_ERROR, "eventName must be a string and callback must be a function");
   }
   const char *objectName = duk_get_string(ctx, 0);
   const char *eventName = duk_get_string(ctx, 1);

   // Store callback in stash
   duk_push_global_stash(ctx);
   duk_dup(ctx, 2);
   duk_put_prop_string(ctx, -2, eventName);
   duk_pop(ctx);

   rbusError_t err = rbus_subscribeToEvent(objectName, eventName, event_callback, NULL, ctx, NULL);
   if (err != RBUS_ERROR_SUCCESS)
   {
      duk_error(ctx, DUK_ERR_ERROR, "rbus_subscribeToEvent failed: %d", err);
   }
   duk_push_true(ctx);
   return 1;
}

static duk_ret_t duk_rbus_publish_event(duk_context *ctx)
{
   if (!duk_is_string(ctx, 0))
   {
      duk_error(ctx, DUK_ERR_TYPE_ERROR, "objectName must be a string");
   }
   if (!duk_is_string(ctx, 1))
   {
      duk_error(ctx, DUK_ERR_TYPE_ERROR, "eventName must be a string");
   }
   if (!duk_is_string(ctx, 2))
   {
      duk_error(ctx, DUK_ERR_TYPE_ERROR, "msg must be a string");
   }
   const char *objectName = duk_get_string(ctx, 0);
   const char *eventName = duk_get_string(ctx, 1);
   const char *eventMsg = duk_get_string(ctx, 2);

   rbusMessage msg;
   rbusMessage_Init(&msg);
   rbusMessage_SetString(msg, eventMsg);
   rbusError_t err = rbus_publishEvent(objectName, eventName, msg);
   if (err != RBUS_ERROR_SUCCESS)
   {
      duk_error(ctx, DUK_ERR_ERROR, "rbus_publishEvent failed: %d", err);
   }
   rbusMessage_Release(msg);
   duk_push_true(ctx);
   return 1;
}

static duk_ret_t duk_rbus_discover_registered_components(duk_context *ctx)
{
   char **componentNameList = NULL;
   int count = 0;
   rbusError_t err = rbus_discoverRegisteredComponents(&count, &componentNameList);
   if (err != RBUS_ERROR_SUCCESS)
   {
      duk_error(ctx, DUK_ERR_ERROR, "rbus_discoverRegisteredComponents failed: %d", err);
   }
   duk_push_array(ctx);
   for (int i = 0; i < count; i++)
   {
      if (componentNameList[i])
      {
         duk_push_string(ctx, componentNameList[i]);
         duk_put_prop_index(ctx, -2, i);
      }
   }
   // Free the component list (assuming rbus provides a free function or standard free)
   if (componentNameList)
   {
      for (int i = 0; i < count; i++)
      {
         free(componentNameList[i]); // Free individual strings
      }
      free(componentNameList); // Free the array
   }
   return 1;
}

// Stash-based get handler for properties
static rbusError_t property_get_handler(rbusHandle_t handle, rbusProperty_t property, rbusGetHandlerOptions_t *opts)
{
   if (!global_ctx)
   {
      return RBUS_ERROR_INVALID_OPERATION;
   }
   duk_context *ctx = global_ctx;
   const char *name = rbusProperty_GetName(property);
   duk_push_global_stash(ctx);
   duk_get_prop_string(ctx, -1, name);
   if (duk_is_undefined(ctx, -1))
   {
      duk_pop_2(ctx);
      return RBUS_ERROR_INVALID_INPUT;
   }
   rbusValue_t value;
   rbusValue_Init(&value);
   if (duk_is_string(ctx, -1))
   {
      rbusValue_SetString(value, duk_get_string(ctx, -1));
   }
   else if (duk_is_number(ctx, -1))
   {
      rbusValue_SetDouble(value, duk_get_number(ctx, -1));
   }
   else if (duk_is_boolean(ctx, -1))
   {
      rbusValue_SetBoolean(value, duk_get_boolean(ctx, -1));
   }
   else
   {
      rbusValue_Release(value);
      duk_pop_2(ctx);
      return RBUS_ERROR_INVALID_INPUT;
   }
   rbusProperty_SetValue(property, value);
   rbusValue_Release(value);
   duk_pop_2(ctx);
   return RBUS_ERROR_SUCCESS;
}

// Stash-based set handler for properties
static rbusError_t property_set_handler(rbusHandle_t handle, rbusProperty_t property, rbusSetHandlerOptions_t *opts)
{
   if (!global_ctx)
   {
      return RBUS_ERROR_INVALID_OPERATION;
   }
   duk_context *ctx = global_ctx;
   const char *name = rbusProperty_GetName(property);
   rbusValue_t value = rbusProperty_GetValue(property);
   duk_push_global_stash(ctx);
   switch (rbusValue_GetType(value))
   {
   case RBUS_STRING:
      duk_push_string(ctx, rbusValue_GetString(value, NULL));
      break;
   case RBUS_DOUBLE:
      duk_push_number(ctx, rbusValue_GetDouble(value));
      break;
   case RBUS_BOOLEAN:
      duk_push_boolean(ctx, rbusValue_GetBoolean(value));
      break;
   case RBUS_INT32:
      duk_push_int(ctx, rbusValue_GetInt32(value));
      break;
   case RBUS_UINT32:
      duk_push_uint(ctx, rbusValue_GetUInt32(value));
      break;
   default:
      duk_pop(ctx);
      return RBUS_ERROR_INVALID_INPUT;
   }
   duk_put_prop_string(ctx, -2, name);
   duk_pop(ctx);
   return RBUS_ERROR_SUCCESS;
}

// Minimal table add row handler
static rbusError_t table_add_row_handler(rbusHandle_t handle, char const *tableName, char const *aliasName, uint32_t *instNum)
{
   if (!global_ctx)
   {
      return RBUS_ERROR_INVALID_OPERATION;
   }

   static uint32_t instance = 1;
   *instNum = instance++;

   printf("%s() tableName: %s aliasName: %s instNum: %u\n", __FUNCTION__, tableName, aliasName, *instNum);

   return RBUS_ERROR_SUCCESS;

   // return rbusTable_addRow(handle, tableName, aliasName, instNum); // 4 params
}

// Minimal table remove row handler
static rbusError_t table_remove_row_handler(rbusHandle_t handle, char const *rowName)
{
   if (!global_ctx)
   {
      return RBUS_ERROR_INVALID_OPERATION;
   }
   return RBUS_ERROR_SUCCESS;
}

static duk_ret_t duk_rbus_add_table_row(duk_context *ctx)
{
   if (!duk_is_string(ctx, 0))
   {
      duk_error(ctx, DUK_ERR_TYPE_ERROR, "tableName must be a string");
   }
   rbusHandle_t handle = get_rbus_handle(ctx);
   if (!handle)
   {
      duk_error(ctx, DUK_ERR_ERROR, "No rbus handle found");
   }
   const char *tableName = duk_get_string(ctx, 0);
   const char *aliasName = duk_is_string(ctx, 1) ? duk_get_string(ctx, 1) : NULL;
   uint32_t instNum;
   rbusError_t err = rbusTable_addRow(handle, tableName, aliasName, &instNum); // 4 params
   if (err != RBUS_ERROR_SUCCESS)
   {
      duk_error(ctx, DUK_ERR_ERROR, "rbusTable_addRow failed: %d", err);
   }
   duk_push_uint(ctx, instNum);
   return 1;
}

static duk_ret_t duk_rbus_remove_table_row(duk_context *ctx)
{
   if (!duk_is_string(ctx, 0))
   {
      duk_error(ctx, DUK_ERR_TYPE_ERROR, "rowName must be a string");
   }
   rbusHandle_t handle = get_rbus_handle(ctx);
   if (!handle)
   {
      duk_error(ctx, DUK_ERR_ERROR, "No rbus handle found");
   }
   const char *rowName = duk_get_string(ctx, 0);
   rbusError_t err = rbusTable_removeRow(handle, rowName); // 2 params
   if (err != RBUS_ERROR_SUCCESS)
   {
      duk_error(ctx, DUK_ERR_ERROR, "rbusTable_removeRow failed: %d", err);
   }
   duk_push_true(ctx);
   return 1;
}

static duk_ret_t duk_rbus_register_table_row(duk_context *ctx)
{
   if (!duk_is_string(ctx, 0) || !duk_is_number(ctx, 1))
   {
      duk_error(ctx, DUK_ERR_TYPE_ERROR, "tableName must be a string, instNum must be a number");
   }
   rbusHandle_t handle = get_rbus_handle(ctx);
   if (!handle)
   {
      duk_error(ctx, DUK_ERR_ERROR, "No rbus handle found");
   }
   const char *tableName = duk_get_string(ctx, 0);
   uint32_t instNum = duk_get_uint(ctx, 1);
   const char *aliasName = duk_is_string(ctx, 2) ? duk_get_string(ctx, 2) : NULL;
   rbusError_t err = rbusTable_registerRow(handle, tableName, instNum, aliasName); // 4 params
   if (err != RBUS_ERROR_SUCCESS)
   {
      duk_error(ctx, DUK_ERR_ERROR, "rbusTable_registerRow failed: %d", err);
   }
   duk_push_true(ctx);
   return 1;
}

static duk_ret_t duk_rbus_set_table_property(duk_context *ctx)
{
   if (!duk_is_string(ctx, 0) || !duk_is_string(ctx, 1))
   {
      duk_error(ctx, DUK_ERR_TYPE_ERROR, "rowName and propertyName must be strings");
   }
   rbusHandle_t handle = get_rbus_handle(ctx);
   if (!handle)
   {
      duk_error(ctx, DUK_ERR_ERROR, "No rbus handle found");
   }
   const char *rowName = duk_get_string(ctx, 0);
   const char *propertyName = duk_get_string(ctx, 1);
   char *fullPath = malloc(strlen(rowName) + strlen(propertyName) + 2);
   snprintf(fullPath, strlen(rowName) + strlen(propertyName) + 2, "%s.%s", rowName, propertyName);
   rbusValue_t value;
   rbusValue_Init(&value);
   if (duk_is_string(ctx, 1))
   {
      rbusValue_SetString(value, duk_get_string(ctx, 1));
   }
   else if (duk_is_number(ctx, 1))
   {
      double num = duk_get_number(ctx, 1);
      if (num == (int32_t)num)
      {
         rbusValue_SetInt32(value, (int32_t)num);
      }
      else
      {
         rbusValue_SetDouble(value, num);
      }
   }
   else if (duk_is_boolean(ctx, 1))
   {
      rbusValue_SetBoolean(value, duk_get_boolean(ctx, 1));
   }
   else
   {
      rbusValue_Release(value);
      free(fullPath);
      duk_error(ctx, DUK_ERR_TYPE_ERROR, "Unsupported value type");
   }
   printf("fullPath: %s\n", fullPath);
   rbusError_t err = rbus_set(handle, fullPath, value, NULL); // 4 params
   rbusValue_Release(value);
   free(fullPath);
   if (err != RBUS_ERROR_SUCCESS)
   {
      duk_error(ctx, DUK_ERR_ERROR, "Failed to set table property: %d", err);
   }
   duk_push_true(ctx);
   return 1;
}

static duk_ret_t duk_rbus_create_data_model(duk_context *ctx)
{
   if (!duk_is_array(ctx, 0))
   {
      duk_error(ctx, DUK_ERR_TYPE_ERROR, "elements must be an array");
   }
   rbusHandle_t handle = get_rbus_handle(ctx);
   if (!handle)
   {
      duk_error(ctx, DUK_ERR_ERROR, "No rbus handle found");
   }
   duk_size_t numElements = duk_get_length(ctx, 0);
   if (numElements == 0)
   {
      duk_error(ctx, DUK_ERR_TYPE_ERROR, "elements array cannot be empty");
   }
   rbusDataElement_t *elements = malloc(numElements * sizeof(rbusDataElement_t));
   // data_element_info_t *elements = malloc(numElements * sizeof(data_element_info_t));
   for (duk_size_t i = 0; i < numElements; i++)
   {
      duk_get_prop_index(ctx, 0, i);
      if (!duk_is_object(ctx, -1))
      {
         for (duk_size_t j = 0; j < i; j++)
            free(elements[j].name);
         free(elements);
         duk_error(ctx, DUK_ERR_TYPE_ERROR, "Each element must be an object");
      }
      duk_get_prop_string(ctx, -1, "name");
      if (!duk_is_string(ctx, -1))
      {
         for (duk_size_t j = 0; j < i; j++)
            free(elements[j].name);
         free(elements);
         duk_error(ctx, DUK_ERR_TYPE_ERROR, "Element name must be a string");
      }
      elements[i].name = strdup(duk_get_string(ctx, -1));
      duk_pop(ctx);
      duk_get_prop_string(ctx, -1, "type");
      if (!duk_is_string(ctx, -1))
      {
         for (duk_size_t j = 0; j < i; j++)
            free(elements[j].name);
         free(elements);
         duk_error(ctx, DUK_ERR_TYPE_ERROR, "Element type must be a string");
      }
      const char *type = duk_get_string(ctx, -1);
      duk_pop(ctx);
      // elements[i].element.name = elements[i].name;
      if (strcmp(type, "property") == 0)
      {
         elements[i].type = RBUS_ELEMENT_TYPE_PROPERTY;
         elements[i].cbTable.getHandler = property_get_handler;
         elements[i].cbTable.setHandler = property_set_handler;
      }
      else if (strcmp(type, "table") == 0)
      {
         elements[i].type = RBUS_ELEMENT_TYPE_TABLE;
         elements[i].cbTable.tableAddRowHandler = table_add_row_handler;
         elements[i].cbTable.tableRemoveRowHandler = table_remove_row_handler;
      }
      else
      {
         for (duk_size_t j = 0; j < i; j++)
            free(elements[j].name);
         free(elements);
         duk_error(ctx, DUK_ERR_TYPE_ERROR, "Invalid element type; use 'property' or 'table'");
      }
      duk_pop(ctx);
   }
   rbusError_t err = rbus_regDataElements(handle, numElements, elements); // 3 params
   for (duk_size_t i = 0; i < numElements; i++)
      free(elements[i].name);
   free(elements);
   if (err != RBUS_ERROR_SUCCESS)
   {
      duk_error(ctx, DUK_ERR_ERROR, "rbus_regDataElements failed: %d", err);
   }
   duk_push_true(ctx);
   return 1;
}

// Function registration array
static const duk_function_list_entry rbus_functions[] = {
    {"close", duk_rbus_close, 0},
    {"setValue", duk_rbus_set_value, 2},
    {"getValue", duk_rbus_get_value, 1},
    {"subscribeToEvent", duk_rbus_subscribe_to_event, 3},
    {"subscribe", duk_rbus_subscribe, 2},
    {"invokeMethod", duk_rbus_invoke_method, 2},
    {"invokeMethodAsync", duk_rbus_invoke_method_async, 3},
    {"registerObj", duk_rbus_register_obj, 1},
    {"unregisterObj", duk_rbus_unregister_obj, 1},
    {"addElement", duk_rbus_add_element, 2},
    {"removeElement", duk_rbus_remove_element, 2},
    {"tableAddRow", duk_rbus_table_add_row, 2},
    {"tableRemoveRow", duk_rbus_table_remove_row, 2},
    {"registerEvent", duk_rbus_register_event, 2},
    {"unregisterEvent", duk_rbus_unregister_event, 2},
    {"publishEvent", duk_rbus_publish_event, 3},
    {"addTableRow", duk_rbus_add_table_row, 2},
    {"removeTableRow", duk_rbus_remove_table_row, 1},
    {"registerTableRow", duk_rbus_register_table_row, 3},
    {"setTableProperty", duk_rbus_set_table_property, 3},
    {"createDataModel", duk_rbus_create_data_model, 1},
    {"discoverRegisteredComponents", duk_rbus_discover_registered_components, 0},
    {NULL, NULL, 0}};

duk_idx_t register_function(duk_context *ctx)
{

#if 0
   for (const duk_function_list_entry *f = rbus_functions; f->key != NULL; f++)
   {
      duk_push_c_function(ctx, f->value, f->nargs);
      duk_put_global_string(ctx, f->key);
   }
#endif

   duk_push_string(ctx, "rbus");
   duk_push_object(ctx);
   duk_put_function_list(ctx, -1, rbus_functions);
   duk_put_global_string(ctx, "rbus");

   // duk_put_prop(ctx, -3);

   duk_rbus_open(ctx, "littlesheens");

   return 1;
}
