
#include <ESP8266WiFi.h>
//#include <ESP8266WebServer.h>
#include "DeviceConfigWWW.h"
#include "PPTP_Client.h"
#include "TcpProxyServer.h"
#include "DebugMsg.h"

const int led = LED_BUILTIN;

#define PPTP_PORT   1723

struct DeviceConfig deviceConfigWeb;
struct DeviceConfig deviceConfigRun;
struct DeviceStatus deviceStatus;


#define SAVECFG_REQ     0x01
#define SAVECFG_REBOOT  0x02
uint8_t saveConfigFlag;
bool devEmergencyMode;
bool pptpStatus = false;
int debug = 1;
uint32_t uptime_ms, uptime_sec;
uint32_t ms;

const char *ap_ssid = "VpnTcpProxy";
const char *ap_password = "";
void wifiModeAP() {
  WiFi.softAP(ap_ssid, ap_password);
  IPAddress IP = WiFi.softAPIP();
  Serial.print("AP IP address: ");
  Serial.println(IP);
}

void setRestartTimeout(uint32_t tmout_after_sec) {
  deviceStatus.restart_timeout_sec = deviceStatus.uptime_sec + tmout_after_sec;
  printf("setRestartTimeout() input=%d, set restart when uptime is %d\n", tmout_after_sec, deviceStatus.restart_timeout_sec);
}

void setup(){
  
  saveConfigFlag = 0;
  devEmergencyMode = false;
  pinMode(led, OUTPUT);
  digitalWrite(led, HIGH);
  
  Serial.begin(115200);
  Serial.print("\n");
  Serial.print("ESP PPTP Client Version: ");
  Serial.print(__DATE__);
  Serial.print(", ");
  Serial.println(__TIME__);

  Serial.println("Reading Device Config From File");
  Web_readDeviceConfig(&deviceConfigWeb);
  Web_bindDeviceStatus(&deviceStatus);
  memcpy(&deviceConfigRun, &deviceConfigWeb, sizeof(struct DeviceConfig));
  printf("Device Config Data. . .\n");
  printf("SSID       :  %s\n", deviceConfigRun.wifi_ssid);
  printf("Password   :  %s\n", deviceConfigRun.wifi_password);
  printf("PPTP Server:  %s\n", deviceConfigRun.pptp_server);
  printf(" - User    :  %s\n", deviceConfigRun.pptp_user);
  printf(" - Pass    :  %s\n", deviceConfigRun.pptp_password);
  printf("Dest Server:  %s\n", deviceConfigRun.tcp_destination);
  Serial.println("Read Device Config From File Complete");
  
  WiFi.begin(deviceConfigRun.wifi_ssid, deviceConfigRun.wifi_password);
  if (WiFi.waitForConnectResult() != WL_CONNECTED) {
    Serial.printf("WiFi Failed! -> Change to AP Mode\n");
    devEmergencyMode = true;
    wifiModeAP();
    deviceStatus.wifi_mode = WIFI_MODE_AP;
    deviceStatus.wifi_ip.addr = WiFi.softAPIP();
    deviceStatus.internet_connect = false;
  } else {
    Serial.printf("WiFi Connected!\n");
    Serial.println(WiFi.localIP());
    deviceStatus.wifi_mode = WIFI_MODE_STA;
    deviceStatus.wifi_ip.addr = WiFi.localIP();
    deviceStatus.internet_connect = true;
  }

  if (devEmergencyMode == false) {
    Serial.println("PPTP Client Init");
    PPTPC_init(deviceConfigRun.pptp_server, PPTP_PORT, deviceConfigRun.pptp_user, deviceConfigRun.pptp_password);
    pptpStatus = PPTPC_connect();
    Serial.print("pptp connect return: ");
    Serial.println(pptpStatus);
    if (pptpStatus == true) {
      Serial.println("PPTP Client Init OK");
      deviceStatus.vpn_connect = true;
      deviceStatus.vpn_local_ip.addr = localIP.addr;
      deviceStatus.vpn_remote_ip.addr = remoteIP.addr;
    } else {
      Serial.println("PPTP Client Init Fail, Set restart in 60 Sec");
      setRestartTimeout(60);
    }
  } else {
    Serial.println("Skip PPTP Client Init in Emergency Mode");
  }
  
  Serial.println("Web Config Init");
  if(!SPIFFS.begin()){
    Serial.println("An Error has occurred while mounting SPIFFS");
    //return;
  }
  Web_init();
  Serial.println("Web Config Init OK");

  Serial.println("Tcp Proxy Init");
  TcpProxyServer_begin( deviceConfigRun.tcp_destination, WEB_CONFIG_PORT);
  TcpProxyServer_start();
  Serial.println("Tcp Proxy Init OK");
  
  digitalWrite(led, LOW);
  Serial.println("Starting Loop...");
  ms = millis();
}


void handleUptime() {
  uint32_t tmp = millis();
  if (tmp - uptime_ms > 1000) {
    uptime_ms = tmp;
    uptime_sec++;

    deviceStatus.uptime_sec = uptime_sec;

    if (deviceStatus.restart_timeout_sec == deviceStatus.uptime_sec) {
      Serial.println("Uptime == Restart Timeout -> Reboot ESP");
      ESP.reset();
    }

    deviceStatus.free_heap = ESP.getFreeHeap();
  }
}

void handleDeviceConfig() {
  
  if (strcmp(deviceConfigWeb.wifi_ssid, deviceConfigRun.wifi_ssid) != 0){
    strcpy(deviceConfigRun.wifi_ssid, deviceConfigWeb.wifi_ssid);
    printf("WiFi SSID Changed to %s\n", deviceConfigWeb.wifi_ssid);
    saveConfigFlag = SAVECFG_REQ | SAVECFG_REBOOT;
  }

  if (strcmp(deviceConfigWeb.wifi_password, deviceConfigRun.wifi_password) != 0){
    strcpy(deviceConfigRun.wifi_password, deviceConfigWeb.wifi_password);
    printf("WiFi Password Changed to %s\n", deviceConfigWeb.wifi_password);
    saveConfigFlag = SAVECFG_REQ | SAVECFG_REBOOT;
  }

  if (strcmp(deviceConfigWeb.pptp_server, deviceConfigRun.pptp_server) != 0){
    strcpy(deviceConfigRun.pptp_server, deviceConfigWeb.pptp_server);
    printf("PPTP Server Changed to %s\n", deviceConfigWeb.pptp_server);
    saveConfigFlag = SAVECFG_REQ | SAVECFG_REBOOT;
  }

  if (strcmp(deviceConfigWeb.pptp_user, deviceConfigRun.pptp_user) != 0){
    strcpy(deviceConfigRun.pptp_user, deviceConfigWeb.pptp_user);
    printf("PPTP User Changed to %s\n", deviceConfigWeb.pptp_user);
    saveConfigFlag = SAVECFG_REQ | SAVECFG_REBOOT;
  }

  if (strcmp(deviceConfigWeb.pptp_password, deviceConfigRun.pptp_password) != 0){
    strcpy(deviceConfigRun.pptp_password, deviceConfigWeb.pptp_password);
    printf("PPTP Password Changed to %s\n", deviceConfigWeb.pptp_password);
    saveConfigFlag = SAVECFG_REQ | SAVECFG_REBOOT;
  }

  

   if (strcmp(deviceConfigWeb.tcp_destination, deviceConfigRun.tcp_destination) != 0){
    strcpy(deviceConfigRun.tcp_destination, deviceConfigWeb.tcp_destination);
    printf("TCP Destination Changed to %s\n", deviceConfigWeb.tcp_destination);
    TcpProxyServer_setDestinationServer(deviceConfigWeb.tcp_destination);
    saveConfigFlag = SAVECFG_REQ;
  }

  if (saveConfigFlag != 0) {
    if (saveConfigFlag & SAVECFG_REQ) {
      Web_saveDeviceConfig(&deviceConfigWeb);
      Serial.println("Save Device Config to flash successfully");
      if (saveConfigFlag & SAVECFG_REBOOT) {
        Serial.println("Need to reboot to apply change -> Reboot ESP");
        delay(2000);
        ESP.reset();
      }
    }
    saveConfigFlag = 0;
  }
}

void loop(){

  handleUptime();
  handleDeviceConfig();
  delay(50);  // delay 50ms
  
  if (devEmergencyMode == false) {
    if (pptpStatus == true) // run only when pptp init success
      PPTPC_handle();
  } else {
    if (millis() - ms > 5 * 60 * 1000) {    // uptime > 5 minute
      Serial.println("Maximum runtime in Emergency Mode is 5 Minute -> Reboot ESP");
      ESP.reset();
    }
  }

  if (Serial.available() > 0) {
    int ch = Serial.read();
    if (ch == 'r') {
      Serial.println("Serial request to Reset Configuration and Reboot");
      SPIFFS.remove("/deviceconfig.bin");
      delay(1000);
      ESP.reset();
    }
    Serial.print("Change Debug Message to ");
    if (debug == 1) debug = 10;
    else debug = 1;
    Serial.println(debug);
    db_setLevel(debug);
  }
  while (Serial.available() > 0) Serial.read();

  if ( (WiFi.status() != WL_CONNECTED) && (devEmergencyMode == false) ) {
    Serial.println("WiFi Disconnected -> Reboot ESP");
    ESP.reset();
  }

  
}
