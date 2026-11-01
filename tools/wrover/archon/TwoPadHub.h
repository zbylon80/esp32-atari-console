#pragma once
#include <Bluepad32.h>
#include <Esp32AtariController.h>
#include <bt/uni_bt_setup.h>
#include <nvs.h>
#include <cstring>

// One radio host, two independent logical backends. Device identity rather
// than discovery order preserves the user's player assignment.
class TwoPadHub {
public:
    esp32controller::BackendFrame frames[2];
    bool begin(){
        instance=this;nvs_handle_t n;
        if(nvs_open("atari_pads",NVS_READONLY,&n)==ESP_OK){
            for(int i=0;i<2;i++){size_t size=6;known[i]=nvs_get_blob(n,i?"p2":"p1",addresses[i],&size)==ESP_OK&&size==6;}
            nvs_close(n);
        }
        BP32.setup(onConnect,onDisconnect);
        Serial.printf("[PADS] persistent_p1=%d persistent_p2=%d; second pad must stay off until P1 registration\n",known[0],known[1]);
        return true;
    }
    void update(){
        bool updated=BP32.update();
        for(auto& f:frames){f.ready=uni_bt_setup_is_ready();f.hasReport=false;f.unsupportedReport=false;}
        for(auto& entry:pending){
            Controller* c=entry.controller;
            if(!c||!c->isConnected()||!updated||!c->hasData())continue;
            if(!c->isGamepad())continue; // Classification waits for real reports.
            if(entry.player<0){
                auto properties=c->getProperties();int player=-1;
                uint8_t addressBits=0;for(auto byte:properties.btaddr)addressBits|=byte;if(!addressBits)continue;
                for(int i=0;i<2;i++)if(known[i]&&!memcmp(addresses[i],properties.btaddr,6))player=i;
                if(player<0){for(int i=0;i<2;i++)if(!known[i]){player=i;break;}}
                if(player<0||claimed(player))continue;
                if(!known[player]){
                    nvs_handle_t n;esp_err_t error=nvs_open("atari_pads",NVS_READWRITE,&n);
                    if(error==ESP_OK){
                        error=nvs_set_blob(n,player?"p2":"p1",properties.btaddr,6);
                        if(error==ESP_OK)error=nvs_commit(n);nvs_close(n);
                    }
                    if(error!=ESP_OK){Serial.printf("[PAD_ERROR] identity save %s\n",esp_err_to_name(error));continue;}
                    memcpy(addresses[player],properties.btaddr,6);known[player]=true;
                    Serial.printf("[PAD_REGISTER] player=%d\n",player+1);
                }
                entry.player=player;auto& f=frames[player];++f.session;f.connected=true;f.input={};
                c->getModelName().toCharArray(f.identity.modelName,sizeof(f.identity.modelName));
                f.identity.vendorId=properties.vendor_id;f.identity.productId=properties.product_id;f.identity.backendModel=c->getModel();
                if(properties.vendor_id==0x045e&&c->getModel()==Controller::CONTROLLER_TYPE_XBoxOneController){
                    f.capabilities.knownButtons=f.capabilities.availableButtons=esp32controller::kAllButtons;
                    f.capabilities.knownAxes=f.capabilities.availableAxes=esp32controller::kAllAxes;
                    f.capabilities.source=esp32controller::CapabilitySource::DocumentedXboxProfile;
                }
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
        }
    }
private:
    struct Pending{Controller* controller=nullptr;int player=-1;};
    Pending pending[BP32_MAX_CONTROLLERS];
    uint8_t addresses[2][6]={};bool known[2]={};
    static TwoPadHub* instance;
    bool claimed(int player){for(auto& e:pending)if(e.player==player)return true;return false;}
    static void onConnect(Controller* c){
        for(auto& e:instance->pending)if(e.controller==c)return;
        for(auto& e:instance->pending)if(!e.controller){e.controller=c;e.player=-1;return;}
    }
    static void onDisconnect(Controller* c){
        for(auto& e:instance->pending)if(e.controller==c){
            if(e.player>=0){auto& f=instance->frames[e.player];f.connected=false;f.input={};Serial.printf("[PAD_DISCONNECT] player=%d\n",e.player+1);}
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
