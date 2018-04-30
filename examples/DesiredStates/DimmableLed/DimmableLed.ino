#include <ConnexThings_BC95.h>

#define THING_TOKEN  "xxxxxxxxxxxxxxxxxxxx"

#define LED_PIN  2

// ----------------------------------------
//   DimmableLed Class
// ----------------------------------------
class DimmableLed {
    private:
        int ledPin;
        uint8_t brightnessPercent;
        uint8_t brightnessDuty;
    
    public:
        void begin(int pin);
        uint8_t getBrightness();
        void setBrightness(uint8_t percent);
};

void DimmableLed::begin(int pin) {
    ledPin = pin;
    brightnessPercent = 0;
    brightnessDuty = 0;

    analogWrite(ledPin, 0);
}

uint8_t DimmableLed::getBrightness() {
    return brightnessPercent;
}

void DimmableLed::setBrightness(uint8_t percent) {
    brightnessPercent = (percent > 100) ? 100 : percent;
    brightnessDuty = map(brightnessPercent, 0, 100, 0, 255);

    analogWrite(ledPin, brightnessDuty);

    Serial.print("LED [");
    Serial.print(ledPin);
    Serial.print("] brightness is ");
    Serial.print(brightnessPercent);
    Serial.println("%");
}

static DimmableLed Led;

// ----------------------------------------
//   LED Task
// ----------------------------------------
void execLedTask() {
    static uint8_t lastReportedBrightness;
    static unsigned long lastReportedMillis;
    
    uint8_t currentBrightness = Led.getBrightness();

    if (currentBrightness != lastReportedBrightness
        || millis() - lastReportedMillis >= 30000)
    {
        Thing.reportState("brightness", currentBrightness);
        
        lastReportedBrightness = currentBrightness;
        lastReportedMillis = millis();
    }
}

// ----------------------------------------
//   Event Hooks
// ----------------------------------------
void thingDesiredStatesChanged(JsonObject &states) {
    uint8_t brightnessPercent;

    if (states.containsKey("brightness")) {
        brightnessPercent = states["brightness"];
        
        Led.setBrightness(brightnessPercent);
    }
}

// ----------------------------------------
//   Main
// ----------------------------------------
void setup() {
    Serial.begin(115200);
    Led.begin(LED_PIN);
    Thing.begin(THING_TOKEN);
}

void loop() {
    execLedTask();
    Thing.execTask();
}


