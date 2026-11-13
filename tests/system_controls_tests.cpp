#include "../tools/wrover/archon/system-controls.h"
#include <cstdio>
#include <stdexcept>
#include <algorithm>
using namespace esp32controller;
#define CHECK(x) do {if(!(x))throw std::runtime_error(#x);}while(false)
int main(){try{
    SystemBindings b,decoded;
    CHECK(b.valid()&&SystemBindings::decode(b.encode(),decoded));
    CHECK(decoded.encode()==b.encode());
    const auto original=decoded.encode();
    for(uint32_t corrupt: {0u,0x534b0200u,0x534b0100u,0x534b013fu,0x534b0140u}){
        CHECK(!SystemBindings::decode(corrupt,decoded));CHECK(decoded.encode()==original);
    }
    auto view=mask(Button::View),menu=mask(Button::Menu),share=mask(Button::Share);
    uint8_t permutation[]={0,1,2};unsigned count=0;
    do{
        for(unsigned i=0;i<3;i++)b.buttons[i]=permutation[i];
        CHECK(SystemBindings::decode(b.encode(),decoded));
        for(unsigned i=0;i<3;i++){
            auto copy=b;copy.cycle(i);CHECK(copy.valid());CHECK(copy.buttons[i]==(b.buttons[i]+1)%3);
            SystemControls controls;controls.update(true,0,0,0,b);
            const auto button=SystemBindings::physical(b.buttons[i]);
            auto down=controls.update(true,button,button,1,b);
            CHECK(!down.openMenu);
            if(button==view){
                CHECK(down.held==0&&down.pressed==0);
                auto up=controls.update(true,0,0,100,b);CHECK(up.pressed==(1u<<i));
            }else CHECK(down.held==(1u<<i)&&down.pressed==(1u<<i));
        }
        ++count;
    }while(std::next_permutation(permutation,permutation+3));CHECK(count==6);
    b=SystemBindings{};
    SystemControls p,q;p.update(true,0,0,0,b);q.update(true,0,0,0,b);
    p.update(true,view,view,100,b);
    CHECK(p.update(true,view,0,799,b).pressed==0);
    auto shortRelease=p.update(true,0,0,799,b);CHECK(shortRelease.pressed==2&&!shortRelease.openMenu);
    CHECK(p.update(true,0,0,800,b).pressed==0);
    p.update(true,view,view,1000,b);
    CHECK(!p.update(true,view,0,1699,b).openMenu);
    CHECK(p.update(true,view,0,1700,b).openMenu);
    CHECK(!p.update(true,view,0,1800,b).openMenu);
    CHECK(p.update(true,0,0,1900,b).pressed==0);
    p.update(true,view,view,2000,b);
    auto lateRelease=p.update(true,0,0,2700,b);CHECK(lateRelease.openMenu&&lateRelease.pressed==0);
    // Timer rollover and independent controller state.
    p.update(true,view,view,0xffffff00u,b);
    CHECK(p.update(true,view,0,0xffffff00u+700u,b).openMenu);
    CHECK(q.update(true,share,share,25,b).pressed==4);
    CHECK(q.update(true,0,0,26,b).held==0);
    // A complete tap captured by the polling task between emulation frames.
    CHECK(q.update(true,0,view,30,b).pressed==2);
    CHECK(q.update(true,0,menu,31,b).pressed==1);
    // Held controls at a UI/configuration/session boundary never become actions.
    p.reset();CHECK(p.update(true,view|menu|share,view|menu|share,0,b).pressed==0);
    CHECK(!p.update(true,view|menu|share,0,1000,b).openMenu);
    CHECK(p.update(true,0,0,1001,b).pressed==0);
    CHECK(p.update(true,share,share,1002,b).pressed==4);
    CHECK(p.update(false,menu,menu,1003,b).held==0);
    CHECK(p.update(true,menu,menu,1004,b).pressed==0);
    p.update(true,0,0,1005,b);
    CHECK(p.update(true,menu,menu,1006,b).pressed==1);
    CHECK(q.update(true,mask(Button::LB)|mask(Button::RB),mask(Button::LB)|mask(Button::RB),200,b).held==0);
    std::puts("PASS six layouts, short/long View, rollover, release gates, disconnect and both pads");return 0;
}catch(const std::exception& e){std::printf("FAIL %s\n",e.what());return 1;}}
