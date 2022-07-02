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
    Serial.write("0");
  }
  Serial.print(number);
}

int myWeekday()
{
  int weekdays[] = {0, 6, 0, 1, 2, 3, 4, 5};
  int convertedWeekday = weekdays[weekday()];
  return convertedWeekday;
}

void blink(int loop)
{
  for (int i = 0; i < loop; i++)
  {
    digitalWrite(powerLed, LOW);
    delay(250);
    digitalWrite(powerLed, HIGH);
    delay(250);
  }
}

void setCrossOrigin()
{
  server.sendHeader(F("Access-Control-Allow-Origin"), F("*"));
  server.sendHeader(F("Access-Control-Max-Age"), F("600"));
  server.sendHeader(F("Access-Control-Allow-Methods"), F("PUT,POST,GET,OPTIONS"));
  server.sendHeader(F("Access-Control-Allow-Headers"), F("*"));
};

void handleSettingsUpdate()
{
  setCrossOrigin();
  String data = server.arg("plain");
  setFileData("events.json", data);
  Serial.println("-----data => " + data);
  server.send(200, "application/json", getFileData("events.json"));
  deserializeJson(events, data);
  blink(3);
}


void giveFoood(int portion)
{
  blink(3);
  servo_engine.attach(engine);
  servo_engine.write(180);
  delay(portion * 1000);
  servo_engine.detach();
}

void handleGiveFood()
{
  setCrossOrigin();
  String portion = server.arg("portion");
  Serial.println("-----PORTİON => " + portion);
  giveFoood(portion.toInt());
  server.send(200, "application/json", "{\"status\" : \"ok\"}");
}

void toggleLED()
{
  setCrossOrigin();
  Serial.println("toggleed Tetiklendi");
  digitalWrite(powerLed, !digitalRead(powerLed));
  server.send(200, "text / plain", "");
}

void getSettings()
{
  setCrossOrigin();
  server.send(200, "application/json", getFileData("events.json"));
}

void handleRoot()
{
  setCrossOrigin();
  server.send(200, "application/json", "{\"status\" : \"ok\"}");
}

void setup()
{
  Serial.begin(115200);
  LittleFS.begin();
  pinMode(powerLed, OUTPUT);

  while (!Serial)
    ; // wait for serial
  delay(200);

  Serial.println("DS1307RTC Read Test");
  Serial.println("-------------------");

  deserializeJson(events, getFileData("events.json"));
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
  server.on("/settings", getSettings);
  server.on("/food", handleGiveFood);
  server.on("/", handleRoot);
  server.on("/toggle", HTTP_POST, toggleLED);
  server.begin();

  MDNS.addService("http", "tcp", 80);
  delay(3000);
  digitalWrite(powerLed, HIGH);
}

void loop()
{
  MDNS.update();
  server.handleClient();
  tmElements_t tm;

  if (RTC.read(tm))
  {
    for (unsigned int i = 0; i < events.size(); i++)
    {
      Serial.println("ilk if");
      // Bu kontrole retry false ise statuse bakacak.
      if(events[i]["status"].as<String>() == "true")
      {
        Serial.println("2 if");
        if (events[i]["retry"].as<String>() == "true")
        {
          Serial.println("3 if");
          if (events[i]["hour"].as<String>() == String(tm.Hour) && events[i]["minute"].as<String>() == String(tm.Minute) && events[i]["status"].as<String>() == "true" && String(tm.Second) == "0" && events[i]["weekDay"][myWeekday()].as<String>() == "1")
          {
            Serial.println("4 if");
            giveFoood(events[i]["portion"].as<int>());
          }
        }
        else
        {
          Serial.println("1 else");
          if (events[i]["status"].as<String>() == "true" && events[i]["hour"].as<String>() == String(tm.Hour) && events[i]["minute"].as<String>() == String(tm.Minute) && String(tm.Second) == "0")
          {
            Serial.println("5 if");
            events[i]["status"] = "false";
            giveFoood(events[i]["portion"].as<int>());
          }
        }
      }
      else
      {
        Serial.println(events[i]["status"].as<String>());
        Serial.println("---------------");
      }
    }

    Serial.println(myWeekday());
    Serial.println("-----------");
    print2digits(tm.Hour);
    Serial.write(":");
    print2digits(tm.Minute);
    Serial.write(":");
    print2digits(tm.Second);
    Serial.print(", Date (D/M/Y) = ");
    Serial.print(tm.Day);
    Serial.write("/");
    Serial.print(tm.Month);
    Serial.write("/");
    Serial.print(tmYearToCalendar(tm.Year));
    Serial.println();

    // if (tm.Hour == 21 && tm.Minute == 31 && tm.Second == 10)
    // {
    //   Serial.println("---------ESKİ---------");
    //   print2digits(tm.Hour);
    //   Serial.write(":");
    //   print2digits(tm.Minute);
    //   Serial.write(":");
    //   print2digits(tm.Second);
    //   Serial.print(", Date (D/M/Y) = ");
    //   Serial.print(tm.Day);
    //   Serial.write("/");
    //   Serial.print(tm.Month);
    //   Serial.write("/");
    //   Serial.print(tmYearToCalendar(tm.Year));
    //   Serial.println();
    //   Serial.println("---------ESKİ---------");
    //   Serial.println();
    //   setTime(21, 31, 11, 29, 5, 2022);
    //   delay(2000);
    //   Serial.println("---------YENİ---------");
    //   print2digits(tm.Hour);
    //   Serial.write(":");
    //   print2digits(tm.Minute);
    //   Serial.write(":");
    //   print2digits(tm.Second);
    //   Serial.print(", Date (D/M/Y) = ");
    //   Serial.print(tm.Day);
    //   Serial.write("/");
    //   Serial.print(tm.Month);
    //   Serial.write("/");
    //   Serial.print(tmYearToCalendar(tm.Year));
    //   Serial.println();
    //   Serial.println("---------YENİ---------");
    //   delay(1000000);
    // }
    

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
  delay(950);
}
