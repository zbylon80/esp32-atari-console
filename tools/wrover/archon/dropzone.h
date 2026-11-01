#pragma once
#include <Arduino.h>
class Emu;
struct DropInput { uint16_t held; float lx,ly; bool valid; uint32_t changedUs; uint16_t physicalHeld;float lt,rt; };
bool drop_begin();
void drop_poll();
DropInput drop_input(int player=0);
uint16_t drop_pressed(int player=0);
bool drop_connected();
bool drop_update_mode();
void drop_update_info();
void drop_perf_begin();
bool drop_perf_active();
void drop_perf_frame(uint32_t core,uint32_t copy,uint32_t audio);
void drop_perf_fail();
bool drop_frame_valid();
void drop_menu(bool active,int selected);
void drop_publish();
void drop_dump(Emu* emulator);
extern "C" bool drop_memory_init(uint8_t*** rows);
extern "C" bool drop_guards();
extern "C" void drop_clear();

unsigned archon_connected();

void archon_input_metrics(uint32_t* maxPollGap,uint32_t* maxPollCost);
