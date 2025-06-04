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
#include <stdlib.h>
#include <string.h>
#include <assert.h>
#include <stdarg.h>

struct _rbusObject
{
    rtRetainable retainable;
    char* name;
    rbusObjectType_t type;      /*single or multi-instance*/
    rbusProperty_t properties;  /*the list of properties(tr181 parameters) on this object*/
    struct _rbusObject* parent;  /*the object's parent (NULL for root object)*/
    struct _rbusObject* children;/*the object's child objects which could be a mix of single or multi instance tables (not rows)*/
    struct _rbusObject* next;    /*this object's siblings where the type of sibling is based on parent type
                                  1) if parent is RBUS_OBJECT_SINGLE_INSTANCE | RBUS_OBJECT_MULTI_INSTANCE_ROW: 
                                        list of RBUS_OBJECT_SINGLE_INSTANCE and/or RBUS_OBJECT_MULTI_INSTANCE_TABLE
                                  2) if parent RBUS_OBJECT_MULTI_INSTANCE_TABLE: next is in a list of RBUS_OBJECT_MULTI_INSTANCE_ROW*/
};

rbusObject_t rbusObject_Init(rbusObject_t* pobject, char const* name)
{
    return NULL;
}

void rbusObject_InitMultiInstance(rbusObject_t* pobject, char const* name)
{
}

void rbusObject_Destroy(rtRetainable* r)
{
}

void rbusObject_Retain(rbusObject_t object)
{
}

void rbusObject_Release(rbusObject_t object)
{
}

void rbusObject_Releases(int count, ...)
{
}

int rbusObject_Compare(rbusObject_t object1, rbusObject_t object2, bool recursive)
{
    return 0;
}

char const* rbusObject_GetName(rbusObject_t object)
{
    return NULL;
}

void rbusObject_SetName(rbusObject_t object, char const* name)
{
}

rbusProperty_t rbusObject_GetProperties(rbusObject_t object)
{
    return NULL;
}

void rbusObject_SetProperties(rbusObject_t object, rbusProperty_t properties)
{
}

rbusProperty_t rbusObject_GetProperty(rbusObject_t object, char const* name)
{
    return NULL;
}

void rbusObject_SetProperty(rbusObject_t object, rbusProperty_t newProp)
{
}

rbusValue_t rbusObject_GetValue(rbusObject_t object, char const* name)
{
    return NULL;
}

rbusValue_t rbusObject_GetPropertyValue(rbusObject_t object, char const* name)
{
    return NULL;
}

void rbusObject_SetValue(rbusObject_t object, char const* name, rbusValue_t value)
{
}

void rbusObject_SetPropertyValue(rbusObject_t object, char const* name, rbusValue_t value)
{
}


rbusObject_t rbusObject_GetParent(rbusObject_t object)
{
    return NULL;
}

void rbusObject_SetParent(rbusObject_t object, rbusObject_t parent)
{
}

rbusObject_t rbusObject_GetChildren(rbusObject_t object)
{
    return NULL;
}

void rbusObject_SetChildren(rbusObject_t object, rbusObject_t children)
{
}

rbusObject_t rbusObject_GetNext(rbusObject_t object)
{
    return NULL;
}

void rbusObject_SetNext(rbusObject_t object, rbusObject_t next)
{
}

void rbusObject_SetPropertyBoolean(rbusObject_t object, char const *name, bool b)
{

}
void rbusObject_SetPropertyInt8(rbusObject_t object, char const *name, int8_t i8)
{

}
void rbusObject_SetPropertyUInt8(rbusObject_t object, char const *name, uint8_t u8)
{

}
void rbusObject_SetPropertyInt16(rbusObject_t object, char const *name, int16_t i16)
{

}
void rbusObject_SetPropertyUInt16(rbusObject_t object, char const *name, uint16_t u16)
{

}
void rbusObject_SetPropertyInt32(rbusObject_t object, char const *name, int32_t i32)
{

}
void rbusObject_SetPropertyUInt32(rbusObject_t object, char const *name, uint32_t u32)
{

}
void rbusObject_SetPropertyInt64(rbusObject_t object, char const *name, int64_t i64)
{

}
void rbusObject_SetPropertyUInt64(rbusObject_t object, char const *name, uint64_t u64)
{

}
void rbusObject_SetPropertySingle(rbusObject_t object, char const *name, float f32)
{

}
void rbusObject_SetPropertyDouble(rbusObject_t object, char const *name, double f64)
{

}
void rbusObject_SetPropertyTime(rbusObject_t object, char const *name, rbusDateTime_t const *tv)
{

}
void rbusObject_SetPropertyString(rbusObject_t object, char const *name, char const *s)
{

}
void rbusObject_SetPropertyBytes(rbusObject_t object, char const *name, uint8_t const *bytes, int len)
{

}
void rbusObject_SetPropertyProperty(rbusObject_t object, char const *name, struct _rbusProperty *p)
{

}
void rbusObject_SetPropertyObject(rbusObject_t object, char const *name, struct _rbusObject *o)
{

}
void rbusObject_SetPropertyChar(rbusObject_t object, char const *name, char c)
{

}
void rbusObject_SetPropertyByte(rbusObject_t object, char const *name, unsigned char c)
{

}

rbusValueError_t rbusObject_GetPropertyString(rbusObject_t object, char const* name, char const** pdata, int* len)
{
    return RBUS_VALUE_ERROR_NULL;
}

rbusValueError_t rbusObject_GetPropertyBytes(rbusObject_t object, char const* name, uint8_t const** pdata, int* len)
{
    return RBUS_VALUE_ERROR_NULL;
}
