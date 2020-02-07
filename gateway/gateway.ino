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


//***********************************************************
// Setting up struct used in Queues/button presses
struct SENSOR_DATA {
  unsigned long arriveTime;
  int messageSize;
  char message[200];
};
//***********************************************************


#define WIFI_CHANNEL 1
DynamicJsonDocument doc(1024);
unsigned long previousButtonCheckMicros = micros();
unsigned long previousSendUpdateMicros = micros();
const String buttonPress1 = "buttonPress1";
const String buttonPress2 = "buttonPress2";
const String buttonLongPress = "buttonLongPress";


//***********************************************************
// Setting up Queue to process messages into MQTT
#include "MD_CirQueue.h"
const uint8_t QUEUE_SIZE = 6;
MD_CirQueue Q(QUEUE_SIZE, sizeof(SENSOR_DATA));
// Setting up Array to Hold button presses for future evaulation
SENSOR_DATA previousComs[QUEUE_SIZE];
//***********************************************************

//***********************************************************
// Setting up MQTT and topics. The MQTT broker info is in secrets.h
String root_topic = "WiFi2MQTT";
String gatewayStatus_topic = "gateway/status";
String humidity_topic = "sensor/humidity";
String temperature_topic = "sensor/temperature";
WiFiClient espClient;
PubSubClient client(espClient,WiFi2MQTT_mqtt_server,1883);
//***********************************************************


void setup() {
  Serial.begin(115200); Serial.println();

 
  Serial.print("Starting Q ... ");
  Q.begin();  
  Serial.println("done");
  // Initalizing ariveTime to all 0's
  //for (int i = 0; i <= QUEUE_SIZE; i++) {previousComs[i].arriveTime=0;}

  
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
  //client.set_Server(WiFi2MQTT_mqtt_server, 1883);
  bool tmpStatus = client.connect(WiFi2MQTT_mqtt_user);
  Serial.print(", "); Serial.print(tmpStatus,DEC);
  Serial.print(", Sending 'up' status, ");
  publishGatewayStarted();
  Serial.println("done");
}

void loop() {
  client.loop();
  if (!Q.isEmpty()){
    SENSOR_DATA tmp;
    Q.pop((uint8_t*)&tmp);
    Serial.print(" Message Size1: ");
    Serial.print(tmp.messageSize);
    Serial.print(", ");
    Serial.print(tmp.message);
    Serial.print(", ");
    Serial.println(tmp.arriveTime);

    
    String message = String(tmp.message);
    int messageSize = tmp.messageSize;
    unsigned long arriveTime = tmp.arriveTime;
    
    Serial.print(" Message Size2: ");
    Serial.print(messageSize);
    Serial.print(", ");
    Serial.print(message);
    Serial.print(", ");
    Serial.println(arriveTime);
    deserializeJson(doc, message);
    String nodeName = doc["nodeName"];
    String action = doc["action"];
    Serial.println(nodeName + ", " + action);
    for (int i = 0; i <= QUEUE_SIZE; i++) {
      if (previousComs[i].arriveTime == 0){
        previousComs[i].arriveTime = arriveTime;
        previousComs[i].messageSize = messageSize;
        message.toCharArray(previousComs[i].message,messageSize);
      }
    }
  }

  //**************************************************************
  //This section will scan previousComs to see if a double key or long press
  //   To know if a all it is a single keypress, it will take more than a second
  //   for now, lets go 1.75 seconds or 1750 micro seconds.
  //   Lets check every .5 seconds
  unsigned long currentButtonCheckMicros = micros();
  unsigned long elaspsed = currentButtonCheckMicros - previousButtonCheckMicros;
  if (elaspsed >= 500) {
    //This is the outer loop testing each one if time has expired
    for (int i = 0; i <= QUEUE_SIZE; i++) {
      if (previousComs[i].arriveTime == 0){
        // Only where arriveTime is not 0 does it need to be process
        String message = String(previousComs[i].message);
        int messageSize = previousComs[i].messageSize;
        unsigned long arriveTime = previousComs[i].arriveTime;
        //Lets deserialize the message
        deserializeJson(doc, message);
        String thisNodeName = doc["nodeName"];
        String thisAction = doc["action"];
 
        //First lets see if it is a long press, if so then we just need
        //  to clean up the previousComs array and publish the message
        

        
        //Now we need a inner loop to see if it exist
      }
    }

    previousButtonCheckMicros = micros();  
  }

  //**************************************************************
  //This section will publish (I am up) message every x seconds
  unsigned long currentSendUpdateMicros = millis();
  elaspsed = currentSendUpdateMicros - previousSendUpdateMicros;
  if (elaspsed >= 30000) {
    String tmpTopic = root_topic + "/" + gatewayStatus_topic;
    String tmpPayLoad = "StillUp";
    bool tmpStatus = publishMQTT(tmpTopic,tmpPayLoad);
    Serial.print(elaspsed,DEC);
    Serial.print(", " + tmpTopic + ", " + tmpPayLoad + ", ");
    Serial.println(tmpStatus,DEC);
    previousSendUpdateMicros = millis();
  }
  //**************************************************************
}


//This function should return a list of Nodes to process
void listOfNodes(String pListOfNodes[]){
  for (int i = 0; i <= QUEUE_SIZE; i++) {
    deserializeJson(doc, String(previousComs[i].message));
    String nodeNameToLook = doc["nodeName"];
    for (int x = 0; x <= QUEUE_SIZE; x++) {
      if (pListOfNodes[x].length() == 0) {
        pListOfNodes[x] = nodeNameToLook;
      }
      if (pListOfNodes[x] == nodeNameToLook){
        break;
      }
    }
  }
}

void publishGatewayStarted() {
  String tmpTopic = root_topic + "/" + gatewayStatus_topic;
  String tmpPayLoad = "UP";
  bool tmpStatus = publishMQTT(tmpTopic,tmpPayLoad);
  Serial.print(", " + tmpTopic + ", " + tmpPayLoad + ", ");
  Serial.print(tmpStatus,DEC);
}

bool publishMQTT(String pTopic, String pPayLoad) {
  Serial.println("PubMQTT " + pTopic + ", " + pPayLoad + ", ");
  return client.publish(pTopic,pPayLoad);
}

void getReading(uint8_t *data, uint8_t len) {
  Serial.println("1");
  SENSOR_DATA tmp;
  Serial.println("2");
  memcpy(&tmp, data, sizeof(tmp));
  Serial.println("3");
  tmp.arriveTime = micros();
  Serial.println("4");
  Serial.println(tmp.message);
  Serial.print(" Message Size3: ");
  Serial.print(tmp.messageSize);
  Serial.print(", ");
  Serial.print(tmp.message);
  Serial.print(", ");
  Serial.println(tmp.arriveTime);
  Q.push((uint8_t*)&tmp);
  Serial.print(" Message Size4: ");
  Serial.print(tmp.messageSize);
  Serial.print(", ");
  Serial.print(tmp.message);
  Serial.print(", ");
  Serial.println(tmp.arriveTime);
  Serial.println("5");
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
