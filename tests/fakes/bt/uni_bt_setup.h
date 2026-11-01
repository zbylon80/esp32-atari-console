#pragma once
extern bool fakeHostReady;
inline bool uni_bt_setup_is_ready() { return fakeHostReady; }
