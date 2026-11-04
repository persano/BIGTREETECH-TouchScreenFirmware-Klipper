#ifndef _HEATSOAK_H_
#define _HEATSOAK_H_

#ifdef __cplusplus
extern "C" {
#endif

#include <stdbool.h>
#include <stdint.h>

typedef struct {
  bool active;
  uint16_t duration_sec;
  uint16_t remaining_sec;
  int16_t target_bed_temp;
} HEATSOAK_STATE;

extern HEATSOAK_STATE heatsoakState;

void heatsoakStart(uint16_t duration_sec, int16_t target_temp);
void heatsoakStop(void);
void menuHeatsoak(void);

#ifdef __cplusplus
}
#endif

#endif
