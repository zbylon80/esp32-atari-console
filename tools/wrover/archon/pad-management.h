#pragma once
#include <stdint.h>

enum class PadCommand : uint8_t {Pair1,Pair2,CancelPairing,Swap,Forget1,Forget2};
enum class PadNotice : uint8_t {None,Paired,Swapped,Forgotten,TimedOut,SaveError,Occupied,Cancelled};
struct PadStatus {
    uint32_t revision=0;
    uint32_t commandSequence=0;
    bool known[2]={},connected[2]={};
    int8_t pairingPlayer=-1;
    uint8_t seconds=0;
    PadNotice notice=PadNotice::None;
};

// The input task owns Bluetooth and NVS. The emulator task only uses queues.
PadStatus drop_pad_status();
bool drop_pad_request(PadCommand command);
void console_pads_ui(uint8_t** screen,int kind,int selected,int player,const PadStatus& status);
