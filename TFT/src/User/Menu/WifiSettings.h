#ifndef _WIFI_SETTINGS_H_
#define _WIFI_SETTINGS_H_

#ifdef __cplusplus
extern "C" {
#endif

#include <stdbool.h>
#include <stdint.h>

#define MAX_WIFI_APS 16
#define MAX_SSID_LEN 33
#define MAX_PASS_LEN 64

typedef struct {
  char ssid[MAX_SSID_LEN];
  uint8_t signal;   // 0 - 100
  uint8_t secured;  // 0 = Open, 1 = WPA/WPA2/WPA3
} WIFI_AP_ITEM;

typedef struct {
  bool connected;
  bool scanning;
  bool connecting;
  char current_ssid[MAX_SSID_LEN];
  char ip_addr[16];
  char status_msg[48];
  uint8_t ap_count;
  WIFI_AP_ITEM aps[MAX_WIFI_APS];
} WIFI_MANAGER;

extern WIFI_MANAGER wifiManager;

void menuWifiSettings(void);
void wifiResetManager(void);

#ifdef __cplusplus
}
#endif

#endif
