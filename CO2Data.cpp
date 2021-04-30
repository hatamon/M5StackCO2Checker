#include "CO2Data.h"
#include "FileUtil.h"

CO2Data::CO2Data()
{
  Clear();
}

void CO2Data::Clear()
{
  co2 = 0;
  year = 0;
  mon = 0;
  day = 0;
  hour = 0;
  min = 0;
  sec = 0;
}

void CO2Data::SerializeToFile(File& f)
{
  String s = String(co2) + ","
    + String(year) + ","
    + String(mon) + ","
    + String(day) + ","
    + String(hour) + ","
    + String(min) + ","
    + String(sec);

  f.println(s);
}

void CO2Data::DeserializeFromFile(File& f)
{
  co2 = FileUtil::ReadInt(f);
  year = FileUtil::ReadInt(f);
  mon = FileUtil::ReadInt(f);
  day = FileUtil::ReadInt(f);
  hour = FileUtil::ReadInt(f);
  min = FileUtil::ReadInt(f);
  sec = FileUtil::ReadInt(f);
}