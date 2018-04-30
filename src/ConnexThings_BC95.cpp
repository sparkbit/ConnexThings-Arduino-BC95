/**
 * Platform-Device integration layer.
 * 
 * Copyright (c) 2018 Sparkbit Co., Ltd. All rights reserved.
 * 
 * This work is licensed under the terms of the MIT license.  
 * See LICENSE file in the project root for details.
 */

#include "ConnexThings_BC95.h"

// ----------------------------------------
//   Things Definition
// ----------------------------------------
#define TP_SHARED_ATTR_OBSERVE_RENEW_INTERVAL       15000
#define TP_INCOMING_RPC_REQ_OBSERVE_RENEW_INTERVAL  15000

#define TP_THING_ID    "8d252e29-efce-40c6-809e-5d3b6666c1b6"
#define TP_THING_NAME  "Demo Thing"

static char thingToken[100];

static thing_info_t thingList[] = {
    {
        TP_THING_ID,
        TP_THING_NAME,
        thingToken,
        {0x0B, 0x5E, 0x2F, 0x01},
        {0x0B, 0x5E, 0x2F, 0x02},
        {0x0B, 0x5E, 0x2F, 0x03},
        {0x0B, 0x5E, 0x2F, 0x04},
        {0x0B, 0x5E, 0x2F, 0x05},
        {0x0B, 0x5E, 0x2F, 0x06},
        {0x0B, 0x5E, 0x2F, 0x07},
        {0x0B, 0x5E, 0x2F, 0x08},
        TP_SHARED_ATTR_OBSERVE_RENEW_INTERVAL,
        TP_INCOMING_RPC_REQ_OBSERVE_RENEW_INTERVAL,
        0,
        0
    }
};

static const uint8_t thingCount = sizeof(thingList) / sizeof(thing_info_t);

// ----------------------------------------
//   Random Seed Init
//   Enable TRNG on supported hardware
// ----------------------------------------
static void randomSeedInit() {
  #if defined(__SAM3X8E__)
    pmc_enable_periph_clk(ID_TRNG);
    trng_enable(TRNG);
    delay(10);
    randomSeed(trng_read_output_data(TRNG));
  #else
    randomSeed(analogRead(0));
  #endif
}

// ----------------------------------------
//   Platform Event Handlers
// ----------------------------------------
static void processSharedAttributesReadResponse(thing_info_t *thing, JsonObject *attr) {
    (void)thing;

    if (thingDesiredStatesReadResponse) {
        thingDesiredStatesReadResponse(*attr);
    }
}

static void processSharedAttributesNotification(thing_info_t *thing, JsonObject *attr) {
    (void)thing;

    if (thingDesiredStatesChanged) {
        thingDesiredStatesChanged(*attr);
    }
}

static void processOutgoingRpcResponse(thing_info_t *thing, JsonObject *rpc) {
    const char *rpcMethod = (*rpc)["method"];

    (void)thing;

    if(rpcMethod == NULL){
        // invalid RPC response
        return;
    }
}

static void processIncomingRpc(thing_info_t *thing, JsonObject *rpc) {
    unsigned long rpcId   = rpc->get<unsigned long>("id");
    const char *rpcMethod = rpc->get<const char *>("method");
    JsonVariant rpcParams = (*rpc)["params"];

    (void)thing;

    if(rpcMethod == NULL){
        // invalid RPC
        return;
    }

    CommandResponse res(rpcId, rpcMethod);

    if (strcmp(rpcMethod, "ping") == 0) {
        res.status(200).send("pong");
    }
    else {
        if (thingCommandReceived) {
            thingCommandReceived(String(rpcMethod), rpcParams, res);
        }

        if (res.wasCalled() != true) {
            res.status(400).send("unsupported command");
        }
    }
}

static void processPlatformEvent(uint8_t type, thing_info_t *thing, JsonObject *jsonObj) {
    switch (type) {
        case TP_EVENT_TELEMETRY_SEND_RESPONSE:
            break;

        case TP_EVENT_CLIENT_ATTR_READ_RESPONSE:
            break;

        case TP_EVENT_CLIENT_ATTR_WRITE_RESPONSE:
            break;

        case TP_EVENT_SHARED_ATTR_READ_RESPONSE:
            processSharedAttributesReadResponse(thing, jsonObj);
            break;

        case TP_EVENT_SHARED_ATTR_NOTIFY:
            processSharedAttributesNotification(thing, jsonObj);
            break;

        case TP_EVENT_OUTGOING_RPC_RESPONSE:
            processOutgoingRpcResponse(thing, jsonObj);
            break;

        case TP_EVENT_INCOMING_RPC_REQUEST:
            processIncomingRpc(thing, jsonObj);
            break;
    }
}

// ----------------------------------------
//   CommandResponse
// ----------------------------------------
CommandResponse::CommandResponse(unsigned long commandId, const char *commandName) {
    _called = false;
    _statusCode = 200;
    _cmdId = commandId;
    _cmdName = commandName;
}

bool CommandResponse::wasCalled() {
    return _called;
}

CommandResponse& CommandResponse::status(uint16_t code) {
    _statusCode = code;
    return *this;
}

template <typename T> void CommandResponse::_sendResponse(T body) {
    StaticJsonBuffer<TP_STATIC_JSON_BUF_LEN> jsonBuffer;
    JsonObject& rspObj = jsonBuffer.createObject();

    _called = true;

    rspObj["status"] = _statusCode;
    rspObj.set("body", body);

    tpSendIncomingRpcResponse(tpGetThingInfoById(TP_THING_ID), _cmdId, _cmdName, &rspObj);
}

template void CommandResponse::_sendResponse(const bool&);
template void CommandResponse::_sendResponse(const float&);
template void CommandResponse::_sendResponse(const double&);
template void CommandResponse::_sendResponse(const signed char&);
template void CommandResponse::_sendResponse(const signed long&);
template void CommandResponse::_sendResponse(const signed int&);
template void CommandResponse::_sendResponse(const signed short&);
template void CommandResponse::_sendResponse(const unsigned char&);
template void CommandResponse::_sendResponse(const unsigned long&);
template void CommandResponse::_sendResponse(const unsigned int&);
template void CommandResponse::_sendResponse(const unsigned short&);
template void CommandResponse::_sendResponse(const char *);
template void CommandResponse::_sendResponse(char *);
template void CommandResponse::_sendResponse(const String&);
template void CommandResponse::_sendResponse(const __FlashStringHelper*);
template void CommandResponse::_sendResponse(const JsonArray&);
template void CommandResponse::_sendResponse(const JsonObject&);
template void CommandResponse::_sendResponse(const JsonVariant&);

// ----------------------------------------
//   ThingClass
// ----------------------------------------
void ThingClass::begin(const String &_thingToken) {
    strcpy(thingToken, _thingToken.c_str());

    randomSeedInit();
    
    tpSetPlatformEventHandler(processPlatformEvent);
    tpInit(thingList, thingCount);
}

bool ThingClass::readDesiredStates() {
    return tpSendSharedAttributesReadRequest(tpGetThingInfoById(TP_THING_ID), NULL);
}

bool ThingClass::readDesiredStates(const String &states) {
    return tpSendSharedAttributesReadRequest(tpGetThingInfoById(TP_THING_ID), states.c_str());
}



bool ThingClass::reportState(const String &key, bool val) {
    char report[TP_JSON_STRING_BUF_LEN];

    sprintf(report, "{\"%s\":%s}", key.c_str(), val ? "true" : "false");
    return tpSendClientAttributesWriteRequest(tpGetThingInfoById(TP_THING_ID), report);
}

bool ThingClass::reportState(const String &key, float val) {
    char report[TP_JSON_STRING_BUF_LEN];

    sprintf(report, "{\"%s\":%f}", key.c_str(), val);
    return tpSendClientAttributesWriteRequest(tpGetThingInfoById(TP_THING_ID), report);
}

bool ThingClass::reportState(const String &key, double val) {
    char report[TP_JSON_STRING_BUF_LEN];

    sprintf(report, "{\"%s\":%f}", key.c_str(), val);
    return tpSendClientAttributesWriteRequest(tpGetThingInfoById(TP_THING_ID), report);
}

bool ThingClass::reportState(const String &key, signed char val) {
    char report[TP_JSON_STRING_BUF_LEN];

    sprintf(report, "{\"%s\":%hhd}", key.c_str(), val);
    return tpSendClientAttributesWriteRequest(tpGetThingInfoById(TP_THING_ID), report);
}

bool ThingClass::reportState(const String &key, signed short val) {
    char report[TP_JSON_STRING_BUF_LEN];

    sprintf(report, "{\"%s\":%hd}", key.c_str(), val);
    return tpSendClientAttributesWriteRequest(tpGetThingInfoById(TP_THING_ID), report);
}

bool ThingClass::reportState(const String &key, signed int val) {
    char report[TP_JSON_STRING_BUF_LEN];

    sprintf(report, "{\"%s\":%d}", key.c_str(), val);
    return tpSendClientAttributesWriteRequest(tpGetThingInfoById(TP_THING_ID), report);
}

bool ThingClass::reportState(const String &key, signed long val) {
    char report[TP_JSON_STRING_BUF_LEN];

    sprintf(report, "{\"%s\":%ld}", key.c_str(), val);
    return tpSendClientAttributesWriteRequest(tpGetThingInfoById(TP_THING_ID), report);
}

bool ThingClass::reportState(const String &key, signed long long val) {
    char report[TP_JSON_STRING_BUF_LEN];

    sprintf(report, "{\"%s\":%lld}", key.c_str(), val);
    return tpSendClientAttributesWriteRequest(tpGetThingInfoById(TP_THING_ID), report);
}

bool ThingClass::reportState(const String &key, unsigned char val) {
    char report[TP_JSON_STRING_BUF_LEN];

    sprintf(report, "{\"%s\":%hhu}", key.c_str(), val);
    return tpSendClientAttributesWriteRequest(tpGetThingInfoById(TP_THING_ID), report);
}

bool ThingClass::reportState(const String &key, unsigned short val) {
    char report[TP_JSON_STRING_BUF_LEN];

    sprintf(report, "{\"%s\":%hu}", key.c_str(), val);
    return tpSendClientAttributesWriteRequest(tpGetThingInfoById(TP_THING_ID), report);
}

bool ThingClass::reportState(const String &key, unsigned int val) {
    char report[TP_JSON_STRING_BUF_LEN];

    sprintf(report, "{\"%s\":%u}", key.c_str(), val);
    return tpSendClientAttributesWriteRequest(tpGetThingInfoById(TP_THING_ID), report);
}

bool ThingClass::reportState(const String &key, unsigned long val) {
    char report[TP_JSON_STRING_BUF_LEN];

    sprintf(report, "{\"%s\":%lu}", key.c_str(), val);
    return tpSendClientAttributesWriteRequest(tpGetThingInfoById(TP_THING_ID), report);
}

bool ThingClass::reportState(const String &key, unsigned long long val) {
    char report[TP_JSON_STRING_BUF_LEN];

    sprintf(report, "{\"%s\":%llu}", key.c_str(), val);
    return tpSendClientAttributesWriteRequest(tpGetThingInfoById(TP_THING_ID), report);
}

bool ThingClass::reportState(const String &key, const char *val){
    char report[TP_JSON_STRING_BUF_LEN];

    sprintf(report, "{\"%s\":\"%s\"}", key.c_str(), val);
    return tpSendClientAttributesWriteRequest(tpGetThingInfoById(TP_THING_ID), report);
}

bool ThingClass::reportState(const String &key, const String &val){
    char report[TP_JSON_STRING_BUF_LEN];

    sprintf(report, "{\"%s\":\"%s\"}", key.c_str(), val.c_str());
    return tpSendClientAttributesWriteRequest(tpGetThingInfoById(TP_THING_ID), report);
}

bool ThingClass::reportStates(JsonObject &states) {
    return tpSendClientAttributesWriteRequest(tpGetThingInfoById(TP_THING_ID), &states);
}

bool ThingClass::reportStates(const String &json) {
    return tpSendClientAttributesWriteRequest(tpGetThingInfoById(TP_THING_ID), json.c_str());
}

bool ThingClass::reportStates(const char *json) {
    return tpSendClientAttributesWriteRequest(tpGetThingInfoById(TP_THING_ID), json);
}

void ThingClass::execTask() {
    tpTaskTick();
}

ThingClass Thing;