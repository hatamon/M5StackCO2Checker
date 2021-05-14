#include <M5Stack.h>
#include <Ticker.h>
#include <WebServer.h>
#include "Buffer.h"
#include "CO2Data.h"
#include "CO2Sensor.h"
#include "Line.h"
#include "Logger.h"

const char* ssid = "decoNd4l90O1";
const char* password = "NSiQA7Gm";

const char* logFileName = "/co2buffer.txt";

const char* lineHost = "notify-api.line.me";
const char* lineToken = "ymvMz7EY761RCA9CKKwcZBUSwsWnpde6j1WVHVrwieC";

#define RX 16
#define TX 17

#define CO2_CHECK_SPAN_SEC 120
#define CO2_WARN_PPM 1000
#define CO2_OVER_PPM 1400

#define LCD_TIMEOUT_SEC 30

#define SAVE_BUFFER_SPAN 5

CO2Sensor co2Sensor(SERIAL_8N1, RX, TX);
Buffer co2Buffer(9 * 30);

Line line(lineHost, lineToken);

Ticker tickerCO2;
Ticker tickerLCD;

unsigned int currentCO2 = 0;
int co2BufferUpdateCount = 0;

WebServer server(80);

void logCO2(unsigned int co2)
{
  String co2Str(co2, DEC);
  String str = "co2 " + co2Str;
  Logger::Log(str);

  CO2Data data;
  data.co2 = co2;
  struct tm datetime;
  if (getLocalTime(&datetime))
  {
    data.year = datetime.tm_year + 1900;
    data.mon = datetime.tm_mon + 1;
    data.day = datetime.tm_mday;
    data.hour = datetime.tm_hour;
    data.min = datetime.tm_min;
    data.sec = datetime.tm_sec;
  }
  co2Buffer.Append(data);

  co2BufferUpdateCount++;
  if (co2BufferUpdateCount >= SAVE_BUFFER_SPAN)
  {
    co2BufferUpdateCount = 0;
    co2Buffer.SaveCO2Buffer(logFileName);
  }
}

void checkCO2()
{
  currentCO2 = co2Sensor.GetCO2();
  logCO2(currentCO2);

  if (currentCO2 > CO2_OVER_PPM)
  {
    String co2Str(currentCO2, DEC);
    String url = "http://" + WiFi.localIP().toString();
    String str = "CO2 Over : " + co2Str + "\n" + url;
    line.Notify(str);
  }
}

void lcdOn()
{
  M5.Lcd.wakeup();
  M5.Lcd.setBrightness(200);
}

void lcdOff()
{
  M5.Lcd.sleep();
  M5.Lcd.setBrightness(0);
}

void serverHandleRoot()
{
  String html = "\
<!DOCTYPE html>\n\
<html lang=\"ja\">\n\
  <head>\n\
    <meta charset=\"utf-8\">\n\
    <meta http-equiv=\"refresh\" content=\"120\">\n\
    <title>グラフ</title> \n\
  </head>\n\
  <body>\n\
    <h1 id=\"currentCO2\">currentCO2</h1>\n\
    <canvas id=\"myChart\"></canvas>\n\
    <script src=\"https://cdn.jsdelivr.net/npm/chart.js\"></script>\n\
    <script src=\"https://ajax.googleapis.com/ajax/libs/jquery/3.3.1/jquery.min.js\"></script>\n\
    <script>\n\
      function updateGraph()\n\
      {\n\
        $.ajax({\n\
          type: 'GET',\n\
          url: '/getco2',\n\
          dataType: 'json'\n\
        }).then(function(data) {\n\
          $(\"title\").text(data.currentCO2 + \" ppm\");\n\
          $(\"#currentCO2\").text(data.currentCO2 + \" ppm\");\n\
          var co2labels = [];\n\
          var co2values = [];\n\
          var co2colors = [];\n\
          $(data.co2s).each(function(index, co2) {\n\
            co2labels.push(co2.label);\n\
            co2values.push(co2.value);\n\
            co2colors.push(co2.color);\n\
          });\n\
          var ctx = document.getElementById(\"myChart\");\n\
          var myChart = new Chart(ctx, {\n\
            type: 'bar',\n\
            data: {labels: co2labels, datasets: [{label: 'CO2(ppm)', data: co2values, backgroundColor: co2colors}]},\n\
          });\n\
        }, function(err) { alert(err); });\n\
      }\n\
      $.ajaxSetup({ cache: false });\n\
      window.addEventListener('load', updateGraph);\n\
    </script>\n\
    \n\
  </body>\n\
</html>\n\
";

  server.send(200, "text/html", html);
}

void serverHandleGetCO2()
{
  String jsonHeader = "{\"co2s\":[";
  String jsonFooter = "], \"currentCO2\":" + String(currentCO2) + "}";
  String jsonBody = "";
  int count = co2Buffer.GetSize();
  String delim = "";
  int day = 0;
  int hour = 0;
  for (int i = max(0, count - 60); i < count; i++)
  {
    auto co2data = co2Buffer.Get(i);
    String color = co2data.co2 > CO2_OVER_PPM ? "red" : (co2data.co2 > CO2_WARN_PPM ? "pink" : "blue");
    String label;
    if (day != co2data.day)
    {
      day = co2data.day;
      label = String(co2data.day) + " ";
    }
    if (hour != co2data.hour)
    {
      hour = co2data.hour;
      label += String(co2data.hour) + ":";
    }
    label += String(co2data.min);
    jsonBody += delim + "{\"label\":\"" + label + "\",\"value\":" + String(co2data.co2) + ",\"color\":\"" + color + "\"}";
    delim = ",";
  }

  server.send(200, "application/json", jsonHeader + jsonBody + jsonFooter);
}

void serverHandleReset()
{
  co2Buffer.Clear();
  co2Buffer.SaveCO2Buffer(logFileName);
  server.send(200, "text/plain", "Reset.");
}

void serverHandleNotFound()
{
  server.send(404, "text/plain", "Not found.");
}

void setupPower()
{
  M5.begin();
  M5.Power.begin();

  M5.Power.setPowerBoostSet(true);  
  M5.Power.setPowerVin(false);  
}

void setupSpeaker()
{
  M5.Speaker.begin();
  M5.Speaker.mute();
}

void setupWiFi()
{
  WiFi.disconnect();
  WiFi.mode(WIFI_STA);
  WiFi.begin(ssid, password);
  while(WiFi.status() != WL_CONNECTED){
    Logger::Log("waiting...");
    delay(500);
  }
  Logger::Log("connected");
  delay(2000);
}

void setupTime()
{
  char* ntpServer = "ntp.nict.jp";
  long  gmtOffset_sec = 3600 * 9;
  int   daylightOffset_sec = 0;

  configTime(gmtOffset_sec, daylightOffset_sec, ntpServer);
}

void setupTicker()
{
  setupCO2Ticker();
  setupLCDTicker();
}

void setupCO2Ticker()
{
  tickerCO2.attach(CO2_CHECK_SPAN_SEC, checkCO2);
}

void setupLCDTicker()
{
  tickerLCD.once(LCD_TIMEOUT_SEC, lcdOff);
}

void setupServer()
{
  server.on("/", serverHandleRoot);
  server.on("/getco2", serverHandleGetCO2);
  server.on("/reset", serverHandleReset);
  server.onNotFound(serverHandleNotFound);
  server.begin();
}

void setup() {
  setupPower();
  setupSpeaker();
  setupWiFi();
  setupTime();
  setupTicker();
  setupServer();

  co2Buffer.LoadCO2Buffer(logFileName);
  draw();
}

void drawBattery()
{
  int batteryLevel = M5.Power.getBatteryLevel();
  M5.Lcd.progressBar(0, 0, 320, 7, batteryLevel);
  M5.Lcd.setTextSize(1);
  M5.Lcd.drawString(String(batteryLevel), 0, 0);
}

void drawCO2()
{
  M5.Lcd.setTextSize(5);
  M5.Lcd.drawString(String(currentCO2), 10, 8);
  M5.Lcd.setTextSize(3);
  M5.Lcd.drawString("ppm", 10 + 7 * 5 * 4 + 3, 8);  
}

void drawCO2Graph()
{
  int top = 50;
  int bottom = 230;
  int left = 35;
  int right = 310;

  int yMin = 400;
  int yMax = 1600;
  int yStep = 150;
  int yCount = (yMax - yMin) / yStep + 1;

  float co2Step = (float)(bottom - top) / (float)(yMax - yMin);
  float timeStep = 0.0;
  if (co2Buffer.GetSize() > 0)
  {
    timeStep = (float)(right - left) / co2Buffer.GetSize();
  }

  for (int i = 0; i < co2Buffer.GetSize(); i++)
  {
    auto co2data = co2Buffer.Get(i);
    int co2 = co2data.co2;
    if (co2 < yMin) co2 = yMin;
    if (co2 > yMax) co2 = yMax;
    
    int timeLeft = left + (int)((float)i * timeStep);
    int timeWidth = timeStep;
    int co2Top = bottom - (int)(co2Step * (float)(co2 - yMin));
    int co2Height = bottom - co2Top;
    int color = co2 > CO2_OVER_PPM ? RED : (co2 > CO2_WARN_PPM ? 0xFC9F/*PINK*/ : BLUE);
    M5.Lcd.fillRect(timeLeft, co2Top, timeWidth, co2Height, color);
  }

  M5.Lcd.setTextSize(1);
  for (int i = yMin; i <= yMax; i += yStep)
  {
    int top = bottom - co2Step * (i - yMin);
    M5.Lcd.drawString(String(i), 5, top - 3);
    M5.Lcd.drawFastHLine(left, top, (right - left), WHITE);
  }
}

void draw()
{
  M5.Lcd.fillRect(0, 0, 320, 240, 0);

  drawBattery();
  drawCO2();
  drawCO2Graph();
}

void loop() {
  if (M5.BtnB.wasPressed())
  {
    lcdOn();
    draw();
    setupLCDTicker();
  }
  
  M5.update();

  server.handleClient();
}
