#pragma once
#include <controller/ControllerState.h>

// Merge A/LT/RT before deriving edges: releasing one source must not release
// fire while another is held, nor generate a second press when sources overlap.
struct PadMapping {
    uint16_t held=0,pressed=0;
    bool left=false,right=false;
    void update(bool valid,uint16_t buttons,float lt,float rt){
        using namespace esp32controller;
        if(!valid){held=pressed=0;left=right=false;return;}
        left=lt>=(left?.15f:.25f);right=rt>=(right?.15f:.25f);
        uint16_t next=buttons;
        if(left||right)next|=mask(Button::A);
        pressed=next&~held;held=next;
    }
};
