/**
 * Things Platform communication layer.
 * 
 * Copyright (c) 2018 Sparkbit Co., Ltd. All rights reserved.
 * 
 * This work is licensed under the terms of the MIT license.  
 * See LICENSE file in the project root for details.
 */

#include "things.h"
#include "debug.h"

static Sparkbit::Debug dbg("TP");

static const char *platformHostName = TP_PLATFORM_HOST_NAME;
static char platformIPAddrStr[16];  // xxx.xxx.xxx.xxx
static const uint16_t platformPort = TP_PLATFORM_PORT;
static const uint16_t localPort = TP_LOCAL_PORT;

static const char *apiPrefix = "/api/v1";

static thing_info_t *thingList;
static uint8_t thingCount;

static uint16_t networkInitRetryCount;

void (*hPlatformEvent)(uint8_t type, thing_info_t *thing, JsonObject *jsonObj) = NULL;

void hIncomingCoAPMessage(const char *srcAddrStr, uint16_t srcPort, uint16_t dstPort, CoapPDU *message);

// ----------------------------------------
//   Initialization
// ----------------------------------------
void tpInit(thing_info_t *_thingList, uint8_t _thingCount) {
    thingList = _thingList;
    thingCount = _thingCount;

    netInit();

    // init the network
    networkInitRetryCount = 0;
    while (netInitNetwork() != true) {
        if (++networkInitRetryCount >= TP_NETWORK_INIT_MAX_RETRY) {
          #ifdef TP_DBG_NETWORK_INIT
            dbg.print("Too many network initialization failures")
                .tagOff()
                .print(" (limit=")
                .print(TP_NETWORK_INIT_MAX_RETRY)
                .println(")")
                .tagOn();
          #endif
            
            // TODO power cycle the board
        }

        delay(100);
    }

    // netSetIncomingUDPPacketHandler(hIncomingUDPPacket);
    netSetIncomingCoAPMessageHandler(hIncomingCoAPMessage);

    // resolve platform host name to IP address
    if (strlen(platformIPAddrStr) == 0) {
        // TODO resolve using DNS
        strcpy(platformIPAddrStr, platformHostName);
    }

  #ifdef TP_DBG_PLATFORM_INFO
    dbg
        .print("Platform")
        .tagOff()
        .print(", ip=")
        .print(platformIPAddrStr)
        .print(", port=")
        .println(platformPort)
        .tagOn();
  #endif
}

// ----------------------------------------
//   Thing
// ----------------------------------------
thing_info_t *tpGetThingInfoById(const char *id) {
    for (int i = 0 ; i < thingCount ; i++) {
        if (strcmp(id, thingList[i].id) == 0) {
            return &thingList[i];
        }
    }

    return NULL;
}

thing_info_t *tpGetThingInfoByName(const char *name) {
    for (int i = 0 ; i < thingCount ; i++) {
        if (strcmp(name, thingList[i].name) == 0) {
            return &thingList[i];
        }
    }

    return NULL;
}

// ----------------------------------------
//   Telemetry
// ----------------------------------------
bool tpSendTelemetry(thing_info_t *thing, JsonObject *telemetryObj) {
    char telemetryJsonStr[TP_JSON_STRING_BUF_LEN];
    telemetryObj->printTo(telemetryJsonStr);

    return tpSendTelemetry(thing, telemetryJsonStr);
}

bool tpSendTelemetry(thing_info_t *thing, char *telemetryJsonStr) {
    uint8_t coapBuf[TP_COAP_BUF_LEN];
    CoapPDU message(coapBuf, TP_COAP_BUF_LEN);

    uint16_t messageId = netGetNextCoAPMessageId();
    char uri[TP_COAP_URI_MAX_LEN];

  #ifdef TP_DBG_TELEMETRY
    dbg
        .print("Send telemetry, thingToken=")
        .tagOff()
        .print(thing->thingToken)
        .print(", mid=")
        .hexShort(messageId, true, false)
        .print(", coapToken=")
        .hexString(thing->sharedAttrObserveToken, TP_COAP_TOKEN_LEN, true, false)
        .print(", telemetry=")
        .println(telemetryJsonStr)
        .tagOn();
  #endif

    sprintf(uri, "%s/%s/telemetry", apiPrefix, thing->thingToken);
    message.reset();
    message.setVersion(TP_COAP_VERSION);
    message.setType(CoapPDU::COAP_CONFIRMABLE);
    message.setCode(CoapPDU::COAP_POST);
    message.setMessageID(messageId);
    message.setToken(thing->telemetrySendToken, TP_COAP_TOKEN_LEN);
    message.setURI(uri);
    message.setPayload((uint8_t *)telemetryJsonStr, strlen(telemetryJsonStr));

    return netSendCoAPMessage(platformIPAddrStr, platformPort, localPort, &message);
}

// ----------------------------------------
//   Attributes
// ----------------------------------------
bool tpSendClientAttributesReadRequest(thing_info_t *thing, const char *attributesList) {
    uint8_t coapBuf[TP_COAP_BUF_LEN];
    CoapPDU message(coapBuf, TP_COAP_BUF_LEN);

    uint16_t messageId = netGetNextCoAPMessageId();
    char uri[TP_COAP_URI_MAX_LEN];

  #ifdef TP_DBG_CLIENT_ATTRIBUTES
    dbg
        .print("Read client attr., thingToken=")
        .tagOff()
        .print(thing->thingToken)
        .print(", mid=")
        .hexShort(messageId, true, false)
        .print(", coapToken=")
        .hexString(thing->clientAttrReadToken, TP_COAP_TOKEN_LEN, true, false)
        .print(", attr=")
        .println(attributesList)
        .tagOn();
  #endif

    if (attributesList != NULL) {
        sprintf(uri, "%s/%s/attributes?clientKeys=%s", apiPrefix, thing->thingToken, attributesList);
    }
    else {
        sprintf(uri, "%s/%s/attributes", apiPrefix, thing->thingToken);
    }

    message.reset();
    message.setVersion(TP_COAP_VERSION);
    message.setType(CoapPDU::COAP_CONFIRMABLE);
    message.setCode(CoapPDU::COAP_GET);
    message.setMessageID(messageId);
    message.setToken(thing->clientAttrReadToken, TP_COAP_TOKEN_LEN);
    message.setURI(uri);

    return netSendCoAPMessage(platformIPAddrStr, platformPort, localPort, &message);
}

bool tpSendClientAttributesWriteRequest(thing_info_t *thing, JsonObject *attrObj) {
    char attrJsonStr[TP_JSON_STRING_BUF_LEN];
    attrObj->printTo(attrJsonStr);

    return tpSendClientAttributesWriteRequest(thing, attrJsonStr);
}

bool tpSendClientAttributesWriteRequest(thing_info_t *thing, const char *attrJsonStr) {
    uint8_t coapBuf[TP_COAP_BUF_LEN];
    CoapPDU message(coapBuf, TP_COAP_BUF_LEN);

    uint16_t messageId = netGetNextCoAPMessageId();
    char uri[TP_COAP_URI_MAX_LEN];

  #ifdef TP_DBG_CLIENT_ATTRIBUTES
    dbg
        .print("Write client attr., thingToken=")
        .tagOff()
        .print(thing->thingToken)
        .print(", mid=")
        .hexShort(messageId, true, false)
        .print(", coapToken=")
        .hexString(thing->clientAttrWriteToken, TP_COAP_TOKEN_LEN, true, false)
        .print(", attr=")
        .println(attrJsonStr)
        .tagOn();
  #endif
    
    sprintf(uri, "%s/%s/attributes", apiPrefix, thing->thingToken);
    message.reset();
    message.setVersion(TP_COAP_VERSION);
    message.setType(CoapPDU::COAP_CONFIRMABLE);
    message.setCode(CoapPDU::COAP_POST);
    message.setMessageID(messageId);
    message.setToken(thing->clientAttrWriteToken, TP_COAP_TOKEN_LEN);
    message.setURI(uri);
    message.setPayload((uint8_t *)attrJsonStr, strlen(attrJsonStr));

    return netSendCoAPMessage(platformIPAddrStr, platformPort, localPort, &message);
}

bool tpSendSharedAttributesReadRequest(thing_info_t *thing, const char *attributesList) {
    uint8_t coapBuf[TP_COAP_BUF_LEN];
    CoapPDU message(coapBuf, TP_COAP_BUF_LEN);

    uint16_t messageId = netGetNextCoAPMessageId();
    char uri[TP_COAP_URI_MAX_LEN];

  #ifdef TP_DBG_SHARED_ATTRIBUTES
    dbg
        .print("Read shared attr., thingToken=")
        .tagOff()
        .print(thing->thingToken)
        .print(", mid=")
        .hexShort(messageId, true, false)
        .print(", coapToken=")
        .hexString(thing->sharedAttrReadToken, TP_COAP_TOKEN_LEN, true, false)
        .print(", attr=")
        .println(attributesList)
        .tagOn();
  #endif

    if (attributesList != NULL) {
        sprintf(uri, "%s/%s/attributes?sharedKeys=%s", apiPrefix, thing->thingToken, attributesList);
    }
    else {
        sprintf(uri, "%s/%s/attributes", apiPrefix, thing->thingToken);
    }

    message.reset();
    message.setVersion(TP_COAP_VERSION);
    message.setType(CoapPDU::COAP_CONFIRMABLE);
    message.setCode(CoapPDU::COAP_GET);
    message.setMessageID(messageId);
    message.setToken(thing->sharedAttrReadToken, TP_COAP_TOKEN_LEN);
    message.setURI(uri);

    return netSendCoAPMessage(platformIPAddrStr, platformPort, localPort, &message);
}

bool tpSendSharedAttributesObserveRequest(thing_info_t *thing) {
    uint8_t coapBuf[TP_COAP_BUF_LEN];
    CoapPDU message(coapBuf, TP_COAP_BUF_LEN);

    uint16_t messageId = netGetNextCoAPMessageId();
    char uri[TP_COAP_URI_MAX_LEN];

    // register observation
    uint8_t obsOptionData[1] = {0};

  #ifdef TP_DBG_SHARED_ATTRIBUTES
    dbg
        .print("Observe shared attr., thingToken=")
        .tagOff()
        .print(thing->thingToken)
        .print(", mid=")
        .hexShort(messageId, true, false)
        .print(", coapToken=")
        .hexString(thing->sharedAttrObserveToken, TP_COAP_TOKEN_LEN, true)
        .tagOn();
  #endif

    sprintf(uri, "%s/%s/attributes", apiPrefix, thing->thingToken);
    message.reset();
    message.setVersion(TP_COAP_VERSION);
    message.setType(CoapPDU::COAP_CONFIRMABLE);
    message.setCode(CoapPDU::COAP_GET);
    message.setMessageID(messageId);
    message.setToken(thing->sharedAttrObserveToken, TP_COAP_TOKEN_LEN);
    message.setURI(uri);
    message.addOption(CoapPDU::COAP_OPTION_OBSERVE, 1, obsOptionData);

    thing->lastSharedAttrObserveMillis = millis() + random(500, 5000);

    return netSendCoAPMessage(platformIPAddrStr, platformPort, localPort, &message);
}

// ----------------------------------------
//   RPC
// ----------------------------------------
bool tpSendOutgoingRpcRequest(thing_info_t *thing, const char *method, JsonObject *paramsObj) {
    char paramsJsonStr[TP_JSON_STRING_BUF_LEN];
    paramsObj->printTo(paramsJsonStr);
    
    return tpSendOutgoingRpcRequest(thing, method, paramsJsonStr);
}

bool tpSendOutgoingRpcRequest(thing_info_t *thing, const char *method, char *paramsJsonStr) {
    uint8_t coapBuf[TP_COAP_BUF_LEN];
    CoapPDU message(coapBuf, TP_COAP_BUF_LEN);

    uint16_t messageId = netGetNextCoAPMessageId();
    char uri[TP_COAP_URI_MAX_LEN];

    char jsonStr[TP_JSON_STRING_BUF_LEN];
    sprintf(jsonStr, "{\"method\":%s,\"params\":%s}", method, paramsJsonStr ? paramsJsonStr : "{}");
    
  #ifdef TP_DBG_OUTGOING_RPC
    dbg
        .print("Send outgoing RPC request, thingToken=")
        .tagOff()
        .print(thing->thingToken)
        .print(", mid=")
        .hexShort(messageId, true, false)
        .print(", coapToken=")
        .hexString(thing->outgoingRpcRequestToken, TP_COAP_TOKEN_LEN, true, false)
        .print(", request=")
        .println(jsonStr)
        .tagOn();
  #endif

    sprintf(uri, "%s/%s/rpc", apiPrefix, thing->thingToken);
    message.reset();
    message.setVersion(TP_COAP_VERSION);
    message.setType(CoapPDU::COAP_CONFIRMABLE);
    message.setCode(CoapPDU::COAP_POST);
    message.setMessageID(messageId);
    message.setToken(thing->outgoingRpcRequestToken, TP_COAP_TOKEN_LEN);
    message.setURI(uri);
    message.setPayload((uint8_t *)jsonStr, strlen(jsonStr));

    return netSendCoAPMessage(platformIPAddrStr, platformPort, localPort, &message);
}

bool tpSendIncomingRpcObserveRequest(thing_info_t *thing) {
    uint8_t coapBuf[TP_COAP_BUF_LEN];
    CoapPDU message(coapBuf, TP_COAP_BUF_LEN);

    uint16_t messageId = netGetNextCoAPMessageId();
    char uri[TP_COAP_URI_MAX_LEN];

    // register observation
    uint8_t obsOptionData[1] = {0};
    
  #ifdef TP_DBG_INCOMING_RPC
    dbg
        .print("Observe incoming RPC, thingToken=")
        .tagOff()
        .print(thing->thingToken)
        .print(", mid=")
        .hexShort(messageId, true, false)
        .print(", coapToken=")
        .hexString(thing->incomingRpcRequestObserveToken, TP_COAP_TOKEN_LEN, true)
        .tagOn();
  #endif

    sprintf(uri, "%s/%s/rpc", apiPrefix, thing->thingToken);
    message.reset();
    message.setVersion(TP_COAP_VERSION);
    message.setType(CoapPDU::COAP_CONFIRMABLE);
    message.setCode(CoapPDU::COAP_GET);
    message.setMessageID(messageId);
    message.setToken(thing->incomingRpcRequestObserveToken, TP_COAP_TOKEN_LEN);
    message.setURI(uri);
    message.addOption(CoapPDU::COAP_OPTION_OBSERVE, 1, obsOptionData);

    thing->lastIncomingRpcRequestObserveMillis = millis() + random(500, 5000);

    return netSendCoAPMessage(platformIPAddrStr, platformPort, localPort, &message);
}

bool tpSendIncomingRpcResponse(thing_info_t *thing, unsigned long rpcId, const char *method, JsonObject *rspObj) {
    char rspJsonStr[TP_JSON_STRING_BUF_LEN];
    rspObj->printTo(rspJsonStr);

    return tpSendIncomingRpcResponse(thing, rpcId, method, rspJsonStr);
}

bool tpSendIncomingRpcResponse(thing_info_t *thing, unsigned long rpcId, const char *method, const char *rspJsonStr) {
    uint8_t coapBuf[TP_COAP_BUF_LEN];
    CoapPDU message(coapBuf, TP_COAP_BUF_LEN);

    uint16_t messageId = netGetNextCoAPMessageId();
    char uri[TP_COAP_URI_MAX_LEN];

    // append method name and response object to corresponding keys
    char jsonStr[TP_JSON_STRING_BUF_LEN];
    // strcpy(jsonStr, rspJsonStr);
    sprintf(jsonStr, "{\"method\":%s,\"response\":%s}", method, rspJsonStr);
    
  #ifdef TP_DBG_INCOMING_RPC
    dbg
        .print("Send incoming RPC response, thingToken=")
        .tagOff()
        .print(thing->thingToken)
        .print(", mid=")
        .hexShort(messageId, true, false)
        .print(", coapToken=")
        .hexString(thing->incomingRpcResponseToken, TP_COAP_TOKEN_LEN, true, false)
        .print(", response=")
        .println(jsonStr)
        .tagOn();
  #endif

    sprintf(uri, "%s/%s/rpc/%lu", apiPrefix, thing->thingToken, rpcId);
    message.reset();
    message.setVersion(TP_COAP_VERSION);
    message.setType(CoapPDU::COAP_CONFIRMABLE);
    message.setCode(CoapPDU::COAP_POST);
    message.setMessageID(messageId);
    message.setToken(thing->incomingRpcResponseToken, TP_COAP_TOKEN_LEN);
    message.setURI(uri);
    message.setPayload((uint8_t *)jsonStr, strlen(jsonStr));

    return netSendCoAPMessage(platformIPAddrStr, platformPort, localPort, &message);
}

// ----------------------------------------
//   Platform Event Handler
// ----------------------------------------
void tpSetPlatformEventHandler(void (*handler)(uint8_t type, thing_info_t *thing, JsonObject *jsonObj)) {
    hPlatformEvent = handler;
}

// ----------------------------------------
//   Task processor
// ----------------------------------------
void _tpObservationTaskTick();
void _tpNetworkConnectivityTaskTick();

void tpTaskTick() {
    netTaskTick();
    _tpObservationTaskTick();
    _tpNetworkConnectivityTaskTick();
}

void _tpObservationTaskTick() {
    static int i = 0;

    thing_info_t *thing = &thingList[i];

    if (thing != NULL) {
        // send observe request for the first time in case of thing->lastSharedAttrObserveMillis or thing->lastIncomingRpcRequestObserveMillis is zero
        if (thing->sharedAttrObserveRenewInterval > 0 && 
            (labs(millis() - thing->lastSharedAttrObserveMillis) >= thing->sharedAttrObserveRenewInterval || thing->lastSharedAttrObserveMillis == 0))
        {
            tpSendSharedAttributesObserveRequest(thing);
        }
        else if (thing->incomingRpcRequestObserveRenewInterval > 0 && 
                (labs(millis() - thing->lastIncomingRpcRequestObserveMillis) >= thing->incomingRpcRequestObserveRenewInterval || thing->lastIncomingRpcRequestObserveMillis == 0))
        {
            tpSendIncomingRpcObserveRequest(thing);
        }
        else {
            i++;
            i = i % thingCount;
        }
    }
}

static const uint32_t NETCONN_TASK_INTERVALS[] = TP_NETWORK_CONNECTIVITY_CHECK_INTERVALS;
static const uint8_t NETCONN_TASK_MAX_FAILURE = sizeof(NETCONN_TASK_INTERVALS) / sizeof(uint32_t);
static uint8_t netConnTaskIntervalIdx = 0;
static unsigned long lastNetConnCheckingTaskMillis;

void _tpNetworkConnectivityTaskTick() {
    if (labs(millis() - lastNetConnCheckingTaskMillis) < NETCONN_TASK_INTERVALS[netConnTaskIntervalIdx]) {
        return;
    }

  #ifdef TP_DBG_CONNECTIVITY_CHECK
    dbg.println("Checking network connectivity...");
  #endif

    if (netSendCoAPPing(platformIPAddrStr, platformPort, TP_NETWORK_CONNECTIVITY_PING_TIMEOUT) != true) {
        // move to next interval
        netConnTaskIntervalIdx++;

      #ifdef TP_DBG_CONNECTIVITY_CHECK
        dbg
            .print("Network connectivity LOST")
            .tagOff()
            .print(", host=")
            .print(platformIPAddrStr)
            .print(", count=")
            .print(netConnTaskIntervalIdx)
            .print(", max=")
            .print(NETCONN_TASK_MAX_FAILURE)
            .println((netConnTaskIntervalIdx < NETCONN_TASK_MAX_FAILURE) ? "" : ", re-init the network")
            .tagOn();
      #endif
        
        if (netConnTaskIntervalIdx >= NETCONN_TASK_MAX_FAILURE) {
            // reset the counter
            netConnTaskIntervalIdx = 0;
            // try to re-init the network
            networkInitRetryCount = 0;

            while(netInitNetwork() != true) {
                if (++networkInitRetryCount >= TP_NETWORK_INIT_MAX_RETRY) {
                  #ifdef TP_DBG_NETWORK_INIT
                    dbg
                        .print("Too many network initialization failures (limit=")
                        .tagOff()
                        .print(TP_NETWORK_INIT_MAX_RETRY)
                        .println(")")
                        .tagOn();
                  #endif

                  #ifdef ESP32
                    // power cycle the board
                    ESP.restart();
                  #endif
                }

                delay(100);
            }
        }
    }
    else {
      #ifdef TP_DBG_CONNECTIVITY_CHECK
        dbg.println("Network connectivity OK");
      #endif
        
        // reset the counter
        netConnTaskIntervalIdx = 0;
    }

    lastNetConnCheckingTaskMillis = millis() + random(500, 5000);
}

// ----------------------------------------
//   Handlers
// ----------------------------------------
void hIncomingCoAPMessage(const char *srcAddrStr, uint16_t srcPort, uint16_t dstPort, CoapPDU *message) {
    uint8_t *tokenBuf = message->getTokenPointer();
    uint8_t *payloadBuf = message->getPayloadPointer();
    int tokenLen = message->getTokenLength();
    int payloadLen = message->getPayloadLength();
    thing_info_t *thing;
    
    StaticJsonBuffer<TP_STATIC_JSON_BUF_LEN> jsonBuffer;
    char jsonStr[TP_JSON_STRING_BUF_LEN] = {0};
    JsonObject *jsonObj = NULL;

    uint8_t tpEventType;

    (void) srcAddrStr;
    (void) srcPort;
    (void) dstPort;

    if (tokenLen == TP_COAP_TOKEN_LEN) {
        // JSON string in the payload should not longer than TP_JSON_STRING_MAX_LEN
        if (payloadLen > TP_JSON_STRING_MAX_LEN) {
            return;
        }

        // copy JSON string from CoAP payload to the buffer and try to parse it
        if (payloadLen == 0) {
            memcpy(jsonStr, "{}", 2);
        }
        else {
            memcpy(jsonStr, payloadBuf, payloadLen);
        }

        // try to parse it
        jsonObj = &jsonBuffer.parseObject(jsonStr);

        if (!jsonObj || !jsonObj->success()) {
            return;
        }

        for (int i = 0 ; i < thingCount ; i++) {
            thing = &thingList[i];
            tpEventType = TP_EVENT_UNDEFINED;

            if (memcmp(tokenBuf, thing->telemetrySendToken, TP_COAP_TOKEN_LEN) == 0) {
                tpEventType = TP_EVENT_TELEMETRY_SEND_RESPONSE;
            }
            else if (memcmp(tokenBuf, thing->clientAttrReadToken, TP_COAP_TOKEN_LEN) == 0) {
                tpEventType = TP_EVENT_CLIENT_ATTR_READ_RESPONSE;
            }
            else if (memcmp(tokenBuf, thing->clientAttrWriteToken, TP_COAP_TOKEN_LEN) == 0) {
                tpEventType = TP_EVENT_CLIENT_ATTR_WRITE_RESPONSE;
            }
            else if (memcmp(tokenBuf, thing->sharedAttrReadToken, TP_COAP_TOKEN_LEN) == 0) {
                tpEventType = TP_EVENT_SHARED_ATTR_READ_RESPONSE;
            }
            else if (memcmp(tokenBuf, thing->sharedAttrObserveToken, TP_COAP_TOKEN_LEN) == 0) {
                tpEventType = TP_EVENT_SHARED_ATTR_NOTIFY;
            }
            else if (memcmp(tokenBuf, thing->incomingRpcRequestObserveToken, TP_COAP_TOKEN_LEN) == 0) {
                tpEventType = TP_EVENT_INCOMING_RPC_REQUEST;
            }
            else if (memcmp(tokenBuf, thing->outgoingRpcRequestToken, TP_COAP_TOKEN_LEN) == 0) {
                tpEventType = TP_EVENT_OUTGOING_RPC_RESPONSE;
            }

            if (tpEventType != TP_EVENT_UNDEFINED && hPlatformEvent != NULL) {
              #ifdef TP_DBG_PLATFORM_EVENT
                dbg
                    .print("Platform event")
                    .tagOff()
                    .print(", type=")
                    .print(tpEventType)
                    .print(", thing=")
                    .print(thing->name).print(" (").print(thing->id).print(")");
    
                if (payloadLen > 0) {
                    dbg
                        .print(", json=")
                        .write(payloadBuf, payloadLen);
                }
                
                dbg.println().tagOn();
              #endif
                
                hPlatformEvent(tpEventType, thing, jsonObj);
            }
        }
    }
    else {
        // TODO process another type of CoAP message
    }
}
