#include <ConnexThings_BC95.h>

#define THING_TOKEN  "xxxxxxxxxxxxxxxxxxxx"

#define LED_PIN  2

#define BUTTON_PIN           3
#define BUTTON_ACTIVE_LEVEL  HIGH
#define BUTTON_DEBOUNCE_INT  200

// ----------------------------------------
//   SimpleLed Class
// ----------------------------------------
class SimpleLed {
    private:
        int ledPin;
    
    public:
        void begin(int pin);
        bool isOn();
        void turnOn();
        void turnOff();
        void toggle();
        void execTask();
};

void SimpleLed::begin(int pin) {
    ledPin = pin;

    pinMode(ledPin, OUTPUT);
    digitalWrite(ledPin, LOW);
}

bool SimpleLed::isOn() {
    return digitalRead(ledPin) == HIGH;
}

void SimpleLed::turnOn() {
    digitalWrite(ledPin, HIGH);

    Serial.print("LED [");
    Serial.print(ledPin);
    Serial.println("] is ON");
}

void SimpleLed::turnOff() {
    digitalWrite(ledPin, LOW);

    Serial.print("LED [");
    Serial.print(ledPin);
    Serial.println("] is OFF");
}

void SimpleLed::toggle() {
    isOn() ? turnOff() : turnOn();
}

SimpleLed Led;

// ----------------------------------------
//   Button Task
// ----------------------------------------
void execButtonTask() {
    static int lastState;
    static unsigned long lastTaskMillis;
    
    int currentState;

    if (millis() - lastTaskMillis < BUTTON_DEBOUNCE_INT) {
        return;
    }

    currentState = digitalRead(BUTTON_PIN);

    if (currentState != lastState) {
        if (currentState == BUTTON_ACTIVE_LEVEL) {
            Led.toggle();
        }
    }

    lastState = currentState;
    lastTaskMillis = millis();
}

// ----------------------------------------
//   LED Task
// ----------------------------------------
void execLedTask() {
    static bool lastReportedOnOff;
    static unsigned long lastReportedMillis;
    
    bool currentOnOff = Led.isOn();

    if (currentOnOff != lastReportedOnOff
        || millis() - lastReportedMillis >= 30000)
    {
        Thing.reportState("onOff", currentOnOff);
        
        lastReportedOnOff = currentOnOff;
        lastReportedMillis = millis();
    }
}

// ----------------------------------------
//   Event Hooks
// ----------------------------------------
void thingCommandReceived(const String &command, JsonVariant &params, CommandResponse &res) {
    if (command ==  "isOn") {
        bool isLedOn = Led.isOn();
        res.send(isLedOn);
    }
    else if (command == "on") {
        Led.turnOn();
        res.send("OK");
    }
    else if (command == "off") {
        Led.turnOff();
        res.send("OK");
    }
    else if (command == "toggle") {
        Led.toggle();
        res.send("OK");
    }
}

// ----------------------------------------
//   Main
// ----------------------------------------
void setup() {
    Serial.begin(115200);
    pinMode(BUTTON_PIN, INPUT);
    Led.begin(LED_PIN);
    Thing.begin(THING_TOKEN);
}

void loop() {
    execButtonTask();
    execLedTask();
    Thing.execTask();
}