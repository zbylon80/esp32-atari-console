#pragma once
#include <Bluepad32.h>
#include <Esp32AtariController.h>
#include <bt/uni_bt_setup.h>
#include <nvs.h>
#include <cstring>
#include "pad-management.h"

// One radio host, two independent logical backends. Device identity rather
// than discovery order preserves the user's player assignment.
class TwoPadHub {
public:
    esp32controller::BackendFrame frames[2];
    bool begin(){
        instance=this;nvs_handle_t n;
        if(nvs_open("atari_pads",NVS_READONLY,&n)==ESP_OK){
            uint8_t roles[14];size_t size=sizeof(roles);
            if(nvs_get_blob(n,"roles",roles,&size)==ESP_OK&&size==sizeof(roles)){
                for(int i=0;i<2;i++){known[i]=roles[i]==1;memcpy(addresses[i],roles+2+i*6,6);}
            }else{
                // Upgrade existing consoles without changing their identities.
                for(int i=0;i<2;i++){size=6;known[i]=nvs_get_blob(n,i?"p2":"p1",addresses[i],&size)==ESP_OK&&size==6;}
            }
            nvs_close(n);
        }
        BP32.setup(onConnect,onDisconnect);
        if(!known[0]&&!known[1])startPairing(0,0); // First use: no pad to operate a menu yet.
        else BP32.enableNewBluetoothConnections(false);
        Serial.printf("[PADS] persistent_p1=%d persistent_p2=%d\n",known[0],known[1]);
        return true;
    }
    PadStatus status(uint32_t now)const{
        PadStatus s{};s.revision=revision;s.commandSequence=commandSequence;s.notice=notice;s.pairingPlayer=pairing;
        for(int i=0;i<2;i++){s.known[i]=known[i];s.connected[i]=frames[i].connected;}
        int32_t remaining=int32_t(pairingDeadline-now);
        s.seconds=pairing>=0&&pairingTimed&&remaining>0?uint8_t((remaining+999)/1000):0;
        return s;
    }
    void command(PadCommand action,uint32_t now){
        ++commandSequence;
        if(action==PadCommand::Pair1||action==PadCommand::Pair2){
            int p=action==PadCommand::Pair1?0:1;
            if(known[p]){notice=PadNotice::Occupied;++revision;return;}
            startPairing(p,now+60000);pairingTimed=true;return;
        }
        if(action==PadCommand::CancelPairing){stopPairing();notice=PadNotice::Cancelled;++revision;return;}
        uint8_t next[2][6];bool present[2];memcpy(next,addresses,sizeof(next));memcpy(present,known,sizeof(present));
        if(action==PadCommand::Swap){
            for(int j=0;j<6;j++){auto b=next[0][j];next[0][j]=next[1][j];next[1][j]=b;}
            bool b=present[0];present[0]=present[1];present[1]=b;
        }else{
            int p=action==PadCommand::Forget1?0:1;present[p]=false;memset(next[p],0,6);
        }
        if(!save(next,present)){notice=PadNotice::SaveError;++revision;return;}
        stopPairing();memcpy(addresses,next,sizeof(next));memcpy(known,present,sizeof(present));
        for(auto& f:frames){f.connected=false;f.input={};f.hasReport=false;++f.session;}
        for(auto& e:pending)if(e.controller){
            int p=find(e.controller->getProperties().btaddr);e.player=p;e.releaseRequired=true;
            if(p<0){e.rejected=true;e.controller->disconnect();}
            else attach(e,p);
        }
        notice=action==PadCommand::Swap?PadNotice::Swapped:PadNotice::Forgotten;++revision;
        // If the last assignment was removed, a replacement must still be able
        // to connect. Rejected live devices cannot immediately enroll again.
        if(!known[0]&&!known[1])startPairing(0,0);
    }
    void update(){
        uint32_t now=millis();
        if(pairing>=0&&pairingTimed&&int32_t(now-pairingDeadline)>=0){stopPairing();notice=PadNotice::TimedOut;++revision;}
        bool updated=BP32.update();
        for(auto& f:frames){f.ready=uni_bt_setup_is_ready();f.hasReport=false;f.unsupportedReport=false;}
        for(auto& entry:pending){
            Controller* c=entry.controller;
            if(!c||entry.rejected||!c->isConnected()||!updated||!c->hasData())continue;
            if(!c->isGamepad())continue; // Classification waits for real reports.
            if(entry.player<0){
                auto properties=c->getProperties();int player=find(properties.btaddr);
                uint8_t addressBits=0;for(auto byte:properties.btaddr)addressBits|=byte;if(!addressBits)continue;
                if(player<0&&pairing<0){entry.rejected=true;c->disconnect();continue;}
                if(!c->a())entry.enrollmentArmed=true;
                if(player<0&&pairing>=0&&entry.enrollmentArmed&&c->a())player=pairing;
                if(player<0||claimed(player))continue;
                if(!known[player]){
                    uint8_t next[2][6];bool present[2];memcpy(next,addresses,sizeof(next));memcpy(present,known,sizeof(present));
                    memcpy(next[player],properties.btaddr,6);present[player]=true;
                    if(!save(next,present)){stopPairing();notice=PadNotice::SaveError;++revision;continue;}
                    memcpy(addresses,next,sizeof(next));memcpy(known,present,sizeof(present));
                    stopPairing();notice=PadNotice::Paired;++revision;
                    Serial.printf("[PAD_REGISTER] player=%d\n",player+1);
                }
                attach(entry,player);
                Serial.printf("[PAD_CONNECT] player=%d mac=%02x:%02x:%02x:%02x:%02x:%02x model=%d\n",player+1,
                    addresses[player][0],addresses[player][1],addresses[player][2],addresses[player][3],addresses[player][4],addresses[player][5],c->getModel());
            }
            auto& f=frames[entry.player];auto& raw=f.input;raw={};f.hasReport=true;
            using namespace esp32controller;
            if(c->a())raw.buttons|=mask(Button::A);if(c->b())raw.buttons|=mask(Button::B);
            if(c->x())raw.buttons|=mask(Button::X);if(c->y())raw.buttons|=mask(Button::Y);
            if(c->l1())raw.buttons|=mask(Button::LB);if(c->r1())raw.buttons|=mask(Button::RB);
            if(c->thumbL())raw.buttons|=mask(Button::L3);if(c->thumbR())raw.buttons|=mask(Button::R3);
            if(c->dpad()&DPAD_UP)raw.buttons|=mask(Button::Up);if(c->dpad()&DPAD_DOWN)raw.buttons|=mask(Button::Down);
            if(c->dpad()&DPAD_LEFT)raw.buttons|=mask(Button::Left);if(c->dpad()&DPAD_RIGHT)raw.buttons|=mask(Button::Right);
            if(c->miscStart())raw.buttons|=mask(Button::Menu);if(c->miscSelect())raw.buttons|=mask(Button::View);
            raw.lx=c->axisX();raw.ly=c->axisY();raw.rx=c->axisRX();raw.ry=c->axisRY();raw.lt=c->brake();raw.rt=c->throttle();
            if(entry.releaseRequired){
                if(!raw.buttons&&raw.lt<150&&raw.rt<150&&abs(raw.lx)<150&&abs(raw.ly)<150)entry.releaseRequired=false;
                raw={}; // Changing roles or enrolling never emits a game action.
            }
        }
    }
private:
    struct Pending{Controller* controller=nullptr;int player=-1;bool rejected=false,releaseRequired=true,enrollmentArmed=false;};
    Pending pending[BP32_MAX_CONTROLLERS];
    uint8_t addresses[2][6]={};bool known[2]={};
    int pairing=-1;uint32_t pairingDeadline=0,revision=1,commandSequence=0;bool pairingTimed=false;PadNotice notice=PadNotice::None;
    static TwoPadHub* instance;
    int find(const uint8_t* address)const{for(int i=0;i<2;i++)if(known[i]&&!memcmp(addresses[i],address,6))return i;return -1;}
    bool save(const uint8_t next[2][6],const bool present[2]){
        uint8_t roles[14]={};for(int i=0;i<2;i++){roles[i]=present[i];memcpy(roles+2+i*6,next[i],6);}
        nvs_handle_t n;esp_err_t error=nvs_open("atari_pads",NVS_READWRITE,&n);
        if(error==ESP_OK){error=nvs_set_blob(n,"roles",roles,sizeof(roles));if(error==ESP_OK)error=nvs_commit(n);nvs_close(n);}
        if(error!=ESP_OK)Serial.printf("[PAD_ERROR] identity save %s\n",esp_err_to_name(error));
        return error==ESP_OK;
    }
    void startPairing(int p,uint32_t deadline){pairing=p;pairingDeadline=deadline;pairingTimed=deadline!=0;notice=PadNotice::None;++revision;BP32.enableNewBluetoothConnections(true);}
    void stopPairing(){pairing=-1;pairingDeadline=0;pairingTimed=false;BP32.enableNewBluetoothConnections(false);}
    void attach(Pending& e,int p){
        e.player=p;e.releaseRequired=true;auto& f=frames[p];++f.session;f.connected=true;f.input={};
        auto properties=e.controller->getProperties();
        e.controller->getModelName().toCharArray(f.identity.modelName,sizeof(f.identity.modelName));
        f.identity.vendorId=properties.vendor_id;f.identity.productId=properties.product_id;f.identity.backendModel=e.controller->getModel();
        f.capabilities={};
        if(properties.vendor_id==0x045e&&e.controller->getModel()==Controller::CONTROLLER_TYPE_XBoxOneController){
            f.capabilities.knownButtons=f.capabilities.availableButtons=esp32controller::kAllButtons;
            f.capabilities.knownAxes=f.capabilities.availableAxes=esp32controller::kAllAxes;
            f.capabilities.source=esp32controller::CapabilitySource::DocumentedXboxProfile;
        }
        ++revision;
    }
    bool claimed(int player){for(auto& e:pending)if(e.player==player)return true;return false;}
    static void onConnect(Controller* c){
        for(auto& e:instance->pending)if(e.controller==c)return;
        for(auto& e:instance->pending)if(!e.controller){e.controller=c;e.player=-1;e.rejected=false;e.releaseRequired=true;e.enrollmentArmed=false;return;}
        c->disconnect();
    }
    static void onDisconnect(Controller* c){
        for(auto& e:instance->pending)if(e.controller==c){
            if(e.player>=0){auto& f=instance->frames[e.player];f.connected=false;f.input={};++instance->revision;Serial.printf("[PAD_DISCONNECT] player=%d\n",e.player+1);}
            e.controller=nullptr;e.player=-1;return;
        }
    }
};
class PlayerBackend:public esp32controller::IControllerBackend{
public:
    PlayerBackend(TwoPadHub& owner,int slot):hub(owner),player(slot){}
    bool begin()override{return true;}
    const esp32controller::BackendFrame& poll()override{return hub.frames[player];}
private:TwoPadHub& hub;int player;
};
