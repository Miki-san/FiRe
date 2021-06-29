#include <WiFiEsp.h>
#include <WiFiEspClient.h>
#include <WiFiEspServer.h>
#include <Thread.h>
#include <MQ135.h>
#include <MQ2.h>
#include <MQ7.h>
#include <MQ9.h>

#define WIFI_SERIAL    Serial1

#define DOWN_MIDDLE_LEVEL_CO2 = 800
#define UP_MIDDLE_LEVEL_CO2 = 5000
#define CRITICAL_LEVEL_CO2 = 15000
#define DOWN_MIDDLE_LEVEL_CO = 200
#define UP_MIDDLE_LEVEL_CO = 800
#define CRITICAL_LEVEL_CO = 1000

#define PIN_MQ2         A0
#define PIN_MQ2_HEATER  13

#define PIN_MQ7         A1
#define PIN_MQ7_HEATER  12

#define PIN_MQ135         A2
#define PIN_MQ135_HEATER  11

#define PIN_MQ9         A3
#define PIN_MQ9_HEATER  10

#define piezoPin         3

char ssid[] = "Miki_san";
char pass[] = "angmihmax";
short status = WL_IDLE_STATUS;

MQ2 mq2(PIN_MQ2, PIN_MQ2_HEATER);
MQ7 mq7(PIN_MQ7, PIN_MQ7_HEATER);
MQ9 mq9(PIN_MQ9, PIN_MQ9_HEATER);
MQ135 mq135(PIN_MQ135, PIN_MQ135_HEATER);



short smoke[100];
short smokeIterator = 0;
short CO_1[50];
short COIterator1 = 0;
short CO_2[50];
short COIterator2 = 0;
short CO2[100];
short CO2Iterator = 0;
bool heat = true;
bool sensorFlag = true;
bool calibratingStatus = false;

Thread MQ2dataThread = Thread();
Thread MQ7dataThread = Thread();
Thread MQ9dataThread = Thread();
Thread MQ135dataThread = Thread();

void printWifiData() {
  IPAddress ip = WiFi.localIP();
  Serial.print("IP Address: ");
  Serial.println(ip);

  byte mac[6];
  WiFi.macAddress(mac);
  char buf[20];
  sprintf(buf, "%02X:%02X:%02X:%02X:%02X:%02X", mac[5], mac[4], mac[3], mac[2], mac[1], mac[0]);
  Serial.print("MAC address: ");
  Serial.println(buf);
}

void printCurrentNet() {
  Serial.print("SSID: ");
  Serial.println(WiFi.SSID());

  byte bssid[6];
  WiFi.BSSID(bssid);
  char buf[20];
  sprintf(buf, "%02X:%02X:%02X:%02X:%02X:%02X", bssid[5], bssid[4], bssid[3], bssid[2], bssid[1], bssid[0]);
  Serial.print("BSSID: ");
  Serial.println(buf);

  long rssi = WiFi.RSSI();
  Serial.print("Signal strength (RSSI): ");
  Serial.println(rssi);
}

void getMQ2smoke() {
  if (mq2.isCalibrated() && mq2.atHeatCycleEnd()) {
    smoke[smokeIterator] = mq2.readSmoke();
    smokeIterator = (smokeIterator + 1) % 100;
    mq2.cycleHeat();
  }
}

void getMQ7CO() {
  if (mq7.isCalibrated() && mq7.atHeatCycleEnd()) {
    CO_1[COIterator1] = mq7.readCarbonMonoxide();
    COIterator1 = (COIterator1 + 1) % 50;
    mq7.cycleHeat();
  }
}

void getMQ9CO() {
  if (mq9.isCalibrated() && mq9.atHeatCycleEnd()) {
    CO_2[COIterator2] = mq9.readCarbonMonoxide();
    COIterator2 = (COIterator2 + 1) % 50;
    mq9.cycleHeat();
  }
}

void getMQ135CO2() {
  if (mq135.isCalibrated() && mq135.atHeatCycleEnd()) {
    CO2[CO2Iterator] = mq135.readCO2();
    CO2Iterator = (CO2Iterator + 1) % 100;
    mq135.cycleHeat();
  }
}

void wifiSetup() {
  WiFi.init(&WIFI_SERIAL);
  Serial.print("Serial init OK\r\n");
  if (WiFi.status() == WL_NO_SHIELD) {
    Serial.println("WiFi shield not present");
    while (true);
  }
  delay(100);

  while ( status != WL_CONNECTED) {
    Serial.print("Attempting to connect to WPA SSID: ");
    Serial.println(ssid);
    status = WiFi.begin(ssid, pass);
  }

  Serial.println("");
  Serial.println("WiFi connected");
  Serial.println("IP address: ");
  Serial.println(WiFi.localIP());

  printCurrentNet();
  printWifiData();
  Serial.println();
  Serial.println("WIFI setup completed!");
  Serial.println();
}

void heatingSensors() {
  mq2.cycleHeat();
  mq7.cycleHeat();
  mq9.cycleHeat();
  mq135.cycleHeat();
  heat = false;
  Serial.println("Heating sensors!");
  Serial.println();
}

bool sensorsCalibrating() {
  if (mq2.isCalibrated() && mq7.isCalibrated() && mq9.isCalibrated() && mq135.isCalibrated()) {
    Serial.println();
    Serial.println("Sensors calibrated!");
    Serial.println();
    heatingSensors();
    return true;
  }
  if (!mq2.isCalibrated() && mq2.atHeatCycleEnd()) {
    mq2.calibrate();
    Serial.println("MQ2 calibrated");
    Serial.print("Ro = ");
    Serial.println(mq2.getRo());
  }
  if (!mq7.isCalibrated() && mq7.atHeatCycleEnd()) {
    mq7.calibrate();
    Serial.println("MQ7 calibrated");
    Serial.print("Ro = ");
    Serial.println(mq7.getRo());
  }
  if (!mq9.isCalibrated() && mq9.atHeatCycleEnd()) {
    mq9.calibrate();
    Serial.println("MQ9 calibrated");
    Serial.print("Ro = ");
    Serial.println(mq9.getRo());
  }
  if (!mq135.isCalibrated() && mq135.atHeatCycleEnd()) {
    mq135.calibrate();
    Serial.println("MQ135 calibrated");
    Serial.print("Ro = ");
    Serial.println(mq135.getRo());
  }
  return false;
}

void sensorThreadSetup() {
  MQ2dataThread.onRun(getMQ2smoke);
  MQ2dataThread.setInterval(100);
  MQ7dataThread.onRun(getMQ7CO);
  MQ7dataThread.setInterval(100);
  MQ9dataThread.onRun(getMQ9CO);
  MQ9dataThread.setInterval(100);
  MQ135dataThread.onRun(getMQ135CO2);
  MQ135dataThread.setInterval(100);
  Serial.println("Sensors reading threads ready!");
  Serial.println();
}

void sensorThreadRun() {
  if (MQ2dataThread.shouldRun()) {
    MQ2dataThread.run();
  }
  if (MQ7dataThread.shouldRun()) {
    MQ7dataThread.run();
  }
  if (MQ9dataThread.shouldRun()) {
    MQ9dataThread.run();
  }
  if (MQ135dataThread.shouldRun()) {
    MQ135dataThread.run();
  }
}

void dataPrint(int avgSmoke, int avgCO, int avgCO2) {
  Serial.print("Smoke: ");
  Serial.print((float)avgSmoke / 100.0);
  Serial.print(" ppm ");
  Serial.println();
  Serial.print("CO: ");
  Serial.print((float)avgCO / 100.0);
  Serial.print(" ppm ");
  Serial.println();
  Serial.print("CO2: ");
  Serial.print((float)avgCO2 / 100.0);
  Serial.print(" ppm ");
  Serial.println();
}

void setup() {
  Serial.begin(9600);
  WIFI_SERIAL.begin(9600);
  while (status != WL_CONNECTED) {
    wifiSetup();
  }
}

void loop() {


  if (status == WL_CONNECTED) {
    if (calibratingStatus) {
      short avgSmoke = 0;
      short avgCO = 0;
      short avgCO2 = 0;

      if (sensorFlag && (smoke[99] != 0) && (CO_1[49] != 0) && (CO_2[49] != 0) && (CO2[99] != 0)) {
        sensorFlag = !sensorFlag;
      }
      sensorThreadRun();

      for (short i = 0; i < 100; i++) {
        avgSmoke += smoke[i];
        avgCO2 += CO2[i];
        if (i < 50) {
          avgCO += CO_1[i] + CO_2[i];
        }
      }

      if (!sensorFlag) {
        dataPrint(avgSmoke, avgCO, avgCO2);
      }
    } else {
      if (heat) {
        heatingSensors();
      }
      while (calibratingStatus == false) {
        calibratingStatus = sensorsCalibrating();
        delay(500);
      }
      Serial.println("Setup completed!");
      Serial.println("___________________________");
      Serial.println();
      tone(piezoPin, 2000, 500);
      sensorThreadSetup();
    }
  }
}
