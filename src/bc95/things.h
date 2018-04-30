/**
 * Things Platform communication layer.
 * 
 * Copyright (c) 2018 Sparkbit Co., Ltd. All rights reserved.
 * 
 * This work is licensed under the terms of the MIT license.  
 * See LICENSE file in the project root for details.
 */

#ifndef THINGS_PLATFORM_H
#define THINGS_PLATFORM_H

#define ARDUINOJSON_USE_LONG_LONG 1
#include "json/ArduinoJson.h"
#include "network.h"

// ----------------------------------------
//   Debugging Switches
// ----------------------------------------
#define TP_DBG_NETWORK_INIT
// #define TP_DBG_PLATFORM_INFO
#define TP_DBG_CONNECTIVITY_CHECK
// #define TP_DBG_RESOLVE_PLATFORM_IP_ADDRESS
// #define TP_DBG_TELEMETRY
// #define TP_DBG_CLIENT_ATTRIBUTES
// #define TP_DBG_SHARED_ATTRIBUTES
// #define TP_DBG_OUTGOING_RPC
// #define TP_DBG_INCOMING_RPC
// #define TP_DBG_PLATFORM_EVENT
// ----------------------------------------

#define TP_PLATFORM_HOST_NAME  "52.220.84.189"
#define TP_PLATFORM_PORT       5683
#define TP_LOCAL_PORT          0

#define TP_NETWORK_INIT_MAX_RETRY                5
#define TP_NETWORK_CONNECTIVITY_CHECK_INTERVALS  { 300000, 60000, 60000, 60000 }
#define TP_NETWORK_CONNECTIVITY_PING_TIMEOUT     5000

#define TP_COAP_VERSION    1
#define TP_COAP_TOKEN_LEN  4

#if defined(__SAM3X8E__) || defined(__SAMD21G18A__) || defined(ESP32)
    #define TP_JSON_STRING_MAX_LEN  350
    #define TP_JSON_STRING_BUF_LEN  (TP_JSON_STRING_MAX_LEN + 1)
    #define TP_STATIC_JSON_BUF_LEN  700
    #define TP_COAP_URI_MAX_LEN     256
    #define TP_COAP_BUF_LEN         512
#elif defined (__AVR_ATmega2560__)
    #define TP_JSON_STRING_MAX_LEN  150
    #define TP_JSON_STRING_BUF_LEN  (TP_JSON_STRING_MAX_LEN + 1)
    #define TP_STATIC_JSON_BUF_LEN  300
    #define TP_COAP_URI_MAX_LEN     100
    #define TP_COAP_BUF_LEN         256
#else
    #define TP_JSON_STRING_MAX_LEN  50
    #define TP_JSON_STRING_BUF_LEN  (TP_JSON_STRING_MAX_LEN + 1)
    #define TP_STATIC_JSON_BUF_LEN  100
    #define TP_COAP_URI_MAX_LEN     50
    #define TP_COAP_BUF_LEN         100
#endif

// things platform events
#define TP_EVENT_UNDEFINED                   0
#define TP_EVENT_TELEMETRY_SEND_RESPONSE     1
#define TP_EVENT_CLIENT_ATTR_READ_RESPONSE   2
#define TP_EVENT_CLIENT_ATTR_WRITE_RESPONSE  3
#define TP_EVENT_SHARED_ATTR_READ_RESPONSE   4
#define TP_EVENT_SHARED_ATTR_NOTIFY          5
#define TP_EVENT_OUTGOING_RPC_RESPONSE       6
#define TP_EVENT_INCOMING_RPC_REQUEST        7

typedef struct {
    const char *id;
    const char *name;
    const char *thingToken;
    uint8_t telemetrySendToken[TP_COAP_TOKEN_LEN];
    uint8_t clientAttrReadToken[TP_COAP_TOKEN_LEN];
    uint8_t clientAttrWriteToken[TP_COAP_TOKEN_LEN];
    uint8_t sharedAttrReadToken[TP_COAP_TOKEN_LEN];
    uint8_t sharedAttrObserveToken[TP_COAP_TOKEN_LEN];
    uint8_t outgoingRpcRequestToken[TP_COAP_TOKEN_LEN];
    uint8_t incomingRpcRequestObserveToken[TP_COAP_TOKEN_LEN];
    uint8_t incomingRpcResponseToken[TP_COAP_TOKEN_LEN];
    uint32_t sharedAttrObserveRenewInterval;
    uint32_t incomingRpcRequestObserveRenewInterval;
    unsigned long lastSharedAttrObserveMillis;
    unsigned long lastIncomingRpcRequestObserveMillis;
} thing_info_t;


void tpInit(thing_info_t *thingList, uint8_t thingCount);

thing_info_t *tpGetThingInfoById(const char *id);
thing_info_t *tpGetThingInfoByName(const char *name);

// telemetry
bool tpSendTelemetry(thing_info_t *thing, JsonObject *telemetryObj);
bool tpSendTelemetry(thing_info_t *thing, char *telemetryJsonStr);

// attributes
bool tpSendClientAttributesReadRequest(thing_info_t *thing, const char *attributesList = NULL);  // comma-separated attributes list
bool tpSendClientAttributesWriteRequest(thing_info_t *thing, JsonObject *attrObj);
bool tpSendClientAttributesWriteRequest(thing_info_t *thing, const char *attrJsonStr);
bool tpSendSharedAttributesReadRequest(thing_info_t *thing, const char *attributesList = NULL);  // comma-separated attributes list
bool tpSendSharedAttributesObserveRequest(thing_info_t *thing);

// rpc
bool tpSendOutgoingRpcRequest(thing_info_t *thing, const char *method, JsonObject *paramsObj);
bool tpSendOutgoingRpcRequest(thing_info_t *thing, const char *method, char *paramsJsonStr = NULL);
bool tpSendIncomingRpcObserveRequest(thing_info_t *thing);
bool tpSendIncomingRpcResponse(thing_info_t *thing, unsigned long rpcId, const char *method, JsonObject *rspObj);
bool tpSendIncomingRpcResponse(thing_info_t *thing, unsigned long rpcId, const char *method, const char *rspJsonStr);

void tpSetPlatformEventHandler(void (*handler)(uint8_t type, thing_info_t *thing, JsonObject *jsonObj));
void tpTaskTick();

#endif  /* THINGS_PLATFORM_H */
