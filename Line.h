#pragma once

#include <M5Stack.h>

class Line
{
  private:
    String _host;
    String _token;

  public:
    Line(String host, String token);

  public:
    void Notify(String msg);
};