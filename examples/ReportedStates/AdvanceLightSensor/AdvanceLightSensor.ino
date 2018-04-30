#include <ConnexThings_BC95.h>

#define THING_TOKEN  "xxxxxxxxxxxxxxxxxxxx"

// ----------------------------------------
//   Light Sensor Task
// ----------------------------------------
#define LIGHT_SENSOR_PIN                  A0
#define LIGHT_SENSOR_MIN_REPORT_INTERVAL  2000
#define LIGHT_SENSOR_MAX_REPORT_INTERVAL  30000
#define LIGHT_SENSOR_REPORTABLE_CHANGE    200

static void execLightSensorTask() {
    static unsigned long lastSensorReadMillis;
    static unsigned long lastReportedMillis;
    static uint16_t lastReportedValue;

    uint16_t sensorValue;

    if (millis() - lastSensorReadMillis < LIGHT_SENSOR_MIN_REPORT_INTERVAL) {
        return;
    }

    sensorValue = analogRead(LIGHT_SENSOR_PIN);
    lastSensorReadMillis = millis();

    // check whether more than LIGHT_SENSOR_MAX_REPORT_INTERVAL ms has passed
    // or sensor value has significantly changed since the last report
    if (millis() - lastReportedMillis >= LIGHT_SENSOR_MAX_REPORT_INTERVAL 
        || abs(sensorValue - lastReportedValue) >= LIGHT_SENSOR_REPORTABLE_CHANGE)
    {
        Serial.print("Light sensor value: ");
        Serial.print(sensorValue);
        Serial.println(" (Report)");

        Thing.reportState("light", sensorValue);
        lastReportedMillis = millis();
        lastReportedValue = sensorValue;
    }
    else {
        Serial.print("Light sensor value: ");
        Serial.println(sensorValue);
    }
}

// ----------------------------------------
//   Main
// ----------------------------------------
void setup() {
    Serial.begin(115200);
    pinMode(LIGHT_SENSOR_PIN, INPUT);
    Thing.begin(THING_TOKEN);
}

void loop() {
    execLightSensorTask();
    Thing.execTask();
}