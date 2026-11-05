#ifndef _BRIDGE_PARSER_H_
#define _BRIDGE_PARSER_H_

#ifdef __cplusplus
extern "C" {
#endif

#include <stdbool.h>
#include <stdint.h>
#include "GUI.h"

#define THUMB_CACHE_ADDR_START 0x7C0000
#define THUMB_CACHE_SLOT_SIZE  0x3000
#define THUMB_SLOT_ADDR(slot)  (THUMB_CACHE_ADDR_START + (slot) * THUMB_CACHE_SLOT_SIZE)
#define THUMB_CACHE_MAX_SLOTS  5

enum {
  THUMB_DEST_NONE  = 0,
  THUMB_DEST_LCD   = (1 << 0),
  THUMB_DEST_FLASH = (1 << 1),
  THUMB_DEST_CACHE = (1 << 2),
  THUMB_DEST_BOTH  = (THUMB_DEST_LCD | THUMB_DEST_FLASH)
};

void parseBridgeMessage(char * msg);

void bridgeThumbnailRequest(const char * filename, uint8_t dest, GUI_POINT pos, uint8_t slot);
void bridgeThumbnailCancel(void);
bool bridgeThumbnailIsActive(void);
bool bridgeThumbnailIsDone(void);
void bridgeThumbnailAcknowledgeDone(void);

void bridgeInvalidateThumbCache(void);
bool bridgeIsSlotThumbValid(uint8_t slot);
void bridgeCopyThumbToPreview(uint8_t slot);

#ifdef __cplusplus
}
#endif

#endif
