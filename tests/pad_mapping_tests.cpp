#include "../tools/wrover/archon/pad-mapping.h"
#include <cstdio>
#include <stdexcept>
using namespace esp32controller;
#define CHECK(x) do {if(!(x))throw std::runtime_error(#x);}while(false)
int main(){
    try {
        const auto a=mask(Button::A);
        PadMapping p,q;
        p.update(true,0,.24f,0);CHECK(p.held==0);
        p.update(true,0,.25f,0);CHECK(p.held==a&&p.pressed==a);
        p.update(true,0,.20f,0);CHECK(p.held==a&&p.pressed==0);
        p.update(true,a,0,0);CHECK(p.held==a&&p.pressed==0);
        p.update(true,a,0,1);CHECK(p.held==a&&p.pressed==0);
        p.update(true,0,0,.16f);CHECK(p.held==a&&p.pressed==0);
        p.update(true,0,0,.14f);CHECK(p.held==0&&p.pressed==0);
        p.update(true,0,0,.25f);CHECK(p.pressed==a);
        p.update(true,0,1,1);CHECK(p.pressed==0);
        p.update(true,0,1,0);CHECK(p.held==a&&p.pressed==0);
        q.update(true,mask(Button::View)|mask(Button::Menu),0,0);
        CHECK(q.held==(mask(Button::View)|mask(Button::Menu))&&!(q.held&a));
        CHECK(p.held==a); // Independent player state.
        p.update(false,a,1,1);CHECK(p.held==0&&p.pressed==0);
        p.update(true,0,.20f,.20f);CHECK(p.held==0); // Disconnect clears hysteresis.
        p.update(true,0,0,1);CHECK(p.pressed==a);
        std::puts("PASS trigger thresholds, overlap, release, disconnect and independent players");
        return 0;
    }catch(const std::exception& e){std::printf("FAIL %s\n",e.what());return 1;}
}
