/*
 MAC: C8:2B:96:08:2E:0A
 Although there are many to say thanks to, Ant Elder code is what inspired the technique.
 Using the MD_CirQueue by Marco Colli
     https://github.com/MajicDesigns/MD_CirQueue
 
 Required libraries (sketch -> include library -> manage libraries)
   - PubSubClient by Nick ‘O Leary     
*/

#include <ESP8266WiFi.h>
#include "secrets.h"
#include <espnow.h>
#include <PubSubClient.h>
#include <ESP8266HTTPClient.h>
#include <ArduinoJson.h>


#define WIFI_CHANNEL 1

struct SENSOR_DATA {
    float temp;
    float hum;
    float t;
    float pressure;
};


//***********************************************************
// Setting up Queue to process messages into MQTT
#include "MD_CirQueue.h"
const uint8_t QUEUE_SIZE = 6;
MD_CirQueue Q(QUEUE_SIZE, sizeof(SENSOR_DATA));
//***********************************************************

//***********************************************************
// Setting up MQTT and topics. The MQTT broker info is in secrets.h
String root_topic = "WiFi2MQTT";
String gatewayStatus_topic = "gateway/status";
String humidity_topic = "sensor/humidity";
String temperature_topic = "sensor/temperature";
WiFiClient espClient;
PubSubClient client(espClient);
//***********************************************************


void setup() {
  Serial.begin(115200); Serial.println();

 
  Serial.print("Starting Q ... ");
  Q.begin();  
  Serial.println("done");

  
  initWifi();
  if (esp_now_init()!=0) {
    Serial.println("*** ESP_Now init failed");
    ESP.restart();
  }
  Serial.setDebugOutput(true);
  WiFi.printDiag(Serial);
  Serial.setDebugOutput(false);
  Serial.print("This node AP mac: "); Serial.print(WiFi.softAPmacAddress());
  Serial.print(", STA mac: "); Serial.println(WiFi.macAddress());

//***********************************************************
//Test JSON

  if (WiFi.status() == WL_CONNECTED) {
    HTTPClient http;  //Object of class HTTPClient
    http.begin("http://jsonplaceholder.typicode.com/users/1");
    int httpCode = http.GET();
    //Check the returning code                                                                  
    if (httpCode > 0) {
      // Get the request response payload
      String payload = http.getString();
      // TODO: Parsing
      Serial.println(payload);
    }
    http.end();   //Close connection
  }
//***********************************************************




  // Note: When ESP8266 is in soft-AP+station mode, this will communicate through station interface
  // if it is in slave role, and communicate through soft-AP interface if it is in controller role,
  // so you need to make sure the remote nodes use the correct MAC address being used by this gateway. 
  esp_now_set_self_role(ESP_NOW_ROLE_SLAVE);

  esp_now_register_recv_cb([](uint8_t *mac, uint8_t *data, uint8_t len) {
    Serial.print("recv_cb, from mac: ");
    char macString[50] = {0};
    sprintf(macString, "%02X:%02X:%02X:%02X:%02X:%02X", mac[0], mac[1], mac[2], mac[3], mac[4], mac[5]);
    Serial.print(macString);

    getReading(data, len);
  });
  Serial.print("Setting up MQTT server");
  client.set_server(WiFi2MQTT_mqtt_server, 1883);
  bool tmpStatus = client.connect(WiFi2MQTT_mqtt_user);
  Serial.print(", "); Serial.print(tmpStatus,DEC);
  Serial.print(", Sending 'up' status, ");
  publishGatewayStarted();
  Serial.println("done");
}

void loop() {
  if (!Q.isEmpty()){
    SENSOR_DATA tmp;
    Q.pop((uint8_t*)&tmp);
    Serial.print(" Temp: ");
    Serial.println(tmp.t);
    delay(500);
  }
}

void publishGatewayStarted() {
  String tmpTopic = root_topic + "/" + gatewayStatus_topic;
  String tmpPayLoad = "UPtest";
  bool tmpStatus = publishMQTT(tmpTopic,tmpPayLoad);
  Serial.print(", " + tmpTopic + ", " + tmpPayLoad + ", ");
  Serial.print(tmpStatus,DEC);
}
bool publishMQTT(String pTopic, String pPayLoad) {
  return client.publish(pTopic,pPayLoad);
  //Serial.println(tmpTopic + ", " + tmpPayLoad + ", ");
}
void getReading(uint8_t *data, uint8_t len) {
  SENSOR_DATA tmp;
  memcpy(&tmp, data, sizeof(tmp));
  Serial.print("push data start, ");
  Q.push((uint8_t*)&tmp);
  Serial.print(tmp.t);
  Serial.println(", done");
}

void initWifi() {

  WiFi.mode(WIFI_AP_STA);
  WiFi.softAP("MyGateway", "12345678", WIFI_CHANNEL, 1);

  Serial.print("Connecting to ");
  Serial.print(WiFi2MQTT_ssid);
  Serial.print(" ");
  Serial.print(WiFi.SSID());
  Serial.print(" ");
  WiFi.begin(WiFi2MQTT_ssid, WiFi2MQTT_password);
 
  int retries = 20; // 10 seconds
  while ((WiFi.status() != WL_CONNECTED) && (retries-- > 0)) {
     Serial.print(WiFi.status());
     delay(500);
     Serial.print(".");
  }
  Serial.println("");
  if (retries < 1) {
     Serial.print("*** WiFi connection failed");  
     ESP.restart();
  }

  Serial.print("WiFi connected, IP address: "); Serial.println(WiFi.localIP());
}
