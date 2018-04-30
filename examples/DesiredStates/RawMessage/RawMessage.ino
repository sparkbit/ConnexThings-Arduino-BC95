#include <ConnexThings_BC95.h>

#define THING_TOKEN  "xxxxxxxxxxxxxxxxxxxx"

// ----------------------------------------
//   Event Hooks
// ----------------------------------------
void thingDesiredStatesReadResponse(JsonObject &states) {
    char jsonStr[200] = {0};

    states.printTo(jsonStr);

    Serial.print("Desired States [Read]: ");
    Serial.println(jsonStr);
}

void thingDesiredStatesChanged(JsonObject &states) {
    char jsonStr[200] = {0};

    states.printTo(jsonStr);

    Serial.print("Desired States [Changed]: ");
    Serial.println(jsonStr);
}

// ----------------------------------------
//   Main
// ----------------------------------------
void setup() {
    Serial.begin(115200);
    Thing.begin(THING_TOKEN);
    Thing.readDesiredStates();
}

void loop() {
    Thing.execTask();
}
