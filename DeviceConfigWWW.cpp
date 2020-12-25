


#include <ESPAsyncWebServer.h>
#include <SPIFFSEditor.h>
#include "FS.h"
#include "DeviceConfigWWW.h"

extern "C"
{
  //#include <lwip/raw.h>
  //#include <user_interface.h>
  #include <lwip/inet.h>
}

#define U_PART U_FS

int WebConfigPort = WEB_CONFIG_PORT;
AsyncWebServer server(WebConfigPort);
size_t content_len;


const char* ssid = "WiFi2.4";
const char* password = "12345678";
const char* pptp_server = "vpn.example.com";
const int pptp_port = 1723;
const char *pptp_user = "pptp-user";
const char *pptp_password = "pptp-password";
const char *dest_server = "192.168.1.1";

struct DeviceConfig *pCfg;
struct DeviceStatus *pStatus;

void Web_bindDeviceStatus(struct DeviceStatus *deviceStatus) {
  
  pStatus = deviceStatus;
  pStatus->internet_connect = false;
  pStatus->vpn_connect = false;
  pStatus->wifi_mode = WIFI_MODE_STA;
  (pStatus->wifi_ip).addr = 0;
  (pStatus->vpn_local_ip).addr = 0;
  (pStatus->vpn_remote_ip).addr = 0;
  strcpy(pStatus->fw_date, __DATE__);
  strcpy(pStatus->fw_time, __TIME__);
  pStatus->uptime_sec = 0;
  pStatus->restart_timeout_sec = 0;
  strcpy(pStatus->wifi_mac, WiFi.macAddress().c_str());
  pStatus->free_heap = 0;
  
}

const char *devConfigFilename = "/deviceconfig.bin";
void Web_makeDefaultDeviceConfig(struct DeviceConfig *cfg) {

  memset(cfg, 0, sizeof(struct DeviceConfig));
  strcpy(cfg->wifi_ssid, ssid);
  strcpy(cfg->wifi_password, password);
  strcpy(cfg->pptp_server, pptp_server);
  strcpy(cfg->pptp_user, pptp_user);
  strcpy(cfg->pptp_password, pptp_password);
  strcpy(cfg->tcp_destination, dest_server);
  
  File fp = SPIFFS.open(devConfigFilename, "w");
  int writeSize;
  if (!fp) {
    Serial.println("makeDefaultDeviceConfig() file create failed");
    //makeDefaultDeviceConfig();
    return;
  }

  writeSize = fp.write((byte*) cfg, sizeof(struct DeviceConfig));
  fp.close(); 
}

bool Web_saveDeviceConfig(struct DeviceConfig *cfg) {
  File fp = SPIFFS.open(devConfigFilename, "w");
  int writeSize;
  if (!fp) {
    Serial.println("Web_saveDeviceConfig() file open failed");
    //makeDefaultDeviceConfig();
    return false;
  }

  writeSize = fp.write((byte*) cfg, sizeof(struct DeviceConfig));
  fp.close();
  return true;
}

void Web_readDeviceConfig(struct DeviceConfig *cfg) {
  int readSize;
  pCfg = cfg;
  if(!SPIFFS.begin()){
    Serial.println("readDeviceConfig() An Error has occurred while mounting SPIFFS");
    return;
  }
  
  if (!SPIFFS.exists(devConfigFilename)) {
    Serial.println("readDeviceConfig() Device Config File Not Exist");
    Web_makeDefaultDeviceConfig(cfg);
    return;
  }
  
  File f = SPIFFS.open(devConfigFilename, "r");
  if (!f) {
    Serial.println("readDeviceConfig() file open failed");
    return;
  }

  readSize = f.readBytes((char*) cfg, sizeof(struct DeviceConfig)); //cast changed from byte*
  
  f.close();
}

void handle_upload_file(AsyncWebServerRequest *request) {
  char* html = "<form method='POST' action='/do_upload_file' enctype='multipart/form-data'><input type='file' name='update'><input type='submit' value='Upload'></form>";
  String info_str = "";
  FSInfo fs_info;
  SPIFFS.info(fs_info);
  /*
   struct FSInfo {
    size_t totalBytes;
    size_t usedBytes;
    size_t blockSize;
    size_t pageSize;
    size_t maxOpenFiles;
    size_t maxPathLength;
};
   */
  info_str  = "SPIFFS Total Bytes: " + String(fs_info.totalBytes) + "<br>\n";
  info_str += "SPIFFS Used Bytes: " + String(fs_info.usedBytes) + "<br>\n";
  info_str += "SPIFFS Block Size: " + String(fs_info.blockSize) + "<br>\n";
  info_str += "SPIFFS Page Size: " + String(fs_info.pageSize) + "<br>\n";
  info_str += "SPIFFS Max Open File: " + String(fs_info.maxOpenFiles) + "<br>\n";
  info_str += "SPIFFS Max Path Length: " + String(fs_info.maxPathLength) + "<br>\n";

  info_str = String(html) + "<br>\n" + info_str;
  request->send(200, "text/html", info_str);
}

File fsUploadFile;              // a File object to temporarily store the received file
void handle_do_upload_file(AsyncWebServerRequest *request, const String& filename, size_t index, uint8_t *data, size_t len, bool final) {
  if (!index){
    Serial.println("Upload file");
    String filename2 = "";
    if(!filename.startsWith("/")) 
      filename2 = "/" + filename;
    Serial.print("handle_do_upload_file Name: "); Serial.println(filename2);
    fsUploadFile = SPIFFS.open(filename2, "w");            // Open the file for writing in SPIFFS (create if it doesn't exist)
    
  }

  if(fsUploadFile)
      fsUploadFile.write(data, len); // Write the received bytes to the file

  if (final) {
    AsyncWebServerResponse *response = request->beginResponse(302, "text/plain", "Please wait. . .");
    response->addHeader("Refresh", "20");  
    response->addHeader("Location", "/upload_file");
    request->send(response);

    if(fsUploadFile) {                                    // If the file was successfully created
      fsUploadFile.close();                               // Close the file again
      Serial.println("handleFileUpload: OK");
    } else {
      Serial.println("handleFileUpload: Fail");
    }
  }
}


void handleUpdate(AsyncWebServerRequest *request) {
  char* html = "<form method='POST' action='/doUpdate' enctype='multipart/form-data'><input type='file' name='update'><input type='submit' value='Update'></form>";
  request->send(200, "text/html", html);
}

void handleDoUpdate(AsyncWebServerRequest *request, const String& filename, size_t index, uint8_t *data, size_t len, bool final) {
  if (!index){
    Serial.println("Update");
    content_len = request->contentLength();
    // if filename includes spiffs, update the spiffs partition
    int cmd = (filename.indexOf("spiffs") > -1) ? U_PART : U_FLASH;
#ifdef ESP8266
    Update.runAsync(true);
    if (!Update.begin(content_len, cmd)) {
#else
    if (!Update.begin(UPDATE_SIZE_UNKNOWN, cmd)) {
#endif
      Update.printError(Serial);
    }
  }

  if (Update.write(data, len) != len) {
    Update.printError(Serial);
#ifdef ESP8266
  } else {
    Serial.printf("Progress: %d%%\n", (Update.progress()*100)/Update.size());
#endif
  }

  if (final) {
    AsyncWebServerResponse *response = request->beginResponse(302, "text/plain", "Please wait while the device reboots");
    response->addHeader("Refresh", "20");  
    response->addHeader("Location", "/");
    request->send(response);
    if (!Update.end(true)){
      Update.printError(Serial);
    } else {
      Serial.println("Update complete");
      Serial.flush();
      ESP.restart();
    }
  }
}

void web_device_config_handle(AsyncWebServerRequest *request) {
  String result = "";

  result += "{";
  result += "\"wifi_ssid\":\"" + String(pCfg->wifi_ssid) + "\",";
  result += "\"wifi_password\":\"" + String(pCfg->wifi_password) + "\",";
  result += "\"pptp_server\":\"" + String(pCfg->pptp_server) + "\",";
  result += "\"pptp_user\":\"" + String(pCfg->pptp_user) + "\",";
  result += "\"pptp_password\":\"" + String(pCfg->pptp_password) + "\",";
  result += "\"tcp_destination\":\"" + String(pCfg->tcp_destination) + "\"";
  result +="}";

  request->send(200, "text/html", result);
}

void web_device_status_handle(AsyncWebServerRequest *request) {
  String result = "", tmp = "";
  struct in_addr address;
  
  result += "{";
  
  result += "\"internet_connect\":\"" + String(pStatus->internet_connect) + "\",";
  result += "\"vpn_connect\":\"" + String(pStatus->vpn_connect) + "\",";
  result += "\"wifi_mode\":\"" + String(pStatus->wifi_mode==WIFI_MODE_AP?"AP":"Station") + "\",";

  address.s_addr = (pStatus->wifi_ip).addr;
  result += "\"wifi_ip\":\"" + String(inet_ntoa(address)) + "\",";
  
  address.s_addr = (pStatus->vpn_local_ip).addr;
  result += "\"vpn_local_ip\":\"" + String(inet_ntoa(address)) + "\",";
  
  address.s_addr = (pStatus->vpn_remote_ip).addr;
  result += "\"vpn_remote_ip\":\"" + String(inet_ntoa(address)) + "\",";
  
  result += "\"fw_date\":\"" + String(pStatus->fw_date) + "\",";
  result += "\"fw_time\":\"" + String(pStatus->fw_time) + "\",";
  result += "\"uptime_sec\":\"" + String(pStatus->uptime_sec) + "\",";
  result += "\"restart_timeout_sec\":\"" + String(pStatus->restart_timeout_sec) + "\",";
  result += "\"wifi_mac\":\"" + String(pStatus->wifi_mac) + "\",";
  result += "\"free_heap\":\"" + String(pStatus->free_heap) + "\"";
  result +="}";

  request->send(200, "text/html", result);
}

const char* http_username = "admin";
const char* http_password = "admin";
void Web_init() {
  //server.on("/", HTTP_GET, [](AsyncWebServerRequest *request) {request->redirect("/update");});
  server.on("/upload_file", HTTP_GET, [](AsyncWebServerRequest *request){handle_upload_file(request);});
  server.on("/do_upload_file", HTTP_POST,
    [](AsyncWebServerRequest *request) {request->redirect("/upload_file");},
    [](AsyncWebServerRequest *request, const String& filename, size_t index, uint8_t *data,
                  size_t len, bool final) {handle_do_upload_file(request, filename, index, data, len, final);}
  );
  server.on("/update", HTTP_GET, [](AsyncWebServerRequest *request){handleUpdate(request);});
  server.on("/doUpdate", HTTP_POST,
    [](AsyncWebServerRequest *request) {},
    [](AsyncWebServerRequest *request, const String& filename, size_t index, uint8_t *data,
                  size_t len, bool final) {handleDoUpdate(request, filename, index, data, len, final);}
  );
  server.on("/device_config", HTTP_GET, [](AsyncWebServerRequest *request){web_device_config_handle(request);});
  server.on("/device_status", HTTP_GET, [](AsyncWebServerRequest *request){web_device_status_handle(request);});
  
  server.addHandler(new SPIFFSEditor(http_username,http_password));
  server.on("/heap", HTTP_GET, [](AsyncWebServerRequest *request){
    request->send(200, "text/plain", String(ESP.getFreeHeap()));
  });
  server.serveStatic("/", SPIFFS, "/").setDefaultFile("index.html");

  server.onNotFound([](AsyncWebServerRequest *request){request->send(404);});

  server.onFileUpload([](AsyncWebServerRequest *request, const String& filename, size_t index, uint8_t *data, size_t len, bool final){
    if(!index)
      Serial.printf("UploadStart: %s\n", filename.c_str());
    Serial.printf("%s", (const char*)data);
    if(final)
      Serial.printf("UploadEnd: %s (%u)\n", filename.c_str(), index+len);
  });
  server.onRequestBody([](AsyncWebServerRequest *request, uint8_t *data, size_t len, size_t index, size_t total){
    if(!index)
      Serial.printf("BodyStart: %u\n", total);
    Serial.printf("%s", (const char*)data);
    if(index + len == total)
      Serial.printf("BodyEnd: %u\n", total);
  });

  // Send a POST request to <IP>/post with a form field message set to <message>
  server.on("/setting-req", HTTP_POST, [](AsyncWebServerRequest *request){
    String message;
    if (request->hasParam("wifi_ssid", true)) {
      message = request->getParam("wifi_ssid", true)->value();
      strcpy(pCfg->wifi_ssid, message.c_str());
      
    }

    if (request->hasParam("wifi_password", true)) {
      message = request->getParam("wifi_password", true)->value();
      strcpy(pCfg->wifi_password, message.c_str());
      
    }

    if (request->hasParam("pptp_server", true)) {
      message = request->getParam("pptp_server", true)->value();
      strcpy(pCfg->pptp_server, message.c_str());
      
    }

    if (request->hasParam("pptp_user", true)) {
      message = request->getParam("pptp_user", true)->value();
      strcpy(pCfg->pptp_user, message.c_str());
      
    }

    if (request->hasParam("pptp_password", true)) {
      message = request->getParam("pptp_password", true)->value();
      strcpy(pCfg->pptp_password, message.c_str());
      
    }

    if (request->hasParam("tcp_destination", true)) {
      message = request->getParam("tcp_destination", true)->value();
      strcpy(pCfg->tcp_destination, message.c_str());
      
    }

    AsyncWebServerResponse *response = request->beginResponse(302); //Sends 302 move temporaya
    response->addHeader("Location", "setting.html");
    request->send(response);
  });
    
  server.begin();
}
