#include <stdio.h>
#include <string.h>
#include <rbus.h>
#include <rbuscore.h>
#include <duktape.h>
#include <register.h>
#include <pthread.h>
#include <unistd.h>

// Global Duktape context for handlers
static duk_context *global_ctx = NULL;
static rbusHandle_t rbus_handle = NULL;

// Structure to hold rbus handle in Duktape context stash
typedef struct {
   rbusHandle_t handle;
} rbus_context_t;

typedef struct {
   duk_context *ctx;
   char *key; // Unique key for storing the JS callback
} async_data_t;

// Subscription structure
typedef struct {
   duk_context *ctx;
   char callback_key[32]; // Stash key for callback
   int sub_id;            // Unique subscription ID
   char *objectName;      // For rbus_subscribeToEvent
   char *eventName;       // For both APIs
   int is_event_sub;      // 1 for rbusEvent_Subscribe, 0 for rbus_subscribeToEvent
} rbus_subscription_t;

// Global subscription array (simplified, use hash table for production)
#define MAX_SUBSCRIPTIONS 100
static rbus_subscription_t subscriptions[MAX_SUBSCRIPTIONS];
static int sub_count = 0;

// Mutex for Duktape context access
static pthread_mutex_t rbus_callback_mutex = PTHREAD_MUTEX_INITIALIZER;

// Initialize subscriptions
static void init_subscriptions() {
   for (int i = 0; i < MAX_SUBSCRIPTIONS; i++) {
      subscriptions[i].ctx = NULL;
      // subscriptions[i].callback_key = NULL;
      subscriptions[i].sub_id = 0;
      subscriptions[i].objectName = NULL;
      subscriptions[i].eventName = NULL;
   }
}

// Cleanup subscriptions (call on shutdown)
static void cleanup_rbus_subscriptions() {
   for (int i = 0; i < MAX_SUBSCRIPTIONS; i++) {
      if (subscriptions[i].sub_id) {
         // Note: rbuscore.h may not provide unsubscribe; adjust if available
         duk_push_global_stash(subscriptions[i].ctx);
         duk_del_prop_string(subscriptions[i].ctx, -1, subscriptions[i].callback_key);
         duk_pop(subscriptions[i].ctx);
         free(subscriptions[i].objectName);
         free(subscriptions[i].eventName);
         subscriptions[i].sub_id = 0;
      }
   }
   sub_count = 0;
}

static rbus_subscription_t *alloc_subscription(void) {
   // Find free subscription slot
   for (int i = 0; i < MAX_SUBSCRIPTIONS; i++) {
      if (!subscriptions[i].sub_id) {
         subscriptions[i].sub_id = i + 1;
         sub_count++;
         return &subscriptions[i];
      }
   }
   return NULL;
}

static const char *event_type_to_string(rbusEventType_t type) {
   switch (type) {
   case RBUS_EVENT_OBJECT_CREATED:
      return "object_created";
   case RBUS_EVENT_OBJECT_DELETED:
      return "object_deleted";
   case RBUS_EVENT_VALUE_CHANGED:
      return "value_changed";
   case RBUS_EVENT_GENERAL:
      return "general";
   case RBUS_EVENT_INITIAL_VALUE:
      return "initial_value";
   case RBUS_EVENT_INTERVAL:
      return "interval";
   case RBUS_EVENT_DURATION_COMPLETE:
      return "duration_complete";
   default:
      return "unknown";
   }
}
// Helper to get rbus handle from stash
static rbusHandle_t get_rbus_handle(duk_context *ctx) {
#if 0   
   duk_push_global_stash(ctx);
   if (duk_has_prop_string(ctx, -1, "rbus_handle")) {
      duk_get_prop_string(ctx, -1, "rbus_handle");
      rbus_context_t *rbus_ctx = (rbus_context_t *)duk_get_pointer(ctx, -1);
      duk_pop_2(ctx);
      return rbus_ctx->handle;
   }
   duk_pop(ctx);
   return NULL;
#else
   if (rbus_handle == NULL) {
      rbusError_t err = rbus_open(&rbus_handle, "littlesheens");
      if (err != RBUS_ERROR_SUCCESS) {
         fprintf(stderr, "rbus_open failed: %d", err);
         return NULL;
      }
   }
   return rbus_handle;

#endif
}

#if 1
// Helper to store rbus handle in stash
static void store_rbus_handle(duk_context *ctx, rbusHandle_t handle) {
   rbus_context_t *rbus_ctx = malloc(sizeof(rbus_context_t));
   rbus_ctx->handle = handle;
   duk_push_global_stash(ctx);
   duk_push_pointer(ctx, rbus_ctx);
   duk_put_prop_string(ctx, -2, "rbus_handle");
   duk_pop(ctx);
}
#endif

static void rbusObject_to_jsObject(duk_context *ctx, rbusObject_t obj) {
   duk_push_object(ctx); // Create a new JS object
   rbusProperty_t prop = rbusObject_GetProperties(obj);
   while (prop) {
      const char *name = rbusProperty_GetName(prop);
      rbusValue_t val = rbusProperty_GetValue(prop);
      switch (rbusValue_GetType(val)) {
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

static int event_callback(const char *object_name, const char *event_name, rbusMessage message, void *user_data) {
   duk_context *ctx = (duk_context *)user_data;
   duk_push_global_stash(ctx);
   duk_get_prop_string(ctx, -1, event_name); // Retrieve callback
   if (duk_is_function(ctx, -1)) {
      // Push arguments
      duk_push_string(ctx, event_name);
      const char *payload = NULL;

      if (rbusMessage_GetString(message, &payload) == RT_OK) {
         duk_push_string(ctx, payload);
      } else {
         duk_push_null(ctx);
      }

      // Call callback with 2 arguments
      if (duk_pcall(ctx, 2) != DUK_EXEC_SUCCESS) {
         fprintf(stderr, "Callback execution failed: %s\n", duk_safe_to_string(ctx, -1));
         duk_pop(ctx);
      } else {
         duk_pop(ctx);
      }
   } else {
      duk_pop(ctx); // Pop non-function
   }
   duk_pop(ctx); // Pop stash

   return 0;
}

static int rbus_event_callback(const char *object_name, const char *event_name, rbusMessage response, void *user_data) {
   rbus_subscription_t *sub = (rbus_subscription_t *)user_data;
   if (!sub || !sub->ctx)
      return 1;

   pthread_mutex_lock(&rbus_callback_mutex);

   // Push callback from stash
   duk_push_global_stash(sub->ctx);
   duk_get_prop_string(sub->ctx, -1, sub->callback_key);
   if (!duk_is_function(sub->ctx, -1)) {
      duk_pop_2(sub->ctx);
      pthread_mutex_unlock(&rbus_callback_mutex);
      return 1;
   }

   // Create event object
   duk_push_object(sub->ctx);

   // Add event properties
   duk_push_string(sub->ctx, object_name ? object_name : "");
   duk_put_prop_string(sub->ctx, -2, "objectName");

   duk_push_string(sub->ctx, event_name ? event_name : "");
   duk_put_prop_string(sub->ctx, -2, "eventName");

   // Add response data
   if (response) {
      // NEW: Use rbusMessage_GetString (assumed available)
      const char *data = NULL;
      if (rbusMessage_GetString(response, &data) == RBUSCORE_SUCCESS && data) {
         duk_push_string(sub->ctx, data);
         // Attempt to parse as JSON, fallback to raw string
         if (duk_peval_string(sub->ctx, data) == 0 && duk_is_object(sub->ctx, -1)) {
            // JSON parsed successfully
         } else {
            duk_pop(sub->ctx);               // Pop eval result
            duk_push_string(sub->ctx, data); // Use raw string
         }
      } else {
         duk_push_null(sub->ctx);
      }
      duk_put_prop_string(sub->ctx, -2, "data");
   } else {
      duk_push_null(sub->ctx);
      duk_put_prop_string(sub->ctx, -2, "data");
   }

   // Call callback
   if (duk_pcall(sub->ctx, 1) != DUK_EXEC_SUCCESS) {
      fprintf(stderr, "RBus callback error: %s\n", duk_safe_to_string(sub->ctx, -1));
   }
   duk_pop_2(sub->ctx); // Pop result and stash

   pthread_mutex_unlock(&rbus_callback_mutex);

   return 0;
}
static void rbus_event_handler(rbusHandle_t handle, rbusEvent_t const *event, rbusEventSubscription_t *subscription) {
   //printf("%s() subscription->userData: %p\n", __FUNCTION__, subscription->userData);

   rbus_subscription_t *sub = (rbus_subscription_t *)subscription->userData;
   if (!sub || !sub->ctx)
      return;

   pthread_mutex_lock(&rbus_callback_mutex);

   duk_push_global_stash(sub->ctx);
   duk_get_prop_string(sub->ctx, -1, sub->callback_key);
   if (!duk_is_function(sub->ctx, -1)) {
      duk_pop_2(sub->ctx);
      pthread_mutex_unlock(&rbus_callback_mutex);
      return;
   }

   duk_push_object(sub->ctx);

   duk_push_string(sub->ctx, event->name ? event->name : "");
   duk_put_prop_string(sub->ctx, -2, "name");

   //duk_push_int(sub->ctx, event->type);
   duk_push_string(sub->ctx, event_type_to_string(event->type));
   duk_put_prop_string(sub->ctx, -2, "type");

   if (event->data) {
#if 0
      rbusObject_to_jsObject(sub->ctx, event->data);
#else
      duk_push_object(sub->ctx);
      rbusObject_t obj = event->data;
      rbusProperty_t prop = rbusObject_GetProperties(obj);
      while (prop) {
         const char *key = rbusProperty_GetName(prop);
         rbusValue_t val = rbusProperty_GetValue(prop);
         if (key && val) {
            switch (rbusValue_GetType(val)) {
            case RBUS_STRING:
               duk_push_string(sub->ctx, rbusValue_GetString(val, NULL));
               break;
            case RBUS_INT32:
               duk_push_int(sub->ctx, rbusValue_GetInt32(val));
               break;
            case RBUS_DOUBLE:
               duk_push_number(sub->ctx, rbusValue_GetDouble(val));
               break;
            case RBUS_BOOLEAN:
               duk_push_boolean(sub->ctx, rbusValue_GetBoolean(val));
               break;
            default:
               duk_push_null(sub->ctx);
               break;
            }
            duk_put_prop_string(sub->ctx, -2, key);
         }
         prop = rbusProperty_GetNext(prop);
      }
      duk_put_prop_string(sub->ctx, -2, "data");
#endif
   } else {
      duk_push_null(sub->ctx);
      duk_put_prop_string(sub->ctx, -2, "data");
   }

   if (duk_pcall(sub->ctx, 1) != DUK_EXEC_SUCCESS) {
      fprintf(stderr, "RBus event handler error: %s\n", duk_safe_to_string(sub->ctx, -1));
   }
   duk_pop_2(sub->ctx);

   pthread_mutex_unlock(&rbus_callback_mutex);
}

static void event_handler(rbusHandle_t handle, rbusEvent_t const *event, rbusEventSubscription_t *subscription) {
   duk_context *ctx = (duk_context *)subscription->userData;
   duk_push_global_stash(ctx);
   duk_get_prop_string(ctx, -1, event->name); // Retrieve callback
   if (duk_is_function(ctx, -1)) {
      // Push arguments
      duk_push_string(ctx, event->name); // eventName
      if (event->data) {
         rbusObject_to_jsObject(ctx, event->data); // Event data
      } else {
         duk_push_null(ctx);
      }

      // Call callback with 2 arguments
      if (duk_pcall(ctx, 2) != DUK_EXEC_SUCCESS) {
         fprintf(stderr, "Callback execution failed: %s\n", duk_safe_to_string(ctx, -1));
         duk_pop(ctx);
      } else {
         duk_pop(ctx);
      }
   } else {
      duk_pop(ctx); // Pop non-function
   }
   duk_pop(ctx); // Pop stash
}

static duk_ret_t duk_rbus_open(duk_context *ctx, const char *componentName) {

#if 0
   rbusCoreError_t err = rbus_openBrokerConnection(componentName);
   if (err != RBUSCORE_SUCCESS) {
      duk_error(ctx, DUK_ERR_ERROR, "rbus_open failed: %d", err);
   }

#else

   rbusError_t err;
   rbusHandle_t handle;
   err = rbus_open(&handle, componentName);
   if (err != RBUS_ERROR_SUCCESS) {
      duk_error(ctx, DUK_ERR_ERROR, "rbus_open failed: %d", err);
   }
   store_rbus_handle(ctx, handle);

#endif

   global_ctx = ctx; // Set global context

   duk_push_true(ctx);
   return 1;
}

static duk_ret_t duk_rbus_set_value(duk_context *ctx) {
   if (!duk_is_string(ctx, 0)) {
      duk_error(ctx, DUK_ERR_TYPE_ERROR, "propertyName must be a string");
   }
   rbusHandle_t handle = get_rbus_handle(ctx);
   if (!handle) {
      duk_error(ctx, DUK_ERR_ERROR, "No rbus handle found");
   }
   const char *propertyName = duk_get_string(ctx, 0);
   rbusValue_t value;
   rbusValue_Init(&value);

   if (duk_is_string(ctx, 1)) {
      rbusValue_SetString(value, duk_get_string(ctx, 1));
   } else if (duk_is_number(ctx, 1)) {
      rbusValue_SetDouble(value, duk_get_number(ctx, 1));
   } else if (duk_is_boolean(ctx, 1)) {
      rbusValue_SetBoolean(value, duk_get_boolean(ctx, 1));
   } else {
      rbusValue_Release(value);
      duk_error(ctx, DUK_ERR_TYPE_ERROR, "Unsupported value type");
   }

   rbusError_t err = rbus_set(handle, propertyName, value, NULL);
   rbusValue_Release(value);
   if (err != RBUS_ERROR_SUCCESS) {
      duk_error(ctx, DUK_ERR_ERROR, "rbus_set failed: %d", err);
   }
   duk_push_true(ctx);
   return 1;
}

static duk_ret_t duk_rbus_get_value(duk_context *ctx) {
   if (!duk_is_string(ctx, 0)) {
      duk_error(ctx, DUK_ERR_TYPE_ERROR, "propertyName must be a string");
   }
   rbusHandle_t handle = get_rbus_handle(ctx);
   if (!handle) {
      duk_error(ctx, DUK_ERR_ERROR, "No rbus handle found");
   }
   const char *propertyName = duk_get_string(ctx, 0);
   rbusValue_t value;
   rbusError_t err = rbus_get(handle, propertyName, &value);
   if (err != RBUS_ERROR_SUCCESS) {
      duk_error(ctx, DUK_ERR_ERROR, "rbus_get failed: %d", err);
   }
   switch (rbusValue_GetType(value)) {
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


static rbusObject_t jsObject_to_rbusObject(duk_context *ctx, duk_idx_t idx) {
   rbusObject_t obj;
   rbusObject_Init(&obj, NULL);                      // Initialize an unnamed object
   duk_enum(ctx, idx, DUK_ENUM_OWN_PROPERTIES_ONLY); // Iterate over object properties
   while (duk_next(ctx, -1, 1)) { // Get key-value pairs
      const char *key = duk_get_string(ctx, -2);
      if (duk_is_string(ctx, -1)) {
         rbusObject_SetPropertyString(obj, key, duk_get_string(ctx, -1));
      } else if (duk_is_number(ctx, -1)) {
         rbusObject_SetPropertyDouble(obj, key, duk_get_number(ctx, -1));
      } else if (duk_is_boolean(ctx, -1)) {
         rbusObject_SetPropertyBoolean(obj, key, duk_get_boolean(ctx, -1));
      } // Add more types as needed
      duk_pop_2(ctx); // Remove key and value
   }
   duk_pop(ctx); // Remove enumerator
   return obj;
}

static duk_ret_t duk_rbus_invoke_method(duk_context *ctx) {
   rbusHandle_t handle = get_rbus_handle(ctx);
   if (!handle) {
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
   if (err != RBUS_ERROR_SUCCESS) {
      duk_error(ctx, DUK_ERR_ERROR, "rbusMethod_Invoke failed: %d", err);
   }

   rbusObject_to_jsObject(ctx, outParams); // Convert response to JS object
   rbusObject_Release(outParams);          // Clean up output parameters
   return 1;                               // Return one value (the result object)
}

// static void async_method_callback(rbusHandle_t handle, rbusError_t error, rbusObject_t response, void *userData)
static void async_method_callback(rbusHandle_t handle, char const *methodName, rbusError_t error, rbusObject_t params) {
#if 0   
   async_data_t *data = (async_data_t *)userData;
   duk_context *ctx = data->ctx;

   // Retrieve the JavaScript callback from the stash
   duk_push_global_stash(ctx);
   duk_get_prop_string(ctx, -1, data->key);
   if (duk_is_function(ctx, -1)) {
      // Push error and result in (err, result) style
      duk_push_int(ctx, error); // Error code (0 if success)
      if (error == RBUS_ERROR_SUCCESS) {
         rbusObject_to_jsObject(ctx, response); // Convert response to JS object
      } else {
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

static duk_ret_t duk_rbus_invoke_method_async(duk_context *ctx) {
   rbusHandle_t handle = get_rbus_handle(ctx);
   if (!handle) {
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

   if (err != RBUS_ERROR_SUCCESS) {
      free(data->key);
      free(data);
      duk_error(ctx, DUK_ERR_ERROR, "rbusMethod_InvokeAsync failed: %d", err);
   }

   duk_push_true(ctx); // Return true to indicate success
   return 1;
}

static int callback(const char *destination, const char *method, rbusMessage message, void *user_data, rbusMessage *response, const rtMessageHeader *hdr) {
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

static duk_ret_t duk_rbus_register_obj(duk_context *ctx) {
   if (!duk_is_string(ctx, 0)) {
      duk_error(ctx, DUK_ERR_TYPE_ERROR, "objectName must be a string");
   }
   const char *objectName = duk_get_string(ctx, 0);
   rbusCoreError_t err = rbus_registerObj(objectName, callback, NULL);

   if (err != RBUSCORE_SUCCESS) {
      duk_error(ctx, DUK_ERR_ERROR, "rbus_registerObj failed: %d", err);
   }
   duk_push_true(ctx);
   return 1;
}

static duk_ret_t duk_rbus_unregister_obj(duk_context *ctx) {
   if (!duk_is_string(ctx, 0)) {
      duk_error(ctx, DUK_ERR_TYPE_ERROR, "objectName must be a string");
   }
   const char *objectName = duk_get_string(ctx, 0);
   rbusCoreError_t err = rbus_unregisterObj(objectName);
   if (err != RBUSCORE_SUCCESS) {
      duk_error(ctx, DUK_ERR_ERROR, "rbus_unregisterObj failed: %d", err);
   }
   duk_push_true(ctx);
   return 1;
}

static duk_ret_t duk_rbus_add_element(duk_context *ctx) {
   if (!duk_is_string(ctx, 0) || !duk_is_string(ctx, 1)) {
      duk_error(ctx, DUK_ERR_TYPE_ERROR, "objectName and elementName must be strings");
   }
   const char *objectName = duk_get_string(ctx, 0);
   const char *elementName = duk_get_string(ctx, 1);
   rbusCoreError_t err = rbus_addElement(objectName, elementName);
   if (err != RBUSCORE_SUCCESS) {
      duk_error(ctx, DUK_ERR_ERROR, "rbus_addElement failed: %d", err);
   }
   duk_push_true(ctx);
   return 1;
}

static duk_ret_t duk_rbus_remove_element(duk_context *ctx) {
   if (!duk_is_string(ctx, 0) || !duk_is_string(ctx, 1)) {
      duk_error(ctx, DUK_ERR_TYPE_ERROR, "objectName and elementName must be strings");
   }
   const char *objectName = duk_get_string(ctx, 0);
   const char *elementName = duk_get_string(ctx, 1);
   rbusCoreError_t err = rbus_removeElement(objectName, elementName);
   if (err != RBUSCORE_SUCCESS) {
      duk_error(ctx, DUK_ERR_ERROR, "rbus_removeElement failed: %d", err);
   }
   duk_push_true(ctx);
   return 1;
}

static duk_ret_t duk_rbus_table_add_row(duk_context *ctx) {
   if (!duk_is_string(ctx, 0) || !duk_is_string(ctx, 1)) {
      duk_error(ctx, DUK_ERR_TYPE_ERROR, "tableName and rowName must be strings");
   }
   rbusHandle_t handle = get_rbus_handle(ctx);
   if (!handle) {
      duk_error(ctx, DUK_ERR_ERROR, "No rbus handle found");
   }
   const char *tableName = duk_get_string(ctx, 0);
   const char *rowName = duk_get_string(ctx, 1);
   u_int32_t instNum;

   rbusError_t err = rbusTable_addRow(handle, tableName, rowName, &instNum);
   if (err != RBUS_ERROR_SUCCESS) {
      duk_error(ctx, DUK_ERR_ERROR, "rbusTable_addRow failed: %d", err);
   }
   duk_push_int(ctx, instNum);
   return 1;
}

static duk_ret_t duk_rbus_table_remove_row(duk_context *ctx) {
   if (!duk_is_string(ctx, 0) || !duk_is_string(ctx, 1)) {
      duk_error(ctx, DUK_ERR_TYPE_ERROR, "tableName and rowName must be strings");
   }
   rbusHandle_t handle = get_rbus_handle(ctx);
   if (!handle) {
      duk_error(ctx, DUK_ERR_ERROR, "No rbus handle found");
   }
   const char *rowName = duk_get_string(ctx, 0);
   rbusError_t err = rbusTable_removeRow(handle, rowName);
   if (err != RBUS_ERROR_SUCCESS) {
      duk_error(ctx, DUK_ERR_ERROR, "rbusTable_removeRow failed: %d", err);
   }
   duk_push_true(ctx);
   return 1;
}

int sub1_callback(const char *object, const char *event, const char *listener, int added, const rbusMessage filter, void *data) {
   (void)filter;
   printf("Received sub_callback object=%s event=%s listerner=%s added=%d data=%p\n", object, event, listener, added, data);
   fflush(stdout);

   duk_context *ctx = (duk_context *)data;
   duk_push_global_stash(ctx);
   duk_get_prop_string(ctx, -1, event);
   if (duk_is_function(ctx, -1)) {
      duk_push_string(ctx, event);
#if 0      
      rbusProperty_t prop = rbusObject_GetProperty(event->data, "value");
      if (prop) {
         rbusValue_t value = rbusProperty_GetValue(prop);
         if (value && rbusValue_GetType(value) == RBUS_STRING) {
            duk_push_string(ctx, rbusValue_GetString(value, NULL));
         } else {
            duk_push_null(ctx);
         }
      } else {
         duk_push_null(ctx);
      }
#endif
      duk_call(ctx, 2);
   }
   duk_pop_2(ctx); // Pop function and stash

   return 0;
}

static duk_ret_t duk_rbus_register_event(duk_context *ctx) {
   // if (!duk_is_string(ctx, 0) || !duk_is_string(ctx, 1) || !duk_is_function(ctx, 2))
   if (!duk_is_string(ctx, 0) || !duk_is_string(ctx, 1)) {
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

   if (!duk_is_string(ctx, 0)) {
      duk_error(ctx, DUK_ERR_TYPE_ERROR, "eventName must be a string");
   }
   // rbusError_t err = rbus_registerEvent(objectName, eventName, sub1_callback, ctx);
   rbusCoreError_t err = rbus_registerEvent(objectName, eventName, NULL, ctx);
   if (err != RBUSCORE_SUCCESS) {
      duk_error(ctx, DUK_ERR_ERROR, "rbus_registerEvent failed: %d", err);
   }
   duk_push_true(ctx);
   return 1;
}

static duk_ret_t duk_rbus_unregister_event(duk_context *ctx) {
   if (!duk_is_string(ctx, 0)) {
      duk_error(ctx, DUK_ERR_TYPE_ERROR, "eventName must be a string");
   }

   const char *objectName = duk_get_string(ctx, 0);
   const char *eventName = duk_get_string(ctx, 1);
   rbusCoreError_t err = rbus_unregisterEvent(objectName, eventName);
   if (err != RBUSCORE_SUCCESS) {
      duk_error(ctx, DUK_ERR_ERROR, "rbus_unregisterEvent failed: %d", err);
   }
   duk_push_true(ctx);
   return 1;
}

static void handlerAsyncSub(
   rbusHandle_t handle,
   rbusEventSubscription_t *subscription,
   rbusError_t error) {
   printf("handlerAsyncSub event=%s\n", subscription->eventName);

   (void)(handle);
}

static duk_ret_t duk_rbus_subscribe(duk_context *ctx) {
   if (!duk_is_string(ctx, 0) || !duk_is_function(ctx, 1)) {
      duk_error(ctx, DUK_ERR_TYPE_ERROR, "eventName must be a string, callback must be a function");
   }
   rbusHandle_t handle = get_rbus_handle(ctx); // Retrieve handle from stash
   if (!handle) {
      duk_error(ctx, DUK_ERR_ERROR, "No rbus handle found");
   }
   const char *eventName = duk_get_string(ctx, 0);

   // Store callback in stash
   duk_push_global_stash(ctx);
   duk_dup(ctx, 1); // Duplicate callback
   duk_put_prop_string(ctx, -2, eventName);
   duk_pop(ctx);

   rbusError_t err = rbusEvent_Subscribe(handle, eventName, event_handler, ctx, 0);
   if (err != RBUS_ERROR_SUCCESS) {
      duk_error(ctx, DUK_ERR_ERROR, "rbusEvent_Subscribe failed: %d", err);
   }
   duk_push_true(ctx);
   return 1;
}

static duk_ret_t duk_rbus_event_subscribe(duk_context *ctx) {
   // Validate parameters
   if (!duk_is_string(ctx, 0))
      duk_error(ctx, DUK_ERR_TYPE_ERROR, "eventName must be a string");
   if (!duk_is_function(ctx, 1))
      duk_error(ctx, DUK_ERR_TYPE_ERROR, "callback must be a function");

   const char *eventName = duk_get_string(ctx, 0);

   rbusHandle_t rbus_handle = get_rbus_handle(ctx); // Retrieve handle from stash

   // Check RBus handle
   if (!rbus_handle)
      duk_error(ctx, DUK_ERR_ERROR, "RBus handle not initialized");


   // Store callback in stash
   rbus_subscription_t *sub = alloc_subscription();
   snprintf(sub->callback_key, sizeof(sub->callback_key), "rbus_event_sub_%d", sub->sub_id);
   duk_push_global_stash(ctx);
   duk_dup(ctx, 1); // Duplicate callback
   duk_put_prop_string(ctx, -2, sub->callback_key);
   duk_pop(ctx);

   // Initialize subscription
   sub->ctx = ctx;
   sub->objectName = NULL; // No objectName for rbusEvent_Subscribe
   sub->eventName = strdup(eventName);
   sub->is_event_sub = 1;

   // Create subscription struct for RBus
   //rbusEventSubscription_t *rbus_sub = malloc(sizeof(rbusEventSubscription_t)); // rbus_sub = {0};
   //rbus_sub->eventName = eventName;
   //rbus_sub->userData = sub;

   //printf("%s() rbus_sub->userData: %p\n", __FUNCTION__, rbus_sub->userData);

   // Subscribe to RBus event
   rbusError_t err = rbusEvent_Subscribe(rbus_handle, eventName, rbus_event_handler, sub, 0);
   if (err != RBUS_ERROR_SUCCESS) {
      free(sub->eventName);
      sub->sub_id = 0;
      duk_push_global_stash(ctx);
      duk_del_prop_string(ctx, -1, sub->callback_key);
      duk_pop(ctx);
      duk_error(ctx, DUK_ERR_ERROR, "rbusEvent_Subscribe failed: %d", err);
   }

   duk_push_int(ctx, sub->sub_id); // Return subscription ID
   return 1;
}

#if 0
static duk_ret_t duk_rbus_subscribe_to_event(duk_context *ctx) {
   if (!duk_is_string(ctx, 0) || !duk_is_string(ctx, 1) || !duk_is_function(ctx, 2)) {
      duk_error(ctx, DUK_ERR_TYPE_ERROR, "eventName must be a string and callback must be a function");
   }
   const char *objectName = duk_get_string(ctx, 0);
   const char *eventName = duk_get_string(ctx, 1);

   // Store callback in stash
   duk_push_global_stash(ctx);
   duk_dup(ctx, 2);
   duk_put_prop_string(ctx, -2, eventName);
   duk_pop(ctx);

   rbusCoreError_t err = rbus_subscribeToEvent(objectName, eventName, event_callback, NULL, ctx, NULL);
   if (err != RBUSCORE_SUCCESS) {
      duk_error(ctx, DUK_ERR_ERROR, "rbus_subscribeToEvent failed: %d", err);
   }
   duk_push_true(ctx);
   return 1;
}

#endif

static duk_ret_t duk_rbus_subscribe_to_event(duk_context *ctx) {
   // Validate parameters
   if (!duk_is_string(ctx, 0))
      duk_error(ctx, DUK_ERR_TYPE_ERROR, "objectName must be a string");
   if (!duk_is_string(ctx, 1))
      duk_error(ctx, DUK_ERR_TYPE_ERROR, "eventName must be a string");
   if (!duk_is_function(ctx, 2))
      duk_error(ctx, DUK_ERR_TYPE_ERROR, "callback must be a function");

   const char *objectName = duk_get_string(ctx, 0);
   const char *eventName = duk_get_string(ctx, 1);

   // Store callback in stash
   rbus_subscription_t *sub = alloc_subscription();
   snprintf(sub->callback_key, sizeof(sub->callback_key), "rbus_sub_%d", sub->sub_id);
   duk_push_global_stash(ctx);
   duk_dup(ctx, 2); // Duplicate callback
   duk_put_prop_string(ctx, -2, sub->callback_key);
   duk_pop(ctx);

   // Initialize subscription
   sub->ctx = ctx;
   sub->objectName = strdup(objectName);
   sub->eventName = strdup(eventName);

   // Subscribe to RBus event
   int providerError = 0;
   rbusCoreError_t err = rbus_subscribeToEvent(objectName, eventName, &rbus_event_callback, NULL, sub, &providerError);
   if (err != RBUSCORE_SUCCESS || providerError != 0) {
      free(sub->objectName);
      free(sub->eventName);
      sub->sub_id = 0;
      duk_push_global_stash(ctx);
      duk_del_prop_string(ctx, -1, sub->callback_key);
      duk_pop(ctx);
      duk_error(ctx, DUK_ERR_ERROR, "subscribeToEvent failed: coreError=%d, providerError=%d", err, providerError);
   }

   duk_push_int(ctx, sub->sub_id); // Return subscription ID

   return 1;
}

static duk_ret_t duk_rbus_publish_event(duk_context *ctx) {
   if (!duk_is_string(ctx, 0)) {
      duk_error(ctx, DUK_ERR_TYPE_ERROR, "objectName must be a string");
   }
   if (!duk_is_string(ctx, 1)) {
      duk_error(ctx, DUK_ERR_TYPE_ERROR, "eventName must be a string");
   }
   if (!duk_is_string(ctx, 2)) {
      duk_error(ctx, DUK_ERR_TYPE_ERROR, "msg must be a string");
   }
   const char *objectName = duk_get_string(ctx, 0);
   const char *eventName = duk_get_string(ctx, 1);
   const char *eventMsg = duk_get_string(ctx, 2);

   rbusMessage msg;
   rbusMessage_Init(&msg);
   rbusMessage_SetString(msg, eventMsg);
   rbusCoreError_t err = rbus_publishEvent(objectName, eventName, msg);
   if (err != RBUSCORE_SUCCESS) {
      duk_error(ctx, DUK_ERR_ERROR, "rbus_publishEvent failed: %d", err);
   }
   rbusMessage_Release(msg);
   duk_push_true(ctx);
   return 1;
}

static duk_ret_t duk_rbus_discover_registered_components(duk_context *ctx) {
   char **componentNameList = NULL;
   int count = 0;
   rbusCoreError_t err = rbus_discoverRegisteredComponents(&count, &componentNameList);
   if (err != RBUSCORE_SUCCESS) {
      duk_error(ctx, DUK_ERR_ERROR, "rbus_discoverRegisteredComponents failed: %d", err);
   }
   duk_push_array(ctx);
   for (int i = 0; i < count; i++) {
      if (componentNameList[i]) {
         duk_push_string(ctx, componentNameList[i]);
         duk_put_prop_index(ctx, -2, i);
      }
   }
   // Free the component list (assuming rbus provides a free function or standard free)
   if (componentNameList) {
      for (int i = 0; i < count; i++) {
         free(componentNameList[i]); // Free individual strings
      }
      free(componentNameList); // Free the array
   }
   return 1;
}

// Stash-based get handler for properties
static rbusError_t property_get_handler(rbusHandle_t handle, rbusProperty_t property, rbusGetHandlerOptions_t *opts) {
   if (!global_ctx) {
      return RBUS_ERROR_INVALID_OPERATION;
   }
   duk_context *ctx = global_ctx;
   const char *name = rbusProperty_GetName(property);
   duk_push_global_stash(ctx);
   duk_get_prop_string(ctx, -1, name);
   if (duk_is_undefined(ctx, -1)) {
      duk_pop_2(ctx);
      return RBUS_ERROR_INVALID_INPUT;
   }
   rbusValue_t value;
   rbusValue_Init(&value);
   if (duk_is_string(ctx, -1)) {
      rbusValue_SetString(value, duk_get_string(ctx, -1));
   } else if (duk_is_number(ctx, -1)) {
      rbusValue_SetDouble(value, duk_get_number(ctx, -1));
   } else if (duk_is_boolean(ctx, -1)) {
      rbusValue_SetBoolean(value, duk_get_boolean(ctx, -1));
   } else {
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
static rbusError_t property_set_handler(rbusHandle_t handle, rbusProperty_t property, rbusSetHandlerOptions_t *opts) {
   if (!global_ctx) {
      return RBUS_ERROR_INVALID_OPERATION;
   }
   duk_context *ctx = global_ctx;
   const char *name = rbusProperty_GetName(property);
   rbusValue_t value = rbusProperty_GetValue(property);
   duk_push_global_stash(ctx);
   switch (rbusValue_GetType(value)) {
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
static rbusError_t table_add_row_handler(rbusHandle_t handle, char const *tableName, char const *aliasName, uint32_t *instNum) {
   if (!global_ctx) {
      return RBUS_ERROR_INVALID_OPERATION;
   }

   static uint32_t instance = 1;
   *instNum = instance++;

   // printf("%s() tableName: %s aliasName: %s instNum: %u\n", __FUNCTION__, tableName, aliasName, *instNum);

   return RBUS_ERROR_SUCCESS;

   // return rbusTable_addRow(handle, tableName, aliasName, instNum); // 4 params
}

// Minimal table remove row handler
static rbusError_t table_remove_row_handler(rbusHandle_t handle, char const *rowName) {
   if (!global_ctx) {
      return RBUS_ERROR_INVALID_OPERATION;
   }
   return RBUS_ERROR_SUCCESS;
}

static duk_ret_t duk_rbus_add_table_row(duk_context *ctx) {
   if (!duk_is_string(ctx, 0)) {
      duk_error(ctx, DUK_ERR_TYPE_ERROR, "tableName must be a string");
   }
   rbusHandle_t handle = get_rbus_handle(ctx);
   if (!handle) {
      duk_error(ctx, DUK_ERR_ERROR, "No rbus handle found");
   }
   const char *tableName = duk_get_string(ctx, 0);
   const char *aliasName = duk_is_string(ctx, 1) ? duk_get_string(ctx, 1) : NULL;
   uint32_t instNum;
   rbusError_t err = rbusTable_addRow(handle, tableName, aliasName, &instNum); // 4 params
   if (err != RBUS_ERROR_SUCCESS) {
      duk_error(ctx, DUK_ERR_ERROR, "rbusTable_addRow failed: %d", err);
   }
   duk_push_uint(ctx, instNum);
   return 1;
}

static duk_ret_t duk_rbus_remove_table_row(duk_context *ctx) {
   if (!duk_is_string(ctx, 0)) {
      duk_error(ctx, DUK_ERR_TYPE_ERROR, "rowName must be a string");
   }
   rbusHandle_t handle = get_rbus_handle(ctx);
   if (!handle) {
      duk_error(ctx, DUK_ERR_ERROR, "No rbus handle found");
   }
   const char *rowName = duk_get_string(ctx, 0);
   rbusError_t err = rbusTable_removeRow(handle, rowName); // 2 params
   if (err != RBUS_ERROR_SUCCESS) {
      duk_error(ctx, DUK_ERR_ERROR, "rbusTable_removeRow failed: %d", err);
   }
   duk_push_true(ctx);
   return 1;
}

static duk_ret_t duk_rbus_register_table_row(duk_context *ctx) {
   if (!duk_is_string(ctx, 0) || !duk_is_number(ctx, 1)) {
      duk_error(ctx, DUK_ERR_TYPE_ERROR, "tableName must be a string, instNum must be a number");
   }
   rbusHandle_t handle = get_rbus_handle(ctx);
   if (!handle) {
      duk_error(ctx, DUK_ERR_ERROR, "No rbus handle found");
   }
   const char *tableName = duk_get_string(ctx, 0);
   uint32_t instNum = duk_get_uint(ctx, 1);
   const char *aliasName = duk_is_string(ctx, 2) ? duk_get_string(ctx, 2) : NULL;
   rbusError_t err = rbusTable_registerRow(handle, tableName, instNum, aliasName); // 4 params
   if (err != RBUS_ERROR_SUCCESS) {
      duk_error(ctx, DUK_ERR_ERROR, "rbusTable_registerRow failed: %d", err);
   }
   duk_push_true(ctx);
   return 1;
}

static duk_ret_t duk_rbus_set_table_property(duk_context *ctx) {
   if (!duk_is_string(ctx, 0)) {
      duk_error(ctx, DUK_ERR_TYPE_ERROR, "rowName and propertyName must be strings");
   }
   rbusHandle_t handle = get_rbus_handle(ctx);
   if (!handle) {
      duk_error(ctx, DUK_ERR_ERROR, "No rbus handle found");
   }
   const char *rowName = duk_get_string(ctx, 0);
   const char *propertyName = duk_get_string(ctx, 1);
   char *fullPath = malloc(strlen(rowName) + strlen(propertyName) + 2);
   snprintf(fullPath, strlen(rowName) + strlen(propertyName) + 2, "%s.%s", rowName, propertyName);
   rbusValue_t value;
   rbusValue_Init(&value);
   if (duk_is_string(ctx, 1)) {
      rbusValue_SetString(value, duk_get_string(ctx, 1));
   } else if (duk_is_number(ctx, 1)) {
      double num = duk_get_number(ctx, 1);
      if (num == (int32_t)num) {
         rbusValue_SetInt32(value, (int32_t)num);
      } else {
         rbusValue_SetDouble(value, num);
      }
   } else if (duk_is_boolean(ctx, 1)) {
      rbusValue_SetBoolean(value, duk_get_boolean(ctx, 1));
   } else {
      rbusValue_Release(value);
      free(fullPath);
      duk_error(ctx, DUK_ERR_TYPE_ERROR, "Unsupported value type");
   }
   printf("fullPath: %s\n", fullPath);
   rbusError_t err = rbus_set(handle, fullPath, value, NULL); // 4 params
   rbusValue_Release(value);
   free(fullPath);
   if (err != RBUS_ERROR_SUCCESS) {
      duk_error(ctx, DUK_ERR_ERROR, "Failed to set table property: %d", err);
   }
   duk_push_true(ctx);
   return 1;
}

static duk_ret_t duk_rbus_create_data_model(duk_context *ctx) {
   if (!duk_is_array(ctx, 0)) {
      duk_error(ctx, DUK_ERR_TYPE_ERROR, "elements must be an array");
   }
   rbusHandle_t handle = get_rbus_handle(ctx);
   if (!handle) {
      duk_error(ctx, DUK_ERR_ERROR, "No rbus handle found");
   }
   duk_size_t numElements = duk_get_length(ctx, 0);
   if (numElements == 0) {
      duk_error(ctx, DUK_ERR_TYPE_ERROR, "elements array cannot be empty");
   }
   rbusDataElement_t *elements = malloc(numElements * sizeof(rbusDataElement_t));
   for (duk_size_t i = 0; i < numElements; i++) {
      duk_get_prop_index(ctx, 0, i);
      if (!duk_is_object(ctx, -1)) {
         for (duk_size_t j = 0; j < i; j++)
            free(elements[j].name);
         free(elements);
         duk_error(ctx, DUK_ERR_TYPE_ERROR, "Each element must be an object");
      }
      duk_get_prop_string(ctx, -1, "name");
      if (!duk_is_string(ctx, -1)) {
         for (duk_size_t j = 0; j < i; j++)
            free(elements[j].name);
         free(elements);
         duk_error(ctx, DUK_ERR_TYPE_ERROR, "Element name must be a string");
      }
      elements[i].name = strdup(duk_get_string(ctx, -1));
      duk_pop(ctx);
      duk_get_prop_string(ctx, -1, "type");
      if (!duk_is_string(ctx, -1)) {
         for (duk_size_t j = 0; j < i; j++)
            free(elements[j].name);
         free(elements);
         duk_error(ctx, DUK_ERR_TYPE_ERROR, "Element type must be a string");
      }
      const char *type = duk_get_string(ctx, -1);
      duk_pop(ctx);
      // elements[i].element.name = elements[i].name;
      if (strcmp(type, "property") == 0) {
         elements[i].type = RBUS_ELEMENT_TYPE_PROPERTY;
         elements[i].cbTable.getHandler = property_get_handler;
         elements[i].cbTable.setHandler = property_set_handler;
      } else if (strcmp(type, "table") == 0) {
         elements[i].type = RBUS_ELEMENT_TYPE_TABLE;
         elements[i].cbTable.tableAddRowHandler = table_add_row_handler;
         elements[i].cbTable.tableRemoveRowHandler = table_remove_row_handler;
      } else {
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
   if (err != RBUS_ERROR_SUCCESS) {
      duk_error(ctx, DUK_ERR_ERROR, "rbus_regDataElements failed: %d", err);
   }
   duk_push_true(ctx);
   return 1;
}

// Function registration array
static const duk_function_list_entry rbus_functions[] = {
    {"setValue", duk_rbus_set_value, 2},
    {"getValue", duk_rbus_get_value, 1},
    {"subscribeToEvent", duk_rbus_subscribe_to_event, 3},
    {"subscribe", duk_rbus_event_subscribe, 2},
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
    {"setTableProperty", duk_rbus_set_table_property, 2},
    {"createDataModel", duk_rbus_create_data_model, 1},
    {"discoverRegisteredComponents", duk_rbus_discover_registered_components, 0},
    {NULL, NULL, 0}};

duk_idx_t register_function(duk_context *ctx) {
   duk_push_string(ctx, "rbus");
   duk_push_object(ctx);
   duk_put_function_list(ctx, -1, rbus_functions);
   duk_put_global_string(ctx, "rbus");

   init_subscriptions();
   duk_rbus_open(ctx, "littlesheens");

   return 0;
}

void close_function(duk_context *ctx) {

   while (sub_count > 0) {
      sleep(1);
   }

   cleanup_rbus_subscriptions();
   rbus_closeBrokerConnection();
   if (rbus_handle) {
      rbus_close(rbus_handle);
   }
}

