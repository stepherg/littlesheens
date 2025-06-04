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

#include <rbus.h>
#include <rtRetainable.h>
#include <rtMemory.h>
#include <string.h>
#include <stdlib.h>
#include <stdarg.h>

#define VERIFY_NULL(T)      if(NULL == T){ return; }

struct _rbusProperty
{
    rtRetainable retainable;
    char* name;
    rbusValue_t value;
    struct _rbusProperty* next;
};

rbusProperty_t rbusProperty_Init(rbusProperty_t* pproperty, char const* name, rbusValue_t value)
{
    return NULL;
}

void rbusProperty_Destroy(rtRetainable* r)
{
}

void rbusProperty_Retain(rbusProperty_t property)
{
}

void rbusProperty_Release(rbusProperty_t property)
{
}

void rbusProperty_Releases(int count, ...)
{
}

void rbusProperty_fwrite(rbusProperty_t prop, int depth, FILE* fout)
{
}

int rbusProperty_Compare(rbusProperty_t property1, rbusProperty_t property2)
{
    return 0;
}

char const* rbusProperty_GetName(rbusProperty_t property)
{
    return NULL;
}

void rbusProperty_SetName(rbusProperty_t property, char const* name)
{
}

rbusValue_t rbusProperty_GetValue(rbusProperty_t property)
{
    return NULL;
}

void rbusProperty_SetValue(rbusProperty_t property, rbusValue_t value)
{
}

rbusProperty_t rbusProperty_GetNext(rbusProperty_t property)
{
    return NULL;
}

void rbusProperty_SetNext(rbusProperty_t property, rbusProperty_t next)
{
}

void rbusProperty_Append(rbusProperty_t property, rbusProperty_t back)
{
}

uint32_t rbusProperty_Count(rbusProperty_t property)
{
    return 0;
}

rbusProperty_t rbusProperty_InitString(char const* name, char const* s)
{
    return NULL;
}

rbusProperty_t rbusProperty_InitBytes(char const* name, uint8_t const* bytes, int len)
{
    return NULL;
}

char const* rbusProperty_GetString(rbusProperty_t property, int* len)
{
    return NULL;
}

uint8_t const* rbusProperty_GetBytes(rbusProperty_t property, int* len)
{
    return NULL;
}

rbusValueError_t rbusProperty_GetStringEx(rbusProperty_t property, char const** s, int* len)
{
    return RBUS_VALUE_ERROR_NULL;
}

rbusValueError_t rbusProperty_GetBytesEx(rbusProperty_t property, uint8_t const** bytes, int* len)
{
    return RBUS_VALUE_ERROR_NULL;
}

void rbusProperty_SetString(rbusProperty_t property, char const* s)
{
}

void rbusProperty_SetBytes(rbusProperty_t property, uint8_t const* bytes, int len)
{
}

rbusProperty_t rbusProperty_AppendString(rbusProperty_t property, char const* name, char const* s)
{
    return NULL;
}

rbusProperty_t rbusProperty_AppendBytes(rbusProperty_t property, char const* name, uint8_t const* bytes, int len)
{
    return NULL;
}

#define DEFINE_PROPERTY_TYPE_FUNCS(T1, T2, T3, T4)\
rbusProperty_t rbusProperty_Init##T1(char const* name, T2 data)\
{\
    rbusValue_t v = rbusValue_Init##T1(data);\
    rbusProperty_t p = rbusProperty_Init(NULL, name, v);\
    rbusValue_Release(v);\
    return p;\
}\
T2 rbusProperty_Get##T1(rbusProperty_t property)\
{\
    return rbusValue_Get##T1(property->value);\
}\
rbusValueError_t rbusProperty_Get##T1##Ex(rbusProperty_t property, T2* data)\
{\
    if(property->value)\
        return rbusValue_Get##T1##Ex(property->value, data);\
    return RBUS_VALUE_ERROR_NULL;\
}\
void rbusProperty_Set##T1(rbusProperty_t property, T2 data)\
{\
    if(property->value)\
        rbusValue_Release(property->value);\
    property->value = rbusValue_Init##T1(data);\
}\
rbusProperty_t rbusProperty_Append##T1(rbusProperty_t property, char const* name, T2 data)\
{\
    rbusProperty_t p = rbusProperty_Init##T1(name, data);\
    if(property)\
    {\
        rbusProperty_Append(property, p);\
        rbusProperty_Release(p);\
    }\
    return p;\
}

DEFINE_PROPERTY_TYPE_FUNCS(Boolean, bool, RBUS_BOOLEAN, b)
DEFINE_PROPERTY_TYPE_FUNCS(Char, char, RBUS_CHAR, c)
DEFINE_PROPERTY_TYPE_FUNCS(Byte, unsigned char, RBUS_BYTE, u)
DEFINE_PROPERTY_TYPE_FUNCS(Int8, int8_t, RBUS_INT8, i8)
DEFINE_PROPERTY_TYPE_FUNCS(UInt8, uint8_t, RBUS_UINT8, u8)
DEFINE_PROPERTY_TYPE_FUNCS(Int16, int16_t, RBUS_INT16, i16)
DEFINE_PROPERTY_TYPE_FUNCS(UInt16, uint16_t, RBUS_UINT16, u16)
DEFINE_PROPERTY_TYPE_FUNCS(Int32, int32_t, RBUS_INT32, i32)
DEFINE_PROPERTY_TYPE_FUNCS(UInt32, uint32_t, RBUS_UINT32, u32)
DEFINE_PROPERTY_TYPE_FUNCS(Int64, int64_t, RBUS_INT64, i64)
DEFINE_PROPERTY_TYPE_FUNCS(UInt64, uint64_t, RBUS_UINT64, u64)
DEFINE_PROPERTY_TYPE_FUNCS(Single, float, RBUS_SINGLE, f32)
DEFINE_PROPERTY_TYPE_FUNCS(Double, double, RBUS_DOUBLE, f64)
DEFINE_PROPERTY_TYPE_FUNCS(Time,rbusDateTime_t const*, RBUS_DATETIME, tv);
DEFINE_PROPERTY_TYPE_FUNCS(Property,struct _rbusProperty*, RBUS_PROPERTY, property);
DEFINE_PROPERTY_TYPE_FUNCS(Object,struct _rbusObject*, RBUS_OBJECT, object);
