/*
  ESP-01 pinout from top:
  
  GND    GP2 GP0 RX/GP3
  TX/GP1 CH  RST VCC
  MAX7219
  ESP-1 from rear
  Re Br Or Ye
  Gr -- -- --
  USB to Serial programming
  ESP-1 from rear, FF to GND, RR to GND before upload
  Gr FF -- Bl
  Wh -- RR Vi
  GPIO 2 - DataIn
  GPIO 1 - LOAD/CS
  GPIO 0 - CLK
  ------------------------
  NodeMCU 1.0 pinout:
  D8 - DataIn
  D7 - LOAD/CS
  D6 - CLK
  
*/


#include "Arduino.h"
#include <ESP8266WiFi.h>
#include <ESP8266WebServer.h>
#include <SPI.h>
#include <Adafruit_GFX.h>
#include <Max72xxPanel.h>
#include <ArduinoJson.h>

// for ESP-01 module
//#define DIN_PIN 2 // D4
//#define CS_PIN  3 // D9/RX
//#define CLK_PIN 0 // D3

// for NodeMCU 1.0
#define DIN_PIN 15  // D8
#define CS_PIN  13  // D7
#define CLK_PIN 12  // D6
#define NUM_MAX 4
#include "max7219.h"
#include "fonts.h"
//=====================================
WiFiClient client;


String weatherMain = "";
String weatherDescription = "";
String weatherLocation = "";
String country;
int Humidity;
int Pressure;
float Temprature;
float tempMin, tempMax;
int clouds;
float windSpeed;
String date;
String currencyRates;
String weatherString;

long period;
//int offset=1,refresh=0;
int pinCS = 0; // Подключение пина CS
int numberOfHorizontalDisplays = 6; // Количество светодиодных матриц по Горизонтали
int numberOfVerticalDisplays = 1; // Количество светодиодных матриц по Вертикали
String decodedMsg;
Max72xxPanel matrix = Max72xxPanel(pinCS, numberOfHorizontalDisplays, numberOfVerticalDisplays);
//matrix.cp437(true);

int wait = 50; // скорость бегущей строки

int spacer = 2;
int width = 5 + spacer; // Регулируем расстояние между символами




// =======================================================================
// CHANGE YOUR CONFIG HERE:
// =======================================================================
const char* ssid     = "Rio_wifi";     // SSID of local network
const char* password = "Rivierio2025";   // Password on network
String weatherKey = "ece15a122e64f574f93f7546879a6458";  // API key, click http://openweathermap.org/api
String weatherLang = "&lang=en";
String cityID = "1645528"; //Denpasar
// =======================================================================

void setup() 
{
  Serial.begin(115200);
  initMAX7219();
  sendCmdAll(CMD_SHUTDOWN,1);
  sendCmdAll(CMD_INTENSITY,0);
  Serial.print("Connecting WiFi ");
  WiFi.begin(ssid, password);
  printStringWithShift("Connecting ",16);
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }
  Serial.println("");
  Serial.print("Connected: "); Serial.println(WiFi.localIP());
}
// =======================================================================
#define MAX_DIGITS 16
byte dig[MAX_DIGITS]={0};
byte digold[MAX_DIGITS]={0};
byte digtrans[MAX_DIGITS]={0};
int updCnt = 0;
int dots = 0;
long dotTime = 0;
long clkTime = 0;
int dx=0;
int dy=0;
byte del=0;
int h,m,s;
// =======================================================================
void loop()
{
  if(updCnt<=0) { // every 10 scrolls, ~450s=7.5m
    updCnt = 10;
    Serial.println("Getting data ...");
    printStringWithShift("  Getting data",15);
    getWeatherData();
    getTime();
    Serial.println("Data loaded");
    clkTime = millis();
  }
 
  if(millis()-clkTime > 40000 && !del && dots) { // clock for 15s, then scrolls for about 30s
    printStringWithShift(date.c_str(),40);
    printStringWithShift(weatherString.c_str(),70);
   delay(5000);
    updCnt--;
    clkTime = millis();
  }
  if(millis()-dotTime > 500) {
    dotTime = millis();
    dots = !dots;
  }
  updateTime();
  showAnimClock();
}

// =======================================================================

void showSimpleClock()
{
  dx=dy=0;
  clr();
  showDigit(h/10,  1, dig6x8);
  showDigit(h%10,  9, dig6x8);
  showDigit(m/10, 18, dig6x8);
  showDigit(m%10, 26, dig6x8);
  showDigit(s/10, 38, dig6x8);
  showDigit(s%10, 46, dig6x8);
  setCol(16,dots ? B00100100 : 0);
  setCol(33,dots ? B00100100 : 0);
  refreshAll();
}

// =======================================================================

void showAnimClock()
{
  byte digPos[6]={1,9,18,26,36,44};
  int digHt = 12;
  int num = 6; 
  int i;
  if(del==0) {
    del = digHt;
    for(i=0; i<num; i++) digold[i] = dig[i];
    dig[0] = h/10 ? h/10 : 10;
    dig[1] = h%10;
    dig[2] = m/10;
    dig[3] = m%10;
    dig[4] = s/10;
    dig[5] = s%10;
    for(i=0; i<num; i++)  digtrans[i] = (dig[i]==digold[i]) ? 0 : digHt;
  } else
    del--;
  
  clr();
  for(i=0; i<num; i++) {
    if(digtrans[i]==0) {
      dy=0;
      showDigit(dig[i], digPos[i], dig6x8);
    } else {
      dy = digHt-digtrans[i];
      showDigit(digold[i], digPos[i], dig6x8);
      dy = -digtrans[i];
      showDigit(dig[i], digPos[i], dig6x8);
      digtrans[i]--;
    }
  }
  dy=0;
  setCol(16,dots ? B00100100 : 0);
  setCol(33,dots ? B00100100 : 0);
  refreshAll();
  delay(30);
}

// =======================================================================

void showDigit(char ch, int col, const uint8_t *data)
{
  if(dy<-8 | dy>8) return;
  int len = pgm_read_byte(data);
  int w = pgm_read_byte(data + 1 + ch * len);
  col += dx;
  for (int i = 0; i < w; i++)
    if(col+i>=0 && col+i<8*NUM_MAX) {
      byte v = pgm_read_byte(data + 1 + ch * len + 1 + i);
      if(!dy) scr[col + i] = v; else scr[col + i] |= dy>0 ? v>>dy : v<<-dy;
    }
}

// =======================================================================

void setCol(int col, byte v)
{
  if(dy<-8 | dy>8) return;
  col += dx;
  if(col>=0 && col<8*NUM_MAX)
    if(!dy) scr[col] = v; else scr[col] |= dy>0 ? v>>dy : v<<-dy;
}

// =======================================================================

int showChar(char ch, const uint8_t *data)
{
  int len = pgm_read_byte(data);
  int i,w = pgm_read_byte(data + 1 + ch * len);
  for (i = 0; i < w; i++)
    scr[NUM_MAX*8 + i] = pgm_read_byte(data + 1 + ch * len + 1 + i);
  scr[NUM_MAX*8 + i] = 0;
  return w;
}

// =======================================================================

void printCharWithShift(unsigned char c, int shiftDelay) {
  
  if (c < ' ' || c > '~'+25) return;
  c -= 32;
  int w = showChar(c, font);
  for (int i=0; i<w+1; i++) {
    delay(shiftDelay);
    scrollLeft();
    refreshAll();
  }
}

// =======================================================================

void printStringWithShift(const char* s, int shiftDelay){
  while (*s) {
    printCharWithShift(*s, shiftDelay);
    s++;
  }
}

// =======================================================================
// Data cuaca
// =======================================================================
const char *weatherHost = "api.openweathermap.org";

void getWeatherData()
{ 
  
      if (client.connect(weatherHost, 80)) {
         Serial.print("connecting to "); Serial.println(weatherHost);
        client.println("GET /data/2.5/weather?id=" + cityID + "&units=metric&APPID=" +weatherKey + weatherLang );
        client.println("Host: api.openweathermap.org");
        client.println("User-Agent: ArduinoWiFi/1.1");
        client.println("Connection: close");
        client.println();
      }
      else {
        Serial.println("connection failed"); //error message if no client connect
        Serial.println();
      }
      String line;
      while (client.connected() && !client.available()) delay(1); //waits for data
      while (client.connected() || client.available()) { //connected or data available
        char c = client.read(); //gets byte from ethernet buffer
        line +=  c;
      }

      client.stop(); //stop client
      line.replace('[', ' ');
      line.replace(']', ' ');
     // Serial.println(result);

      char jsonArray [line.length() + 1];
      line.toCharArray(jsonArray, sizeof(jsonArray));
      jsonArray[line.length() + 1] = '\0';

      StaticJsonBuffer<1024> json_buf;
      JsonObject &root = json_buf.parseObject(jsonArray);
      if (!root.success())
      {
        Serial.println("parseObject() failed");
      } 

  JsonObject& weather_0 = root["weather"][0];
  //weatherMain = root["weather"]["main"].as<String>();
  weatherDescription = root["weather"][0]["description"].as<String>();
  weatherDescription.toLowerCase();
  //  weatherLocation = root["name"].as<String>();
  //  country = root["sys"]["country"].as<String>();
  JsonObject& main = root["main"];
  Temprature = root["main"]["temp"];
  Humidity = root["main"]["humidity"];
  Pressure = root["main"]["pressure"];
  tempMin = root["main"]["temp_min"];
  tempMax = root["main"]["temp_max"];
  windSpeed = root["wind"]["speed"];
  clouds = root["clouds"]["all"];
  String deg = String(char('~'+25));
  weatherString = "         Suhu: " + String(Temprature,1)+" C  ";
  weatherString += weatherDescription;
  weatherString += " Kelembaban : " + String(Humidity) + "% ";
  weatherString += "Tekanan udara : " + String(Pressure/1.3332239) + " mm ";
  weatherString += "Awan : " + String(clouds) + "% ";
  weatherString += "Kecepatan angin : " + String(windSpeed,1) + " M/s";
  Serial.println(weatherString);


}
// =======================================================================
// jam update
// =======================================================================

float utcOffset = 12;
long localEpoc = 0;
long localMillisAtUpdate = 0;

void getTime()
{
  WiFiClient client;
  if (!client.connect("www.google.com", 80)) {
    Serial.println("connection to google failed");
    return;
  }

  client.print(String("GET / HTTP/1.1\r\n") +
               String("Host: www.google.com\r\n") +
               String("Connection: close\r\n\r\n"));
  int repeatCounter = 0;
  while (!client.available() && repeatCounter < 10) {
    delay(500);
    //Serial.println(".");
    repeatCounter++;
    
  }

  String line;
  client.setNoDelay(false);
  while(client.connected() && client.available()) {
    line = client.readStringUntil('\n');
    line.toUpperCase();
    if (line.startsWith("DATE: ")) {
      date = "     "+line.substring(6, 22);
      h = line.substring(23, 25).toInt();
      m = line.substring(26, 28).toInt();
      s = line.substring(29, 31).toInt();
      localMillisAtUpdate = millis();
      localEpoc = (h * 60 * 60 + m * 60 + s);
      
    }
  }
  client.stop();
}

// =======================================================================

void updateTime()
{
  long curEpoch = localEpoc + ((millis() - localMillisAtUpdate) / 1000);
  long epoch = round(curEpoch + 3600 * utcOffset + 86400L)+28800;
  h = ((epoch  % 86400L) / 3600) % 24;
  m = (epoch % 3600) / 60;
  s = epoch % 60;
}

// =======================================================================
