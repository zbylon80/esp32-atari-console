#pragma once
#include <controller/ControllerState.h>

// Merge A/RT before deriving edges: releasing one source must not release
// fire while another is held, nor generate a second press when sources overlap.

struct PadMapping {
    uint16_t held=0,pressed=0;
    bool right=false;
    void update(bool valid,uint16_t buttons,float /*lt*/,float rt){
        using namespace esp32controller;
        if(!valid){held=pressed=0;right=false;return;}
        right=rt>=(right?.15f:.25f);
        uint16_t next=buttons;
        if(right)next|=mask(Button::A);
        pressed=next&~held;held=next;
    }
};
