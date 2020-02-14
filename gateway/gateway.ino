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
// Because of Structure Packing, the elements of this struct
//  must be in this order to work. And match the gateway/node
struct SENSOR_DATA {
  int messageSize;
  char message[175];
  unsigned long arriveTime;
};
//***********************************************************


#define WIFI_CHANNEL 1
DynamicJsonDocument doc(1024);
unsigned long previousButtonCheckMillis = millis();
unsigned long previousSendUpdateMillis = millis();
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
String buttonPress_topic = "action/buttonPress";
void callback(char* pTopic, byte* pPayload, unsigned int pLength) {
  String topic = String(pTopic);
  char tmpPayload[pLength];
  memcpy(&tmpPayload,pPayload,pLength);
  String payload = String(tmpPayload);
  Serial.println("R mesg: " + topic + ", " + payload);
}
WiFiClient espClient;
PubSubClient client(WiFi2MQTT_mqtt_server,1883,callback,espClient);
//***********************************************************


void setup() {
  Serial.begin(115200); Serial.println();

 
  Serial.print("Starting Q ... ");
  Q.begin();  
  Serial.println("done");
  // Initalizing ariveTime to all 0's
  for (int i = 0; i <= QUEUE_SIZE; i++) {previousComs[i].arriveTime=0;}

  
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
  bool tmpStatus = client.connect("WiFi2MQTT",WiFi2MQTT_mqtt_user,WiFi2MQTT_mqtt_password);
  Serial.print(", "); Serial.print(tmpStatus,DEC);
  Serial.print(", Sending 'up' status, ");
  publishGatewayStatus("Started","Up");
  Serial.println("done");
}

void loop() {
  client.loop();
  //*******************************************************************************
  //Any messages in the Q needs to be process
  if (!Q.isEmpty()){
    SENSOR_DATA tmp;
    Q.pop((uint8_t*)&tmp);
    Serial.print(" Message Size1: ");
    Serial.print(tmp.messageSize);
    Serial.print(String(tmp.message));
    Serial.println(tmp.arriveTime);

    String message = String(tmp.message);
    int messageSize = tmp.messageSize;
    unsigned long arriveTime = tmp.arriveTime;
    deserializeJson(doc, message);
    String nodeName = doc["nodeName"];
    String action = doc["action"];
    Serial.println(nodeName + ", " + action);
    String action2Add = action;  //This will get changed to button 2 press if it is a button 1 and there is
                                 //  a button 1 press found. Other wise add it as is
    Serial.println("1 message: " + message + " nodeName: " + nodeName + " action: " + action + " action2Add: " + action2Add);
    if (action == buttonPress1) { // Is this the first or second button press for this node
      Serial.println("Button Press 1 ");
      // Check to see if a button 1 press exist for this node and change action2Add
      for (int i = 0; i < QUEUE_SIZE; i++) { //Start of search for button 1 press
        message = String(previousComs[i].message);
        deserializeJson(doc, message);
        String tmpNodeName = doc["nodeName"];
        String tmpAction = doc["action"];
        Serial.print("On loop: "); 
        Serial.print(i);
        Serial.print(", " + message + ", "); 
        Serial.print(QUEUE_SIZE);
        Serial.print(", " + tmpNodeName + ", " + nodeName + String(previousComs[i].arriveTime));
        if ((previousComs[i].arriveTime != 0) &&
             (nodeName == tmpNodeName) &&
             (tmpAction == buttonPress1)){
            Serial.print(" This is a second button push ");
            action2Add = buttonPress2;
            break;
        }
        Serial.println();
      } //End of search for buttonPress1
    }
    //Build message with corrected action2Add
    Serial.println("2 message: " + message + " nodeName: " + nodeName + " action: " + action + " action2Add: " + action2Add);
    String tmpMessage = "{\"nodeName\":\""+nodeName+"\",\"action\":\""+action2Add+"\"}";
    Serial.println("The tmpMessage: " + tmpMessage);
    // Add to Message 
    for (int i = 0; i < QUEUE_SIZE; i++) { //Search for next empty slot
      if (previousComs[i].arriveTime == 0){
        previousComs[i].arriveTime = arriveTime;
        previousComs[i].messageSize = tmpMessage.length()+1;
        tmpMessage.toCharArray(previousComs[i].message,tmpMessage.length()+1);
        break;
      }
    } // End for Search of empty slot
    Serial.println("Dump previousComs[] ");
    for (int x = 0; x < QUEUE_SIZE; x++) {
      Serial.print(String(x));
      Serial.print(", " + String(previousComs[x].arriveTime));
      Serial.println(", " + String(previousComs[x].message));
    }

  }
  //*******************************************************************************

  //*******************************************************************************
  //This section will scan previousComs to see if a double key or long press
  //   To know if a all it is a single keypress, it will take more than a second
  //   for now, lets go 1.75 seconds or 1750 micro seconds.
  //   Lets check every .5 seconds
  unsigned long currentButtonCheckMillis = millis();
  unsigned long elaspsed = currentButtonCheckMillis - previousButtonCheckMillis;
  if (elaspsed > 500) {
    //This is the outer loop testing each one if time has expired
    for (int i = 0; i < QUEUE_SIZE; i++) {
      bool sendMessageAndRemove = false;
      if (previousComs[i].arriveTime != 0){
        Serial.println("Record to process "+String(i)+", "+String(previousComs[i].message));
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
        Serial.println("3 message: " + message + " thisNodeName: " + thisNodeName + " thisAction: " + thisAction + " arriveTime: " + String(arriveTime));
        if (thisAction == buttonLongPress){
          sendMessageAndRemove = true;  
        } else if (thisAction == buttonPress2){
          sendMessageAndRemove = true;  
        } else if (thisAction == buttonPress1){
          unsigned long tmpCurrent = millis();
          elaspsed = tmpCurrent - previousComs[i].arriveTime;
          Serial.println("elaspsed time: "+String(elaspsed));
          if (elaspsed > 1750){
            sendMessageAndRemove = true;
          }
        }
        if (sendMessageAndRemove){
          publishButtonPress(thisNodeName,thisAction);
          //Inner loop to clean up other nodes
          for (int x = 0; x < QUEUE_SIZE; x++) {
            if (previousComs[x].arriveTime != 0){
              message = String(previousComs[x].message);
              messageSize = previousComs[x].messageSize;
              //Lets deserialize the message
              deserializeJson(doc, message);
              String tmpNodeName = doc["nodeName"];
              String tmpAction = doc["action"];
              Serial.print("tmpNodeName: " + tmpNodeName + "thisNodeName: " + thisNodeName + " tmpAction: " + tmpAction + ", ");
              if (tmpNodeName == thisNodeName){
                Serial.print("setting arrive time 0 " + x);
                previousComs[x].arriveTime = 0;
              }
              Serial.println();
            }
         }
       }//End of sendMessageAndRemove test
     }// end of test for non zero arriveTime 
    }//end of for
    previousButtonCheckMillis = millis();  
  } //end of previousComs[] scans ends
  //*******************************************************************************

  //*******************************************************************************
  //This section will publish (I am up) message every x seconds
  unsigned long currentSendUpdateMillis = millis();
  elaspsed = currentSendUpdateMillis - previousSendUpdateMillis;
  if (elaspsed > 30000) {
    Serial.setDebugOutput(true);
    WiFi.printDiag(Serial);
    Serial.setDebugOutput(false);
    bool tmpStatus = publishGatewayStatus("Status","StillUp");
    Serial.println(tmpStatus,DEC);
    Serial.setDebugOutput(true);
    WiFi.printDiag(Serial);
    Serial.setDebugOutput(false);
//    Serial.println("Dump (30) previousComs[] ");
//    for (int x = 0; x < QUEUE_SIZE; x++) {
//      Serial.print(String(x));
//      Serial.print(", " + String(previousComs[x].arriveTime));
//      Serial.println(", " + String(previousComs[x].message));
//    }
    previousSendUpdateMillis = millis();
  }
  //*******************************************************************************
}


// This function rormats and sends the button press
bool publishButtonPress(String pNode, String pAction){
  String tmpTopic = root_topic + "/" + buttonPress_topic;
  String tmpPayLoad = "{\"node\":\""+pNode+"\",\"action\":\""+pAction+"\"}";
  bool tmpStatus = publishMQTT(tmpTopic,tmpPayLoad);
  Serial.print(", " + tmpTopic + ", " + tmpPayLoad + ", ");
  Serial.print(tmpStatus,DEC);
  return tmpStatus;
}

bool publishGatewayStatus(String pStatusType, String pStatusValue) {
  String tmpTopic = root_topic + "/" + gatewayStatus_topic;
  String tmpPayLoad = "{\"status\":\""+pStatusType+"\",\"value\":\""+pStatusValue+"\"}";
  bool tmpStatus = publishMQTT(tmpTopic,tmpPayLoad);
  Serial.print(", " + tmpTopic + ", " + tmpPayLoad + ", ");
  Serial.print(tmpStatus,DEC);
  return tmpStatus;
}

bool publishMQTT(String pTopic, String pPayLoad) {
  Serial.println("PubMQTT " + pTopic + ", " + pPayLoad + ", ");
  char topic[pTopic.length()+1];
  char payLoad[pPayLoad.length()+1];
  pTopic.toCharArray(topic,pTopic.length()+1);
  pPayLoad.toCharArray(payLoad,pPayLoad.length()+1);
  return client.publish(topic,payLoad);
}

void getReading(uint8_t *data, uint8_t len) {
  SENSOR_DATA tmp;
  SENSOR_DATA tmp1;
  Serial.println("size of tmp: " + String(sizeof(tmp)) + " tmp1: " + String(sizeof(tmp1)));
  memcpy(&tmp, data, sizeof(tmp));
  tmp.arriveTime = millis();
  Serial.println(tmp.message);
  Serial.print(" Message Size3: ");
  Serial.print(tmp.messageSize);
  Serial.print(tmp.message);
  Serial.println(tmp.arriveTime);
  Q.push((uint8_t*)&tmp);
  Q.peek((uint8_t*)&tmp1);
  Serial.print(" Message Size4: ");
  Serial.print(tmp1.messageSize);
  Serial.print(", ");
  Serial.print(tmp1.message);
  Serial.print(", ");
  Serial.println(tmp1.arriveTime);
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
