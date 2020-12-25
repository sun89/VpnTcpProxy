
#include <ESP8266WiFi.h>
#include <ESP8266WebServer.h>

#include "PPTP_Client.h"
const int led = 13;

ESP8266WebServer server(80);
const char* ssid = "JOY_2G";
const char* password = "0856868216";
const char* pptp_server = "192.168.250.2";
const int pptp_port = 1723;
const char *pptp_user = "ppp-test";
const char *pptp_password = "ppp-password";

void setup(){
  bool pptpStatus = 0;

  pinMode(led, OUTPUT);
  digitalWrite(led, 0);
  
  Serial.begin(115200);
  Serial.print("\n");
  Serial.print("ESP PPTP Client Version: ");
  Serial.print(__DATE__);
  Serial.print(", ");
  Serial.println(__TIME__);
  
  WiFi.begin(ssid, password);
  if (WiFi.waitForConnectResult() != WL_CONNECTED) {
    Serial.printf("WiFi Failed!\n");
    return;
  }
  Serial.printf("WiFi Connected!\n");
  Serial.println(WiFi.localIP());

  PPTPC_init(pptp_server, pptp_port, pptp_user, pptp_password);
  pptpStatus = PPTPC_connect();
  Serial.print("pptp connect return: ");
  Serial.println(pptpStatus);

  server.on("/inline", []() {
    server.send(200, "text/plain", "this works as well");
  });

  server.begin();
}

void loop(){
  server.handleClient();
  PPTPC_handle();
  delay(1);
}
