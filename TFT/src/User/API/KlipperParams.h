#ifndef _KLIPPER_PARAMS_H_
#define _KLIPPER_PARAMS_H_

#ifdef __cplusplus
extern "C" {
#endif

#include <stdbool.h>
#include <stdint.h>

typedef struct {
  float pa;
  uint32_t accel;
  float scv;
  float z_offset;
  bool loaded;
} KLIPPER_PARAMS;

extern KLIPPER_PARAMS klipperParams;

void klipperParamsUpdate(float pa, uint32_t accel, float scv, float z_offset);

#ifdef __cplusplus
}
#endif

#endif
