#include "emu.h"
// Keep legacy upstream HID declarations out of the Bluepad32 translation unit.
extern "C" {
wii_state wii_states[4]={};
uint32_t wii_map(int,const uint32_t*,const uint32_t*){return 0;}
}
