#include <M5Stack.h>
#include <MHZ19.h>
#include <WiFiClientSecure.h>
#include <ssl_client.h>
#include <Ticker.h>
#include <Ambient.h>
#include <WebServer.h>
#include <ESPmDNS.h>

const char* ssid = "decoNd4l90O1";
const char* password = "NSiQA7Gm";

const char* logFileName = "/co2buffer.txt";
const char* tmpLogFileName = "/tmp.co2buffer.txt";

unsigned int ambientChannelId = 34764;
const char* ambientWriteKey = "bd1b9cb95274fd0a";

#define RX 16
#define TX 17
#define INTERVAL 6

#define CO2_CHECK_SPAN_SEC 120
#define CO2_WARN_PPM 1000
#define CO2_OVER_PPM 1400

#define LCD_TIMEOUT_SEC 30

#define SAVE_BUFFER_SPAN 5

MHZ19 myMHZ19;
HardwareSerial  mySerial(1);

const char* ntpServer = "ntp.nict.jp";
const long  gmtOffset_sec = 3600 * 9;
const int   daylightOffset_sec = 0;

class RingBuffer
{
  private:
    int _startPos;
    int _nextPos;
    int _length;
    static const int _bufferSize = 9 * 30;
    int _buffer[_bufferSize];

  private:
    int ReadIntFromFile(File& f)
    {
      String s = "0";
      while (f.available())
      {
        char ch = f.read();
        if (ch == '\r') continue;
        if (ch == '\n') break;
        s.concat(ch);
      }
    
      return s.toInt();
    }
    
    void SaveCO2BufferToFile(const char* filename)
    {
      if (SD.exists(filename))
      {
        SD.remove(filename);
      }
    
      File f = SD.open(filename, FILE_WRITE);
      if (!f) return;
      
      f.println(_startPos);
      f.println(_nextPos);
      f.println(_length);
      for (int i = 0; i < _bufferSize; i++)
      {
        f.println(_buffer[i]);
      }

      f.close();
    }

  public:
    RingBuffer()
    {
      Clear();
    }

    void Clear()
    {
      _startPos = 0;
      _nextPos = 0;
      _length = 0;
      for (int i = 0; i < _bufferSize; i++)
      {
        _buffer[i] = 0;
      }
    }

    void Append(int value)
    {
      _buffer[_nextPos] = value;

      _nextPos++;
      if (_nextPos >= _bufferSize) _nextPos = 0;

      if (_length >= _bufferSize)
      {
        _startPos++;
        if (_startPos >= _bufferSize) _startPos = 0;
      }
      else
      {
        _length++;
      }
    }

    int Length()
    {
      return _length;
    }

    int Get(int index)
    {
      if (index < 0 || index >= _length) return -1;

      int realIndex = _startPos + index;
      if (realIndex >= _bufferSize) realIndex -= _bufferSize;
      return _buffer[realIndex];
    }

    void LoadCO2Buffer()
    {
      Clear();
      const char* filename = SD.exists(tmpLogFileName) ? tmpLogFileName : logFileName;

      if (!SD.exists(filename)) return;
    
      File f = SD.open(filename, FILE_READ);
      if (!f) return;
      
      _startPos = ReadIntFromFile(f);
      _nextPos = ReadIntFromFile(f);
      _length = ReadIntFromFile(f);
      for (int i = 0; i < _bufferSize; i++)
      {
        _buffer[i] = ReadIntFromFile(f);
      }

      f.close();
    }
    
    void SaveCO2Buffer()
    {
    SaveCO2BufferToFile(tmpLogFileName);
    SaveCO2BufferToFile(logFileName);
    SD.remove(tmpLogFileName);
    }
};

RingBuffer co2Buffer;

Ticker tickerCO2;
Ticker tickerLCD;

struct tm co2CheckStartTime;
int currentCO2 = 0;
int co2BufferUpdateCount = 0;

Ambient ambient;
WiFiClient ambientWiFiClient;

WebServer server(80);

void outputLog(String msg)
{
  Serial.println(msg);
}

void logCO2(int co2)
{
  String co2Str(co2, DEC);
  String str = "co2 " + co2Str;
  outputLog(str);

  co2Buffer.Append(co2);

  ambient.set(1, co2);
  ambient.send();

  co2BufferUpdateCount++;
  if (co2BufferUpdateCount >= SAVE_BUFFER_SPAN)
  {
    co2BufferUpdateCount = 0;
    co2Buffer.SaveCO2Buffer();
  }
}

void checkCO2()
{
  currentCO2 = myMHZ19.getCO2();
  logCO2(currentCO2);

  if (currentCO2 > CO2_WARN_PPM)
  {
    String co2Str(currentCO2, DEC);
    String url = "http://" + WiFi.localIP().toString();
    String str = "CO2 Warning : " + co2Str + "\n" + url;
    l_notify(str);
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
          url: 'http://" + WiFi.localIP().toString() + "/getco2',\n\
          dataType: 'json'\n\
        }).then(function(data) {\n\
          $(\"#currentCO2\").text(data.currentCO2 + \" ppm\");\n\
          var co2labels = [];\n\
          var co2values = [];\n\
          var co2colors = [];\n\
          $(data.co2s).each(function(index, co2) {\n\
            co2labels.push('');\n\
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
  int count = co2Buffer.Length();
  String delim = "";
  for (int i = max(0, count - 60); i < count; i++)
  {
    int co2 = co2Buffer.Get(i);
    String color = co2 > CO2_OVER_PPM ? "red" : (co2 > CO2_WARN_PPM ? "pink" : "blue");
    jsonBody += delim + "{\"value\":" + String(co2Buffer.Get(i)) + ",\"color\":\"" + color + "\"}";
    delim = ",";
  }

  server.send(200, "application/json", jsonHeader + jsonBody + jsonFooter);
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

void setupSerial()
{
  mySerial.begin(9600,SERIAL_8N1,RX,TX);
  delay(100);
}

void setupMHZ19()
{
  myMHZ19.begin(mySerial);
  myMHZ19.autoCalibration(false);
}

void setupWiFi()
{
  WiFi.disconnect();
  WiFi.mode(WIFI_STA);
  WiFi.begin(ssid, password);
  while(WiFi.status() != WL_CONNECTED){
    outputLog("waiting...");
    delay(500);
  }
  outputLog("connected");
  delay(2000);
}

void setupTime()
{
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

  if (!getLocalTime(&co2CheckStartTime)) {
    outputLog("Failed to obtain time");
    return;
  }

  String YYYY(co2CheckStartTime.tm_year + 1900, DEC);
  String MM(co2CheckStartTime.tm_mon+1, DEC);
  String dd(co2CheckStartTime.tm_mday, DEC);
  String hh(co2CheckStartTime.tm_hour, DEC);
  String mm(co2CheckStartTime.tm_min, DEC);
  String ss(co2CheckStartTime.tm_sec, DEC);
  String startMsg = "CO2 check start : " + YYYY + "/" + MM + "/" + dd + " " + hh + ":" + mm + ":" + ss;
  outputLog(startMsg);
}

void setupLCDTicker()
{
  tickerLCD.once(LCD_TIMEOUT_SEC, lcdOff);
}

void setupAmbient()
{
  ambient.begin(ambientChannelId, ambientWriteKey, &ambientWiFiClient);
}

void setupServer()
{
  MDNS.begin("m5stack");

  server.on("/", serverHandleRoot);
  server.on("/getco2", serverHandleGetCO2);
  server.onNotFound(serverHandleNotFound);
  server.begin();
}

void setup() {
  setupPower();
  setupSpeaker();
  setupSerial();
  setupMHZ19();
  setupWiFi();
  setupTime();
  setupTicker();
  setupAmbient();
  setupServer();

  co2Buffer.LoadCO2Buffer();
  draw();
}

void l_notify(String msg) {
  const char* host = "notify-api.line.me";
  const char* token = "ymvMz7EY761RCA9CKKwcZBUSwsWnpde6j1WVHVrwieC";
  WiFiClientSecure client;
  client.setInsecure();
  outputLog(msg);
  if (!client.connect(host, 443)) {
    outputLog("connect failed");
    delay(2000);
    return;
  }
  String query = String("message=") + msg;
  String request = String("") +
               "POST /api/notify HTTP/1.1\r\n" +
               "Host: " + host + "\r\n" +
               "Authorization: Bearer " + token + "\r\n" +
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
  if (co2Buffer.Length() > 0)
  {
    timeStep = (float)(right - left) / co2Buffer.Length();
  }

  for (int i = 0; i < co2Buffer.Length(); i++)
  {
    int co2 = co2Buffer.Get(i);
    if (co2 < yMin) co2 = yMin;
    if (co2 > yMax) co2 = yMax;
    
    int timeLeft = left + (int)((float)i * timeStep);
    int timeWidth = timeStep;
    int co2Top = bottom - (int)(co2Step * (float)(co2 - yMin));
    int co2Height = bottom - co2Top;
    int color = co2 > CO2_OVER_PPM ? RED : (co2 > CO2_WARN_PPM ? 0xFFE0/*YELLOW*/ : BLUE);
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
