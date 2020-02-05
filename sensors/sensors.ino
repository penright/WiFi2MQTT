/*
 MAC: C8:2B:96:08:2C:67
 Although there are many to say thanks to, Ant Elder code is what inspired the technique. 
*/

#include "ESP8266WiFi.h"
#include "espnow.h"
#include <ESP8266HTTPClient.h>
#include <ArduinoJson.h>


// this is the MAC Address of the remote ESP which this ESP sends its data too
uint8_t remoteMac[] = {0xC8, 0x2B, 0x96, 0x8, 0x2E, 0xA};

#define WIFI_CHANNEL 1
const String nodeName = "Button1";

struct SENSOR_DATA {
  unsigned long arriveTime;
  int messageSize;
  char message[200];
};

volatile boolean readingSent;

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

    readingSent = true;
  });

  sendReading();

//  if (readingSent || (millis() > SEND_TIMEOUT)) {
//    Serial.print("Going to sleep, uptime: "); Serial.println(millis());  
//    ESP.deepSleep(SLEEP_TIME, WAKE_RF_DEFAULT);
//  }

}

void loop() {

}

void sendReading() {
    readingSent = false;
    sendMessage("buttonPress1");
    delay(1000);
    sendMessage("buttonLongPress");

    
//    DynamicJsonDocument doc(300);
//      String input =
//      "{\"sensor\":\"gps\",\"time\":1351824120,\"data\":[48.756080,2.302038]}";
//    deserializeJson(doc, input);
//    JsonObject jsonOBJ = doc.as<JsonObject>();
//    Serial.print("-"); Serial.print(jsonOBJ); Serial.print("-"); Serial.print(sizeof(jsonOBJ)); Serial.println("-");
//    u8 bs[sizeof(jsonOBJ)];
//    memcpy(bs, &jsonOBJ, sizeof(jsonOBJ));
//    esp_now_send(NULL, bs, sizeof(bs)); // NULL means send to all peers
//    
//    SENSOR_DATA sd;
//    sd.t = millis();
//    Serial.print("sendReading, t="); Serial.println(sd.t);
//
//    u8 bs[sizeof(sd)];
//    memcpy(bs, &sd, sizeof(sd));
//    esp_now_send(NULL, bs, sizeof(bs)); // NULL means send to all peers
}

void sendMessage(String pmessage){
     SENSOR_DATA sd;
    String input =
      "{\"nodeName\":\"" + nodeName + "\",\"action\":\"" + pmessage +"\"}";
    Serial.print(String(input));
    sd.messageSize=input.length();
    Serial.print(", ");
    Serial.print(sd.messageSize);
    Serial.print(", ");
    Serial.println(input.length());
    //memset(0, sd.message, sizeof(sd.message));
    char tmp[sd.messageSize+1]; 
    input.toCharArray(tmp,sizeof(tmp));
    memcpy(sd.message, &tmp, sizeof(tmp));
    Serial.println(String(sd.message));
    u8 bs[sizeof(sd)];
    memcpy(bs, &sd, sizeof(sd));
    esp_now_send(NULL, bs, sizeof(bs)); // NULL means send to all peers

}
