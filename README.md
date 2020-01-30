# WiFi2MQTT

Need to make a secrets.h file in the gateway directory with this contents...
```
#ifndef _SECRETS_H    // Put these two lines at the top of your file.
#define _SECRETS_H    // (Use a suitable name, usually based on the file name.)

#define ssid "SSID"
#define password "Password"
#define mqtt_server "xxx.xxx.xxx.xxx"
#define mqtt_user "User"
#define mqtt_password "Password"

#endif // _SECRETS_H    // Put this line at the end of your file.
```
