#include <Arduino.h>
#include <ModbusRTU.h>
#include "sensorBuilder.hpp"

ModbusRTU mb;
SensorBuilderClass SensorBuilder;
sensorLight *light = nullptr;

// RS485 pins
#define RS485_TX 0
#define RS485_RX 1
#define RE_DE_PIN 2   // DE + RE connected here

void setup() {
  Serial.begin(115200);
  Serial.println("=== S2110 Modbus Slave Starting ===");

  // Sensor init
  SensorBuilder.check_grove();

  light = new sensorLight();
  SensorBuilder.addSensor(light);
  SensorBuilder.begin();

  // Serial1 for RS485
  Serial1.setTX(RS485_TX);
  Serial1.setRX(RS485_RX);
  Serial1.begin(9600, SERIAL_8N1);

  // 🔥 THIS handles RS485 direction automatically
  mb.begin(&Serial1, RE_DE_PIN);

  mb.slave(1);

  // 🔥 Create holding register
  mb.addHreg(10);

  Serial.println("Slave Ready!");
  Serial.println("Sending Light Sensor on Register 10 (0x000A)");
}

void loop() {
  mb.task();              // MUST be first
  SensorBuilder.poll();   // MUST be second

  if (light != nullptr) {
    auto m = light->getMeasureValue();

    if (!m.empty()) {
      uint16_t val = m[0].value.u16;

      // 🔥 Update Modbus register
      mb.Hreg(10, val);

      Serial.print("Light RAW: ");
      Serial.println(val);
    }
  }

  delay(100);
}
