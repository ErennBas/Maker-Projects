#include <Arduino.h>
#include <Wire.h>
#include <Time.h>
#include <FS.h>
#include "LittleFS.h"
#include <DS1307RTC.h>
#include <ESP8266WiFi.h>
#include <WiFiClient.h>
#include <ESP8266WebServer.h>
#include <ESP8266mDNS.h>
#include <ArduinoJson.h>
#include <Servo.h>

#define FORMAT_LITTLEFS_IF_FAILED true
#define powerLed 14
#define engine 12

#ifndef STASSID
#define STASSID "Dokuzbit"
#define STAPSK "9bit4444"
#endif

tmElements_t tm;

struct Event
{
  int id;
  int portion;
  int hour;
  int minute;
  int weekDay;
  bool retry;
  bool status;
};

#define FDSFFDSA "[{id: 0,portion: 3,hour: 10,minute: 1,weekDay: [1,1,1,1,1,1,1],retry: true,status: true},{id: 1,portion: 4,hour: 11,minute: 1,weekDay: [0,1,0,1,0,1,0],retry: true,status: true},{id: 2,portion: 4,hour: 13,minute: 1,weekDay: [1,1,1,1,1,1,1],retry: true,status: true},{id: 3,portion: 4,hour: 14,minute: 1,weekDay: [0,1,0,1,0,1,0],retry: true,status: true},{id: 4,portion: 4,hour: 16,minute: 1,weekDay: [1,1,1,1,1,1,1],retry: true,status: true}]";

const char *testString = FDSFFDSA;
const char *ssid = STASSID;
const char *password = STAPSK;

ESP8266WebServer server(80);
DynamicJsonDocument events(8192);
Servo servo_engine;

String getFileData(String filename)
{
  String result = "";

  File this_file = LittleFS.open(filename, "r");
  if (!this_file)
  { // failed to open the file, retrn empty result
    return result;
  }
  while (this_file.available())
  {
    result += (char)this_file.read();
  }
  this_file.close();
  Serial.println("---GET FİLE-->" + filename + "<-----");
  Serial.println(result);
  Serial.println("---GET FİLE-->" + filename + "<-----");
  return result;
}

bool setFileData(String filename, String data)
{
  File this_file = LittleFS.open(filename, "w");
  if (!this_file)
  { // failed to open the file, return false
    return false;
  }
  int bytesWritten = this_file.print(data);

  if (bytesWritten == 0)
  { // write failed
    return false;
  }

  this_file.close();
  return true;
}

void print2digits(int number)
{
  if (number >= 0 && number < 10)
  {
    Serial.write('0');
  }
  Serial.print(number);
}

void handleSettingsUpdate()
{
  String data = server.arg("plain");
  setFileData("config.json", data);
  Serial.println("-----data => " + data);
  server.send(200, "application/json", "{\"status\" : \"ok\"}");
  int i = 3;
  while (i > 0)
  {
    digitalWrite(powerLed, !digitalRead(powerLed));
    Serial.println("ESP Yeniden Başlatılıyor... " + i);
    i--;
    delay(500);
    digitalWrite(powerLed, !digitalRead(powerLed));
  }
  ESP.restart();
}

void blink(int loop){
  for (int i = 0; i < loop; i++)
  {
    digitalWrite(powerLed, HIGH);
    delay(500);
    digitalWrite(powerLed, LOW);
  }  
}

void giveFoood(int portion)
{
  blink(3);
  servo_engine.write(180);
  delay(portion * 1000);
  servo_engine.write(92);
}

void toggleLED()
{
  Serial.println("toggleed Tetiklendi");
  digitalWrite(powerLed, !digitalRead(powerLed));
  server.send(200, "text / plain", "");
}

void handleRoot()
{
  server.send(200, "text / plain", "Hello resolved by mDNS !");
}

void setup()
{
  Serial.begin(115200);
  LittleFS.begin();
  pinMode(powerLed, OUTPUT);
  pinMode(engine, INPUT);
  Serial.println("----------------------------------------------------");
  Serial.println(digitalRead(engine));
  Serial.println("----------------------------------------------------");
  delay(100000);
  servo_engine.attach(engine);
  while (!Serial)
    ; // wait for serial
  delay(200);
  Serial.println("DS1307RTC Read Test");
  Serial.println("-------------------");

  deserializeJson(events, testString);
  serializeJsonPretty(events, Serial);

  WiFi.begin(STASSID, STAPSK);
  while (WiFi.status() != WL_CONNECTED)
  {
    delay(500);
    Serial.print(".");
  }
  
  Serial.println("");
  Serial.print("Connected! IP address: ");
  Serial.println(WiFi.localIP());

  if (!MDNS.begin("panda"))
  {
    Serial.println("Error setting up MDNS responder!");
    while (1)
    {
      delay(1000);
    }
  }
  Serial.println("mDNS responder started");

  server.on("/settings", HTTP_POST, handleSettingsUpdate);
  server.on("/", handleRoot);
  server.on("/toggle", HTTP_POST, toggleLED);
  server.begin();

  MDNS.addService("http", "tcp", 80);
  delay(3000);
}

void loop()
{
  MDNS.update();
  server.handleClient();
  tmElements_t tm;

  if (RTC.read(tm))
  {

    Serial.println("-----------");
    Serial.println(weekday());
    Serial.println("-----------");
    print2digits(tm.Hour);
    Serial.write(':');
    print2digits(tm.Minute);
    Serial.write(':');
    print2digits(tm.Second);
    Serial.print(", Date (D/M/Y) = ");
    Serial.print(tm.Day);
    Serial.write('/');
    Serial.print(tm.Month);
    Serial.write('/');
    Serial.print(tmYearToCalendar(tm.Year));
    Serial.println();
  }
  else
  {
    if (RTC.chipPresent())
    {
      Serial.println("The DS1307 is stopped.  Please run the SetTime");
      Serial.println("example to initialize the time and begin running.");
      Serial.println();
    }
    else
    {
      Serial.println("DS1307 read error!  Please check the circuitry.");
      Serial.println();
    }
    delay(9000);
  }
  delay(1000);
}
