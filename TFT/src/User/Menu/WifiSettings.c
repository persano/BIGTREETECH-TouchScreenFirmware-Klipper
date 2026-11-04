#include "WifiSettings.h"
#include "includes.h"
#include "Keyboard.h"
#include <stdio.h>
#include <string.h>

WIFI_MANAGER wifiManager = {
  .connected = false,
  .scanning = false,
  .connecting = false,
  .current_ssid = "",
  .ip_addr = "0.0.0.0",
  .status_msg = "",
  .ap_count = 0,
};

static uint16_t wifiCurPage = 0;
static char wifiTitleBuf[64] = "Wi-Fi Settings";
static char wifiValStr[MAX_WIFI_APS + 2][24];

void wifiResetManager(void)
{
  wifiManager.ap_count = 0;
  wifiManager.scanning = false;
  wifiManager.connecting = false;
  wifiManager.status_msg[0] = '\0';
}

static void loadWifiItem(LISTITEM * item, uint16_t index, uint8_t itemPos)
{
  if (wifiManager.ap_count == 0)
  {
    item->icon = wifiManager.scanning ? CHARICON_WAIT : CHARICON_INFO;
    item->itemType = LIST_LABEL;
    item->titlelabel.index = LABEL_DYNAMIC;
    setDynamicLabel(itemPos, wifiManager.scanning ? textSelect(LABEL_WIFI_SCANNING) : "No APs (Tap to scan)");
    return;
  }

  if (index < wifiManager.ap_count)
  {
    item->icon = wifiManager.aps[index].secured ? CHARICON_WIFI_ON : CHARICON_WIFI_OFF;
    item->itemType = LIST_CUSTOMVALUE;
    item->titlelabel.index = LABEL_DYNAMIC;
    setDynamicLabel(itemPos, wifiManager.aps[index].ssid);

    snprintf(wifiValStr[index], sizeof(wifiValStr[index]), "%d%% %s",
             wifiManager.aps[index].signal,
             wifiManager.aps[index].secured ? "[Sec]" : "[Open]");

    item->valueLabel.index = LABEL_DYNAMIC;
    item->valueLabel.address = wifiValStr[index];
  }
  else
  {
    // Rescan button
    item->icon = CHARICON_SD_REFRESH;
    item->itemType = LIST_LABEL;
    item->titlelabel.index = LABEL_DYNAMIC;
    setDynamicLabel(itemPos, "Rescan Networks");
  }
}

static void updateWifiTitle(void)
{
  if (wifiManager.connecting)
  {
    snprintf(wifiTitleBuf, sizeof(wifiTitleBuf), "Wi-Fi: Connecting...");
  }
  else if (wifiManager.scanning)
  {
    snprintf(wifiTitleBuf, sizeof(wifiTitleBuf), "Wi-Fi: Scanning...");
  }
  else if (wifiManager.connected && wifiManager.ip_addr[0] != '\0')
  {
    snprintf(wifiTitleBuf, sizeof(wifiTitleBuf), "Wi-Fi: %s", wifiManager.ip_addr);
  }
  else
  {
    snprintf(wifiTitleBuf, sizeof(wifiTitleBuf), "Wi-Fi: Disconnected");
  }
  listViewSetTitle((LABEL){.address = wifiTitleBuf});
}

void menuWifiSettings(void)
{
  wifiCurPage = 0;
  wifiManager.ap_count = 0;
  wifiManager.scanning = true;

  updateWifiTitle();

  mustStoreCmd("M980\n");
  mustStoreCmd("M981\n");

  uint16_t totalItems = (wifiManager.ap_count == 0) ? 1 : (wifiManager.ap_count + 1);
  listViewCreate((LABEL){.address = wifiTitleBuf}, NULL, totalItems, &wifiCurPage, false, NULL, loadWifiItem);

  uint8_t last_ap_count = 0;
  bool last_scanning = true;
  bool last_connecting = false;
  bool last_connected = false;

  while (MENU_IS(menuWifiSettings))
  {
    uint16_t key = listViewGetSelectedIndex();

    switch (key)
    {
      case KEY_BACK:
        CLOSE_MENU();
        break;

      default:
        if (wifiManager.ap_count == 0)
        {
          // tap to rescan
          if (!wifiManager.scanning)
          {
            wifiManager.scanning = true;
            mustStoreCmd("M981\n");
            updateWifiTitle();
            listViewRefreshMenu();
          }
        }
        else if (key < wifiManager.ap_count)
        {
          // Selected an AP to connect
          uint16_t sel = key;
          char pass[MAX_PASS_LEN] = {0};
          bool proceed = true;

          if (wifiManager.aps[sel].secured)
          {
            proceed = keyboardGetText(wifiManager.aps[sel].ssid, pass, sizeof(pass));
          }

          if (proceed)
          {
            char hex_ssid[MAX_SSID_LEN * 2 + 1];
            char hex_pass[MAX_PASS_LEN * 2 + 1];
            toHex(wifiManager.aps[sel].ssid, hex_ssid, sizeof(hex_ssid));
            toHex(pass, hex_pass, sizeof(hex_pass));

            wifiManager.connecting = true;
            snprintf(wifiManager.status_msg, sizeof(wifiManager.status_msg), "Connecting to %s...", wifiManager.aps[sel].ssid);
            updateWifiTitle();

            if (pass[0] != '\0')
              mustStoreCmd("M982 S%s P%s\n", hex_ssid, hex_pass);
            else
              mustStoreCmd("M982 S%s\n", hex_ssid);
          }

          // Re-create list view after returning from keyboard
          totalItems = (wifiManager.ap_count == 0) ? 1 : (wifiManager.ap_count + 1);
          listViewCreate((LABEL){.address = wifiTitleBuf}, NULL, totalItems, &wifiCurPage, false, NULL, loadWifiItem);
        }
        else if (key == wifiManager.ap_count)
        {
          // Rescan Networks tapped
          wifiManager.ap_count = 0;
          wifiManager.scanning = true;
          mustStoreCmd("M981\n");
          updateWifiTitle();
          totalItems = 1;
          listViewCreate((LABEL){.address = wifiTitleBuf}, NULL, totalItems, &wifiCurPage, false, NULL, loadWifiItem);
        }
        break;
    }

    if (wifiManager.ap_count != last_ap_count ||
        wifiManager.scanning != last_scanning ||
        wifiManager.connecting != last_connecting ||
        wifiManager.connected != last_connected)
    {
      last_ap_count = wifiManager.ap_count;
      last_scanning = wifiManager.scanning;
      last_connecting = wifiManager.connecting;
      last_connected = wifiManager.connected;

      updateWifiTitle();
      totalItems = (wifiManager.ap_count == 0) ? 1 : (wifiManager.ap_count + 1);
      listViewCreate((LABEL){.address = wifiTitleBuf}, NULL, totalItems, &wifiCurPage, false, NULL, loadWifiItem);
    }

    loopProcess();
  }
}
