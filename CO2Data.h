#pragma once

#include <M5Stack.h>

struct CO2Data
{
  int co2;
  int year;
  int mon;
  int day;
  int hour;
  int min;
  int sec;
    
  CO2Data();

  void Clear();
  void SerializeToFile(File& f);
  void DeserializeFromFile(File& f);
};
