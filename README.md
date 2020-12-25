# VpnTcpProxy
 PPTP VPN + TCP Proxy For ESP8266
 
 ## How to Use
 Just download this project and open with Arduino
 
 ## Require Library
 1. [ESPAsyncWebServer](https://github.com/me-no-dev/ESPAsyncWebServer)
 2. [ESPAsyncTCP](https://github.com/me-no-dev/ESPAsyncTCP)
 
 ## Default Configuration
 - If can't connect to AP, Device will active SoftAP mode with SSID=VpnTcpProxy no password (Reboot every 5 minute)
 - Default Config can see in file DeviceConfig.cpp
 
 const char* ssid = "WiFi2.4";
 const char* password = "12345678";
 const char* pptp_server = "vpn.example.com";
 const int pptp_port = 1723;
 const char *pptp_user = "pptp-user";
 const char *pptp_password = "pptp-password";
 const char *dest_server = "192.168.1.1";

## Web File
- Store in ESP8266 flash by SPIFFS
- Directory "data" is Web File
