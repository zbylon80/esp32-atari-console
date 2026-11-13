#pragma once
bool console_sd_mount(int clockKhz=1000);
bool console_sd_no_response();
int console_sd_last_error();
const char* console_sd_error_name();
void console_sd_unmount();
constexpr const char* console_sd_root = "/sdcard/atari-console/games";
