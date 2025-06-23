#include <node_api.h>
#include <rbus.h>
#include <pthread.h>
#include <string.h>
#include <stdio.h>
#include <stdlib.h>

// --- Constants ---
#define RBUS_TEMP_ERROR_MSG_LEN 256
#define EVENT_NAME_MAX_LEN 256
#define PROPERTY_NAME_MAX_LEN 256
#define VALUE_MAX_LEN 512
#define CALLBACK_KEY_LEN 64
#define MAX_PROPERTIES 10
#define MAX_SUBSCRIPTIONS 50
#define MAX_TSFN_DATA MAX_SUBSCRIPTIONS

// --- Global Mutexes ---
static pthread_mutex_t subscriptions_mutex = PTHREAD_MUTEX_INITIALIZER;
static pthread_mutex_t tsfn_data_mutex = PTHREAD_MUTEX_INITIALIZER;

// Structure to store subscription data
typedef struct SubscriptionData {
   napi_env env;
   napi_threadsafe_function tsfn;
   char name[EVENT_NAME_MAX_LEN];
   char callback_key[CALLBACK_KEY_LEN];
   napi_ref callback_ref;
   napi_ref user_data_ref;
   bool in_use;
} SubscriptionData;

// Structure to store a single property
typedef struct {
   char key[PROPERTY_NAME_MAX_LEN];
   rbusValue_t value; // Stores any RBus value type
} PropertyData;

// Thread-safe function callback data
typedef struct {
   char event_name[EVENT_NAME_MAX_LEN];
   rbusEventType_t event_type;
   rbusHandle_t handle;
   PropertyData properties[MAX_PROPERTIES];
   int property_count;
   SubscriptionData *sub;
   bool in_use;
} ThreadSafeData;

// Global arrays
static SubscriptionData subscriptions[MAX_SUBSCRIPTIONS];
static ThreadSafeData tsfn_data_pool[MAX_TSFN_DATA];
static int subscription_count = 0;

static const char *event_type_to_string(rbusEventType_t type) {
   static const char *event_type_strings[] = {
       [RBUS_EVENT_OBJECT_CREATED] = "object-created",
       [RBUS_EVENT_OBJECT_DELETED] = "object-deleted",
       [RBUS_EVENT_VALUE_CHANGED] = "value-changed",
       [RBUS_EVENT_GENERAL] = "general",
       [RBUS_EVENT_INITIAL_VALUE] = "initial-value",
       [RBUS_EVENT_INTERVAL] = "interval",
       [RBUS_EVENT_DURATION_COMPLETE] = "duration-complete"
   };
   return (type >= 0 && type < sizeof(event_type_strings) / sizeof(event_type_strings[0]) && event_type_strings[type]) ? event_type_strings[type] : "unknown";
}

// Helper to throw N-API error
static void throw_error(napi_env env, const char *msg, rbusError_t err, napi_status status) {
   char error_msg[RBUS_TEMP_ERROR_MSG_LEN];
   if (err != 0) {
      snprintf(error_msg, sizeof(error_msg), "%s: rbus error %d", msg, err);
   } else if (status != napi_ok) {
      snprintf(error_msg, sizeof(error_msg), "%s: N-API error %d", msg, status);
   } else {
      snprintf(error_msg, sizeof(error_msg), "%s", msg);
   }
   napi_throw_error(env, NULL, error_msg);
}

// Helper to get rbus handle
static rbusHandle_t get_rbus_handle(napi_env env, napi_value exports) {
   napi_value handle_ptr;
   napi_status status = napi_get_named_property(env, exports, "rbusHandle", &handle_ptr);
   if (status != napi_ok) {
      throw_error(env, "No rbus handle found. Ensure rbusOpenHandle was called.", 0, status);
      return NULL;
   }
   void *handle;
   status = napi_get_value_external(env, handle_ptr, &handle);
   if (status != napi_ok) {
      throw_error(env, "Failed to get handle value from external", 0, status);
      return NULL;
   }
   return (rbusHandle_t)handle;
}

// Helper to convert rbus value to JavaScript
static napi_value convert_rbus_value_to_js(napi_env env, rbusValue_t value) {
   napi_value result;
   napi_status status;

   if (!value) {
      status = napi_get_null(env, &result);
      if (status != napi_ok) {
         throw_error(env, "Failed to create null value", 0, status);
         return NULL;
      }
      return result;
   }

   switch (rbusValue_GetType(value)) {
   case RBUS_BOOLEAN: {
      bool bool_val = rbusValue_GetBoolean(value);
      status = napi_get_boolean(env, bool_val, &result);
      break;
   }
   case RBUS_CHAR: {
      char char_val = rbusValue_GetChar(value);
      status = napi_create_int32(env, (int32_t)char_val, &result);
      break;
   }
   case RBUS_BYTE: {
      unsigned char byte_val = rbusValue_GetByte(value);
      status = napi_create_uint32(env, (uint32_t)byte_val, &result);
      break;
   }
   case RBUS_INT8: {
      int8_t int8_val = rbusValue_GetInt8(value);
      status = napi_create_int32(env, (int32_t)int8_val, &result);
      break;
   }
   case RBUS_UINT8: {
      uint8_t uint8_val = rbusValue_GetUInt8(value);
      status = napi_create_uint32(env, (uint32_t)uint8_val, &result);
      break;
   }
   case RBUS_INT16: {
      int16_t int16_val = rbusValue_GetInt16(value);
      status = napi_create_int32(env, (int32_t)int16_val, &result);
      break;
   }
   case RBUS_UINT16: {
      uint16_t uint16_val = rbusValue_GetUInt16(value);
      status = napi_create_uint32(env, (uint32_t)uint16_val, &result);
      break;
   }
   case RBUS_INT32: {
      int32_t int32_val = rbusValue_GetInt32(value);
      status = napi_create_int32(env, int32_val, &result);
      break;
   }
   case RBUS_UINT32: {
      uint32_t uint32_val = rbusValue_GetUInt32(value);
      status = napi_create_uint32(env, uint32_val, &result);
      break;
   }
   case RBUS_INT64: {
      int64_t int64_val = rbusValue_GetInt64(value);
      status = napi_create_bigint_int64(env, int64_val, &result);
      break;
   }
   case RBUS_UINT64: {
      uint64_t uint64_val = rbusValue_GetUInt64(value);
      status = napi_create_bigint_uint64(env, uint64_val, &result);
      break;
   }
   case RBUS_SINGLE: {
      float single_val = rbusValue_GetSingle(value);
      status = napi_create_double(env, (double)single_val, &result);
      break;
   }
   case RBUS_DOUBLE: {
      double double_val = rbusValue_GetDouble(value);
      status = napi_create_double(env, double_val, &result);
      break;
   }
   case RBUS_DATETIME: {
      const rbusDateTime_t *time_val = rbusValue_GetTime(value);
      char time_str[128];
      if (time_val) {
         snprintf(time_str, sizeof(time_str),
            "%04d-%02d-%02dT%02d:%02d:%02d%s%02d:%02d",
            time_val->m_time.tm_year + 1900,
            time_val->m_time.tm_mon + 1,
            time_val->m_time.tm_mday,
            time_val->m_time.tm_hour,
            time_val->m_time.tm_min,
            time_val->m_time.tm_sec,
            time_val->m_tz.m_isWest ? "-" : "+",
            time_val->m_tz.m_tzhour,
            time_val->m_tz.m_tzmin);
         status = napi_create_string_utf8(env, time_str, NAPI_AUTO_LENGTH, &result);
      } else {
         status = napi_get_null(env, &result);
      }
      break;
   }
   case RBUS_STRING: {
      const char *str_val = rbusValue_GetString(value, NULL);
      if (str_val) {
         status = napi_create_string_utf8(env, str_val, NAPI_AUTO_LENGTH, &result);
      } else {
         status = napi_get_null(env, &result);
      }
      break;
   }
   case RBUS_BYTES: {
      int len;
      const uint8_t *bytes_val = rbusValue_GetBytes(value, &len);
      napi_value array_buffer;
      void *buffer_data;
      status = napi_create_arraybuffer(env, len, &buffer_data, &array_buffer);
      if (status != napi_ok) {
         throw_error(env, "Failed to create ArrayBuffer for bytes", 0, status);
         return NULL;
      }
      if (bytes_val && len > 0) {
         memcpy(buffer_data, bytes_val, len);
      }
      status = napi_create_typedarray(env, napi_uint8_array, len, array_buffer, 0, &result);
      break;
   }
   case RBUS_PROPERTY:
   case RBUS_OBJECT: {
      rbusObject_t obj = rbusValue_GetObject(value);
      napi_value obj_result;
      status = napi_create_object(env, &obj_result);
      if (status != napi_ok) {
         throw_error(env, "Failed to create object for RBUS_PROPERTY or RBUS_OBJECT", 0, status);
         return NULL;
      }
      rbusProperty_t prop = rbusObject_GetProperties(obj);
      while (prop) {
         const char *key = rbusProperty_GetName(prop);
         rbusValue_t val = rbusProperty_GetValue(prop);
         if (key && val) {
            napi_value prop_val = convert_rbus_value_to_js(env, val);
            if (!prop_val) {
               return NULL;
            }
            status = napi_set_named_property(env, obj_result, key, prop_val);
            if (status != napi_ok) {
               fprintf(stderr, "N-API Error: Failed to set property '%s' for RBUS_OBJECT (%d)\n", key, status);
               return NULL;
            }
         }
         prop = rbusProperty_GetNext(prop);
      }
      result = obj_result;
      break;
   }
   case RBUS_NONE: {
      status = napi_get_null(env, &result);
      break;
   }
   default:
      napi_throw_type_error(env, NULL, "Unsupported or unknown rbus value type");
      return NULL;
   }

   if (status != napi_ok) {
      throw_error(env, "Failed to convert rbus value to JavaScript value", 0, status);
      return NULL;
   }
   return result;
}

static void tsfn_finalize(napi_env env, void *finalize_data, void *finalize_hint) {
   SubscriptionData *sub = (SubscriptionData *)finalize_data;
   if (sub) {
      pthread_mutex_lock(&subscriptions_mutex);
      fprintf(stderr, "Finalizing subscription for event '%s'\n", sub->name);
      if (sub->in_use) {
         if (sub->callback_ref) {
            napi_delete_reference(env, sub->callback_ref);
         }
         if (sub->user_data_ref) {
            napi_delete_reference(env, sub->user_data_ref);
         }
         sub->name[0] = '\0';
         sub->callback_key[0] = '\0';
         sub->in_use = false;
         subscription_count--;
      }
      pthread_mutex_unlock(&subscriptions_mutex);
   }
}

static void tsfn_call(napi_env env, napi_value js_callback, void *context, void *data) {
   ThreadSafeData *ts_data = (ThreadSafeData *)data;
   SubscriptionData *sub = ts_data->sub;

   if (!sub) {
      fprintf(stderr, "Error: Null subscription in tsfn_call\n");
      goto cleanup;
   }
   if (!sub->env) {
      fprintf(stderr, "Error: Null env in tsfn_call\n");
      goto cleanup;
   }
   pthread_mutex_lock(&subscriptions_mutex);
   bool sub_in_use = sub->in_use;
   pthread_mutex_unlock(&subscriptions_mutex);

   if (!sub_in_use) {
      fprintf(stderr, "Error: Subscription not in use in tsfn_call\n");
      goto cleanup;
   }
   if (!ts_data) {
      fprintf(stderr, "Error: Null ts_data in tsfn_call\n");
      goto cleanup;
   }

   napi_handle_scope scope;
   napi_status status = napi_open_handle_scope(env, &scope);
   if (status != napi_ok) {
      fprintf(stderr, "N-API Error: Failed to open handle scope in tsfn_call (%d)\n", status);
      goto cleanup;
   }

   napi_value callback;
   status = napi_get_reference_value(env, sub->callback_ref, &callback);
   if (status != napi_ok || !callback) {
      fprintf(stderr, "N-API Error: Failed to get callback reference (%d)\n", status);
      napi_close_handle_scope(env, scope);
      goto cleanup;
   }

   napi_valuetype callback_type;
   status = napi_typeof(env, callback, &callback_type);
   if (status != napi_ok || callback_type != napi_function) {
      fprintf(stderr, "N-API Error: Callback is not a function (%d)\n", status);
      napi_close_handle_scope(env, scope);
      goto cleanup;
   }

   napi_value event_obj;
   status = napi_create_object(env, &event_obj);
   if (status != napi_ok) {
      fprintf(stderr, "N-API Error: Failed to create event object (%d)\n", status);
      napi_close_handle_scope(env, scope);
      goto cleanup;
   }

   napi_value name, type;
   status = napi_create_string_utf8(env, ts_data->event_name, NAPI_AUTO_LENGTH, &name);
   if (status != napi_ok) {
      fprintf(stderr, "N-API Error: Failed to create event name string (%d)\n", status);
      napi_close_handle_scope(env, scope);
      goto cleanup;
   }
   status = napi_create_string_utf8(env, event_type_to_string(ts_data->event_type), NAPI_AUTO_LENGTH, &type);
   if (status != napi_ok) {
      fprintf(stderr, "N-API Error: Failed to create event type string (%d)\n", status);
      napi_close_handle_scope(env, scope);
      goto cleanup;
   }

   status = napi_set_named_property(env, event_obj, "name", name);
   if (status != napi_ok) {
      fprintf(stderr, "N-API Error: Failed to set event name property (%d)\n", status);
      napi_close_handle_scope(env, scope);
      goto cleanup;
   }
   status = napi_set_named_property(env, event_obj, "type", type);
   if (status != napi_ok) {
      fprintf(stderr, "N-API Error: Failed to set event type property (%d)\n", status);
      napi_close_handle_scope(env, scope);
      goto cleanup;
   }

   if (ts_data->property_count > 0) {
      napi_value data_obj;
      status = napi_create_object(env, &data_obj);
      if (status != napi_ok) {
         fprintf(stderr, "N-API Error: Failed to create data object (%d)\n", status);
         napi_close_handle_scope(env, scope);
         goto cleanup;
      }

      for (int i = 0; i < ts_data->property_count; i++) {
         PropertyData *prop = &ts_data->properties[i];
         if (prop->key[0] && prop->value) {
            napi_value prop_val = convert_rbus_value_to_js(env, prop->value);
            if (!prop_val) {
               fprintf(stderr, "N-API Error: Failed to convert property value for key '%s'\n", prop->key);
               napi_close_handle_scope(env, scope);
               goto cleanup;
            }
            status = napi_set_named_property(env, data_obj, prop->key, prop_val);
            if (status != napi_ok) {
               fprintf(stderr, "N-API Error: Failed to set data property '%s' (%d)\n", prop->key, status);
               napi_close_handle_scope(env, scope);
               goto cleanup;
            }
         }
      }
      status = napi_set_named_property(env, event_obj, "data", data_obj);
      if (status != napi_ok) {
         fprintf(stderr, "N-API Error: Failed to set event data property (%d)\n", status);
         napi_close_handle_scope(env, scope);
         goto cleanup;
      }
   } else {
      napi_value null_val;
      status = napi_get_null(env, &null_val);
      if (status != napi_ok) {
         fprintf(stderr, "N-API Error: Failed to create null data property (%d)\n", status);
         napi_close_handle_scope(env, scope);
         goto cleanup;
      }
      status = napi_set_named_property(env, event_obj, "data", null_val);
      if (status != napi_ok) {
         fprintf(stderr, "N-API Error: Failed to set event data to null (%d)\n", status);
         napi_close_handle_scope(env, scope);
         goto cleanup;
      }
   }

   napi_value user_data;
   if (sub->user_data_ref) {
      status = napi_get_reference_value(env, sub->user_data_ref, &user_data);
      if (status != napi_ok) {
         fprintf(stderr, "N-API Error: Failed to get user data reference (%d)\n", status);
         napi_close_handle_scope(env, scope);
         goto cleanup;
      }
   } else {
      status = napi_get_null(env, &user_data);
      if (status != napi_ok) {
         fprintf(stderr, "N-API Error: Failed to create null user data (%d)\n", status);
         napi_close_handle_scope(env, scope);
         goto cleanup;
      }
   }

   napi_value global;
   status = napi_get_global(env, &global);
   if (status != napi_ok) {
      fprintf(stderr, "N-API Error: Failed to get global object (%d)\n", status);
      napi_close_handle_scope(env, scope);
      goto cleanup;
   }

   napi_value argv[2] = {event_obj, user_data};
   status = napi_call_function(env, global, callback, 2, argv, NULL);
   if (status != napi_ok) {
      napi_value exception;
      napi_get_and_clear_last_exception(env, &exception);
      napi_value err_str;
      napi_coerce_to_string(env, exception, &err_str);
      char buffer[RBUS_TEMP_ERROR_MSG_LEN];
      size_t err_len;
      napi_get_value_string_utf8(env, err_str, buffer, sizeof(buffer), &err_len);
      fprintf(stderr, "RBus event handler error: %.*s\n", (int)err_len, buffer);
   }

cleanup:
   if (ts_data) {
      pthread_mutex_lock(&tsfn_data_mutex);
      for (int i = 0; i < ts_data->property_count; i++) {
         if (ts_data->properties[i].value) {
            rbusValue_Release(ts_data->properties[i].value);
            ts_data->properties[i].value = NULL;
         }
         ts_data->properties[i].key[0] = '\0';
      }
      ts_data->event_name[0] = '\0';
      ts_data->property_count = 0;
      ts_data->in_use = false;
      pthread_mutex_unlock(&tsfn_data_mutex);
   }
   napi_close_handle_scope(env, scope);
}

static void event_handler(rbusHandle_t handle, rbusEvent_t const *event, rbusEventSubscription_t *subscription) {
   SubscriptionData *sub = (SubscriptionData *)subscription->userData;
   pthread_mutex_lock(&subscriptions_mutex);
   if (!sub || !sub->in_use || !sub->name[0] || !event->name || strcmp(sub->name, event->name) != 0) {
      pthread_mutex_unlock(&subscriptions_mutex);
      fprintf(stderr, "Error: Invalid subscription data or event name mismatch in event_handler\n");
      return;
   }
   bool in_use = sub->in_use;
   pthread_mutex_unlock(&subscriptions_mutex);

   pthread_mutex_lock(&tsfn_data_mutex);
   ThreadSafeData *ts_data = NULL;
   for (int i = 0; i < MAX_TSFN_DATA; ++i) {
      if (!tsfn_data_pool[i].in_use) {
         ts_data = &tsfn_data_pool[i];
         ts_data->in_use = true;
         break;
      }
   }
   pthread_mutex_unlock(&tsfn_data_mutex);

   if (!ts_data) {
      pthread_mutex_unlock(&subscriptions_mutex);
      fprintf(stderr, "Warning: ThreadSafeData pool exhausted, event '%s' dropped\n", event->name);
      return;
   }

   ts_data->event_name[0] = '\0';
   ts_data->event_type = 0;
   ts_data->handle = NULL;
   ts_data->sub = NULL;
   ts_data->property_count = 0;
   for (int i = 0; i < MAX_PROPERTIES; i++) {
      ts_data->properties[i].key[0] = '\0';
      ts_data->properties[i].value = NULL;
   }

   if (!in_use) {
      pthread_mutex_lock(&tsfn_data_mutex);
      ts_data->in_use = false;
      pthread_mutex_unlock(&tsfn_data_mutex);
      return;
   }

   size_t name_len = strlen(event->name ? event->name : "");
   if (name_len >= EVENT_NAME_MAX_LEN) {
      fprintf(stderr, "Error: Event name too long in event_handler\n");
      pthread_mutex_lock(&tsfn_data_mutex);
      ts_data->in_use = false;
      pthread_mutex_unlock(&tsfn_data_mutex);
      return;
   }
   memcpy(ts_data->event_name, event->name ? event->name : "", name_len + 1);
   ts_data->event_type = event->type;
   ts_data->handle = handle;
   ts_data->sub = sub;

   if (event->data) {
      rbusObject_t obj = event->data;
      rbusProperty_t prop = rbusObject_GetProperties(obj);
      int prop_count = 0;

      while (prop && prop_count < MAX_PROPERTIES) {
         const char *key = rbusProperty_GetName(prop);
         rbusValue_t val = rbusProperty_GetValue(prop);
         if (key && val) {
            size_t key_len = strlen(key);
            if (key_len >= PROPERTY_NAME_MAX_LEN) {
               fprintf(stderr, "Warning: Property key too long, skipping\n");
               prop = rbusProperty_GetNext(prop);
               continue;
            }
            memcpy(ts_data->properties[prop_count].key, key, key_len + 1);
            rbusValue_Init(&ts_data->properties[prop_count].value);
            rbusValue_Copy(ts_data->properties[prop_count].value, val);
            prop_count++;
         }
         prop = rbusProperty_GetNext(prop);
      }
      ts_data->property_count = prop_count;

      if (prop) {
         fprintf(stderr, "Warning: Exceeded maximum properties (%d), some properties ignored\n", MAX_PROPERTIES);
      }
   }

   napi_status status = napi_call_threadsafe_function(sub->tsfn, ts_data, napi_tsfn_nonblocking);
   if (status != napi_ok) {
      fprintf(stderr, "Error: Failed to call threadsafe function for event '%s' (N-API status: %d)\n",
         event->name, status);
      pthread_mutex_lock(&tsfn_data_mutex);
      for (int i = 0; i < ts_data->property_count; i++) {
         if (ts_data->properties[i].value) {
            rbusValue_Release(ts_data->properties[i].value);
            ts_data->properties[i].value = NULL;
         }
         ts_data->properties[i].key[0] = '\0';
      }
      ts_data->event_name[0] = '\0';
      ts_data->property_count = 0;
      ts_data->in_use = false;
      pthread_mutex_unlock(&tsfn_data_mutex);
   }
}

static napi_value rbus_open_handle(napi_env env, napi_callback_info info) {
   rbusHandle_t handle;
   rbusError_t err = rbus_open(&handle, "nodeClient");
   if (err != RBUS_ERROR_SUCCESS) {
      throw_error(env, "rbus_open failed", err, napi_ok);
      return NULL;
   }
   napi_value result;
   napi_status status = napi_create_external(env, handle, NULL, NULL, &result);
   if (status != napi_ok) {
      rbus_close(handle);
      throw_error(env, "Failed to create external handle", 0, status);
      return NULL;
   }
   return result;
}

static napi_value rbus_close_handle(napi_env env, napi_callback_info info) {
   napi_value exports;
   napi_status status = napi_get_cb_info(env, info, NULL, NULL, &exports, NULL);
   if (status != napi_ok) {
      throw_error(env, "Failed to get callback info", 0, status);
      return NULL;
   }
   rbusHandle_t handle = get_rbus_handle(env, exports);
   if (!handle) {
      return NULL;
   }

   pthread_mutex_lock(&subscriptions_mutex);
   for (int i = 0; i < MAX_SUBSCRIPTIONS; ++i) {
      if (subscriptions[i].in_use) {
         fprintf(stderr, "Unsubscribing event '%s'\n", subscriptions[i].name);
         rbusEvent_Unsubscribe(handle, subscriptions[i].name);
         napi_release_threadsafe_function(subscriptions[i].tsfn, napi_tsfn_release);
      }
   }
   subscription_count = 0;
   pthread_mutex_unlock(&subscriptions_mutex);

   rbusError_t err = rbus_close(handle);
   if (err != RBUS_ERROR_SUCCESS) {
      throw_error(env, "rbus_close failed", err, napi_ok);
      return NULL;
   }

   napi_value result;
   napi_get_boolean(env, true, &result);
   return result;
}

static napi_value rbus_set_value(napi_env env, napi_callback_info info) {
   size_t argc = 2;
   napi_value args[2];
   napi_value exports;
   napi_status status = napi_get_cb_info(env, info, &argc, args, &exports, NULL);
   if (status != napi_ok || argc != 2) {
      throw_error(env, "Expected 2 arguments: propertyName (string), value (string)", 0, status);
      return NULL;
   }

   napi_valuetype arg_type;
   status = napi_typeof(env, args[0], &arg_type);
   if (status != napi_ok || arg_type != napi_string) {
      throw_error(env, "propertyName must be a string", 0, status);
      return NULL;
   }

   status = napi_typeof(env, args[1], &arg_type);
   if (status != napi_ok || arg_type != napi_string) {
      throw_error(env, "value must be a string", 0, status);
      return NULL;
   }

   char propertyName[PROPERTY_NAME_MAX_LEN];
   size_t prop_len;
   status = napi_get_value_string_utf8(env, args[0], propertyName, PROPERTY_NAME_MAX_LEN, &prop_len);
   if (status != napi_ok || prop_len >= PROPERTY_NAME_MAX_LEN) {
      throw_error(env, prop_len >= PROPERTY_NAME_MAX_LEN ? "propertyName too long" : "Failed to get propertyName string", 0, status);
      return NULL;
   }

   char val_str[VALUE_MAX_LEN];
   size_t val_len;
   status = napi_get_value_string_utf8(env, args[1], val_str, VALUE_MAX_LEN, &val_len);
   if (status != napi_ok || val_len >= VALUE_MAX_LEN) {
      throw_error(env, val_len >= VALUE_MAX_LEN ? "value string too long" : "Failed to get value string", 0, status);
      return NULL;
   }

   rbusHandle_t handle = get_rbus_handle(env, exports);
   if (!handle) {
      return NULL;
   }

   rbusValue_t value;
   rbusValue_Init(&value);

   rbusValue_t current_value;
   rbusValueType_t target_type = RBUS_STRING;
   rbusError_t err = rbus_get(handle, propertyName, &current_value);
   if (err == RBUS_ERROR_SUCCESS) {
      target_type = rbusValue_GetType(current_value);
      rbusValue_Release(current_value);
   } else {
      fprintf(stderr, "Warning: rbus_get failed for '%s' during type inference (%d). Defaulting to RBUS_STRING.\n", propertyName, err);
   }

   bool conversion_success = false;
   switch (target_type) {
   case RBUS_BOOLEAN:
   case RBUS_CHAR:
   case RBUS_BYTE:
   case RBUS_INT8:
   case RBUS_UINT8:
   case RBUS_INT16:
   case RBUS_UINT16:
   case RBUS_INT32:
   case RBUS_UINT32:
   case RBUS_INT64:
   case RBUS_UINT64:
   case RBUS_SINGLE:
   case RBUS_DOUBLE:
   case RBUS_STRING: {
      conversion_success = rbusValue_SetFromString(value, target_type, val_str);
      break;
   }
   case RBUS_DATETIME: {
      struct tm tm = {0};
      int year, month, day, hour, min, sec;
      int tz_hour_val = 0, tz_min_val = 0;
      char tz_sign = '+';
      int scanned = sscanf(val_str, "%d-%d-%dT%d:%d:%d%c%d:%d",
         &year, &month, &day, &hour, &min, &sec,
         &tz_sign, &tz_hour_val, &tz_min_val);
      if (scanned >= 6) {
         tm.tm_year = year - 1900;
         tm.tm_mon = month - 1;
         tm.tm_mday = day;
         tm.tm_hour = hour;
         tm.tm_min = min;
         tm.tm_sec = sec;
         rbusDateTime_t dt;
         rbusValue_MarshallTMtoRBUS(&dt, &tm);
         if (scanned == 9) {
            dt.m_tz.m_tzhour = tz_hour_val;
            dt.m_tz.m_tzmin = tz_min_val;
            dt.m_tz.m_isWest = (tz_sign == '-');
         } else {
            dt.m_tz.m_isWest = 0;
            dt.m_tz.m_tzhour = 0;
            dt.m_tz.m_tzmin = 0;
         }
         rbusValue_SetTime(value, &dt);
         conversion_success = true;
      }
      break;
   }
   case RBUS_BYTES: {
      size_t str_len = strlen(val_str);
      if (str_len % 2 == 0) {
         size_t byte_len = str_len / 2;
         uint8_t bytes[VALUE_MAX_LEN / 2];
         if (byte_len <= sizeof(bytes)) {
            conversion_success = true;
            for (size_t i = 0; i < byte_len; i++) {
               unsigned int byte;
               if (sscanf(&val_str[i * 2], "%2x", &byte) == 1) {
                  bytes[i] = (uint8_t)byte;
               } else {
                  conversion_success = false;
                  break;
               }
            }
            if (conversion_success) {
               rbusValue_SetBytes(value, bytes, byte_len);
            }
         } else {
            throw_error(env, "Byte array too long for static buffer", 0, napi_ok);
         }
      }
      break;
   }
   case RBUS_PROPERTY:
   case RBUS_OBJECT: {
      rbusValue_Release(value);
      throw_error(env, "Unsupported target value type for rbus_set: RBUS_PROPERTY or RBUS_OBJECT", 0, napi_ok);
      return NULL;
   }
   case RBUS_NONE:
      rbusValue_SetString(value, val_str);
      conversion_success = true;
      break;
   default:
      rbusValue_Release(value);
      throw_error(env, "Unsupported or unknown target rbus value type for rbus_set", 0, napi_ok);
      return NULL;
   }

   if (!conversion_success) {
      rbusValue_Release(value);
      throw_error(env, "Failed to convert string to target rbus type", 0, napi_ok);
      return NULL;
   }

   err = rbus_set(handle, propertyName, value, NULL);
   rbusValue_Release(value);

   if (err != RBUS_ERROR_SUCCESS) {
      throw_error(env, "rbus_set failed", err, napi_ok);
      return NULL;
   }

   napi_value result;
   napi_get_boolean(env, true, &result);
   return result;
}

static napi_value rbus_subscribe_event(napi_env env, napi_callback_info info) {
   size_t argc = 4;
   napi_value args[4];
   napi_value exports;
   napi_status status = napi_get_cb_info(env, info, &argc, args, &exports, NULL);
   if (status != napi_ok || argc < 2 || argc > 4) {
      throw_error(env, "Expected 2-4 arguments: eventName (string), handler (function), [userData], [timeout (number)]", 0, status);
      return NULL;
   }

   rbusHandle_t handle = get_rbus_handle(env, exports);
   if (!handle) {
      return NULL;
   }

   napi_valuetype arg_type;
   status = napi_typeof(env, args[0], &arg_type);
   if (status != napi_ok || arg_type != napi_string) {
      throw_error(env, "eventName must be a string", 0, status);
      return NULL;
   }

   status = napi_typeof(env, args[1], &arg_type);
   if (status != napi_ok || arg_type != napi_function) {
      throw_error(env, "handler must be a function", 0, status);
      return NULL;
   }

   char event_name[EVENT_NAME_MAX_LEN];
   size_t event_len;
   status = napi_get_value_string_utf8(env, args[0], event_name, EVENT_NAME_MAX_LEN, &event_len);
   if (status != napi_ok || event_len >= EVENT_NAME_MAX_LEN) {
      throw_error(env, event_len >= EVENT_NAME_MAX_LEN ? "eventName too long" : "Failed to get eventName string", 0, status);
      return NULL;
   }

   int timeout = 0;
   if (argc > 3) {
      status = napi_typeof(env, args[3], &arg_type);
      if (status != napi_ok || arg_type != napi_number) {
         throw_error(env, "timeout must be a number", 0, status);
         return NULL;
      }
      napi_get_value_int32(env, args[3], &timeout);
      if (timeout < 0) {
         throw_error(env, "timeout must be non-negative", 0, napi_ok);
         return NULL;
      }
   }

   pthread_mutex_lock(&subscriptions_mutex);
   SubscriptionData *sub = NULL;
   for (int i = 0; i < MAX_SUBSCRIPTIONS; ++i) {
      if (!subscriptions[i].in_use) {
         sub = &subscriptions[i];
         break;
      }
   }

   if (!sub) {
      pthread_mutex_unlock(&subscriptions_mutex);
      throw_error(env, "Maximum number of subscriptions reached", 0, napi_ok);
      return NULL;
   }

   memset(sub, 0, sizeof(SubscriptionData));
   sub->env = env;
   strncpy(sub->name, event_name, EVENT_NAME_MAX_LEN - 1);
   sub->in_use = true;

   snprintf(sub->callback_key, CALLBACK_KEY_LEN, "rbus_callback_%p", sub);

   status = napi_create_reference(env, args[1], 1, &sub->callback_ref);
   if (status != napi_ok) {
      sub->in_use = false;
      sub->name[0] = '\0';
      sub->callback_key[0] = '\0';
      pthread_mutex_unlock(&subscriptions_mutex);
      throw_error(env, "Failed to create callback reference", 0, status);
      return NULL;
   }

   napi_ref user_data_ref = NULL;
   if (argc > 2) {
      status = napi_create_reference(env, args[2], 1, &user_data_ref);
      if (status != napi_ok) {
         sub->in_use = false;
         napi_delete_reference(env, sub->callback_ref);
         sub->name[0] = '\0';
         sub->callback_key[0] = '\0';
         pthread_mutex_unlock(&subscriptions_mutex);
         throw_error(env, "Failed to create userData reference", 0, status);
         return NULL;
      }
   }
   sub->user_data_ref = user_data_ref;

   napi_value async_resource_name;
   status = napi_create_string_utf8(env, "rbus_event_callback", NAPI_AUTO_LENGTH, &async_resource_name);
   if (status != napi_ok) {
      sub->in_use = false;
      if (sub->user_data_ref) {
         napi_delete_reference(env, sub->user_data_ref);
      }
      napi_delete_reference(env, sub->callback_ref);
      sub->name[0] = '\0';
      sub->callback_key[0] = '\0';
      pthread_mutex_unlock(&subscriptions_mutex);
      throw_error(env, "Failed to create async resource name", 0, status);
      return NULL;
   }

   status = napi_create_threadsafe_function(
      env,
      NULL,
      NULL,
      async_resource_name,
      0,
      1,
      sub,
      tsfn_finalize,
      NULL,
      tsfn_call,
      &sub->tsfn
   );
   if (status != napi_ok) {
      sub->in_use = false;
      if (sub->user_data_ref) {
         napi_delete_reference(env, sub->user_data_ref);
      }
      napi_delete_reference(env, sub->callback_ref);
      sub->name[0] = '\0';
      sub->callback_key[0] = '\0';
      pthread_mutex_unlock(&subscriptions_mutex);
      throw_error(env, "Failed to create thread-safe function", 0, status);
      return NULL;
   }

   rbusError_t err = rbusEvent_Subscribe(handle, sub->name, event_handler, sub, timeout);
   if (err != RBUS_ERROR_SUCCESS) {
      sub->in_use = false;
      napi_release_threadsafe_function(sub->tsfn, napi_tsfn_release);
      if (sub->user_data_ref) {
         napi_delete_reference(env, sub->user_data_ref);
      }
      napi_delete_reference(env, sub->callback_ref);
      sub->name[0] = '\0';
      sub->callback_key[0] = '\0';
      pthread_mutex_unlock(&subscriptions_mutex);
      throw_error(env, "rbusEvent_Subscribe failed", err, napi_ok);
      return NULL;
   }

   subscription_count++;
   pthread_mutex_unlock(&subscriptions_mutex);

   napi_value result;
   napi_get_boolean(env, true, &result);
   return result;
}

static napi_value rbus_get_value(napi_env env, napi_callback_info info) {
   size_t argc = 1;
   napi_value args[1];
   napi_value exports;
   napi_status status = napi_get_cb_info(env, info, &argc, args, &exports, NULL);
   if (status != napi_ok || argc != 1) {
      throw_error(env, "Expected 1 argument: propertyName (string)", 0, status);
      return NULL;
   }

   napi_valuetype arg_type;
   status = napi_typeof(env, args[0], &arg_type);
   if (status != napi_ok || arg_type != napi_string) {
      throw_error(env, "propertyName must be a string", 0, status);
      return NULL;
   }

   char propertyName[PROPERTY_NAME_MAX_LEN];
   size_t prop_len;
   status = napi_get_value_string_utf8(env, args[0], propertyName, PROPERTY_NAME_MAX_LEN, &prop_len);
   if (status != napi_ok || prop_len >= PROPERTY_NAME_MAX_LEN) {
      throw_error(env, prop_len >= PROPERTY_NAME_MAX_LEN ? "propertyName too long" : "Failed to get propertyName string", 0, status);
      return NULL;
   }

   rbusHandle_t handle = get_rbus_handle(env, exports);
   if (!handle) {
      return NULL;
   }

   rbusValue_t value;
   rbusError_t err = rbus_get(handle, propertyName, &value);
   if (err != RBUS_ERROR_SUCCESS) {
      throw_error(env, "rbus_get failed", err, napi_ok);
      return NULL;
   }

   napi_value result = convert_rbus_value_to_js(env, value);
   rbusValue_Release(value);
   return result;
}

static napi_value rbus_unsubscribe_event(napi_env env, napi_callback_info info) {
   size_t argc = 1;
   napi_value args[1];
   napi_value exports;
   napi_status status = napi_get_cb_info(env, info, &argc, args, &exports, NULL);
   if (status != napi_ok || argc != 1) {
      throw_error(env, "Expected 1 argument: eventName (string)", 0, status);
      return NULL;
   }

   napi_valuetype arg_type;
   status = napi_typeof(env, args[0], &arg_type);
   if (status != napi_ok || arg_type != napi_string) {
      throw_error(env, "eventName must be a string", 0, status);
      return NULL;
   }

   char event_name[EVENT_NAME_MAX_LEN];
   size_t event_len;
   status = napi_get_value_string_utf8(env, args[0], event_name, EVENT_NAME_MAX_LEN, &event_len);
   if (status != napi_ok || event_len >= EVENT_NAME_MAX_LEN) {
      throw_error(env, event_len >= EVENT_NAME_MAX_LEN ? "eventName too long" : "Failed to get eventName string", 0, status);
      return NULL;
   }

   rbusHandle_t handle = get_rbus_handle(env, exports);
   if (!handle) {
      return NULL;
   }

   pthread_mutex_lock(&subscriptions_mutex);
   SubscriptionData *sub = NULL;
   for (int i = 0; i < MAX_SUBSCRIPTIONS; ++i) {
      if (subscriptions[i].in_use && strcmp(subscriptions[i].name, event_name) == 0) {
         sub = &subscriptions[i];
         break;
      }
   }
   pthread_mutex_unlock(&subscriptions_mutex);

   if (!sub) {
      throw_error(env, "No subscription found for eventName", 0, napi_ok);
      return NULL;
   }

   rbusError_t err = rbusEvent_Unsubscribe(handle, sub->name);
   if (err != RBUS_ERROR_SUCCESS) {
      throw_error(env, "rbusEvent_Unsubscribe failed", err, napi_ok);
      return NULL;
   }

   napi_release_threadsafe_function(sub->tsfn, napi_tsfn_release);

   napi_value result;
   napi_get_boolean(env, true, &result);
   return result;
}

static void module_cleanup(void *arg) {
   pthread_mutex_lock(&subscriptions_mutex);
   for (int i = 0; i < MAX_SUBSCRIPTIONS; ++i) {
      if (subscriptions[i].in_use) {
         napi_release_threadsafe_function(subscriptions[i].tsfn, napi_tsfn_release);
      }
   }
   subscription_count = 0;
   pthread_mutex_unlock(&subscriptions_mutex);

   pthread_mutex_lock(&tsfn_data_mutex);
   for (int i = 0; i < MAX_TSFN_DATA; ++i) {
      if (tsfn_data_pool[i].in_use) {
         for (int j = 0; j < tsfn_data_pool[i].property_count; j++) {
            if (tsfn_data_pool[i].properties[j].value) {
               rbusValue_Release(tsfn_data_pool[i].properties[j].value);
               tsfn_data_pool[i].properties[j].value = NULL;
            }
            tsfn_data_pool[i].properties[j].key[0] = '\0';
         }
         tsfn_data_pool[i].event_name[0] = '\0';
         tsfn_data_pool[i].property_count = 0;
         tsfn_data_pool[i].in_use = false;
      }
   }
   pthread_mutex_unlock(&tsfn_data_mutex);

   pthread_mutex_destroy(&subscriptions_mutex);
   pthread_mutex_destroy(&tsfn_data_mutex);
}

static napi_value init(napi_env env, napi_value exports) {
   pthread_mutex_init(&subscriptions_mutex, NULL);
   pthread_mutex_init(&tsfn_data_mutex, NULL);
   for (int i = 0; i < MAX_SUBSCRIPTIONS; ++i) {
      subscriptions[i].in_use = false;
   }
   for (int i = 0; i < MAX_TSFN_DATA; ++i) {
      tsfn_data_pool[i].in_use = false;
   }
   subscription_count = 0;

   napi_add_env_cleanup_hook(env, module_cleanup, env);

   napi_property_descriptor desc[] = {
       {"rbusOpenHandle", NULL, rbus_open_handle, NULL, NULL, NULL, napi_default, NULL},
       {"rbusCloseHandle", NULL, rbus_close_handle, NULL, NULL, NULL, napi_default, NULL},
       {"setValue", NULL, rbus_set_value, NULL, NULL, NULL, napi_default, NULL},
       {"getValue", NULL, rbus_get_value, NULL, NULL, NULL, napi_default, NULL},
       {"subscribe", NULL, rbus_subscribe_event, NULL, NULL, NULL, napi_default, NULL},
       {"unsubscribe", NULL, rbus_unsubscribe_event, NULL, NULL, NULL, napi_default, NULL}
   };
   napi_status status = napi_define_properties(env, exports, sizeof(desc) / sizeof(desc[0]), desc);
   if (status != napi_ok) {
      pthread_mutex_destroy(&subscriptions_mutex);
      pthread_mutex_destroy(&tsfn_data_mutex);
      throw_error(env, "Failed to define module properties", 0, status);
      return NULL;
   }
   return exports;
}

NAPI_MODULE(NODE_GYP_MODULE_NAME, init)