#ifndef _KEYBOARD_H_
#define _KEYBOARD_H_

#ifdef __cplusplus
extern "C" {
#endif

#include <stdbool.h>
#include <stdint.h>
#include <stddef.h>

void toHex(const char * in, char * out, size_t max_out);
bool keyboardGetText(const char * title, char * target_buf, uint16_t max_len);

#ifdef __cplusplus
}
#endif

#endif
