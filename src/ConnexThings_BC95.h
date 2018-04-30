/**
 * Platform-Device integration layer.
 * 
 * Copyright (c) 2018 Sparkbit Co., Ltd. All rights reserved.
 * 
 * This work is licensed under the terms of the MIT license.  
 * See LICENSE file in the project root for details.
 */

#ifndef CONNEXTHINGS_BC95_H
#define CONNEXTHINGS_BC95_H

#include <Arduino.h>
#include "bc95/quectel_bc95.h"
#include "bc95/network.h"
#include "bc95/things.h"

// ----------------------------------------
//   Event Hooks
// ----------------------------------------
class CommandResponse;
extern void thingDesiredStatesReadResponse(JsonObject &states) __attribute__((weak));
extern void thingDesiredStatesChanged(JsonObject &states) __attribute__((weak));
extern void thingCommandReceived(const String &command, JsonVariant &params, CommandResponse &res) __attribute__((weak));

class CommandResponse {
    public:
        CommandResponse(unsigned long commandId, const char *commandName);

        bool wasCalled();
        CommandResponse& status(uint16_t code);

        template <typename T> void send(T* body) {
            _sendResponse<T*>(body);
        }

        template <typename T> void send(const T& body) {
            _sendResponse<const T&>(body);
        }

    private:
        bool _called;
        uint16_t _statusCode;
        unsigned long _cmdId;
        const char *_cmdName;

        template <typename T> void _sendResponse(T body);
};

class ThingClass {
    public:
        void begin(const String &thingToken);

        bool readDesiredStates();
        bool readDesiredStates(const String &states);
        
        bool reportState(const String &key, bool val);
        bool reportState(const String &key, float val);
        bool reportState(const String &key, double val);
        bool reportState(const String &key, signed char val);
        bool reportState(const String &key, signed short val);
        bool reportState(const String &key, signed int val);
        bool reportState(const String &key, signed long val);
        bool reportState(const String &key, signed long long val);
        bool reportState(const String &key, unsigned char val);
        bool reportState(const String &key, unsigned short val);
        bool reportState(const String &key, unsigned int val);
        bool reportState(const String &key, unsigned long val);
        bool reportState(const String &key, unsigned long long val);
        bool reportState(const String &key, const char *val);
        bool reportState(const String &key, const String &val);

        bool reportStates(JsonObject &states);
        bool reportStates(const String &json);
        bool reportStates(const char *json);

        // continuously call this method in loop()
        void execTask();
};

extern ThingClass Thing;

#endif  /* CONNEXTHINGS_BC95_H */