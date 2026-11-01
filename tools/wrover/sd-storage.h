#pragma once
bool console_sd_mount(int clockKhz=1000);
bool console_sd_no_response();
void console_sd_unmount();
constexpr const char* console_sd_root = "/sdcard/atari-console/games";
