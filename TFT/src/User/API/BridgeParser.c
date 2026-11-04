#include "BridgeParser.h"
#include "includes.h"
#include "WifiSettings.h"
#include "KlipperMacros.h"
#include "KlipperParams.h"
#include <stdlib.h>
#include <string.h>

void parseBridgeMessage(char * msg)
{
  if (msg == NULL || *msg == '\0')
    return;

  // Trim trailing newlines and spaces
  char * end = msg + strlen(msg) - 1;
  while (end >= msg && (*end == '\r' || *end == '\n' || *end == ' '))
  {
    *end = '\0';
    end--;
  }

  if (strncmp(msg, "wifi_status:", 12) == 0)
  {
    char * p = msg + 12;
    // Format: <0|1>|<ip>|<ssid>
    char * token1 = p;
    char * bar1 = strchr(p, '|');
    if (bar1 != NULL)
    {
      *bar1 = '\0';
      char * token2 = bar1 + 1;
      char * bar2 = strchr(token2, '|');
      if (bar2 != NULL)
      {
        *bar2 = '\0';
        char * token3 = bar2 + 1;

        wifiManager.connected = (atoi(token1) == 1);
        strncpy_no_pad(wifiManager.ip_addr, token2, sizeof(wifiManager.ip_addr));
        wifiManager.ip_addr[sizeof(wifiManager.ip_addr) - 1] = '\0';
        strncpy_no_pad(wifiManager.current_ssid, token3, sizeof(wifiManager.current_ssid));
        wifiManager.current_ssid[sizeof(wifiManager.current_ssid) - 1] = '\0';

        infoSetAccessPoint(wifiManager.current_ssid, strlen(wifiManager.current_ssid));
        infoSetIPAddress(wifiManager.ip_addr, strlen(wifiManager.ip_addr));

        wifiManager.scanning = false;
        wifiManager.connecting = false;
      }
    }
  }
  else if (strncmp(msg, "wifi_ap:", 8) == 0)
  {
    char * p = msg + 8;
    // Format: <signal>|<0|1>|<ssid>
    char * token1 = p;
    char * bar1 = strchr(p, '|');
    if (bar1 != NULL)
    {
      *bar1 = '\0';
      char * token2 = bar1 + 1;
      char * bar2 = strchr(token2, '|');
      if (bar2 != NULL)
      {
        *bar2 = '\0';
        char * token3 = bar2 + 1;

        if (wifiManager.ap_count < MAX_WIFI_APS)
        {
          wifiManager.aps[wifiManager.ap_count].signal = (uint8_t) atoi(token1);
          wifiManager.aps[wifiManager.ap_count].secured = (uint8_t) atoi(token2);
          strncpy_no_pad(wifiManager.aps[wifiManager.ap_count].ssid, token3, MAX_SSID_LEN);
          wifiManager.aps[wifiManager.ap_count].ssid[MAX_SSID_LEN - 1] = '\0';
          wifiManager.ap_count++;
        }
      }
    }
  }
  else if (strcmp(msg, "wifi_scan_done") == 0)
  {
    wifiManager.scanning = false;
  }
  else if (strncmp(msg, "wifi_conn:", 10) == 0)
  {
    char * p = msg + 10;
    // Format: <0|1>|<msg>
    char * token1 = p;
    char * bar1 = strchr(p, '|');
    if (bar1 != NULL)
    {
      *bar1 = '\0';
      char * token2 = bar1 + 1;
      uint8_t ok = (uint8_t) atoi(token1);
      wifiManager.connecting = false;
      if (ok == 1)
      {
        wifiManager.connected = true;
        strncpy_no_pad(wifiManager.ip_addr, token2, sizeof(wifiManager.ip_addr));
        wifiManager.ip_addr[sizeof(wifiManager.ip_addr) - 1] = '\0';
        infoSetIPAddress(token2, strlen(token2));
        snprintf(wifiManager.status_msg, sizeof(wifiManager.status_msg), "Connected: %s", token2);
      }
      else
      {
        wifiManager.connected = false;
        snprintf(wifiManager.status_msg, sizeof(wifiManager.status_msg), "Fail: %s", token2);
      }
    }
  }
  else if (strncmp(msg, "wifi_err:", 9) == 0)
  {
    char * p = msg + 9;
    snprintf(wifiManager.status_msg, sizeof(wifiManager.status_msg), "Err: %s", p);
    wifiManager.scanning = false;
    wifiManager.connecting = false;
  }
  else if (strncmp(msg, "macro:", 6) == 0)
  {
    char * p = msg + 6;
    klipperMacroAdd(p);
  }
  else if (strcmp(msg, "macro_done") == 0)
  {
    klipperMacroDone();
  }
  else if (strncmp(msg, "klipper_params:", 15) == 0)
  {
    // Format: PA=<f>|ACCEL=<d>|SCV=<f>|ZOFF=<f>
    float pa = klipperParams.pa;
    uint32_t accel = klipperParams.accel;
    float scv = klipperParams.scv;
    float zoff = klipperParams.z_offset;

    char * p = msg + 15;
    char * token = strtok(p, "|");
    while (token != NULL)
    {
      if (strncmp(token, "PA=", 3) == 0)
        pa = strtof(token + 3, NULL);
      else if (strncmp(token, "ACCEL=", 6) == 0)
        accel = (uint32_t) strtoul(token + 6, NULL, 10);
      else if (strncmp(token, "SCV=", 4) == 0)
        scv = strtof(token + 4, NULL);
      else if (strncmp(token, "ZOFF=", 5) == 0)
        zoff = strtof(token + 5, NULL);

      token = strtok(NULL, "|");
    }

    klipperParamsUpdate(pa, accel, scv, zoff);
  }
}
