# WiFi2MQTT

Need to make a secrets.h file in the gateway directory with this contents...
```code
{
#ifndef _SECRETS_H    // Put these two lines at the top of your file.
#define _SECRETS_H    // (Use a suitable name, usually based on the file name.)

#define WiFi2MQTT_ssid "SSID"
#define WiFi2MQTT_password "Password"
#define WiFi2MQTT_mqtt_server "xxx.xxx.xxx.xxx"
#define WiFi2MQTT_mqtt_user "User"
#define WiFi2MQTT_mqtt_password "Password"

#endif // _SECRETS_H    // Put this line at the end of your file.
}
```
