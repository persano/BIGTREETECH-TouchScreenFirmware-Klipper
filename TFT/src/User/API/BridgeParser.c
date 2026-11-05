#include "BridgeParser.h"
#include "includes.h"
#include "WifiSettings.h"
#include "KlipperMacros.h"
#include "KlipperParams.h"
#include "boot.h"
#include <stdlib.h>
#include <string.h>

typedef enum {
  THUMB_STATE_IDLE = 0,
  THUMB_STATE_WAIT_HEADER,
  THUMB_STATE_STREAMING,
} THUMB_STREAM_STATE;

typedef struct {
  THUMB_STREAM_STATE state;
  uint8_t dest;
  GUI_POINT target_pos;
  uint8_t slot;
  uint16_t width;
  uint16_t height;
  uint16_t pixels_received;
  uint16_t total_pixels;
  uint32_t start_time;
  bool done;

  // SPI Flash write buffer
  uint32_t flash_addr;
  uint16_t flash_bnum;
  uint8_t flash_buf[256];
} THUMB_STREAM_INFO;

static THUMB_STREAM_INFO thumbStream = {THUMB_STATE_IDLE, THUMB_DEST_NONE, {0, 0}, 0, 0, 0, 0, 0, 0, false, 0, 0, {0}};
static bool slotThumbValid[THUMB_CACHE_MAX_SLOTS] = {false, false, false, false, false};

void bridgeInvalidateThumbCache(void)
{
  for (uint8_t i = 0; i < THUMB_CACHE_MAX_SLOTS; i++)
    slotThumbValid[i] = false;
}

bool bridgeIsSlotThumbValid(uint8_t slot)
{
  if (slot < THUMB_CACHE_MAX_SLOTS)
    return slotThumbValid[slot];
  return false;
}

void bridgeCopyThumbToPreview(uint8_t slot)
{
  if (slot >= THUMB_CACHE_MAX_SLOTS || !slotThumbValid[slot])
    return;

  uint32_t src_addr = THUMB_SLOT_ADDR(slot);
  uint32_t dst_addr = ICON_ADDR(ICON_PREVIEW);
  uint16_t w = 0, h = 0;
  W25Qxx_ReadBuffer((uint8_t *)&w, src_addr, 2);
  W25Qxx_ReadBuffer((uint8_t *)&h, src_addr + 2, 2);
  if (w == 0 || w > 160 || h == 0 || h > 160)
    return;

  uint32_t total_bytes = (uint32_t)w * h * 2 + 4;
  uint16_t sectors = (total_bytes + W25QXX_SECTOR_SIZE - 1) / W25QXX_SECTOR_SIZE;
  for (uint16_t b = 0; b < sectors; b++)
  {
    W25Qxx_EraseSector(dst_addr + b * W25QXX_SECTOR_SIZE);
  }

  uint8_t buf[256];
  uint32_t bytes_copied = 0;
  while (bytes_copied < total_bytes)
  {
    uint16_t chunk = (total_bytes - bytes_copied > 256) ? 256 : (total_bytes - bytes_copied);
    W25Qxx_ReadBuffer(buf, src_addr + bytes_copied, chunk);
    W25Qxx_WritePage(buf, dst_addr + bytes_copied, chunk);
    bytes_copied += chunk;
  }
  setPrintModelIcon(true);
}

static void bridgeThumbnailDone(void);
static void bridgeThumbnailParseLine(char * line);

static inline uint8_t hexCharVal(char c)
{
  if (c >= '0' && c <= '9') return c - '0';
  if (c >= 'a' && c <= 'f') return c - 'a' + 10;
  if (c >= 'A' && c <= 'F') return c - 'A' + 10;
  return 0xFF;
}

static inline uint16_t parseHex16(const char * p)
{
  uint8_t h0 = hexCharVal(p[0]);
  uint8_t h1 = hexCharVal(p[1]);
  uint8_t h2 = hexCharVal(p[2]);
  uint8_t h3 = hexCharVal(p[3]);
  if ((h0 | h1 | h2 | h3) & 0x80) return 0;
  return ((uint16_t)h0 << 12) | ((uint16_t)h1 << 8) | ((uint16_t)h2 << 4) | (uint16_t)h3;
}

void bridgeThumbnailRequest(const char * filename, uint8_t dest, GUI_POINT pos, uint8_t slot)
{
  if (filename == NULL || *filename == '\0')
    return;

  // Erase flash sectors upfront before sending command to avoid blocking during serial streaming
  if (dest & (THUMB_DEST_FLASH | THUMB_DEST_CACHE))
  {
    uint32_t addr = (dest & THUMB_DEST_CACHE) ? THUMB_SLOT_ADDR(slot) : ICON_ADDR(ICON_PREVIEW);
    uint32_t total_bytes = (uint32_t)ICON_WIDTH * ICON_HEIGHT * 2 + 4;
    uint16_t sectors = (total_bytes + W25QXX_SECTOR_SIZE - 1) / W25QXX_SECTOR_SIZE;
    for (uint16_t b = 0; b < sectors; b++)
    {
      W25Qxx_EraseSector(addr + b * W25QXX_SECTOR_SIZE);
    }
  }

  thumbStream.state = THUMB_STATE_WAIT_HEADER;
  thumbStream.dest = dest;
  thumbStream.target_pos = pos;
  thumbStream.slot = slot;
  thumbStream.pixels_received = 0;
  thumbStream.total_pixels = 0;
  thumbStream.start_time = OS_GetTimeMs();
  thumbStream.done = false;

  char cmd[CMD_MAX_SIZE];
  strcpy(cmd, "M987 S");
  size_t len = strlen(cmd);

  const char * p = filename;
  while (*p && len + 3 < CMD_MAX_SIZE)
  {
    sprintf(cmd + len, "%02x", (unsigned char)*p);
    len += 2;
    p++;
  }
  strcat(cmd, "\n");

  mustStoreCmd("%s", cmd);
}

void bridgeThumbnailCancel(void)
{
  thumbStream.state = THUMB_STATE_IDLE;
  thumbStream.dest = THUMB_DEST_NONE;
  thumbStream.done = false;
  thumbStream.pixels_received = 0;
}

bool bridgeThumbnailIsActive(void)
{
  return (thumbStream.state != THUMB_STATE_IDLE);
}

bool bridgeThumbnailIsDone(void)
{
  if (thumbStream.state != THUMB_STATE_IDLE && (OS_GetTimeMs() - thumbStream.start_time > 6000))
  {
    thumbStream.state = THUMB_STATE_IDLE;
    thumbStream.done = true;
  }
  return thumbStream.done;
}

void bridgeThumbnailAcknowledgeDone(void)
{
  thumbStream.done = false;
}

static void bridgeThumbnailDone(void)
{
  if (thumbStream.state == THUMB_STATE_IDLE || thumbStream.state == THUMB_STATE_WAIT_HEADER)
    return;

  if (thumbStream.dest & (THUMB_DEST_FLASH | THUMB_DEST_CACHE))
  {
    if (thumbStream.flash_bnum > 0)
    {
      W25Qxx_WritePage(thumbStream.flash_buf, thumbStream.flash_addr, thumbStream.flash_bnum);
      thumbStream.flash_bnum = 0;
    }
    if (thumbStream.pixels_received > 0)
    {
      if (thumbStream.dest & THUMB_DEST_FLASH)
        setPrintModelIcon(true);
      if (thumbStream.dest & THUMB_DEST_CACHE && thumbStream.slot < THUMB_CACHE_MAX_SLOTS)
        slotThumbValid[thumbStream.slot] = true;
    }
  }

  thumbStream.state = THUMB_STATE_IDLE;
  thumbStream.done = true;
}

static void bridgeThumbnailParseLine(char * line)
{
  if (thumbStream.state == THUMB_STATE_IDLE)
    return;

  while (*line == ' ' || *line == ';')
    line++;

  if (*line == '\0')
    return;

  if (thumbStream.state == THUMB_STATE_WAIT_HEADER)
  {
    if (strlen(line) >= 8)
    {
      uint16_t w = parseHex16(line);
      uint16_t h = parseHex16(line + 4);

      if (w > 0 && w <= 160 && h > 0 && h <= 160)
      {
        thumbStream.width = w;
        thumbStream.height = h;
        thumbStream.total_pixels = w * h;
        thumbStream.pixels_received = 0;

        if (thumbStream.dest & THUMB_DEST_LCD)
        {
          LCD_SetWindow(thumbStream.target_pos.x, thumbStream.target_pos.y,
                        thumbStream.target_pos.x + w - 1, thumbStream.target_pos.y + h - 1);
        }

        if (thumbStream.dest & (THUMB_DEST_FLASH | THUMB_DEST_CACHE))
        {
          if (thumbStream.dest & THUMB_DEST_CACHE)
            thumbStream.flash_addr = THUMB_SLOT_ADDR(thumbStream.slot);
          else
            thumbStream.flash_addr = ICON_ADDR(ICON_PREVIEW);

          memcpy(thumbStream.flash_buf, &w, 2);
          memcpy(thumbStream.flash_buf + 2, &h, 2);
          thumbStream.flash_bnum = 4;
        }

        thumbStream.state = THUMB_STATE_STREAMING;
        return;
      }
    }
  }

  if (thumbStream.state == THUMB_STATE_STREAMING)
  {
    const char * p = line;
    while (*p && thumbStream.pixels_received < thumbStream.total_pixels)
    {
      if (hexCharVal(p[0]) == 0xFF)
      {
        p++;
        continue;
      }
      if (hexCharVal(p[1]) == 0xFF || hexCharVal(p[2]) == 0xFF || hexCharVal(p[3]) == 0xFF)
        break;

      uint16_t color = parseHex16(p);
      p += 4;

      if (thumbStream.dest & THUMB_DEST_LCD)
      {
        LCD_WR_16BITS_DATA(color);
      }

      if (thumbStream.dest & (THUMB_DEST_FLASH | THUMB_DEST_CACHE))
      {
        thumbStream.flash_buf[thumbStream.flash_bnum++] = (uint8_t)(color >> 8);
        thumbStream.flash_buf[thumbStream.flash_bnum++] = (uint8_t)(color & 0xFF);
        if (thumbStream.flash_bnum == 256)
        {
          W25Qxx_WritePage(thumbStream.flash_buf, thumbStream.flash_addr, 256);
          thumbStream.flash_addr += 256;
          thumbStream.flash_bnum = 0;
        }
      }

      thumbStream.pixels_received++;
    }

    if (thumbStream.pixels_received >= thumbStream.total_pixels)
    {
      bridgeThumbnailDone();
    }
  }
}

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

  if (strncmp(msg, "thumb:", 6) == 0)
  {
    bridgeThumbnailParseLine(msg + 6);
  }
  else if (strcmp(msg, "thumb_done") == 0)
  {
    if (thumbStream.state == THUMB_STATE_STREAMING)
    {
      bridgeThumbnailDone();
    }
    else if (thumbStream.state == THUMB_STATE_WAIT_HEADER)
    {
      thumbStream.state = THUMB_STATE_IDLE;
      thumbStream.done = true;
    }
  }
  else if (strncmp(msg, "wifi_status:", 12) == 0)
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
