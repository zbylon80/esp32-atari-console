#pragma once
#include <stdint.h>
int library_test_begin();
bool library_test_active();
void library_test_input();
void library_test_frame(uint32_t used);
void library_test_fail();
