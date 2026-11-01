#pragma once
#include <stdint.h>
#ifdef __cplusplus
extern "C" {
#endif
uint16_t* drop_scanline_begin(int draw);
uint16_t* drop_scanline_next(void);
int drop_overlay_begin(void);
void drop_overlay_end(void);
#ifdef __cplusplus
}
#endif
