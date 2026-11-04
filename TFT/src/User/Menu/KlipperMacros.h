#ifndef _KLIPPER_MACROS_H_
#define _KLIPPER_MACROS_H_

#ifdef __cplusplus
extern "C" {
#endif

#include <stdbool.h>
#include <stdint.h>

#define MAX_KLIPPER_MACROS 32
#define MAX_MACRO_NAME_LEN 32

typedef struct {
  char names[MAX_KLIPPER_MACROS][MAX_MACRO_NAME_LEN];
  uint8_t count;
  bool loaded;
  bool loading;
} KLIPPER_MACROS;

extern KLIPPER_MACROS klipperMacros;

void menuKlipperMacros(void);
void klipperMacroAdd(const char * name);
void klipperMacroDone(void);
void klipperMacroReset(void);

#ifdef __cplusplus
}
#endif

#endif
