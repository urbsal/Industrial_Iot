#ifndef _SENSOR_BUILDER_CLASS_H
#define _SENSOR_BUILDER_CLASS_H

#include <Arduino.h>
#include <ModbusRTU.h>
#include "sensorClass.hpp"
#include "sensorLight.hpp" 
#include <map>

// Master Modbus object defined in main .ino
extern ModbusRTU mb;

#define SENSOR_BUILDER_DEF_BAUD 9600
#define SENSOR_BUILDER_DEF_SLAVE 1

class SensorBuilderClass
{
private:
    uint8_t _slave;
    uint32_t _baudrate;
    uint16_t _regs;
    bool _i2c_available = false;
    std::map<uint16_t, sensorClass *> m_sensorMap;

public:
    SensorBuilderClass() : _regs(4){};
    ~SensorBuilderClass(){};
    
    uint16_t addSensor(sensorClass *sensor);
    void check_grove(void);
    bool begin(uint8_t slave = SENSOR_BUILDER_DEF_SLAVE, uint32_t baudrate = SENSOR_BUILDER_DEF_BAUD);
    int poll();
};

void SensorBuilderClass::check_grove()
{
    // Specific power logic for SenseCAP S2110
    GROVE_SWITCH_ADC;
    pinMode(SENSOR_ANALOG_PIN, OUTPUT);
    digitalWrite(SENSOR_ANALOG_PIN, HIGH);
    delay(10);
    pinMode(SENSOR_ANALOG_PIN, INPUT);
    _i2c_available = (digitalRead(SENSOR_ANALOG_PIN) == HIGH);
}

bool SensorBuilderClass::begin(uint8_t slave, uint32_t baudrate)
{
    _slave = slave;
    _baudrate = baudrate;
    return true;
}

int SensorBuilderClass::poll()
{
    for (auto iter = m_sensorMap.begin(); iter != m_sensorMap.end(); ++iter)
    {
        sensorClass *sensor = (sensorClass *)iter->second;
        if (!sensor->connected()) continue;

        sensor->sample();
        auto m_measureValue = sensor->getMeasureValue();
        
        for (auto m_iter = m_measureValue.begin(); m_iter != m_measureValue.end(); ++m_iter)
        {
            // Update the Modbus register in the main 'mb' object
            mb.Hreg(m_iter->addr, m_iter->value.u16);
        }
    }
    return 0;
}

uint16_t SensorBuilderClass::addSensor(sensorClass *_sensor)
{
    uint16_t regs = 0;
    m_sensorMap.insert(std::pair<uint16_t, sensorClass *>(m_sensorMap.size(), _sensor));
    regs = _sensor->init(_regs, _i2c_available);
    _regs += regs;
    return regs;
}

#endif