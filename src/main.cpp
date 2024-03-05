#include <ESP8266WiFi.h>
#include <TimeLib.h>
#include <ESP8266HTTPClient.h>
#include <ArduinoJson.h>
#include "secrets.h" // defines WIFI credecials

long delayTime = 0;
String location;

struct SunTimes {
  unsigned long sunrise;
  unsigned long lastLight;
  unsigned long sunset;
  unsigned long dimTime;
};

unsigned long parseTime(String time);
void connectToWiFi();
void initCurrentTime();
String getTimeStr(long time);
SunTimes fetchSunriseSunsetTimes(String location, String date);

void setup() {
  Serial.begin(115200);
  Serial.println("");
  Serial.println("");
  Serial.println("Starting...");

  pinMode(0, OUTPUT);
  pinMode(2, OUTPUT);
  analogWrite(0, 0);
  analogWrite(2, 0);
  connectToWiFi();

  // Fetch location and timezone
  WiFiClient client;
  HTTPClient httpLocation;
  httpLocation.begin(client, "http://ipinfo.io/" + LOCATION_SERVER);
  int httpCodeLocation = httpLocation.GET();
  Serial.println(httpLocation.getString());

  if (httpCodeLocation == 200) {
    String payloadLocation = httpLocation.getString();
    DynamicJsonDocument docLocation(1024);
    deserializeJson(docLocation, payloadLocation);
    location = docLocation["loc"].as<String>();
    Serial.println("Location: " + location);
    httpLocation.end();
  } else {
    Serial.println("Failed to fetch location");
  }
}

void loop() {
  if (millis() > delayTime) {
    if (WiFi.status() != WL_CONNECTED) {
      Serial.println("WiFi is not connected");
      WiFi.mode(WIFI_STA);
      WiFi.forceSleepWake();
      connectToWiFi();
    }
    initCurrentTime();
    SunTimes sunTimes = fetchSunriseSunsetTimes(location, "today");
    long sunsetTime = sunTimes.sunset;
    long sunriseTime = sunTimes.sunrise;
    long dimTime = sunTimes.dimTime;
    long lastLight = sunTimes.lastLight;

    if (now() > lastLight) {
      sunTimes = fetchSunriseSunsetTimes(location, "tomorrow");
      sunriseTime = sunTimes.sunrise;
      dimTime = sunTimes.dimTime;
    }

    Serial.println("Sunset: " + getTimeStr(sunsetTime));
    Serial.println("Sunrise: " + getTimeStr(sunriseTime));
    Serial.println("Last Light Time: " + getTimeStr(sunTimes.lastLight));
    Serial.println("Dim Time: " + getTimeStr(dimTime));

    unsigned long nextEventTime = 0;
    bool turnLightOn = false;

    if (now() >= sunsetTime && now() < lastLight) {
      nextEventTime = lastLight; // Next sunrise or tomorrow's sunrise
      turnLightOn = true;
      Serial.println("LED State: ON (Dim)");
      analogWrite(0, 60);
      analogWrite(2, 60);
    } else if (now() >= lastLight && now() < dimTime) {
      nextEventTime = dimTime; // Next sunrise or tomorrow's sunrise
      turnLightOn = true;
      Serial.println("LED State: ON");
      analogWrite(0, 1023);
      analogWrite(2, 1023);
    } else if (now() >= dimTime && now() < sunriseTime) {
      nextEventTime = sunriseTime;
      turnLightOn = false;
      Serial.println("LED State: ON (Dim)");
      analogWrite(0, 60);
      analogWrite(2, 60);
    } else {
      nextEventTime = sunsetTime;
      turnLightOn = false;
      Serial.println("LED State: OFF");
      analogWrite(0, 0);
      analogWrite(2, 0);
    }
    
    Serial.println("Next Event Time: " + getTimeStr(nextEventTime));

    unsigned long sleepDuration = nextEventTime > now() ? nextEventTime - now() : 0;
    Serial.println("Sleep Duration: " + String(sleepDuration) + " seconds");

    WiFi.disconnect();
    WiFi.mode(WIFI_OFF);
    WiFi.forceSleepBegin(sleepDuration * 1000000);
    delayTime = millis() + sleepDuration * 1000;
  } else {
    Serial.println("Sleeping...   Seconds remaining: " + String((delayTime - millis()) / 1000));
    delay(5000);
  }
}

SunTimes fetchSunriseSunsetTimes(String location, String date) {
  SunTimes times = {0, 0};

  String lat = location.substring(0, location.indexOf(','));
  String lng = location.substring(location.indexOf(',') + 1);

  // Fetch sunrise and sunset times
  HTTPClient httpSun;
  WiFiClientSecure client;
  client.setInsecure();
  String url = "https://api.sunrisesunset.io/json?lat=" + lat + "&lng=" + lng + "&date=" + date;
  httpSun.begin(client, url);
  int httpCodeSun = httpSun.GET();

  if (httpCodeSun == 200) {
    String payloadSun = httpSun.getString();
    DynamicJsonDocument docSun(1024);
    deserializeJson(docSun, payloadSun);
    String date = docSun["results"]["date"].as<String>();
    String sunrise = date + " " + docSun["results"]["sunrise"].as<String>();
    String sunset = date + " " + docSun["results"]["sunset"].as<String>();
    String lastLight = date + " " + docSun["results"]["last_light"].as<String>();

    times.sunrise = parseTime(sunrise);
    times.sunset = parseTime(sunset);
    times.lastLight = parseTime(lastLight) - 3600;
    String dimTime = date + " 02:00:00 AM";
    Serial.println("Set Dim Time: " + dimTime);
    times.dimTime = parseTime(dimTime);
    httpSun.end();
  } else {
    Serial.println("Failed to fetch sunrise and sunset times");
  }

  return times;
}

unsigned long parseTime(String time) {
  int hour, minute, second, day, month, year;
  char ampm[3] = "";
  
  // Parse the time string
  sscanf(time.c_str(), "%d-%d-%d %d:%d:%d %s", &year, &month, &day, &hour, &minute, &second, ampm);

  // Adjust the hour for AM/PM
  if (strcmp(ampm, "PM") == 0 && hour < 12) {
    hour += 12;
  } else if (strcmp(ampm, "AM") == 0 && hour == 12) {
    hour = 0;
  }

  // Get current date from the Arduino's internal clock
  tmElements_t tm;
  breakTime(now(), tm); // breakTime converts a time_t to tmElements_t
  
  // Set the parsed time
  tm.Year = year - 1970;
  tm.Month = month;
  tm.Day = day;
  tm.Hour = hour;
  tm.Minute = minute;
  tm.Second = second;

  // Convert back to time_t (Unix time)
  time_t parsedTime = makeTime(tm);

  // Return as unsigned long
  return (unsigned long) parsedTime;
}

void connectToWiFi() {
  WiFi.begin(SECRET_SSID, SECRET_PASS);

  Serial.print("Connecting to WiFi...");
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
    if (WiFi.status() == WL_CONNECT_FAILED) {
      Serial.println("Failed to connect to WiFi");
      return;
    }
  }
  Serial.println();
  Serial.println("Connected!");
}

String getTimeStr(long time) {
  tmElements_t tm;
  breakTime(time, tm);
  return String(tm.Year + 1970) + "-" + String(tm.Month) + "-" + String(tm.Day) + " " + String(tm.Hour) + ":" + String(tm.Minute) + ":" + String(tm.Second);
}

void initCurrentTime() {
  HTTPClient httpTime;
  WiFiClient client;
  httpTime.begin(client, "http://worldtimeapi.org/api/timezone/Europe/Kyiv");

  if (httpTime.GET() == 200) {
    String payloadTime = httpTime.getString();
    DynamicJsonDocument docTime(1024);
    deserializeJson(docTime, payloadTime);
    String datetime = docTime["datetime"].as<String>();
    Serial.println("Date and Time: " + datetime);
    int year = datetime.substring(0, 4).toInt();
    int month = datetime.substring(5, 7).toInt();
    int day = datetime.substring(8, 10).toInt();
    int hour = datetime.substring(11, 13).toInt();
    int minute = datetime.substring(14, 16).toInt();
    int second = datetime.substring(17, 19).toInt();
    setTime(hour, minute, second, day, month, year);
    
    Serial.println("Current Time: " + getTimeStr(now()));
    httpTime.end();
  } else {
    Serial.println("Failed to fetch time");
  }
}