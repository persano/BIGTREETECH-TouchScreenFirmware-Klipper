#ifndef _BRIDGE_PARSER_H_
#define _BRIDGE_PARSER_H_

#ifdef __cplusplus
extern "C" {
#endif

#include <stdbool.h>
#include <stdint.h>
#include "GUI.h"

enum {
  THUMB_DEST_NONE  = 0,
  THUMB_DEST_LCD   = (1 << 0),
  THUMB_DEST_FLASH = (1 << 1),
  THUMB_DEST_BOTH  = (THUMB_DEST_LCD | THUMB_DEST_FLASH)
};

void parseBridgeMessage(char * msg);

void bridgeThumbnailRequest(const char * filename, uint8_t dest, GUI_POINT pos);
void bridgeThumbnailCancel(void);
bool bridgeThumbnailIsActive(void);
bool bridgeThumbnailIsDone(void);
void bridgeThumbnailAcknowledgeDone(void);

#ifdef __cplusplus
}
#endif

#endif
