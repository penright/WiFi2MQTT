/*
 MAC: C8:2B:96:08:2C:67
 Although there are many to say thanks to, Ant Elder code is what inspired the technique. 
*/

#include "ESP8266WiFi.h"
#include "espnow.h"
#include <ESP8266HTTPClient.h>
#include <ArduinoJson.h>


// this is the MAC Address of the remote ESP which this ESP sends its data too
// ESP-12       Mac uint8_t remoteMac[] = {0xC8, 0x2B, 0x96, 0x8, 0x2E, 0xA};
// ESP-01       Mac uint8_t remoteMac[] = {0xEE, 0xFA, 0xBC, 0xC5, 0x7B, 0x63};
// ESP-01(4meg) Mac uint8_t remoteMac[] = {0xEE, 0xFA, 0xBC, 0xC5, 0x7B, 0xA5};
uint8_t remoteMac[] = {0xEE, 0xFA, 0xBC, 0xC5, 0x7B, 0xA5};

#define WIFI_CHANNEL 1
const String nodeName = "Button1";
volatile boolean messageSent;
const String buttonPress1 = "buttonPress1";
const String buttonPress2 = "buttonPress2";
const String buttonLongPress = "buttonLongPress";

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


void setup() {
  Serial.begin(115200); Serial.println();  
  WiFi.mode(WIFI_STA); // Station mode for sensor/controller node
  WiFi.begin();
  Serial.print("This node mac: "); Serial.println(WiFi.macAddress());

  if (esp_now_init()!=0) {
    Serial.println("*** ESP_Now init failed");
    ESP.restart();
  }

  delay(1); // This delay seems to make it work more reliably???

  esp_now_set_self_role(ESP_NOW_ROLE_CONTROLLER);
  esp_now_add_peer(remoteMac, ESP_NOW_ROLE_SLAVE, WIFI_CHANNEL, NULL, 0);
  Serial.setDebugOutput(true);
  WiFi.printDiag(Serial);
  Serial.setDebugOutput(false);

  esp_now_register_send_cb([](uint8_t* mac, uint8_t status) {
    Serial.print("send_cb, status = "); Serial.print(status); 
    Serial.print(", to mac: "); 
    char macString[50] = {0};
    sprintf(macString,"%02X:%02X:%02X:%02X:%02X:%02X", mac[0], mac[1], mac[2], mac[3], mac[4], mac[5]);
    Serial.println(macString);
    messageSent = true;
  });
  Serial.println("Call back is setup");
  messageSent = false;
  sendMessage("buttonPress1");
  while(!messageSent){Serial.print("."); yield();}
  unsigned long previousWaitForLongPress = millis();
  unsigned long currentWaitForLongPress = millis();
  unsigned long elaspsed = currentWaitForLongPress - previousWaitForLongPress;
  Serial.print("Start of delay for LongPress "); Serial.println(elaspsed);
  while(elaspsed<1000){
    //Serial.print(" ."); Serial.print(elaspsed);
    yield();
    currentWaitForLongPress = millis();
    elaspsed = currentWaitForLongPress - previousWaitForLongPress;
  }
  Serial.println();
  messageSent = false;
  sendMessage("buttonLongPress");
  while(!messageSent){Serial.print("."); yield();}
}

void loop() {
}

void sendMessage(String pmessage){
  SENSOR_DATA sd;
  String input =
    "{\"nodeName\":\"" + nodeName + "\",\"action\":\"" + pmessage +"\"}";
  Serial.print("Start Send Message: " + input);
  sd.messageSize=input.length();
  Serial.print(", ");
  Serial.print(sd.messageSize);
  Serial.print(", ");
  Serial.println(input.length());
  //memset(0, sd.message, sizeof(sd.message));
  char tmp[sd.messageSize+1]; 
  input.toCharArray(tmp,sizeof(tmp));
  memcpy(sd.message, &tmp, sizeof(tmp));
  Serial.print("Send " + String(sd.message) + ", ");
  u8 bs[sizeof(sd)];
  memcpy(bs, &sd, sizeof(sd));
  esp_now_send(NULL, bs, sizeof(bs)); // NULL means send to all peers
  Serial.println("sent");
}
