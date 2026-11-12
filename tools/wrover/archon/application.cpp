#include <Arduino.h>
#include <atomic>
#include <driver/ledc.h>
#include <freertos/event_groups.h>
#include <Esp32AtariController.h>
#include <nvs.h>
#include "emu.h"
#include "dropzone.h"
#include "controls.h"
#include "game-catalog.h"
#include "library-smoke.h"
#include "perf-report.h"
#include "video_out.h"
#include "pad-management.h"
#include "pad-mapping.h"
extern "C" {
#include "atari800/atari.h"
#include "atari800/memory.h"
#include "atari800/input.h"
#include "atari800/akey.h"
#include "atari800/libatari800.h"
#include "atari800/cartridge.h"
#include "atari800/cpu.h"
#include "atari800/sio.h"
#include "atari800/gtia.h"
#include "atari800/pokey.h"
}
using namespace esp32controller;
extern int _joy[4],_trig[4],console_keys;
extern int library_storage_error();
extern bool library_card_unavailable();
extern bool library_game_cached(int);
extern bool library_prepare_game(int);
static Emu* emulator;
static std::atomic<bool> ready{false};
static std::atomic<unsigned> commands{0};
static EventGroupHandle_t control;
enum class Screen {Library=0,Pause=2,Settings=3,Playing=4,Error=5,Help=6,BasicMenu=7,Controllers=9,PadDetails=10,Pairing=11,ForgetPad=12};
static Screen ui=Screen::Library;
static Screen settingsReturn=Screen::Library;
static Screen helpReturn=Screen::Library;
static int helpGame,helpSelection;
static int librarySelected;
static uint32_t repeatAt;
static bool diskSide;
static int lastGameError,keyPulseFrames;
static bool menuOpen=true,uiArmed[2],startBlocked[2];
static int owner,selected,lastDirection,syntheticStart,inject[2],game,genericStart,genericOption;
static bool basicMode;
static int managedPlayer;
static uint32_t padAwaitCommand,padDrawRevision;
static uint8_t padDrawSeconds;
static bool padScreen(){return int(ui)>=9&&int(ui)<=12;}
static const char* activeTitle(){return basicMode?"BASIC":game_info(game).title;}
static bool isArchon(){return !basicMode&&game_info(game).profile==GameProfile::Archon;}
static int nativePulse[3];
static uint8_t nativeModeAtPress[3],nativeSideAtPress[3],nativeAttractAtPress[3];
static void nativePress(int key){
    nativePulse[key]=120;nativeModeAtPress[key]=MEMORY_mem[0x600];
    nativeSideAtPress[key]=MEMORY_mem[0x601];nativeAttractAtPress[key]=MEMORY_mem[0x609];
}
static uint16_t suppressed[2];
void console_ui(uint8_t**,int,int,int,bool,int);
void console_set_language(bool english);
static bool english;
static void restoreLanguage(){
    nvs_handle_t settings;
    uint8_t value=0;
    if(nvs_open("atari_ui",NVS_READONLY,&settings)==ESP_OK){
        nvs_get_u8(settings,"english",&value);
        nvs_close(settings);
    }
    english=value==1;
    console_set_language(english);
}
static void toggleLanguage(){
    nvs_handle_t settings;
    if(nvs_open("atari_ui",NVS_READWRITE,&settings)!=ESP_OK)return;
    esp_err_t result=nvs_set_u8(settings,"english",!english);
    if(result==ESP_OK)result=nvs_commit(settings);
    nvs_close(settings);
    if(result==ESP_OK){
        english=!english;
        console_set_language(english);
    }
}
void console_boot_ui(uint8_t**,int,int,const char*);
void console_load_ui(uint8_t**,int,int,const char*);
void console_game_progress(int done,int total,const char* file){
    console_load_ui(_lines,done,total,game_info(game).title);
    Serial.printf("[LOAD_PROGRESS] done=%d total=%d file=%s video_fields=%d guards=%d\n",done,total,file,_frame_counter,drop_guards());
}
void console_boot_progress(int done,int total,const char* file){
    const char* title=file;
    if(file)for(int i=0;i<console_game_count;i++)if(!strcmp(file,game_info(i).file)||(game_info(i).disk2&&!strcmp(file,game_info(i).disk2))){title=game_info(i).title;break;}
    console_boot_ui(_lines,done,total,title);
    Serial.printf("[BOOT_PROGRESS] done=%d total=%d video_fields=%d guards=%d\n",done,total,_frame_counter,drop_guards());
}
static void drawUi(){
    if(padScreen()){
        auto s=drop_pad_status();padDrawRevision=s.revision;padDrawSeconds=s.seconds;
        console_pads_ui(emulator->video_buffer(),int(ui),selected,managedPlayer,s);
    }else console_ui(emulator->video_buffer(),int(ui),selected,ui==Screen::Help?helpGame:game,diskSide,ui==Screen::Help?int(helpReturn):lastGameError);
}
static void showUi(Screen screen){ui=screen;menuOpen=screen!=Screen::Playing;selected=screen==Screen::Library?librarySelected:0;lastDirection=0;repeatAt=0;uiArmed[0]=uiArmed[1]=false;
    if(menuOpen){for(auto& pulse:nativePulse)pulse=0;genericOption=keyPulseFrames=0;drawUi();}Serial.printf("[UI] screen=%d game=%d\n",int(ui),game);
}
static void rememberGame(int index){
    librarySelected=index;nvs_handle_t n;
    if(nvs_open("atari_ui",NVS_READWRITE,&n)==ESP_OK){nvs_set_str(n,"lastGame",game_info(index).id);nvs_commit(n);nvs_close(n);}
}
static void restoreSelection(){
    nvs_handle_t n;char id[40]={};size_t size=sizeof(id);
    if(nvs_open("atari_ui",NVS_READONLY,&n)==ESP_OK){nvs_get_str(n,"lastGame",id,&size);nvs_close(n);}
    for(int i=0;i<console_game_count;i++)if(!strcmp(id,game_info(i).id))librarySelected=i;
}
static void openSettings(Screen from){settingsReturn=from;showUi(Screen::Settings);}
static void openHelp(Screen from,int index){helpReturn=from;helpGame=index;helpSelection=selected;showUi(Screen::Help);}
static void closeHelp(){showUi(helpReturn);selected=helpSelection;drawUi();}
static uint32_t nextFrame,lastStatus,lastVideo,frames,totalUs,maxUs;
static uint32_t costCore,costCopy,costSound,costOut;
static int16_t audio[640];
static bool synchronizedVideo=true;static int lastRenderField=-1;
extern volatile uint32_t archon_lead_min,archon_crossings;
static uint32_t inputSeen[2],inputSamples[2],inputMaxAge[2],phaseMin=262,phaseMax;
static constexpr uint16_t face=mask(Button::A)|mask(Button::B)|mask(Button::X)|mask(Button::Y)|mask(Button::Menu)|mask(Button::LB)|mask(Button::RB)|kAtariStart;
static void saveFailure(int error){
    lastGameError=error;PerfReport r={};r.magic=perfMagic;r.request=esp_random();
    snprintf(r.variant,sizeof(r.variant),"failure-%s",game_info(game).id);
    r.frames=Atari800_nframes;r.elapsedMs=millis();r.guards=drop_guards();r.connected=archon_connected();
    r.pc=CPU_regPC;r.a=error;r.x=CPU_cim_encountered;r.y=drop_frame_valid();r.p=CPU_regP;
    r.internalFree=heap_caps_get_free_size(MALLOC_CAP_INTERNAL|MALLOC_CAP_8BIT);
    nvs_handle_t n;if(nvs_open("atari_perf",NVS_READWRITE,&n)==ESP_OK){nvs_set_blob(n,"result",&r,sizeof(r));nvs_commit(n);nvs_close(n);}
}
static bool loadGame(){
    diskSide=false;
    char path[64];snprintf(path,sizeof(path),"/game/%s",game_info(game).file);
    int result=emulator->insert(path,1,0);
    // Spy reads KBCODE's Control bit even with no keyboard key down. The
    // emulator's power-on $FF latch otherwise looks like held Control/fire.
    // Clear only this uninitialized latch, without generating a key event.
    if(game_info(game).profile==GameProfile::Spy&&POKEY_KBCODE==0xff&&INPUT_key_code==AKEY_NONE)POKEY_KBCODE=0;
    bool ok=result==0&&(CARTRIDGE_main.type==game_info(game).cartType)&&MEMORY_ram_size==64&&drop_guards();
    Serial.printf("[GAME_READY] game=%s result=%d cart=%d ram_kb=%d guards=%d ready=%d\n",game_info(game).title,result,CARTRIDGE_main.type,MEMORY_ram_size,drop_guards(),ok);
    if(!ok)saveFailure(library_storage_error()?library_storage_error():libatari800_error_code?libatari800_error_code:1001);return ok;
}
static bool beginBasic(){
    basicMode=true;syntheticStart=genericStart=genericOption=keyPulseFrames=0;
    for(auto& pulse:nativePulse)pulse=0;
    for(int i=0;i<4;i++){_joy[i]=0;_trig[i]=0;}
    console_keys=INPUT_key_consol=7;INPUT_key_code=AKEY_NONE;
    int result=emulator->insert(":basic:",1,0);
    bool ok=result==0&&!Atari800_disable_basic&&CARTRIDGE_main.type==CARTRIDGE_NONE&&MEMORY_ram_size==64;
    Serial.printf("[BASIC_READY] result=%d enabled=%d ram_kb=%d guards=%d ready=%d\n",result,!Atari800_disable_basic,MEMORY_ram_size,drop_guards(),ok);
    if(!ok)lastGameError=2005;
    showUi(ok?Screen::Playing:Screen::Error);return ok;
}
static void managePads(const DropInput sample[2],const uint16_t pressed[2]){
    for(int i=0;i<4;i++){_joy[i]=0;_trig[i]=0;}console_keys=INPUT_key_consol=7;INPUT_key_code=AKEY_NONE;
    auto s=drop_pad_status();
    if(ui==Screen::Pairing&&s.commandSequence!=padAwaitCommand&&s.pairingPlayer<0){showUi(Screen::Controllers);return;}
    if(s.revision!=padDrawRevision||s.seconds!=padDrawSeconds)drawUi();
    if(!sample[owner].valid)for(int i=0;i<2;i++)if(sample[i].valid){owner=i;break;}
    for(int i=0;i<2;i++)if(pressed[i]){owner=i;break;}
    auto& in=sample[owner];auto keys=pressed[owner];
    if(keys&(mask(Button::B)|mask(Button::View))){
        if(ui==Screen::Pairing){if(!drop_pad_request(PadCommand::CancelPairing))return;showUi(Screen::Controllers);}
        else if(ui==Screen::Controllers)showUi(Screen::Settings);
        else if(ui==Screen::ForgetPad)showUi(Screen::PadDetails);
        else showUi(Screen::Controllers);
        return;
    }
    int count=ui==Screen::Controllers?4:ui==Screen::PadDetails?3:ui==Screen::ForgetPad?2:1;
    int direction=(in.held&mask(Button::Up))||in.ly>.35f?-1:(in.held&mask(Button::Down))||in.ly<-.35f?1:0;
    uint32_t now=millis();
    if(in.valid&&direction&&(direction!=lastDirection||int32_t(now-repeatAt)>=0)){
        repeatAt=now+(direction!=lastDirection?380:160);selected=(selected+direction+count)%count;drawUi();
    }
    lastDirection=direction;
    if(!(keys&mask(Button::A)))return;
    if(ui==Screen::Controllers){
        if(selected<2){managedPlayer=selected;showUi(Screen::PadDetails);}
        else if(selected==2){if(s.known[0]||s.known[1])drop_pad_request(PadCommand::Swap);}
        else showUi(Screen::Settings);
    }else if(ui==Screen::PadDetails){
        if(selected==0&&!s.known[managedPlayer]){
            padAwaitCommand=s.commandSequence;
            if(drop_pad_request(managedPlayer?PadCommand::Pair2:PadCommand::Pair1))showUi(Screen::Pairing);
        }else if(selected==1&&s.known[managedPlayer]){showUi(Screen::ForgetPad);selected=1;drawUi();}
        else if(selected==2)showUi(Screen::Controllers);
    }else if(ui==Screen::ForgetPad){
        if(selected==1)showUi(Screen::PadDetails);
        else if(drop_pad_request(managedPlayer?PadCommand::Forget2:PadCommand::Forget1))showUi(Screen::Controllers);
    }else if(drop_pad_request(PadCommand::CancelPairing))showUi(Screen::Controllers);
}
static void beginGame(int which){
    basicMode=false;
    game=which;rememberGame(which);lastGameError=keyPulseFrames=0;syntheticStart=genericStart=genericOption=0;for(auto& pulse:nativePulse)pulse=0;
    for(int i=0;i<2;i++){startBlocked[i]=false;suppressed[i]=face;}
    for(int i=0;i<4;i++){_joy[i]=0;_trig[i]=0;}console_keys=INPUT_key_consol=7;INPUT_key_code=AKEY_NONE;
    if(!library_game_cached(which)){
        // Close every disk/cartridge reference before freeing the previous
        // game's buffers. The BASIC core does not refer to any SD media.
        int detached=emulator->insert(":basic:",1,0);
        if(detached!=0||!library_prepare_game(which)){
            lastGameError=library_storage_error()?library_storage_error():2006;
            showUi(Screen::Error);return;
        }
    }
    bool loaded=loadGame();showUi(loaded?Screen::Playing:Screen::Error);if(loaded)drop_publish();
    Serial.println("[NATIVE_GAME] automatic_console_keys=0; fresh title; LT=START RB=OPTION LB=SELECT");
}
static void applyInput(){
    DropInput sample[]={drop_input(0),drop_input(1)};
    uint16_t pressed[]={drop_pressed(0),drop_pressed(1)};
    unsigned pending=commands.exchange(0);
    if(pending&8)drop_update_mode();
    if(pending&65536)for(int i=0;i<console_game_count;i++)if(!strcmp(game_info(i).id,"spy")){beginGame(i);return;}
    if(pending&64){synchronizedVideo=!synchronizedVideo;phaseMin=262;phaseMax=0;archon_lead_min=262;archon_crossings=0;lastRenderField=-1;}
    // Diagnostic UI actions follow the same navigation/confirm branch, not pad evidence.
    uint16_t synthetic=(pending&128)?mask(Button::A):(pending&256)?mask(Button::Down):(pending&512)?mask(Button::Up):(pending&1024)?mask(Button::View):(pending&2048)?mask(Button::B):(pending&8192)?mask(Button::X):(pending&16384)?mask(Button::Y):0;
    if(synthetic){sample[0].valid=true;sample[0].held=0;sample[0].lx=sample[0].ly=0;pressed[0]|=synthetic;Serial.println("[SYNTHETIC] UI action; not physical evidence");}
    if(pending&4096)showUi(Screen::Library);
    if(pending&131072){beginBasic();return;}
    if(pending&4){if(basicMode)beginBasic();else beginGame(game);}
    if(pending&1){syntheticStart=10;Serial.println("[SYNTHETIC] START; not physical evidence");}
    if(pending&16)inject[0]=10;if(pending&32)inject[1]=10;
    unsigned phase=_line_counter;if(phase<phaseMin)phaseMin=phase;if(phase>phaseMax)phaseMax=phase;
    uint32_t now=micros();
    for(int i=0;i<2;i++){
        if(!sample[i].valid){pressed[i]=0;uiArmed[i]=false;continue;}
        if(sample[i].changedUs&&sample[i].changedUs!=inputSeen[i]){inputSeen[i]=sample[i].changedUs;++inputSamples[i];uint32_t age=now-sample[i].changedUs;if(age>inputMaxAge[i])inputMaxAge[i]=age;}
        if(!(sample[i].held&face))uiArmed[i]=true;
        if(!uiArmed[i])pressed[i]=0;
    }
    if(ui==Screen::Help){
        bool turn=false,axis=false;
        for(int i=0;i<2;i++){
            if(pressed[i]&(mask(Button::B)|mask(Button::View))){closeHelp();return;}
            if(pressed[i]&mask(Button::A)){
                if(helpReturn==Screen::Library)beginGame(helpGame);
                else{drop_publish();showUi(Screen::Playing);suppressed[0]=suppressed[1]=face;}
                return;
            }
            turn|=pressed[i]&(mask(Button::X)|mask(Button::Left)|mask(Button::Right));
            axis|=sample[i].valid&&(sample[i].lx<-.35f||sample[i].lx>.35f);
        }
        if(turn||(axis&&!lastDirection)){selected^=1;drawUi();}
        lastDirection=axis;
        return;
    }
    if(padScreen()){managePads(sample,pressed);return;}
    if(ui==Screen::Playing){for(int i=0;i<2;i++)if(pressed[i]&mask(Button::View)){owner=i;drop_menu(true,0);showUi(basicMode?Screen::BasicMenu:Screen::Pause);break;}}
    else if(ui==Screen::Pause&&(pressed[owner]&mask(Button::View))){drop_publish();showUi(Screen::Playing);suppressed[0]=suppressed[1]=face;return;}
    else if(ui==Screen::Library){for(int i=0;i<2;i++)if(pressed[i]&mask(Button::View)){owner=i;openSettings(Screen::Library);return;}}
    else if(ui==Screen::Settings&&(pressed[owner]&mask(Button::View))){showUi(settingsReturn);return;}
    else if(ui==Screen::BasicMenu&&(pressed[owner]&mask(Button::View))){drop_publish();showUi(Screen::Playing);return;}
    if(ui==Screen::Library||ui==Screen::Pause)for(int i=0;i<2;i++)if(pressed[i]&mask(Button::X)){owner=i;openHelp(ui,ui==Screen::Library?librarySelected:game);return;}
    if(ui==Screen::Library||ui==Screen::Pause||ui==Screen::Settings||ui==Screen::Error||ui==Screen::BasicMenu){
        for(int i=0;i<4;i++){_joy[i]=0;_trig[i]=0;}console_keys=INPUT_key_consol=7;INPUT_key_code=AKEY_NONE;
        if(ui==Screen::Error)for(int i=0;i<2;i++)if(pressed[i]&(mask(Button::View)|mask(Button::B))){showUi(Screen::Library);return;}
        if(!sample[owner].valid)for(int i=0;i<2;i++)if(sample[i].valid){owner=i;break;}
        if(ui!=Screen::Pause)for(int i=0;i<2;i++)if(pressed[i]){owner=i;break;}
        auto& in=sample[owner];int direction=(in.held&mask(Button::Up))||in.ly>.35f?-1:(in.held&mask(Button::Down))||in.ly<-.35f?1:0;
        if(!direction&&ui==Screen::Library)direction=(in.held&mask(Button::Left))||in.lx<-.35f?-2:(in.held&mask(Button::Right))||in.lx>.35f?2:0;
        if(synthetic&mask(Button::Down))direction=1;if(synthetic&mask(Button::Up))direction=-1;
        int count=ui==Screen::Pause?(game_info(game).disk2?6:5):ui==Screen::Settings?4:ui==Screen::BasicMenu?2:console_game_count;
        uint32_t nowMs=millis();
        if(in.valid&&direction&&(direction!=lastDirection||int32_t(nowMs-repeatAt)>=0)){
            bool initial=direction!=lastDirection;repeatAt=nowMs+(initial?380:abs(direction)==2?260:110);
            if(abs(direction)==2){
                int page=(selected/6+(direction>0?1:-1)+(count+5)/6)%((count+5)/6);
                selected=min(page*6+selected%6,count-1);
            }else selected=(selected+direction+count)%count;
            if(ui==Screen::Library)librarySelected=selected;drawUi();
        }
        lastDirection=direction;
        if(pressed[owner]&mask(Button::B)){
            if(ui==Screen::Pause){drop_publish();showUi(Screen::Playing);suppressed[0]=suppressed[1]=face;}
            else if(ui==Screen::Settings)showUi(settingsReturn);
            else if(ui==Screen::BasicMenu){drop_publish();showUi(Screen::Playing);}
        }else if(pressed[owner]&mask(Button::A)){
            if(ui==Screen::Library)beginGame(selected);
            else if(ui==Screen::Settings){if(selected==0)showUi(Screen::Controllers);else if(selected==1)drop_update_mode();else if(selected==2){toggleLanguage();drawUi();}else showUi(settingsReturn);}
            else if(ui==Screen::BasicMenu){if(selected==0){drop_publish();showUi(Screen::Playing);}else openSettings(Screen::BasicMenu);}

            else if(ui==Screen::Pause){if(selected==0){drop_publish();showUi(Screen::Playing);suppressed[0]=suppressed[1]=face;}else if(selected==1){beginGame(game);}else if(selected==2)showUi(Screen::Library);else if(selected==3)openHelp(Screen::Pause,game);else if(selected==4)openSettings(Screen::Pause);else{
                char path[64];snprintf(path,sizeof(path),"/game/%s",diskSide?game_info(game).file:game_info(game).disk2);
                if(SIO_Mount(1,path,TRUE)){diskSide=!diskSide;drop_publish();showUi(Screen::Playing);suppressed[0]=suppressed[1]=face;}else showUi(Screen::Error);
            }}
        }
        return; // A used by any UI can never fall through to gameplay/START.
    }
    console_keys=7;INPUT_key_code=AKEY_NONE;
    if(basicMode){
        for(int i=0;i<4;i++){_joy[i]=0;_trig[i]=0;}
        for(int i=0;i<2;i++)if(sample[i].valid&&(sample[i].held&mask(Button::RB)))console_keys&=~4;
        INPUT_key_consol=console_keys;return;
    }
    bool misjaLoader=game_info(game).profile==GameProfile::Misja&&CPU_regPC>=0x1d00&&CPU_regPC<0x1d91&&MEMORY_mem[0x1d00]==0xa9&&MEMORY_mem[0x1d01]==0x91&&MEMORY_mem[0x1d02]==0x8d;
    bool nativeTitle=(isArchon()&&MEMORY_mem[0x609]!=0)||misjaLoader;
    for(int i=0;i<2;i++){
        suppressed[i]&=sample[i].held;uint16_t held=sample[i].valid?(sample[i].held|pressed[i])&~suppressed[i]:0;
        float x=sample[i].valid?sample[i].lx:0,y=sample[i].valid?sample[i].ly:0;
        if(inject[i]){--inject[i];held=mask(Button::A)|mask(i?Button::Left:Button::Right);x=y=0;}
        _joy[i]=drop_joystick((held&mask(Button::Left))||x<-.35f,(held&mask(Button::Right))||x>.35f,(held&mask(Button::Up))||y>.35f,(held&mask(Button::Down))||y<-.35f);
        bool a=held&mask(Button::A);
        bool start=held&kAtariStart;
        if(!(sample[i].held&kAtariStart))startBlocked[i]=false;
        if(misjaLoader&&start&&!startBlocked[i]){keyPulseFrames=12;startBlocked[i]=true;}
        if(isArchon()){
            if(nativeTitle&&start&&!startBlocked[i]){nativePress(0);startBlocked[i]=true;}
            if(pressed[i]&~suppressed[i]&mask(Button::LB))nativePress(1);
            if(pressed[i]&~suppressed[i]&mask(Button::RB))nativePress(2);
        }
        _trig[i]=a&&!startBlocked[i]&&!nativeTitle;
        if(game_info(game).profile==GameProfile::Spy){
            uint16_t option=mask(Button::RB);
            if(pressed[i]&~suppressed[i]&option){
                genericOption=12;
                // Returning to native options must wait for every fire source
                // to be released, including the other controller's triggers.
                suppressed[0]|=mask(Button::A);suppressed[1]|=mask(Button::A);
            }
        }
        if(!isArchon()){
            if(held&mask(Button::RB))console_keys&=~4;
            if(held&mask(Button::LB))console_keys&=~2;
            if(pressed[i]&~suppressed[i]&kAtariStart)genericStart=8;
            if(start)console_keys&=~1;
        }
        if(!isArchon()&&i==0){
            auto profile=game_info(game).profile;
            if(profile==GameProfile::Dropzone){
                if(held&mask(Button::B))INPUT_key_code=AKEY_SPACE;else if(held&mask(Button::X))INPUT_key_code=AKEY_c;
            }else if(profile==GameProfile::Goonies||profile==GameProfile::Zorro){
                if(profile==GameProfile::Goonies&&(held&mask(Button::B)))INPUT_key_code=AKEY_SPACE;
                else if(held&mask(Button::X))INPUT_key_code=AKEY_s;
            }else if(profile==GameProfile::BoulderDash||profile==GameProfile::BlueMax||profile==GameProfile::NewYorkCity){
                if(held&mask(Button::B))INPUT_key_code=AKEY_SPACE;
                else if(profile==GameProfile::BoulderDash&&(held&mask(Button::Y)))INPUT_key_code=AKEY_ESCAPE;
            }else if(profile==GameProfile::RoadRace){
                if(held&mask(Button::B))INPUT_key_code=AKEY_SPACE;
                else if(held&mask(Button::X))INPUT_key_code=AKEY_n;
                else if(held&mask(Button::Y))INPUT_key_code=AKEY_y;
            }
        }
    }
    if(isArchon())for(int key=0;key<3;key++)if(nativePulse[key]>0){
        // The cartridge polls console keys asynchronously. Hold until its first
        // response, then release: one host press must not skip multiple modes.
        bool accepted=(key==1&&MEMORY_mem[0x601]!=nativeSideAtPress[key])||
            (key==2&&MEMORY_mem[0x600]!=nativeModeAtPress[key])||MEMORY_mem[0x609]!=nativeAttractAtPress[key];
        if(accepted)nativePulse[key]=0;
        else {--nativePulse[key];console_keys&=~(1<<key);}
    }
    if(genericStart>0){--genericStart;console_keys&=~1;}
    if(genericOption>0){--genericOption;console_keys&=~4;_trig[0]=_trig[1]=0;}
    if(keyPulseFrames>0){--keyPulseFrames;INPUT_key_code=AKEY_RETURN;}
    if(isArchon()&&!(console_keys&1))_trig[0]=_trig[1]=0;
    if(syntheticStart){--syntheticStart;console_keys&=~1;_trig[0]=_trig[1]=0;}
    INPUT_key_consol=console_keys;
    static int lastConsole=7;
    if(isArchon()&&console_keys!=lastConsole){lastConsole=console_keys;Serial.printf("[NATIVE_KEYS] console=%d mode=%u first=%u attract=%u\n",console_keys,MEMORY_mem[0x600],MEMORY_mem[0x601],MEMORY_mem[0x609]);}
    if(pending&32768)Serial.printf("[INPUT_SNAPSHOT] console=%d fire=%d/%d joy=%d/%d mode=%u first=%u attract=%u\n",console_keys,_trig[0],_trig[1],_joy[0],_joy[1],MEMORY_mem[0x600],MEMORY_mem[0x601],MEMORY_mem[0x609]);
    if(pending&32768)for(int i=0;i<2;i++)Serial.printf("[PAD_SNAPSHOT] p=%d valid=%d physical=%04x merged=%04x lt=%d rt=%d lx=%d ly=%d gtia=%d latch=%d\n",i+1,sample[i].valid,sample[i].physicalHeld,sample[i].held,int(sample[i].lt*1000),int(sample[i].rt*1000),int(sample[i].lx*1000),int(sample[i].ly*1000),GTIA_TRIG[i],GTIA_TRIG_latch[i]);
}
static void status(){
    uint32_t now=millis(),elapsed=now-lastStatus;if(elapsed<5000)return;
    Serial.printf("[GAME_STATUS] game=%s ui=%d frames=%d emu_fps_x100=%u video_fps_x100=%u avg_us=%u max_us=%u guards=%d rows=%d free=%u largest=%u pads=0x%x mode=%u side=%u demo=%u pc=%04x stack_free=%u\n",
        activeTitle(),int(ui),Atari800_nframes,unsigned(frames*100000u/elapsed),unsigned((_frame_counter-lastVideo)*100000u/elapsed),frames?totalUs/frames:0,maxUs,
        drop_guards(),drop_frame_valid(),heap_caps_get_free_size(MALLOC_CAP_INTERNAL|MALLOC_CAP_8BIT),heap_caps_get_largest_free_block(MALLOC_CAP_INTERNAL|MALLOC_CAP_8BIT),
        archon_connected(),MEMORY_mem[0x600],MEMORY_mem[0x601],MEMORY_mem[0x609],CPU_regPC,unsigned(uxTaskGetStackHighWaterMark(nullptr)));
    uint32_t pollGap,pollCost;archon_input_metrics(&pollGap,&pollCost);
    Serial.printf("[LATENCY] samples=%u/%u input_max_us=%u/%u poll_gap_max_us=%u poll_cost_max_us=%u render_phase=%u..%u isr_calls=%u isr_gap_max_us=%u isr_late=%u\n",inputSamples[0],inputSamples[1],inputMaxAge[0],inputMaxAge[1],pollGap,pollCost,phaseMin,phaseMax,unsigned(_diag_isr_calls),unsigned(_diag_isr_gap_max/240),unsigned(_diag_isr_late));
    Serial.printf("[VIDEO_PHASE] sync=%d lead_min_lines=%u close_publications=%u\n",synchronizedVideo,unsigned(archon_lead_min),unsigned(archon_crossings));
    Serial.printf("[COST] core=%u copy=%u sound=%u out=%u cpu_mhz=%u tv=%d os=%d ram=%p\n",frames?costCore/frames:0,frames?costCopy/frames:0,frames?costSound/frames:0,frames?costOut/frames:0,getCpuFrequencyMhz(),Atari800_tv_mode,Atari800_os_version,MEMORY_mem);
    extern void drop_overlay_info();drop_overlay_info();
    frames=totalUs=maxUs=costCore=costCopy=costSound=costOut=0;lastStatus=now;lastVideo=_frame_counter;
}
static void run(void*){
    nextFrame=micros();lastStatus=millis();lastVideo=_frame_counter;
    while(ready){
        if(xEventGroupGetBits(control)&1){xEventGroupSetBits(control,2);while(xEventGroupGetBits(control)&1)vTaskDelay(1);nextFrame=micros();}
        if(synchronizedVideo){
            // Draw behind the current scan and ahead of the next. Leave the
            // old bottom rows untouched until scanout has passed them.
            int field=_frame_counter,line=_line_counter;
            if(field==lastRenderField || line<200 || line>232){vTaskDelay(1);continue;}
            lastRenderField=field;nextFrame=micros()+16667;
        }else{
            uint32_t now=micros();if(int32_t(now-nextFrame)<0){vTaskDelay(1);continue;}
            nextFrame+=16667;if(int32_t(now-nextFrame)>16667)nextFrame=now+16667;
        }
        if(library_test_active())library_test_input();else applyInput();if(!ready)break;if(menuOpen){status();vTaskDelay(1);continue;}
        uint32_t start=micros();libatari800_error_code=0;int ok=emulator->update();uint32_t coreEnd=micros();drop_publish();uint32_t copyEnd=micros();
        int n=emulator->audio_buffer(audio,640);uint32_t soundEnd=micros();audio_write_16(audio,n,1);uint32_t outEnd=micros();
        costCore+=coreEnd-start;costCopy+=copyEnd-coreEnd;costSound+=soundEnd-copyEnd;costOut+=outEnd-soundEnd;
        uint32_t used=micros()-start;totalUs+=used;if(used>maxUs)maxUs=used;++frames;
        if(!drop_guards()||!drop_frame_valid()||(!ok&&!(libatari800_error_code==LIBATARI800_DLIST_ERROR))){
            if(library_test_active())library_test_fail();
            Serial.printf("[FAIL] game=%s frame=%d guards=%d rows=%d error=%d pc=%04x\n",game_info(game).id,Atari800_nframes,drop_guards(),drop_frame_valid(),libatari800_error_code,CPU_regPC);
            saveFailure(libatari800_error_code?libatari800_error_code:1002);
            if(!drop_guards()){drop_update_mode();ready=false;break;}
            showUi(Screen::Error);continue;
        }
        if(library_test_active())library_test_frame(used);
        status();vTaskDelay(1);
    }
    vTaskDelete(nullptr);
}
void drop_setup(){
    Serial.setTxBufferSize(1024);Serial.begin(115200);delay(800);setCpuFrequencyMhz(240);
    Serial.println("[BOOT] WroverArchon-v1.14.3-pad-neutral; ATARI-WROVER-GAME-OTA-v1; two independent joystick ports");drop_update_info();
    if(ESP.getFlashChipSize()!=16777216||ESP.getChipRevision()<3||!psramFound()){Serial.println("[FAIL] board configuration");return;}
    control=xEventGroupCreate();if(!control)return;
    // Allocate the same internal framebuffer once. Video stays active while
    // ordinary (non-IRAM) SPI interrupts load media; no flash writes are made.
    emulator=NewAtari800(1);if(!emulator||emulator->insert(":basic:",1,0)!=0||!emulator->video_buffer()){Serial.println("[FAIL] boot display");return;}
    restoreLanguage();
    _lines=emulator->video_buffer();console_boot_ui(_lines,0,console_media_count,nullptr);
    video_init(4,EMU_ATARI,emulator->composite_palette(),1);
    Serial.printf("[BOOT_VIDEO] active=1 internal_free=%u\n",heap_caps_get_free_size(MALLOC_CAP_INTERNAL|MALLOC_CAP_8BIT));
    if(!drop_begin()){Serial.println("[FAIL] controllers/VFS");return;}
    int testGame=library_test_begin();if(testGame>=0)game=testGame;
    restoreSelection();
    if(library_card_unavailable()){
        Serial.println("[BOOT_FALLBACK] card_no_response=1 basic=1; module has no card-detect contact");
        console_ui(_lines,8,0,0,false,0);delay(900);beginBasic();
    }else{
        lastGameError=library_storage_error();
        showUi(lastGameError?Screen::Error:Screen::Library);
    }
    ready=true;if(xTaskCreatePinnedToCore(run,"archon",8192,nullptr,1,nullptr,0)!=pdPASS){ready=false;Serial.println("[FAIL] emulator task");}
    Serial.println("[COMMANDS] p=capture s=syntheticSTART a/b=synthetic ports r=restart u=updater t=toggle video sync c=BASIC");
}
void drop_loop(){
    drop_poll();
    if(Serial.available()){
        int c=Serial.read();
        if(c=='u'){if(ready)commands.fetch_or(8);else drop_update_mode();}
        else if(c=='j')commands.fetch_or(128);else if(c=='k')commands.fetch_or(256);else if(c=='h')commands.fetch_or(512);else if(c=='n')commands.fetch_or(1024);else if(c=='v')commands.fetch_or(2048);else if(c=='l')commands.fetch_or(4096);
        else if(c=='x')commands.fetch_or(8192);else if(c=='y')commands.fetch_or(16384);else if(c=='i')commands.fetch_or(32768);
        else if(c=='t')commands.fetch_or(64);
        else if(c=='s')commands.fetch_or(1);else if(c=='r')commands.fetch_or(4);
        else if(c=='a')commands.fetch_or(16);else if(c=='b')commands.fetch_or(32);
        else if(c=='g')commands.fetch_or(65536);
        else if(c=='c')commands.fetch_or(131072);
        else if(c=='m'&&ready){
            xEventGroupSetBits(control,1);
            if(xEventGroupWaitBits(control,2,false,true,pdMS_TO_TICKS(2000))&2){
                Serial.printf("\n[MEMORY_BEGIN] bytes=65536 game=%s frame=%d pc=%04x console=%d fire=%d/%d key=%d gra=%02x skstat=%02x kbcode=%02x\n",game_info(game).id,Atari800_nframes,CPU_regPC,console_keys,_trig[0],_trig[1],INPUT_key_code,GTIA_GRACTL,POKEY_SKSTAT,POKEY_KBCODE);
                Serial.flush();Serial.write(MEMORY_mem,65536);Serial.println("\n[MEMORY_END]");Serial.flush();
            }
            xEventGroupClearBits(control,3);
        }
        else if(c=='p'&&ready){
            xEventGroupSetBits(control,1);if(xEventGroupWaitBits(control,2,false,true,pdMS_TO_TICKS(2000))&2)drop_dump(emulator);
            xEventGroupClearBits(control,3);
        }
    }
    delay(2);
}
