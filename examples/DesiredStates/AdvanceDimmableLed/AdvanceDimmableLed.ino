#include <ConnexThings_BC95.h>

#define THING_TOKEN  "xxxxxxxxxxxxxxxxxxxx"

#define LED_PIN  2

#define BUTTON_PIN           3
#define BUTTON_ACTIVE_LEVEL  HIGH
#define BUTTON_DEBOUNCE_INT  200

// ----------------------------------------
//   ThingConfig Class (EEPROM Storage)
// ----------------------------------------
#if defined(__SAM3X8E__)
#include <DueFlashStorage.h>
DueFlashStorage EEPROM;
#else
#include <EEPROM.h>
#endif

#define DESIRED_STATE_VERSION_LEN  4

class ThingConfig {
    private:
        struct ConfigFile {
            uint8_t brightness;
            char brightness_v[DESIRED_STATE_VERSION_LEN+1];
        } cfg;

        bool dirty;
    
    public:
        void begin();
        void load();
        bool save();

        uint8_t getBrightness();
        void setBrightness(uint8_t percent);
        const char *getBrightnessVersion();
        void setBrightnessVersion(const char *version);
        bool isBrightnessVersionEqual(const char *version);
};

void ThingConfig::begin() {
  #if defined(__SAM3X8E__)
    if (EEPROM.read(0) != 0xEE) {
        memset(&cfg, 0, sizeof(ConfigFile));
        EEPROM.write(0, 0xEE);
        EEPROM.write(4, (byte *)&cfg, sizeof(ConfigFile));
    }
  #else
    if (EEPROM.read(0) != 0xEE) {
        memset(&cfg, 0, sizeof(ConfigFile));
        EEPROM.write(0, 0xEE);
        EEPROM.put(4, cfg);
    }
  #endif

    load();
}

void ThingConfig::load() {
  #if defined(__SAM3X8E__)
    memcpy(&cfg, EEPROM.readAddress(4), sizeof(ConfigFile));
  #else
    EEPROM.get(4, cfg);
  #endif

    dirty = false;
}

bool ThingConfig::save() {
    if (dirty) {
      #if defined(__SAM3X8E__)
        EEPROM.write(4, (byte *)&cfg, sizeof(ThingConfig));
      #else
        EEPROM.put(4, cfg);
      #endif

        dirty = false;
    }

    return !dirty;
}

uint8_t ThingConfig::getBrightness() {
    return cfg.brightness;
}

void ThingConfig::setBrightness(uint8_t percent) {
    cfg.brightness = percent;
    dirty = true;   
}

const char * ThingConfig::getBrightnessVersion() {
    return cfg.brightness_v;
}

void ThingConfig::setBrightnessVersion(const char *version) {
    if (version != NULL) {
        memcpy(cfg.brightness_v, version, DESIRED_STATE_VERSION_LEN);
        cfg.brightness_v[DESIRED_STATE_VERSION_LEN] = '\0';
        dirty = true;
    }
}

bool ThingConfig::isBrightnessVersionEqual(const char *version) {
    return memcmp(cfg.brightness_v, version, DESIRED_STATE_VERSION_LEN) == 0;
}

static ThingConfig Config;

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
        void decreaseBrightness();
        void increaseBrightness();
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

void DimmableLed::decreaseBrightness() {
    int targetBrightness = brightnessPercent - ((brightnessPercent % 20) > 0 ? (brightnessPercent % 20) : 20);
    
    targetBrightness = (targetBrightness < 0) ? 0 : targetBrightness;
    setBrightness(targetBrightness);
}

void DimmableLed::increaseBrightness() {
    int targetBrightness = (brightnessPercent + 20) - (brightnessPercent % 20);

    targetBrightness = (targetBrightness > 100) ? 100 : targetBrightness;
    setBrightness(targetBrightness);
}

static DimmableLed Led;

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
            if (Led.getBrightness() == 100) {
                Led.setBrightness(0);
            }
            else {
                Led.increaseBrightness();
            }

            Config.setBrightness(Led.getBrightness());
        }
    }

    lastState = currentState;
    lastTaskMillis = millis();
}

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
void processThingStates(JsonObject &states) {
    uint8_t brightnessPercent;
    const char *brightnessVersion;

    if (states.containsKey("brightness")) {
        brightnessPercent = states["brightness"];
        brightnessVersion = states["brightness$"];

        if (Config.isBrightnessVersionEqual(brightnessVersion)) {
            Serial.print("Duplicate version of brightness [");
            Serial.print(Config.getBrightnessVersion());
            Serial.println("] was received, ignored");

            return;
        }

        Led.setBrightness(brightnessPercent);

        Config.setBrightness(Led.getBrightness());
        Config.setBrightnessVersion(brightnessVersion);
    }
}

void thingDesiredStatesReadResponse(JsonObject &states) {
    processThingStates(states);
}

void thingDesiredStatesChanged(JsonObject &states) {
    processThingStates(states);
}

void thingCommandReceived(const String &command, JsonVariant &params, CommandResponse &res) {
    if (command == "down") {
        Led.decreaseBrightness();
        Config.setBrightness(Led.getBrightness());
        res.send(Led.getBrightness());
    }
    else if (command == "up") {
        Led.increaseBrightness();
        Config.setBrightness(Led.getBrightness());
        res.send(Led.getBrightness());
    }
}

// ----------------------------------------
//   Main
// ----------------------------------------
void setup() {
    Config.begin();
    Serial.begin(115200);
    pinMode(BUTTON_PIN, INPUT);
    
    Serial.println("Setting initial LED brightness...");
    Led.begin(LED_PIN);
    Led.setBrightness(Config.getBrightness());

    Thing.begin(THING_TOKEN);
    Thing.readDesiredStates();
}

void loop() {
    execButtonTask();
    execLedTask();
    Config.save();
    Thing.execTask();
}
