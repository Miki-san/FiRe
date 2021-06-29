#include <PubSubClient.h>
#include <WiFiEsp.h>
#include <WiFiEspClient.h>
#include <WiFiEspServer.h>
#include <Thread.h>
#include <MQ135.h>
#include <MQ2.h>
#include <MQ7.h>
#include <MQ9.h>

#define WIFI_SERIAL            Serial1
#define DOWN_MIDDLE_LEVEL_CO2  800
#define UP_MIDDLE_LEVEL_CO2    5000
#define CRITICAL_LEVEL_CO2     15000
#define DOWN_MIDDLE_LEVEL_CO   200
#define UP_MIDDLE_LEVEL_CO     800
#define CRITICAL_LEVEL_CO      1000
#define PIN_MQ2                A0
#define PIN_MQ7                A1
#define PIN_MQ135              A2
#define PIN_MQ9                A3
#define PIN_MQ2_HEATER         13
#define PIN_MQ7_HEATER         12
#define PIN_MQ135_HEATER       11
#define PIN_MQ9_HEATER         10
#define piezoPin               3
#define PORT                   1883
#define TEMP_TOPIC             "home/temp"
#define CO_TOPIC               "home/co"
#define CO2_TOPIC              "home/co2"
#define SMOKE_TOPIC            "home/smoke"
#define CONCLUSION_TOPIC       "home/conclusion"

unsigned long timing;
char ssid[] = "Miki_san";
char pass[] = "angmihmax";
int status = WL_IDLE_STATUS;
const char *mqtt_server = "192.168.31.125";
const char *clientId = "ESP32Client";
const char *mqtt_user = "miki";
const char *mqtt_pass = "091101miki";
char msg[20];
float avgSmoke = 0;
float avgCO = 0;
float avgCO2 = 0;
int smoke[100];
int CO_1[50];
int CO_2[50];
int CO2[100];
int smokeIterator = 0;
int COIterator1 = 0;
int COIterator2 = 0;
int CO2Iterator = 0;
bool heat = true;
bool sensorFlag = true;
bool calibratingStatus = false;

MQ2 mq2(PIN_MQ2, PIN_MQ2_HEATER);
MQ7 mq7(PIN_MQ7, PIN_MQ7_HEATER);
MQ9 mq9(PIN_MQ9, PIN_MQ9_HEATER);
MQ135 mq135(PIN_MQ135, PIN_MQ135_HEATER);
WiFiEspClient espClient;
PubSubClient client(espClient);
Thread MQ2dataThread = Thread();
Thread MQ7dataThread = Thread();
Thread MQ9dataThread = Thread();
Thread MQ135dataThread = Thread();

void receivedCallback(char* topic, byte* payload, unsigned int length) {
  Serial.print("Message received: ");
  Serial.println(topic);
  Serial.print("payload: ");
  for (int i = 0; i < length; i++) {
    Serial.print((char)payload[i]);
  }
  Serial.println();
}

void mqttconnect() {
  while (!client.connected()) {
    Serial.print("MQTT connecting ...");
    if (client.connect(clientId, mqtt_user, mqtt_pass)) {
      Serial.println("connected");
    } else {
      Serial.print("failed, status code =");
      Serial.print(client.state());
      Serial.println("try again in 5 seconds");
      delay(5000);
    }
  }
}

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
  if (mq2.isCalibrated() && mq2.heatingCompleted()) {
    smoke[smokeIterator] = mq2.readSmoke();
    smokeIterator = (smokeIterator + 1) % 100;
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
  if (mq135.isCalibrated() && mq135.heatingCompleted()) {
    CO2[CO2Iterator] = mq135.readCO2();
    CO2Iterator = (CO2Iterator + 1) % 100;
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
  client.setServer(mqtt_server, PORT);
  client.setCallback(receivedCallback);
  Serial.println("MQTT setup completed!");
  Serial.println();
}

void heatingSensors() {
  mq135.heaterPwrHigh();
  mq2.heaterPwrHigh();
  mq7.cycleHeat();
  mq9.cycleHeat();
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
  if (!mq2.isCalibrated() && mq2.heatingCompleted()) {
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
  if (!mq135.isCalibrated() && mq135.heatingCompleted()) {
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

void dataPrint() {
  Serial.println();
  Serial.print("Smoke: ");
  Serial.print(avgSmoke);
  Serial.print(" ppm ");
  Serial.println();
  Serial.print("CO: ");
  Serial.print(avgCO);
  Serial.print(" ppm ");
  Serial.println();
  Serial.print("CO2: ");
  Serial.print(avgCO2);
  Serial.print(" ppm ");
  Serial.println();
}

void dataSend() {
  //strcpy(msg, );
  //client.publish(TEMP_TOPIC, msg);
  strcpy(msg, ((String)avgCO).c_str());
  client.publish(CO_TOPIC, msg);
    delay(50);
  strcpy(msg, ((String)avgCO2).c_str());
  client.publish(CO2_TOPIC, msg);
    delay(50);
  strcpy(msg, ((String)avgSmoke).c_str());
  client.publish(SMOKE_TOPIC, msg);
      delay(50);
  //strcpy(msg, );
  //client.publish(CONCLUSION_TOPIC, msg);
  delay(200);
}

void setup() {
  Serial.begin(9600);
  WIFI_SERIAL.begin(9600);
  Serial.println(millis());
  client.setKeepAlive(60);
  client.setSocketTimeout(60);
  for (int i = 0; i < 100; i++) {
    CO2[i] = 200;
    smoke[i] = 15;
    if (i < 50) {
      CO_1[i] = 15;
      CO_2[i] = 15;
    }
    if (i < 20) {
      msg[i] = " ";
    }
  }
  while (status != WL_CONNECTED) {
    wifiSetup();
  }
}

void loop() {
  Serial.println(millis());
  client.loop();
  if (!client.connected()) {
    mqttconnect();
  } else {
    if (status == WL_CONNECTED) {
      if (calibratingStatus) {
        if (sensorFlag && (smoke[99] != 0) && (CO_1[49] != 0) && (CO_2[49] != 0) && (CO2[99] != 0)) {
          sensorFlag = !sensorFlag;
        }
        sensorThreadRun();
        for (int i = 0; i < 100; i++) {
          avgSmoke += smoke[i];
          avgCO2 += CO2[i];
          if (i < 50) {
            avgCO += CO_1[i] + CO_2[i];
          }
        }
        avgSmoke /= 100.0;
        avgCO /= 100.0;
        avgCO2 /= 100.0;
        if (!sensorFlag) {
          dataPrint();
          dataSend();
        }
        avgSmoke = 0;
        avgCO = 0;
        avgCO2 = 0;
      } else {
        if (heat) {
          heatingSensors();
          delay(20000);
        }
        while (calibratingStatus == false) {
          calibratingStatus = sensorsCalibrating();
          delay(500);
        }
                sensorThreadSetup();
        Serial.println("Setup completed!");
        Serial.println("___________________________");
        Serial.println();
        tone(piezoPin, 2000, 500);
      }
    }
  }
}
