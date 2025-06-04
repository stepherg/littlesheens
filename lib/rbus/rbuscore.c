/*
 * If not stated otherwise in this file or this component's Licenses.txt file
 * the following copyright and licenses apply:
 *
 * Copyright 2016 RDK Management
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 * http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */
#define _GNU_SOURCE 1
#include <stdlib.h>
#include <string.h>
#include <pthread.h>
#include <stdbool.h>
#include <unistd.h>
#include <ctype.h>
#include <sys/stat.h>
#include <sys/types.h>

#include "rbuscore.h"
#include "rbuscore_logger.h"
#include "rtVector.h"
#include "rtAdvisory.h"
#include "rtMemory.h"

#include "rtrouteBase.h"

/* Begin type definitions.*/
typedef struct _rbusOpenTelemetryContext
{
    char otTraceParent[RBUS_OPEN_TELEMETRY_DATA_MAX];
    char otTraceState[RBUS_OPEN_TELEMETRY_DATA_MAX];
} rbusOpenTelemetryContext;


struct _server_object;
typedef struct _server_object *server_object_t;

typedef struct _server_method
{
    char name[MAX_METHOD_NAME_LENGTH + 1];
    rbus_callback_t callback;
    void *data;
} *server_method_t;

typedef struct _server_event
{
    char name[MAX_EVENT_NAME_LENGTH + 1];
    server_object_t object;
    rtVector listeners /*list of strings*/;
    rbus_event_subscribe_callback_t sub_callback;
    void *sub_data;
} *server_event_t;

typedef struct _server_object
{
    char name[MAX_OBJECT_NAME_LENGTH + 1];
    void *data;
    rbus_callback_t callback;
    bool process_event_subscriptions;
    rtVector methods;       /*list of server_method_t*/
    rtVector subscriptions; /*list of server_event_t*/
    rbus_event_subscribe_callback_t subscribe_handler_override;
    void *subscribe_handler_data;
} *server_object_t;

/////////////
extern char *__progname;
/////

void server_method_create(server_method_t *meth, char const *name, rbus_callback_t callback, void *data)
{
}

int server_method_compare(const void *left, const void *right)
{
    return 0;
}

int server_event_compare(const void *left, const void *right)
{
    return 0;
}

void server_event_create(server_event_t *event, const char *event_name, server_object_t obj, rbus_event_subscribe_callback_t sub_callback, void *sub_data)
{
}

void server_event_destroy(void *p)
{
}

void server_event_addListener(server_event_t event, char const *listener)
{
}

rbusCoreError_t server_object_subscription_handler(server_object_t obj, const char *event, char const *subscriber, int added, rbusMessage payload)
{
    return RBUSCORE_SUCCESS;
}

typedef struct _queued_request
{
    rtMessageHeader hdr;
    rbusMessage msg;
    server_object_t obj;
} *queued_request_t;

void queued_request_create(queued_request_t *req, rtMessageHeader hdr, rbusMessage msg, server_object_t obj)
{
}

/* End rbus_server */

/* Begin rbus_client */
typedef struct _client_event
{
    char name[MAX_EVENT_NAME_LENGTH + 1];
    rbus_event_callback_t callback;
    void *data;
} *client_event_t;

typedef struct _client_subscription
{
    char object[MAX_OBJECT_NAME_LENGTH + 1];
    rtVector events; /*list of client_event_t*/
} *client_subscription_t;

void client_event_create(client_event_t *event, const char *name, rbus_event_callback_t callback, void *data)
{
}

int client_event_compare(const void *left, const void *right)
{
    return 0;
}

int client_subscription_compare(const void *left, const void *right)
{
    return 0;
}

void client_subscription_create(client_subscription_t *sub, const char *object_name)
{
}

void client_subscription_destroy(void *p)
{
}

rbusCoreError_t rbus_openBrokerConnection(const char *component_name)
{
    return RBUSCORE_SUCCESS;
}

rbusCoreError_t rbus_openBrokerConnection2(const char *component_name, const char *broker_address)
{
    return RBUSCORE_SUCCESS;
}

rbusCoreError_t rbus_closeBrokerConnection()
{
    return RBUSCORE_SUCCESS;
}

rbusCoreError_t rbus_registerObj(const char *object_name, rbus_callback_t handler, void *user_data)
{
    return RBUSCORE_SUCCESS;
}

rbusCoreError_t rbus_registerMethod(const char *object_name, const char *method_name, rbus_callback_t handler, void *user_data)
{
    return RBUSCORE_SUCCESS;
}

rbusCoreError_t rbus_unregisterMethod(const char *object_name, const char *method_name)
{
    return RBUSCORE_SUCCESS;
}

rbusCoreError_t rbus_addElementEvent(const char *object_name, const char *event)
{
    return RBUSCORE_SUCCESS;
}

rbusCoreError_t rbus_registerMethodTable(const char *object_name, rbus_method_table_entry_t *table, unsigned int num_entries)
{
    return RBUSCORE_SUCCESS;
}

rbusCoreError_t rbus_unregisterMethodTable(const char *object_name, rbus_method_table_entry_t *table, unsigned int num_entries)
{
    return RBUSCORE_SUCCESS;
}

rbusCoreError_t rbus_unregisterObj(const char *object_name)
{
    return RBUSCORE_SUCCESS;
}

rbusCoreError_t rbus_addElement(const char *object_name, const char *element)
{
    return RBUSCORE_SUCCESS;
}

rbusCoreError_t rbus_removeElement(const char *object, const char *element)
{
    return RBUSCORE_SUCCESS;
}

rbusCoreError_t rbus_pushObj(const char *object_name, rbusMessage message, int timeout_millisecs)
{
    return RBUSCORE_SUCCESS;
}

rbusCoreError_t rbus_invokeRemoteMethod(const char *object_name, const char *method, rbusMessage out, int timeout_millisecs, rbusMessage *in)
{
    return RBUSCORE_SUCCESS;
}

rbusCoreError_t rbus_invokeRemoteMethod2(rtConnection myConn, const char *object_name, const char *method, rbusMessage out, int timeout_millisecs, rbusMessage *in)
{
    return RBUSCORE_SUCCESS;
}

/*TODO: make this really fire and forget.*/
rbusCoreError_t rbus_pushObjNoAck(const char *object_name, rbusMessage message)
{
    return RBUSCORE_SUCCESS;
}

rbusCoreError_t rbus_pullObj(const char *object_name, int timeout_millisecs, rbusMessage *response)
{
    return RBUSCORE_SUCCESS;
}

rbusCoreError_t rbus_registerEvent(const char *object_name, const char *event_name, rbus_event_subscribe_callback_t callback, void *user_data)
{
    return RBUSCORE_SUCCESS;
}

rbusCoreError_t rbus_unregisterEvent(const char *object_name, const char *event_name)
{
    return RBUSCORE_SUCCESS;
}

rbusCoreError_t rbus_subscribeToEvent(const char *object_name, const char *event_name, rbus_event_callback_t callback, const rbusMessage payload, void *user_data, int *providerError)
{
    return RBUSCORE_SUCCESS;
}

rbusCoreError_t rbus_subscribeToEventTimeout(const char *object_name, const char *event_name, rbus_event_callback_t callback, const rbusMessage payload, void *user_data, int *providerError, int timeout, bool publishOnSubscribe, rbusMessage *response)
{
    return RBUSCORE_SUCCESS;
}

rbusCoreError_t rbus_unsubscribeFromEvent(const char *object_name, const char *event_name, const rbusMessage payload)
{
    return RBUSCORE_SUCCESS;
}

rbusCoreError_t rbus_publishEvent(const char *object_name, const char *event_name, rbusMessage out)
{
    return RBUSCORE_SUCCESS;
}

rbusCoreError_t rbus_registerSubscribeHandler(const char *object_name, rbus_event_subscribe_callback_t callback, void *user_data)
{
    return RBUSCORE_SUCCESS;
}

rbusCoreError_t rbus_registerMasterEventHandler(rbus_event_callback_t callback, void *user_data)
{
    return RBUSCORE_SUCCESS;
}
rbusCoreError_t rbus_registerClientDisconnectHandler(rbus_client_disconnect_callback_t callback)
{
    return RBUSCORE_SUCCESS;
}

rbusCoreError_t rbus_unregisterClientDisconnectHandler()
{
    return RBUSCORE_SUCCESS;
}

rbusCoreError_t rbus_publishSubscriberEvent(const char *object_name, const char *event_name, const char *listener, rbusMessage out)
{
    return RBUSCORE_SUCCESS;
}

rbusCoreError_t rbus_discoverWildcardDestinations(const char *expression, int *count, char ***destinations)
{
    return RBUSCORE_SUCCESS;
}

rbusCoreError_t rbus_discoverObjectElements(const char *object, int *count, char ***elements)
{
    return RBUSCORE_SUCCESS;
}

rbusCoreError_t rbus_discoverElementObjects(const char *element, int *count, char ***objects)
{
    return RBUSCORE_SUCCESS;
}

rbusCoreError_t rbus_discoverElementsObjects(int numElements, const char **elements, int *count, char ***objects)
{
    return RBUSCORE_SUCCESS;
}

rbusCoreError_t rbus_discoverRegisteredComponents(int *count, char ***components)
{
    return RBUSCORE_SUCCESS;
}

rbusCoreError_t rbus_sendResponse(const rtMessageHeader *hdr, rbusMessage response)
{
    return RBUSCORE_SUCCESS;
}

void rbus_getOpenTelemetryContext(const char **traceParent, const char **traceState)
{
}

void rbus_clearOpenTelemetryContext()
{
}

void rbus_setOpenTelemetryContext(const char *traceParent, const char *traceState)
{
}

rbusCoreError_t rbuscore_updatePrivateListener(const char *pConsumerName, const char *pDMLName)
{
    return RBUSCORE_SUCCESS;
}
