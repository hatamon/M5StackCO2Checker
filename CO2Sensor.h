
#pragma once

#include <MHZ19.h>

class CO2Sensor
{
  private:
    MHZ19* _pMHZ19;
    HardwareSerial* _pSerial;
    
  public:
    CO2Sensor(uint32_t config, int8_t rxPin, int8_t txPin);
    ~CO2Sensor();

  public:
    unsigned int GetCO2();
};