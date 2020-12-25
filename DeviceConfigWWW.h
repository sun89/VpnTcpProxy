#ifndef DEVICE_CONFIG_WWW_h
#define DEVICE_CONFIG_WWW_h

struct DeviceConfig {
  char wifi_ssid[50];
  char wifi_password[50];
  char pptp_server[100];
  char pptp_user[50];
  char pptp_password[50];
  char tcp_destination[50];
};

#define WIFI_MODE_AP  0
#define WIFI_MODE_STA 1
struct DeviceStatus {
  bool internet_connect;
  bool vpn_connect;
  uint8_t wifi_mode;
  uint8_t free1;
  ip_addr_t wifi_ip;
  ip_addr_t vpn_local_ip;
  ip_addr_t vpn_remote_ip;
  char fw_date[50];
  char fw_time[50];
  uint32_t uptime_sec;
  uint32_t restart_timeout_sec;
  char wifi_mac[18];
  uint32_t free_heap;
};

#define WEB_CONFIG_PORT   8555

void Web_bindDeviceStatus(struct DeviceStatus *deviceStatus);
void Web_makeDefaultDeviceConfig(struct DeviceConfig *cfg);
bool Web_saveDeviceConfig(struct DeviceConfig *cfg);
void Web_readDeviceConfig(struct DeviceConfig *cfg);
void Web_init();

#endif
