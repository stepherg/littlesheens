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
//#include <msgpack.h>
#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <pthread.h>
#include "rbuscore_logger.h"
#include "rtRetainable.h"
#include "rtMemory.h"
#include "rbuscore_message.h"

#define VERIFY_UNPACK_NEXT_ITEM()\
    if(msgpack_unpack_next(&message->upk, message->sbuf.data, message->sbuf.size, &message->read_offset) != MSGPACK_UNPACK_SUCCESS)\
    {\
        RBUSCORELOG_ERROR("%s failed to unpack next item", __FUNCTION__);\
        return RT_FAIL;\
    }

#define VERIFY_UNPACK(T)\
    VERIFY_UNPACK_NEXT_ITEM()\
    if(message->upk.data.type != T)\
    {\
        RBUSCORELOG_ERROR("%s unexpected date type %d", __FUNCTION__, message->upk.data.type);\
        return RT_FAIL;\
    }

#define VERIFY_UNPACK2(T,T2)\
    VERIFY_UNPACK_NEXT_ITEM()\
    if(message->upk.data.type != T && message->upk.data.type != T2)\
    {\
        RBUSCORELOG_ERROR("%s unexpected date type %d", __FUNCTION__, message->upk.data.type);\
        return RT_FAIL;\
    }

#define VERIFY_PACK(T)\
    if(msgpack_pack_##T(&message->pk, value) != 0)\
    {\
        RBUSCORELOG_ERROR("%s failed pack value", __FUNCTION__);\
        return RT_FAIL;\
    }\
    return RT_OK;

#define VERIFY_PACK_BUFFER(T, V, L)\
    if(msgpack_pack_##T(&message->pk, (L)) != 0)\
    {\
        RBUSCORELOG_ERROR("%s failed pack buffer length", __FUNCTION__);\
        return RT_FAIL;\
    }\
    if(msgpack_pack_##T##_body(&message->pk, (V), (L)) != 0)\
    {\
        RBUSCORELOG_ERROR("%s failed pack buffer body", __FUNCTION__);\
        return RT_FAIL;\
    }\
    return RT_OK;


struct _rbusMessage
{
    rtRetainable retainable;
    size_t read_offset;
    int meta_offset;
};

void rbusMessage_Init(rbusMessage* message)
{
}

void rbusMessage_Destroy(rtRetainable* r)
{
}

void rbusMessage_Retain(rbusMessage message)
{
}

void rbusMessage_Release(rbusMessage message)
{
}

void rbusMessage_FromBytes(rbusMessage* message, uint8_t const* buff, uint32_t n)
{
}

void rbusMessage_ToBytes(rbusMessage message, uint8_t** buff, uint32_t* n)
{
}

void rbusMessage_ToDebugString(rbusMessage const m, char** s, uint32_t* n)
{
}

rtError rbusMessage_SetString(rbusMessage message, char const* value)
{
    return RT_OK;
}

rtError rbusMessage_GetString(rbusMessage const message, char const** value)
{
    return RT_OK;
}

rtError rbusMessage_SetBytes(rbusMessage message, uint8_t const* bytes, const uint32_t size)
{
    return RT_OK;
}

rtError rbusMessage_GetBytes(rbusMessage message, uint8_t const** value, uint32_t* size)
{
    return RT_OK;
}

rtError rbusMessage_SetInt32(rbusMessage message, int32_t value)
{
    return RT_OK;
}

rtError rbusMessage_GetInt32(rbusMessage const message, int32_t* value)
{
    return RT_OK;
}

rtError rbusMessage_GetUInt32(rbusMessage const message, uint32_t* value)
{
    return RT_OK;
}

rtError rbusMessage_SetInt64(rbusMessage message, int64_t value)
{
    return RT_OK;
}

rtError rbusMessage_GetInt64(rbusMessage const message, int64_t* value)
{
    return RT_OK;
}

rtError rbusMessage_SetDouble(rbusMessage message, double value)
{
    return RT_OK;
}

rtError rbusMessage_GetDouble(rbusMessage const message, double* value)
{
    return RT_OK;
}

rtError rbusMessage_SetMessage(rbusMessage message, rbusMessage const item)
{
    return RT_OK;
}

rtError rbusMessage_GetMessage(rbusMessage const message, rbusMessage* value)
{
    return RT_OK;
}

void rbusMessage_BeginMetaSectionWrite(rbusMessage message)
{
}

void rbusMessage_EndMetaSectionWrite(rbusMessage message)
{
}

void rbusMessage_BeginMetaSectionRead(rbusMessage message)
{
}

void rbusMessage_EndMetaSectionRead(rbusMessage message)
{
}
