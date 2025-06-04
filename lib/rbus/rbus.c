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

#include <stdio.h>
#include <ctype.h>
#include <string.h>
#include <stdlib.h>
#include <assert.h>
#include <pthread.h>
#include <unistd.h>
#include <rtVector.h>
#include <rtMemory.h>
#include <rbuscore.h>
#include <rbus_session_mgr.h>
#include <rbus.h>
#include "rbus_buffer.h"
#include "rbus_element.h"
#include "rbus_valuechange.h"
#include "rbus_subscriptions.h"
#include "rbus_asyncsubscribe.h"
#include "rbus_intervalsubscription.h"
#include "rbus_config.h"
#include "rbus_log.h"
#include "rbus_handle.h"
#include "rbus_message.h"

//******************************* MACROS *****************************************//
#define UNUSED1(a)              (void)(a)
#define UNUSED2(a,b)            UNUSED1(a),UNUSED1(b)
#define UNUSED3(a,b,c)          UNUSED1(a),UNUSED2(b,c)
#define UNUSED4(a,b,c,d)        UNUSED1(a),UNUSED3(b,c,d)
#define UNUSED5(a,b,c,d,e)      UNUSED1(a),UNUSED4(b,c,d,e)
#define UNUSED6(a,b,c,d,e,f)    UNUSED1(a),UNUSED5(b,c,d,e,f)
#define RBUS_MIN(a,b) ((a)<(b) ? (a) : (b))

#ifndef FALSE
#define FALSE                               0
#endif
#ifndef TRUE
#define TRUE                                1
#endif
#define VERIFY_NULL(T)          if(NULL == T){ RBUSLOG_WARN(#T" is NULL"); return RBUS_ERROR_INVALID_INPUT; }
#define VERIFY_ZERO(T)          if(0 == T){ RBUSLOG_WARN(#T" is 0"); return RBUS_ERROR_INVALID_INPUT; }

//********************************************************************************//

//******************************* STRUCTURES *************************************//
struct _rbusMethodAsyncHandle
{
    rtMessageHeader hdr;
};

typedef enum _rbus_legacy_support
{
    RBUS_LEGACY_STRING = 0,    /**< Null terminated string                                           */
    RBUS_LEGACY_INT,           /**< Integer (2147483647 or -2147483648) as String                    */
    RBUS_LEGACY_UNSIGNEDINT,   /**< Unsigned Integer (ex: 4,294,967,295) as String                   */
    RBUS_LEGACY_BOOLEAN,       /**< Boolean as String (ex:"true", "false"                            */
    RBUS_LEGACY_DATETIME,      /**< ISO-8601 format (YYYY-MM-DDTHH:MM:SSZ) as String                 */
    RBUS_LEGACY_BASE64,        /**< Base64 representation of data as String                          */
    RBUS_LEGACY_LONG,          /**< Long (ex: 9223372036854775807 or -9223372036854775808) as String */
    RBUS_LEGACY_UNSIGNEDLONG,  /**< Unsigned Long (ex: 18446744073709551615) as String               */
    RBUS_LEGACY_FLOAT,         /**< Float (ex: 1.2E-38 or 3.4E+38) as String                         */
    RBUS_LEGACY_DOUBLE,        /**< Double (ex: 2.3E-308 or 1.7E+308) as String                      */
    RBUS_LEGACY_BYTE,
    RBUS_LEGACY_NONE
} rbusLegacyDataType_t;

typedef enum _rbus_legacy_returns {
    RBUS_LEGACY_ERR_SUCCESS = 100,
    RBUS_LEGACY_ERR_MEMORY_ALLOC_FAIL = 101,
    RBUS_LEGACY_ERR_FAILURE = 102,
    RBUS_LEGACY_ERR_NOT_CONNECT = 190,
    RBUS_LEGACY_ERR_TIMEOUT = 191,
    RBUS_LEGACY_ERR_NOT_EXIST = 192,
    RBUS_LEGACY_ERR_NOT_SUPPORT = 193,
    RBUS_LEGACY_ERR_RESOURCE_EXCEEDED = 9004,
    RBUS_LEGACY_ERR_INVALID_PARAMETER_NAME = 9005,
    RBUS_LEGACY_ERR_INVALID_PARAMETER_TYPE = 9006,
    RBUS_LEGACY_ERR_INVALID_PARAMETER_VALUE = 9007,
    RBUS_LEGACY_ERR_NOT_WRITABLE = 9008,
} rbusLegacyReturn_t;

//********************************************************************************//

extern char* __progname;
//******************************* GLOBALS *****************************************//

//********************************************************************************//

void rbusEventSubscription_free(void* p)
{
}

rbusError_t rbusValue_initFromMessage(rbusValue_t* value, rbusMessage msg);
void rbusValue_appendToMessage(char const* name, rbusValue_t value, rbusMessage msg);
rbusError_t rbusProperty_initFromMessage(rbusProperty_t* property, rbusMessage msg);
void rbusPropertyList_initFromMessage(rbusProperty_t* prop, rbusMessage msg);
void rbusPropertyList_appendToMessage(rbusProperty_t prop, rbusMessage msg);
void rbusObject_initFromMessage(rbusObject_t* obj, rbusMessage msg);
void rbusObject_appendToMessage(rbusObject_t obj, rbusMessage msg);
void rbusEventData_updateFromMessage(rbusEvent_t* event, rbusFilter_t* filter, uint32_t* interval,
        uint32_t* duration, int32_t* componentId, rbusMessage msg);
void rbusEventData_appendToMessage(rbusEvent_t* event, rbusFilter_t filter, uint32_t interval,
        uint32_t duration, int32_t componentId, rbusMessage msg);
void rbusFilter_AppendToMessage(rbusFilter_t filter, rbusMessage msg);
void rbusFilter_InitFromMessage(rbusFilter_t* filter, rbusMessage msg);

rbusError_t rbusValue_initFromMessage(rbusValue_t* value, rbusMessage msg)
{
    return RBUS_ERROR_SUCCESS;
}

rbusError_t rbusProperty_initFromMessage(rbusProperty_t* property, rbusMessage msg)
{
    return RBUS_ERROR_SUCCESS;
}

void rbusPropertyList_appendToMessage(rbusProperty_t prop, rbusMessage msg)
{
}

void rbusPropertyList_initFromMessage(rbusProperty_t* prop, rbusMessage msg)
{
}

void rbusObject_appendToMessage(rbusObject_t obj, rbusMessage msg)
{
}

void rbusObject_initFromMessage(rbusObject_t* obj, rbusMessage msg)
{
}

void rbusValue_appendToMessage(char const* name, rbusValue_t value, rbusMessage msg)
{
}

void rbusFilter_AppendToMessage(rbusFilter_t filter, rbusMessage msg)
{
}

void rbusFilter_InitFromMessage(rbusFilter_t* filter, rbusMessage msg)
{
}

void rbusEventData_updateFromMessage(rbusEvent_t* event, rbusFilter_t* filter,
        uint32_t* interval, uint32_t* duration, int32_t* componentId, rbusMessage msg)
{
}

void rbusEventData_appendToMessage(rbusEvent_t* event, rbusFilter_t filter,
        uint32_t interval, uint32_t duration, int32_t componentId, rbusMessage msg)
{
}

void valueChangeTableRowUpdate(rbusHandle_t handle, elementNode* rowNode, bool added)
{
}

rbusError_t get_recursive_wildcard_handler (rbusHandle_t handle, char const *parameterName, const char* pRequestingComp, rbusProperty_t properties, int *pCount)
{
    return RBUS_ERROR_SUCCESS;
}

//******************************* Bus Initialization *****************************//

rbusError_t rbus_open(rbusHandle_t* handle, char const* componentName)
{
    return RBUS_ERROR_SUCCESS;
}


rbusError_t rbus_openDirect(rbusHandle_t handle, rbusHandle_t* myDirectHandle, char const* pParameterName)
{
    return RBUS_ERROR_SUCCESS;    
}

rbusError_t rbus_closeDirect(rbusHandle_t handle)
{
    return RBUS_ERROR_SUCCESS;
}

rbusError_t rbus_close(rbusHandle_t handle)
{
    return RBUS_ERROR_SUCCESS;
}

rbusError_t rbus_regDataElements(
    rbusHandle_t handle,
    int numDataElements,
    rbusDataElement_t *elements)
{
    return RBUS_ERROR_SUCCESS;
}

rbusError_t rbus_unregDataElements(
    rbusHandle_t handle,
    int numDataElements,
    rbusDataElement_t *elements)
{
    return RBUS_ERROR_SUCCESS;
}

//************************* Discovery related Operations *******************//
rbusError_t rbus_discoverComponentName (rbusHandle_t handle,
                            int numElements, char const** elementNames,
                            int *numComponents, char ***componentName)
{
    return RBUS_ERROR_SUCCESS;
}

rbusError_t rbus_discoverComponentDataElements (rbusHandle_t handle,
                            char const* name, bool nextLevel,
                            int *numElements, char*** elementNames)
{
    return RBUS_ERROR_SUCCESS;
}

//************************* Parameters related Operations *******************//
rbusError_t rbus_get(rbusHandle_t handle, char const* name, rbusValue_t* value)
{
    return RBUS_ERROR_SUCCESS;
}


rbusError_t rbus_getExt(rbusHandle_t handle, int paramCount, char const** pParamNames, int *numValues, rbusProperty_t* retProperties)
{
    return RBUS_ERROR_SUCCESS;
}

static rbusError_t rbus_getByType(rbusHandle_t handle, char const* paramName, void* paramVal, rbusValueType_t type)
{
    return RBUS_ERROR_SUCCESS;
}

rbusError_t rbus_getBoolean(rbusHandle_t handle, char const* paramName, bool* paramVal)
{
    return rbus_getByType(handle, paramName, paramVal, RBUS_BOOLEAN);
}

rbusError_t rbus_getInt(rbusHandle_t handle, char const* paramName, int* paramVal)
{
    return rbus_getByType(handle, paramName, paramVal, RBUS_INT32);
}

rbusError_t rbus_getUint (rbusHandle_t handle, char const* paramName, unsigned int* paramVal)
{
    return rbus_getByType(handle, paramName, paramVal, RBUS_UINT32);
}

rbusError_t rbus_getStr (rbusHandle_t handle, char const* paramName, char** paramVal)
{
    return rbus_getByType(handle, paramName, paramVal, RBUS_STRING);
}

rbusError_t rbus_set(rbusHandle_t handle, char const* name,rbusValue_t value, rbusSetOptions_t* opts)
{
    return RBUS_ERROR_SUCCESS;
}

rbusError_t rbus_setMulti(rbusHandle_t handle, int numProps, rbusProperty_t properties, rbusSetOptions_t* opts)
{
    return RBUS_ERROR_SUCCESS;
}

static rbusError_t rbus_setByType(rbusHandle_t handle, char const* paramName, void const* paramVal, rbusValueType_t type)
{
    return RBUS_ERROR_SUCCESS;
}

rbusError_t rbus_setBoolean(rbusHandle_t handle, char const* paramName, bool paramVal)
{
    return rbus_setByType(handle, paramName, &paramVal, RBUS_BOOLEAN);
}

rbusError_t rbus_setInt(rbusHandle_t handle, char const* paramName, int paramVal)
{
    return rbus_setByType(handle, paramName, &paramVal, RBUS_INT32);
}

rbusError_t rbus_setUInt(rbusHandle_t handle, char const* paramName, unsigned int paramVal)
{
    return rbus_setByType(handle, paramName, &paramVal, RBUS_UINT32);
}

rbusError_t rbus_setStr(rbusHandle_t handle, char const* paramName, char const* paramVal)
{
    return rbus_setByType(handle, paramName, paramVal, RBUS_STRING);
}

rbusError_t rbusTable_addRow(
    rbusHandle_t handle,
    char const* tableName,
    char const* aliasName,
    uint32_t* instNum)
{
    return RBUS_ERROR_SUCCESS;
}

rbusError_t rbusTable_removeRow(
    rbusHandle_t handle,
    char const* rowName)
{
    return RBUS_ERROR_SUCCESS;
}

rbusError_t rbusTable_registerRow(
    rbusHandle_t handle,
    char const* tableName,
    uint32_t instNum,
    char const* aliasName)
{
    return RBUS_ERROR_SUCCESS;
}

rbusError_t rbusTable_unregisterRow(
    rbusHandle_t handle,
    char const* rowName)
{
    return RBUS_ERROR_SUCCESS;
}

rbusError_t rbusTable_getRowNames(
    rbusHandle_t handle,
    char const* tableName,
    rbusRowName_t** rowNames)
{
    return RBUS_ERROR_SUCCESS;
}

rbusError_t rbusTable_freeRowNames(
    rbusHandle_t handle,
    rbusRowName_t* rowNames)
{
    return RBUS_ERROR_SUCCESS;
}

rbusError_t rbusElementInfo_get(
    rbusHandle_t handle,
    char const* elemName,
    int depth,
    rbusElementInfo_t** elemInfo)
{
    return RBUS_ERROR_SUCCESS;
}

rbusError_t rbusElementInfo_free(
    rbusHandle_t handle, 
    rbusElementInfo_t* elemInfo)
{
    return RBUS_ERROR_SUCCESS;
}

rbusError_t rbusEvent_Subscribe(
    rbusHandle_t handle,
    char const *eventName,
    rbusEventHandler_t handler,
    void *userData,
    int timeout)
{
    return RBUS_ERROR_SUCCESS;
}

rbusError_t  rbusEvent_SubscribeAsync(
    rbusHandle_t                    handle,
    char const*                     eventName,
    rbusEventHandler_t              handler,
    rbusSubscribeAsyncRespHandler_t subscribeHandler,
    void*                           userData,
    int                             timeout)
{
    return RBUS_ERROR_SUCCESS;
}

rbusError_t rbusEvent_Unsubscribe(
    rbusHandle_t        handle,
    char const*         eventName)
{
    return RBUS_ERROR_SUCCESS;
}

rbusError_t rbusEvent_SubscribeEx(
    rbusHandle_t                handle,
    rbusEventSubscription_t*    subscription,
    int                         numSubscriptions,
    int                         timeout)
{

    return RBUS_ERROR_SUCCESS;
}

rbusError_t rbusEvent_SubscribeExAsync(
    rbusHandle_t                    handle,
    rbusEventSubscription_t*        subscription,
    int                             numSubscriptions,
    rbusSubscribeAsyncRespHandler_t subscribeHandler,
    int                             timeout)
{

    return RBUS_ERROR_SUCCESS;
}

rbusError_t rbusEvent_UnsubscribeEx(
    rbusHandle_t                handle,
    rbusEventSubscription_t*    subscription,
    int                         numSubscriptions)
{
    return RBUS_ERROR_SUCCESS;
}

rbusError_t  rbusEvent_Publish(
  rbusHandle_t          handle,
  rbusEvent_t*          eventData)
{
    return RBUS_ERROR_SUCCESS;
}

rbusError_t rbusMethod_InvokeInternal(
    rbusHandle_t handle, 
    char const* methodName, 
    rbusObject_t inParams, 
    rbusObject_t* outParams,
    int timeout)
{
    return RBUS_ERROR_SUCCESS;
}

rbusError_t rbusMethod_Invoke(
    rbusHandle_t handle, 
    char const* methodName, 
    rbusObject_t inParams, 
    rbusObject_t* outParams)
{
    return RBUS_ERROR_SUCCESS;
}

rbusError_t rbusMethod_InvokeAsync(
    rbusHandle_t handle, 
    char const* methodName, 
    rbusObject_t inParams, 
    rbusMethodAsyncRespHandler_t callback, 
    int timeout)
{
    return RBUS_ERROR_SUCCESS;
}

rbusError_t rbusMethod_SendAsyncResponse(
    rbusMethodAsyncHandle_t asyncHandle,
    rbusError_t error,
    rbusObject_t outParams)
{
    return RBUS_ERROR_SUCCESS;
}

rbusError_t rbus_createSession(rbusHandle_t handle, uint32_t *pSessionId)
{
    return RBUS_ERROR_SUCCESS;
}

rbusError_t rbus_closeSession(rbusHandle_t handle, uint32_t sessionId)
{
    return RBUS_ERROR_SUCCESS;
}

rbusError_t rbusHandle_ClearTraceContext(
    rbusHandle_t  rbus)
{
    return RBUS_ERROR_SUCCESS;
}

rbusError_t rbusHandle_SetTraceContextFromString(
    rbusHandle_t  rbus,
    char const*   traceParent,
    char const*   traceState)
{
    return RBUS_ERROR_SUCCESS;
}

rbusError_t rbusHandle_GetTraceContextAsString(
    rbusHandle_t  rbus,
    char*         traceParent,
    int           traceParentLength,
    char*         traceState,
    int           traceStateLength)
{
    return RBUS_ERROR_SUCCESS;
}
