#include <ConnexThings_BC95.h>

#define THING_TOKEN  "xxxxxxxxxxxxxxxxxxxx"

// ----------------------------------------
//   Light Sensor Task
// ----------------------------------------
#define LIGHT_SENSOR_PIN              A0
#define LIGHT_SENSOR_REPORT_INTERVAL  5000

static void execLightSensorTask() {
    static unsigned long lastReportedMillis;
    
    uint16_t sensorValue;

    if (millis() - lastReportedMillis < LIGHT_SENSOR_REPORT_INTERVAL) {
        return;
    }

    sensorValue = analogRead(LIGHT_SENSOR_PIN);

    Serial.print("Light sensor value: ");
    Serial.println(sensorValue);

    Thing.reportState("light", sensorValue);
    lastReportedMillis = millis();
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