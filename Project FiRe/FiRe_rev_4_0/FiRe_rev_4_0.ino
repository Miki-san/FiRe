#include <TroykaDHT.h>
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

char ssid[] = "Miki_san";
char pass[] = "angmihmax";
int status = WL_IDLE_STATUS;
const char *mqtt_server = "192.168.31.125";
const char *clientId = "ESP32Client";
const char *mqtt_user = "miki";
const char *mqtt_pass = "091101miki";
char msg[20];
double avgSmoke = 0;
double avgCO = 0;
double avgCO2 = 0;
double avgTemp = 0;
const int numOfData = 50;
double smoke[numOfData];
double co_1[numOfData / 2];
double co_2[numOfData / 2];
double co2[numOfData];
double temp[numOfData];
int smokeIterator = 0;
int COIterator1 = 0;
int COIterator2 = 0;
int CO2Iterator = 0;
int TempIterator = 0;
double standartSmoke = 0;
double standartCO_1 = 0;
double standartCO_2 = 0;
double standartCO2 = 0;
double standartTemp = 0;
bool calibratingStatus = false;

DHT dht(4, DHT11);
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
Thread TempdataThread = Thread();

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

void wifiConnect() {
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
}

void wifiSetup() {
  wifiConnect();
  printCurrentNet();
  printWifiData();
  Serial.println();
  Serial.println("WIFI setup completed!");
  Serial.println();
}

void receivedCallback(char* topic, byte* payload, unsigned int length) {
  Serial.print("Message received: ");
  Serial.println(topic);
  Serial.print("payload: ");
  for (int i = 0; i < length; i++) {
    Serial.print((char)payload[i]);
  }
  Serial.println();
}

void mqttConnect() {
  while (!client.connected()) {
    Serial.print("MQTT connecting ...");
    if (client.connect(clientId, mqtt_user, mqtt_pass)) {
      Serial.println("Connected");
    } else {
      Serial.print("Failed, status code =");
      Serial.print(client.state());
      Serial.println("Try again in 5 seconds");
      delay(5000);
    }
  }
}

void mqttSetup() {
  client.setKeepAlive(60);
  client.setSocketTimeout(60);
  client.setServer(mqtt_server, PORT);
  client.setCallback(receivedCallback);
  mqttConnect();
  Serial.println("MQTT setup completed!");
  Serial.println();
}

void heatingSensors() {
  mq135.heaterPwrHigh();
  mq2.heaterPwrHigh();
  mq7.heaterPwrHigh();
  mq9.heaterPwrHigh();
  Serial.println("Heating sensors!");
  Serial.println();
}

bool sensorsCalibrating() {
  if (mq2.isCalibrated() && mq7.isCalibrated() && mq9.isCalibrated() && mq135.isCalibrated()) {
    Serial.println();
    Serial.println("Sensors calibrated!");
    Serial.println();
    return true;
  }
  if (!mq2.isCalibrated() && mq2.heatingCompleted()) {
    mq2.calibrate();
    Serial.println("MQ2 calibrated");
    Serial.print("Ro = ");
    Serial.println(mq2.getRo());
  }
  if (!mq7.isCalibrated() && mq7.heatingCompleted()) {
    mq7.calibrate();
    Serial.println("MQ7 calibrated");
    Serial.print("Ro = ");
    Serial.println(mq7.getRo());
  }
  if (!mq9.isCalibrated() && mq9.heatingCompleted()) {
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

double getMQ2Smoke() {
  if (mq2.isCalibrated() && mq2.heatingCompleted()) {
    return mq2.readSmoke();
  }
  return 0;
}

void writeSmoke() {
  double temp = getMQ2Smoke();
  if (temp != 0) {
    smoke[smokeIterator] = temp;
    smokeIterator = (smokeIterator + 1) % numOfData;
  }
}

double getMQ7CO() {
  if (mq7.isCalibrated() && mq7.heatingCompleted()) {
    return mq7.readCarbonMonoxide();
  }
  return 0;
}

void writeCO_1() {
  double temp  = getMQ7CO();
  if (temp != 0) {
    co_1[COIterator1] = temp;
    COIterator1 = (COIterator1 + 1) % (numOfData / 2);
  }
}

double getMQ9CO() {
  if (mq9.isCalibrated() && mq9.heatingCompleted()) {
    return mq9.readCarbonMonoxide();
  }
  return 0;
}

void writeCO_2() {
  double temp = getMQ9CO();
  if (temp != 0) {
    co_2[COIterator2] = temp;
    COIterator2 = (COIterator2 + 1) % (numOfData / 2);
  }
}

double getMQ135CO2() {
  if (mq135.isCalibrated() && mq135.heatingCompleted()) {
    return mq135.readCO2();
  }
  return 0;
}

void writeCO2() {
  double temp = getMQ135CO2();
  if (temp != 0) {
    co2[CO2Iterator] = temp;
    CO2Iterator = (CO2Iterator + 1) % numOfData;
  }
}

double getDHTTemp() {
  dht.read();
  switch (dht.getState()) {
    case DHT_OK:
      return dht.getTemperatureC();
      break;
    case DHT_ERROR_CHECKSUM:
      Serial.println("Checksum error");
      break;
    case DHT_ERROR_TIMEOUT:
      Serial.println("Time out error");
      break;
    case DHT_ERROR_NO_REPLY:
      Serial.println("Sensor not connected");
      break;
  }
  return 0;
}

void writeTemp() {
  double temporary = getDHTTemp();
  if (temp != 0) {
    temp[TempIterator] = temporary;
    TempIterator = (TempIterator + 1) % numOfData;
  }
}

void getStandart() {
  while ((standartTemp == 0) or (standartCO_1 == 0) or (standartCO_2 == 0) or (standartCO2 == 0)) {
    standartSmoke = getMQ2Smoke();
    standartCO_1 = getMQ7CO();
    standartCO_2 = getMQ9CO();
    standartCO2 = getMQ135CO2();
    standartTemp = getDHTTemp();
    delay(100);
  }
  Serial.print("standartTemp: ");
  Serial.println(standartTemp);
  Serial.print("standartCO_1: ");
  Serial.println(standartCO_1);
  Serial.print("standartCO_2: ");
  Serial.println(standartCO_2);
  Serial.print("standartCO2: ");
  Serial.println(standartCO2);
  Serial.println();
}

void setStandartArray() {
  for (int i = 0; i < numOfData; i++) {
    co2[i] = standartCO2;
    smoke[i] = standartSmoke;
    temp[i] = standartTemp;
    if (i < numOfData / 2) {
      co_1[i] = standartCO_1;
      co_2[i] = standartCO_2;
    }
  }
}

void sensorSetup() {
  heatingSensors();
  delay(20000);
  while (calibratingStatus == false) {
    calibratingStatus = sensorsCalibrating();
    delay(100);
  }
  getStandart();
  setStandartArray();
  Serial.println("Sensors setup completed!");
  Serial.println();
}

void sensorThreadSetup() {
  MQ2dataThread.onRun(writeSmoke);
  MQ2dataThread.setInterval(100);
  MQ7dataThread.onRun(writeCO_1);
  MQ7dataThread.setInterval(100);
  MQ9dataThread.onRun(writeCO_2);
  MQ9dataThread.setInterval(100);
  MQ135dataThread.onRun(writeCO2);
  MQ135dataThread.setInterval(100);
  TempdataThread.onRun(writeTemp);
  TempdataThread.setInterval(100);
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
  if (TempdataThread.shouldRun()) {
    TempdataThread.run();
  }
}

void getAverage() {
  for (int i = 0; i < numOfData; i++) {
    avgSmoke += smoke[i];
    avgCO2 += co2[i];
    avgTemp += temp[i];
    if (i < numOfData / 2) {
      avgCO += co_1[i] + co_2[i];
    }
  }
  avgSmoke /= (double)numOfData;
  avgCO /= (double)numOfData;
  avgCO2 /= (double)numOfData;
  avgTemp /= (double)numOfData;
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
  Serial.print("Temperature = ");
  Serial.print(avgTemp);
  Serial.println(" °C \t");
}

void dataSend() {
  strcpy(msg, ((String)avgTemp).c_str());
  client.publish(TEMP_TOPIC, msg);
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

void zeroingVars() {
  avgSmoke = 0;
  avgCO = 0;
  avgCO2 = 0;
  avgTemp = 0;
}

void setup() {
  Serial.begin(9600);
  WIFI_SERIAL.begin(9600);
  wifiSetup();
  mqttSetup();
  sensorSetup();
  sensorThreadSetup();
  Serial.println("Setup completed!");
  Serial.println("___________________________");
  Serial.println();
  tone(piezoPin, 2000, 500);
}

void loop() {
  client.loop();
  if (status == WL_CONNECTED) {
    if (client.connected()) {
      if (calibratingStatus) {
        sensorThreadRun();
        getAverage();
        dataPrint();
        dataSend();
        zeroingVars();
      } else {
        calibratingStatus = sensorsCalibrating();
      }
    } else {
      mqttConnect();
    }
  } else {
    wifiConnect();
  }
}
