#include "CO2Sensor.h"

CO2Sensor::CO2Sensor(uint32_t config, int8_t rxPin, int8_t txPin)
{
  _pSerial = new HardwareSerial(1);
  _pSerial->begin(9600, config, rxPin, txPin);
  delay(100);

  _pMHZ19 = new MHZ19;
  _pMHZ19->begin(*_pSerial);
  _pMHZ19->autoCalibration(false);
}

CO2Sensor::~CO2Sensor()
{
  if (_pSerial != nullptr)
  {
    delete _pSerial;
  }

  if (_pMHZ19 != nullptr)
  {
    delete _pMHZ19;
  }
}

unsigned int CO2Sensor::GetCO2()
{
  return (unsigned int)_pMHZ19->getCO2();
}