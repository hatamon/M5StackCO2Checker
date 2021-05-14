#include <M5Stack.h>
#include <WiFiClientSecure.h>
#include <ssl_client.h>
#include "Line.h"
#include "Logger.h"

Line::Line(String host, String token)
{
  _host = host;
  _token = token;
}

void Line::Notify(String msg)
{
  WiFiClientSecure client;
  client.setInsecure();
  Logger::Log(msg);
  if (!client.connect(_host.c_str(), 443)) {
    Logger::Log("connect failed");
    delay(2000);
    return;
  }
  String query = String("message=") + msg;
  String request = String("") +
               "POST /api/notify HTTP/1.1\r\n" +
               "Host: " + _host + "\r\n" +
               "Authorization: Bearer " + _token + "\r\n" +
               "Content-Length: " + String(query.length()) +  "\r\n" + 
               "Content-Type: application/x-www-form-urlencoded\r\n\r\n" +
                query + "\r\n";
  client.print(request);
  while (client.connected()) {
    String line = client.readStringUntil('\n');
    if (line == "\r") {
      break;
    }
  }
  String line = client.readStringUntil('\n');
  delay(2000);
}