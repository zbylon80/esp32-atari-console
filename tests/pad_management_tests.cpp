#include <cstdint>
#include <cstdlib>
#include <cstdio>
#include <cstring>
#include <map>
#include <vector>
#include <string>
#include <stdexcept>
#include "nvs.h"
static uint32_t now=100;
uint32_t millis(){return now;}
struct FakeSerial {template<class... T> void printf(const char*,T...) {}} Serial;
#include "../tools/wrover/archon/TwoPadHub.h"
FakeBluepad32 BP32;
bool fakeHostReady=true;
TwoPadHub* TwoPadHub::instance=nullptr;
static std::map<std::string,std::vector<uint8_t>> saved,staged;
static bool failCommit=false;
int nvs_open(const char*,int,nvs_handle_t* n){*n=1;return ESP_OK;}
int nvs_get_blob(int,const char* key,void* out,size_t* size){
    auto it=saved.find(key);if(it==saved.end()||*size<it->second.size())return 1;
    *size=it->second.size();memcpy(out,it->second.data(),*size);return ESP_OK;
}
int nvs_set_blob(int,const char* key,const void* p,size_t n){auto b=(const uint8_t*)p;staged[key]={b,b+n};return ESP_OK;}
int nvs_commit(int){if(failCommit)return 1;for(auto& p:staged)saved[p.first]=p.second;staged.clear();return ESP_OK;}
void nvs_close(int){staged.clear();}
#define CHECK(x) do{if(!(x))throw std::runtime_error(std::string(__func__)+": "+#x);}while(false)
static Controller pad(int id){Controller c;c.properties.btaddr[5]=uint8_t(id);c.gamepad=true;return c;}
static void connect(TwoPadHub& h,Controller& c){c.connected=true;c.fresh=true;BP32.connectCallback(&c);BP32.active=&c;BP32.pendingReport=true;h.update();}
static void report(TwoPadHub& h,Controller& c){BP32.active=&c;BP32.pendingReport=true;h.update();}
static void disconnect(Controller& c){BP32.disconnectCallback(&c);c.connected=false;c.fresh=false;}
static void reset(){saved.clear();staged.clear();failCommit=false;BP32=FakeBluepad32{};now=100;}

static void migrationAndSwap(){
    reset();auto a=pad(1),b=pad(2);
    saved["p1"]={a.properties.btaddr,a.properties.btaddr+6};saved["p2"]={b.properties.btaddr,b.properties.btaddr+6};
    saved["unrelated"]={42};TwoPadHub h;h.begin();CHECK(!BP32.discovery);
    connect(h,b);connect(h,a);CHECK(h.frames[0].connected&&h.frames[1].connected);
    a.aDown=true;report(h,a);CHECK(h.frames[0].input.buttons&esp32controller::mask(esp32controller::Button::A));
    h.command(PadCommand::Swap,now);CHECK(h.frames[0].input.buttons==0&&h.frames[1].input.buttons==0);
    report(h,a);CHECK(h.frames[1].input.buttons==0); // held fire cannot become a fresh press
    a.aDown=false;report(h,a);a.aDown=true;report(h,a);
    CHECK(h.frames[1].input.buttons&esp32controller::mask(esp32controller::Button::A));
    CHECK(saved["unrelated"][0]==42);
    disconnect(a);disconnect(b);TwoPadHub reboot;reboot.begin();connect(reboot,a);connect(reboot,b);
    a.aDown=false;report(reboot,a);a.dpadValue=DPAD_LEFT;report(reboot,a);
    CHECK(reboot.frames[1].input.buttons&esp32controller::mask(esp32controller::Button::Left));
}
static void deliberateEnrollmentAndRemoval(){
    reset();TwoPadHub h;h.begin();CHECK(h.status(now).pairingPlayer==0&&BP32.discovery);
    auto a=pad(1),b=pad(2),other=pad(3);connect(h,a);CHECK(!h.status(now).known[0]);
    a.aDown=true;report(h,a);CHECK(h.status(now).known[0]&&!BP32.discovery);
    a.aDown=false;report(h,a);connect(h,other);CHECK(other.disconnects==1);disconnect(other);
    h.command(PadCommand::Pair2,now);connect(h,b);b.aDown=true;report(h,b);
    CHECK(h.status(now).known[1]&&!BP32.discovery);b.aDown=false;report(h,b);
    h.command(PadCommand::Forget2,now);
    CHECK(h.status(now).known[0]&&!h.status(now).known[1]);CHECK(a.connected&&!b.connected);
    CHECK(h.status(now).connected[0]&&!h.status(now).connected[1]);disconnect(b);
    connect(h,b);CHECK(b.disconnects==2&&!h.status(now).known[1]);disconnect(b);
    h.command(PadCommand::Pair2,now);connect(h,b);b.aDown=true;report(h,b);CHECK(h.status(now).known[1]);
}
static void timeoutCancelAndFailedSave(){
    reset();auto a=pad(1);saved["p1"]={a.properties.btaddr,a.properties.btaddr+6};TwoPadHub h;h.begin();connect(h,a);
    h.command(PadCommand::Pair2,now);CHECK(h.status(now).seconds==60);
    now+=60001;h.update();CHECK(h.status(now).notice==PadNotice::TimedOut&&!BP32.discovery);
    h.command(PadCommand::Pair2,now);h.command(PadCommand::CancelPairing,now);CHECK(h.status(now).pairingPlayer<0&&!BP32.discovery);
    auto before=saved;failCommit=true;h.command(PadCommand::Forget1,now);
    CHECK(h.status(now).notice==PadNotice::SaveError&&h.status(now).known[0]&&a.connected);CHECK(saved==before);
    h.command(PadCommand::Swap,now);CHECK(h.status(now).known[0]&&!h.status(now).known[1]);
    failCommit=false;h.command(PadCommand::Forget1,now);CHECK(!h.status(now).known[0]&&BP32.discovery); // replacement can connect
}
static void heldEnrollmentAndClockWrap(){
    reset();TwoPadHub h;h.begin();auto a=pad(1);a.aDown=true;connect(h,a);CHECK(!h.status(now).known[0]);
    a.aDown=false;report(h,a);a.aDown=true;report(h,a);CHECK(h.status(now).known[0]);
    now=uint32_t(0)-60000;h.command(PadCommand::Pair2,now);CHECK(h.status(now).seconds==60);
    now=0;h.update();CHECK(h.status(now).pairingPlayer<0&&h.status(now).notice==PadNotice::TimedOut);
}
int main(){try{migrationAndSwap();deliberateEnrollmentAndRemoval();timeoutCancelAndFailedSave();heldEnrollmentAndClockWrap();puts("PASS pad management: persistence, migration, two players, release gate, removal, timeout and failed writes");}catch(const std::exception& e){puts(e.what());return 1;}}
