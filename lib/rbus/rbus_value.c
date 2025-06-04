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
#include <stdio.h>
#include <ctype.h>
#include <assert.h>
#include <stdbool.h>
#include <stdlib.h>
#include <stdint.h>
#include <string.h>
#include <sys/time.h>
#include <time.h>
#include <errno.h>
#include <math.h>
#include <inttypes.h>
#include <float.h>
#include <rtRetainable.h>
#include <limits.h>
#include <rtMemory.h>
#include "rbus_buffer.h"
#include "rbus_log.h"

#define VERIFY_NULL(T)      if(NULL == T){ return; }
#define RBUS_TIMEZONE_LEN   6

struct _rbusValue
{
    rtRetainable retainable;
    union
    {
        bool                    b;
        char                    c;
        unsigned char           u;
        int8_t                  i8;
        uint8_t                 u8;
        int16_t                 i16;
        uint16_t                u16;
        int32_t                 i32;
        uint32_t                u32;
        int64_t                 i64;
        uint64_t                u64;
        float                   f32;
        double                  f64;
        rbusDateTime_t          tv;
        rbusBuffer_t            bytes;
        struct  _rbusProperty*  property;
        struct  _rbusObject*    object;
    } d;
    rbusValueType_t type;
};

char const* rbusValueType_ToDebugString(rbusValueType_t type)
{
    char const* s = NULL;
    #define __RBUSVALUE_STRINGIFY(E) case E: s = #E; break

    switch(type)
    {
      __RBUSVALUE_STRINGIFY(RBUS_STRING);
      __RBUSVALUE_STRINGIFY(RBUS_BYTES);
      __RBUSVALUE_STRINGIFY(RBUS_BOOLEAN);
      __RBUSVALUE_STRINGIFY(RBUS_CHAR);
      __RBUSVALUE_STRINGIFY(RBUS_BYTE);
      __RBUSVALUE_STRINGIFY(RBUS_INT16);
      __RBUSVALUE_STRINGIFY(RBUS_UINT16);
      __RBUSVALUE_STRINGIFY(RBUS_INT32);
      __RBUSVALUE_STRINGIFY(RBUS_UINT32);
      __RBUSVALUE_STRINGIFY(RBUS_INT64);
      __RBUSVALUE_STRINGIFY(RBUS_UINT64);
      __RBUSVALUE_STRINGIFY(RBUS_SINGLE);
      __RBUSVALUE_STRINGIFY(RBUS_DOUBLE);
      __RBUSVALUE_STRINGIFY(RBUS_DATETIME);
      __RBUSVALUE_STRINGIFY(RBUS_PROPERTY);
      __RBUSVALUE_STRINGIFY(RBUS_OBJECT);
      __RBUSVALUE_STRINGIFY(RBUS_NONE);
      default:
        s = "unknown type";
        break;
    }
    return s;
}

static void rbusValue_FreeInternal(rbusValue_t v)
{
}

rbusValue_t rbusValue_Init(rbusValue_t* pvalue)
{
    return NULL;
}

rbusValue_t rbusValue_InitString(char const* s)
{
    return NULL;
}

rbusValue_t rbusValue_InitBytes(uint8_t const* bytes, int len)
{
    return NULL;
}

rbusValue_t rbusValue_InitProperty(struct _rbusProperty* property)
{
    return NULL;
}

rbusValue_t rbusValue_InitObject(struct _rbusObject* object)
{
    return NULL;
}

void rbusValue_Destroy(rtRetainable* r)
{
}

void rbusValue_Retain(rbusValue_t v)
{
}

void rbusValue_Release(rbusValue_t v)
{
}

void rbusValue_Releases(int count, ...)
{
}


void rbusValue_MarshallTMtoRBUS(rbusDateTime_t* outvalue, struct tm* invalue){
    outvalue->m_time.tm_sec =  (int32_t)invalue->tm_sec;
    outvalue->m_time.tm_min =  (int32_t)invalue->tm_min;
    outvalue->m_time.tm_hour =  (int32_t)invalue->tm_hour;
    outvalue->m_time.tm_mday =  (int32_t)invalue->tm_mday;
    outvalue->m_time.tm_mon =  (int32_t)invalue->tm_mon;
    outvalue->m_time.tm_year =  (int32_t)invalue->tm_year;
    outvalue->m_time.tm_wday =  (int32_t)invalue->tm_wday;
    outvalue->m_time.tm_yday =  (int32_t)invalue->tm_yday;
    outvalue->m_time.tm_isdst =  (int32_t)invalue->tm_isdst;
}

void rbusValue_UnMarshallRBUStoTM(struct tm* outvalue, rbusDateTime_t* invalue){
    outvalue->tm_sec =  (int)invalue->m_time.tm_sec;
    outvalue->tm_min =  (int)invalue->m_time.tm_min;
    outvalue->tm_hour =  (int)invalue->m_time.tm_hour;
    outvalue->tm_mday =  (int)invalue->m_time.tm_mday;
    outvalue->tm_mon =  (int)invalue->m_time.tm_mon;
    outvalue->tm_year =  (int)invalue->m_time.tm_year;
    outvalue->tm_wday =  (int)invalue->m_time.tm_wday;
    outvalue->tm_yday =  (int)invalue->m_time.tm_yday;
    outvalue->tm_isdst =  (int)invalue->m_time.tm_isdst;
}

char* rbusValue_ToString(rbusValue_t v, char* buf, size_t buflen)
{
    return NULL;
}

char* rbusValue_ToDebugString(rbusValue_t v, char* buf, size_t buflen)
{
    return NULL;}

rbusValueType_t rbusValue_GetType(rbusValue_t v)
{
    if(!v)
        return RBUS_NONE;
    return v->type;
}

char const* rbusValue_GetString(rbusValue_t v, int* len)
{
    if(!v)
        return NULL;
    char const* bytes = (char const*)rbusValue_GetBytes(v, len);
    /* For strings, subtract 1 so the user gets back strlen instead of byte length */
    if(bytes && len && v->type == RBUS_STRING)
        (*len)--; 
    return bytes;
}

rbusValueError_t rbusValue_GetStringEx(rbusValue_t v, char const** s, int* len)
{
    if(!v)
        return RBUS_VALUE_ERROR_NULL;
    if(v->type != RBUS_STRING && v->type != RBUS_BYTES)
        return RBUS_VALUE_ERROR_TYPE;
    *s = rbusValue_GetString(v, len);
    return RBUS_VALUE_ERROR_SUCCESS;
}

/*If type is RBUS_BYTES or RBUS_STRING and data is not NULL, len will be set to the length of the byte array
    which will captures the null terminator if the type is RBUS_STRING.
  For anything else len is set to 0 and the function returns NULL
*/
uint8_t const* rbusValue_GetBytes(rbusValue_t v, int* len)
{
    if(!v)
        return NULL;
    /*v->d.bytes is NULL in the case SetBytes was called with NULL*/
    if(!v->d.bytes)
    {
        if(len)
            *len = 0;
        return NULL;
    }
    assert(v->d.bytes->data);
    assert(v->type == RBUS_STRING || v->type == RBUS_BYTES);
    assert(v->type != RBUS_STRING || strlen((char const*)v->d.bytes->data) == (size_t)v->d.bytes->posWrite-1);
    if(len)
        *len = v->d.bytes->posWrite;
    return v->d.bytes->data;
}

rbusValueError_t rbusValue_GetBytesEx(rbusValue_t v, uint8_t const** bytes, int* len)
{
    if(!v)
        return RBUS_VALUE_ERROR_NULL;
    if(v->type != RBUS_STRING && v->type != RBUS_BYTES)
        return RBUS_VALUE_ERROR_TYPE;
    *bytes = rbusValue_GetBytes(v, len);
    return RBUS_VALUE_ERROR_SUCCESS;
}

struct _rbusProperty* rbusValue_GetProperty(rbusValue_t v)
{
    assert(v->type == RBUS_PROPERTY);
    return v->d.property;
}

rbusValueError_t rbusValue_GetPropertyEx(rbusValue_t v, struct _rbusProperty** prop)
{
    if(!v)
        return RBUS_VALUE_ERROR_NULL;
    if(v->type != RBUS_PROPERTY)
        return RBUS_VALUE_ERROR_TYPE;
    *prop = v->d.property;
    return RBUS_VALUE_ERROR_SUCCESS;
}

struct _rbusObject* rbusValue_GetObject(rbusValue_t v)
{
    assert(v->type == RBUS_OBJECT);
    return v->d.object;
}

rbusValueError_t rbusValue_GetObjectEx(rbusValue_t v, struct _rbusObject** obj)
{
    if(!v)
        return RBUS_VALUE_ERROR_NULL;
    if(v->type != RBUS_OBJECT)
        return RBUS_VALUE_ERROR_TYPE;
    *obj = v->d.object;
    return RBUS_VALUE_ERROR_SUCCESS;
}

rbusValue_t rbusValue_InitTime(rbusDateTime_t const* tv)
{
    return NULL;
}

rbusDateTime_t const* rbusValue_GetTime(rbusValue_t v)
{
    if(!v)
        return NULL;
    assert(v->type == RBUS_DATETIME);
    return &v->d.tv;
}

rbusValueError_t rbusValue_GetTimeEx(rbusValue_t v, rbusDateTime_t const** tv)
{
    if(!v)
        return RBUS_VALUE_ERROR_NULL;
    if(v->type == RBUS_DATETIME)
    {
        *tv = &v->d.tv;
        return RBUS_VALUE_ERROR_SUCCESS;
    }
    return RBUS_VALUE_ERROR_TYPE;
}

void rbusValue_SetTime(rbusValue_t v, rbusDateTime_t const* tv)
{
    VERIFY_NULL(v);
    rbusValue_FreeInternal(v);
    v->d.tv = *tv;
    v->type = RBUS_DATETIME;
}

void rbusValue_SetString(rbusValue_t v, char const* s)
{
}

void rbusValue_SetBytes(rbusValue_t v, uint8_t const* p, int len)
{
}

void rbusValue_SetProperty(rbusValue_t v, struct _rbusProperty* property)
{
}

void rbusValue_SetObject(rbusValue_t v, struct _rbusObject* object)
{
}

uint8_t const* rbusValue_GetV(rbusValue_t v)
{
    if(!v)
        return NULL;
    switch(v->type)
    {
    case RBUS_STRING:
    case RBUS_BYTES:
        return v->d.bytes->data;
    default:
        return (uint8_t const*)&v->d.b;
    }
}

uint32_t rbusValue_GetL(rbusValue_t v)
{
    if(!v)
        return 1;
    switch(v->type)
    {
    case RBUS_STRING:       return v->d.bytes->posWrite; 
    case RBUS_BOOLEAN:      return sizeof(bool);
    case RBUS_INT32:        return sizeof(int32_t);
    case RBUS_UINT32:       return sizeof(uint32_t);
    case RBUS_CHAR:         return sizeof(char);
    case RBUS_BYTE:        return sizeof(unsigned char);
    case RBUS_INT8:         return sizeof(int8_t);
    case RBUS_UINT8:        return sizeof(uint8_t);
    case RBUS_INT16:        return sizeof(int16_t);
    case RBUS_UINT16:       return sizeof(uint16_t);
    case RBUS_INT64:        return sizeof(int64_t);
    case RBUS_UINT64:       return sizeof(uint64_t);
    case RBUS_SINGLE:       return sizeof(float);
    case RBUS_DOUBLE:       return sizeof(double);
    case RBUS_DATETIME:     return sizeof(rbusDateTime_t);
    case RBUS_BYTES:        return v->d.bytes->posWrite;
    default:                return 0;
    }
}

void rbusValue_SetTLV(rbusValue_t v, rbusValueType_t type, uint32_t length, void const* value)
{
    VERIFY_NULL(v);
    VERIFY_NULL(value);

    switch(type)
    {
    /*Calling rbusValue_SetString/rbusValue_SetBuffer so the value's internal buffer is created.*/
    case RBUS_STRING:
        rbusValue_SetString(v, (char const*)value);
        break;
    case RBUS_BYTES:
        rbusValue_SetBytes(v, value, length);
        break;
    default:
        rbusValue_FreeInternal(v);
        v->type = type;
        memcpy(&v->d.b, value, length);
        break;
    }
    assert(rbusValue_GetL(v) == length);
}

int rbusValue_Decode(rbusValue_t* value, rbusBuffer_t const buff)
{
    return -1;
}

void rbusValue_Encode(rbusValue_t value, rbusBuffer_t buff)
{
}

int rbusValue_Compare(rbusValue_t v1, rbusValue_t v2)
{
    return 0;
}

void rbusValue_SetPointer(rbusValue_t* value1, rbusValue_t value2)
{
}

void rbusValue_Swap(rbusValue_t* v1, rbusValue_t* v2)
{
}


void rbusValue_Copy(rbusValue_t dest, rbusValue_t source)
{
}

bool rbusValue_SetFromString(rbusValue_t value, rbusValueType_t type, const char* pStringInput)
{
    return true;
}

void rbusValue_fwrite(rbusValue_t value, int depth, FILE* fout)
{
}

#define DEFINE_VALUE_TYPE_FUNCS(T1, T2, T3, T4)\
rbusValue_t rbusValue_Init##T1(T2 data)\
{\
    rbusValue_t v;\
    rbusValue_Init(&v);\
    rbusValue_Set##T1(v,data);\
    return v;\
}\
T2 rbusValue_Get##T1(rbusValue_t v)\
{\
    assert(v->type == T3);\
    return v->d.T4;\
}\
rbusValueError_t rbusValue_Get##T1##Ex(rbusValue_t v, T2* data)\
{\
    if(v->type == T3)\
    {\
        *data = v->d.T4;\
        return RBUS_VALUE_ERROR_SUCCESS;\
    }\
    return RBUS_VALUE_ERROR_TYPE;\
}\
void rbusValue_Set##T1(rbusValue_t v, T2 data)\
{\
    rbusValue_FreeInternal(v);\
    v->d.T4 = data;\
    v->type = T3;\
}

DEFINE_VALUE_TYPE_FUNCS(Boolean, bool, RBUS_BOOLEAN, b)
DEFINE_VALUE_TYPE_FUNCS(Char, char, RBUS_CHAR, c)
DEFINE_VALUE_TYPE_FUNCS(Byte, unsigned char, RBUS_BYTE, u)
DEFINE_VALUE_TYPE_FUNCS(Int8, int8_t, RBUS_INT8, i8)
DEFINE_VALUE_TYPE_FUNCS(UInt8, uint8_t, RBUS_UINT8, u8)
DEFINE_VALUE_TYPE_FUNCS(Int16, int16_t, RBUS_INT16, i16)
DEFINE_VALUE_TYPE_FUNCS(UInt16, uint16_t, RBUS_UINT16, u16)
DEFINE_VALUE_TYPE_FUNCS(Int32, int32_t, RBUS_INT32, i32)
DEFINE_VALUE_TYPE_FUNCS(UInt32, uint32_t, RBUS_UINT32, u32)
DEFINE_VALUE_TYPE_FUNCS(Int64, int64_t, RBUS_INT64, i64)
DEFINE_VALUE_TYPE_FUNCS(UInt64, uint64_t, RBUS_UINT64, u64)
DEFINE_VALUE_TYPE_FUNCS(Single, float, RBUS_SINGLE, f32)
DEFINE_VALUE_TYPE_FUNCS(Double, double, RBUS_DOUBLE, f64)
