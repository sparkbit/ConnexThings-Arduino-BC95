#include <ConnexThings_BC95.h>
#include <math.h>

#define THING_TOKEN  "xxxxxxxxxxxxxxxxxxxx"

// ----------------------------------------
//   Event Hooks
// ----------------------------------------
void thingCommandReceived(const String &command, JsonVariant &params, CommandResponse &res) {
    String asString = params;
    float asFloat   = params;
    double asDouble = params;
    bool asBoolean  = params;
    signed asInt    = params;
    unsigned asUInt = params;
    
    Serial.println("--------------------------------------------------------------------------------");

    Serial.print("Command: ");
    Serial.println(command);

    Serial.print(" Params: ");
    Serial.print("string=");
    Serial.print(asString);
    Serial.print(", signed=");
    Serial.print(asInt);
    Serial.print(", unsigned=");
    Serial.print(asUInt);
    Serial.print(", float=");
    Serial.print(asFloat);
    Serial.print(", double=");
    Serial.print(asDouble);
    Serial.print(", bool=");
    Serial.print(asBoolean ? "true" : "false");
    Serial.println();

    // Use ArduinoJson Assistant (https://arduinojson.org/assistant/)
    // to calculate an appropriate size for StaticJsonBuffer.

    if (command == "getString") {
        res.send("The quick brown fox jumps over the lazy dog.");
    }
    else if (command == "getInteger") {
        int intRes = 42;
        res.send(intRes);
    }
    else if (command == "getFloat") {
        float floatRes = 3.1415928;
        res.send(floatRes);
    }
    else if (command == "getBoolean") {
        bool boolRes = random(0, 10) > 5;
        res.send(boolRes);
    }
    else if (command == "getObject") {
        StaticJsonBuffer<JSON_OBJECT_SIZE(3)> objBuf;
        JsonObject &objRes = objBuf.createObject();

        objRes["foo"] = "bar";
        objRes["answer"] = 42;
        objRes["enabled"] = (bool)true;
        
        res.send(objRes);
    }
    else if (command == "getArray") {
        StaticJsonBuffer<JSON_ARRAY_SIZE(3)> arrBuf;
        JsonArray &arrRes = arrBuf.createArray();

        arrRes.add("qux");
        arrRes.add(42);
        arrRes.add((bool)false);

        res.send(arrRes);
    }
    else if (command == "getCircleArea") {
        float radius = params;
        float result;

        if (radius > 0) {
            result = M_PI * radius * radius;
            res.send(result);
        }
        else {
            res.status(400).send("Radius should be positive");
        }
    }
}

// ----------------------------------------
//   Main
// ----------------------------------------
void setup() {
    Serial.begin(115200);
    Thing.begin(THING_TOKEN);
}

void loop() {
    Thing.execTask();
}
