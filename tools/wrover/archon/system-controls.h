#pragma once
#include <cstdint>
#include <controller/ControllerState.h>

// Atari action indices also match the active-low CONSOL register bits.
struct SystemBindings {
    uint8_t buttons[3]={0,1,2}; // START, SELECT, OPTION -> Menu, View, Share
    static uint16_t physical(unsigned button){
        using namespace esp32controller;
        return mask(button==0?Button::Menu:button==1?Button::View:Button::Share);
    }
    static const char* name(unsigned button){return button==0?"MENU":button==1?"VIEW":"SHARE";}
    uint8_t action(unsigned button) const {
        for(unsigned i=0;i<3;i++)if(buttons[i]==button)return uint8_t(1u<<i);
        return 0;
    }
    bool valid() const {return buttons[0]<3&&buttons[1]<3&&buttons[2]<3&&buttons[0]!=buttons[1]&&buttons[0]!=buttons[2]&&buttons[1]!=buttons[2];}
    uint32_t encode() const {return 0x534b0100u|buttons[0]|(buttons[1]<<2)|(buttons[2]<<4);}
    static bool decode(uint32_t value,SystemBindings& out){
        if((value&~0x3fu)!=0x534b0100u)return false;
        SystemBindings candidate;
        for(unsigned i=0;i<3;i++)candidate.buttons[i]=(value>>(i*2))&3;
        if(!candidate.valid())return false;
        out=candidate;return true;
    }
    void cycle(unsigned actionIndex){
        if(actionIndex>=3)return;
        unsigned next=(buttons[actionIndex]+1)%3;
        for(unsigned i=0;i<3;i++)if(buttons[i]==next){buttons[i]=buttons[actionIndex];break;}
        buttons[actionIndex]=next;
    }
};

struct SystemInput {uint8_t held=0,pressed=0;bool openMenu=false;};
struct SystemControls {
    static constexpr uint32_t holdMs=700;
    static constexpr uint16_t central=(1u<<10)|(1u<<11)|(1u<<14);
    uint16_t previous=0,blocked=central;
    uint32_t viewSince=0;
    bool viewDown=false,viewLong=false;
    void reset(){previous=0;blocked=central;viewDown=viewLong=false;}
    SystemInput update(bool valid,uint16_t held,uint16_t edges,uint32_t now,const SystemBindings& bindings){
        SystemInput out;
        if(!valid){reset();return out;}
        blocked&=held;
        held&=central&~blocked;edges&=central&~blocked;
        uint16_t fresh=(held&~previous)|edges;
        for(unsigned button=0;button<3;button+=2){
            auto bit=SystemBindings::physical(button);auto action=bindings.action(button);
            if(held&bit)out.held|=action;
            if(fresh&bit)out.pressed|=action;
        }
        const auto view=SystemBindings::physical(1);
        if(held&view){
            if(!viewDown){viewSince=now;viewDown=true;viewLong=false;}
            if(!viewLong&&uint32_t(now-viewSince)>=holdMs){viewLong=true;out.openMenu=true;}
        }else{
            if(viewDown){
                // A release observed after the deadline is still a long press.
                if(!viewLong){
                    if(uint32_t(now-viewSince)>=holdMs)out.openMenu=true;
                    else out.pressed|=bindings.action(1);
                }
            }else if(fresh&view)out.pressed|=bindings.action(1); // A tap between frames.
            viewDown=viewLong=false;
        }
        previous=held;return out;
    }
};
