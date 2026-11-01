#include <Arduino.h>
#include <esp_heap_caps.h>
#include "emu.h"
#include "dropzone.h"
#include "drop-font.h"
#include "perf-config.h"
#include "drop-scanline.h"
extern "C" {
#include "atari800/screen.h"
#include "atari800/memory.h"
#include "atari800/atari.h"
#include "atari800/input.h"
#include "atari800/sio.h"
#include "atari800/cassette.h"
extern uint8_t* under_cart809F;
extern uint8_t* under_cartA0BF;
}
struct Block { uint32_t* raw; size_t bytes; };
void drop_overlay_info(){Serial.printf("[OVERLAY] speed=%d disk_time=%d disk_show=%d/%d leds=%d/%d mouse=%d tape=%d/%d\n",Screen_show_atari_speed,SIO_last_op_time,Screen_show_disk_led,Screen_show_sector_counter,Atari800_keyboard_leds,Screen_show_1200_leds,INPUT_mouse_mode,CASSETTE_readable,CASSETTE_writable);}
static Block blocks[6];
static uint8_t** displayRows;
static uint32_t* display;
static constexpr uint32_t guard=0xD20FA731;
#if PERF_SCANLINE
static uint32_t scanline[98];
static int scanY;
static bool menuSaved;
#endif
bool drop_frame_valid(){
#if PERF_SCANLINE
    return scanY==240 && Screen_atari==display;
#else
    return true;
#endif
}
static bool allocate(int slot,size_t bytes,bool external) {
    auto p=(uint32_t*)heap_caps_calloc(1,bytes+8,MALLOC_CAP_8BIT|(external?MALLOC_CAP_SPIRAM:MALLOC_CAP_INTERNAL));
    blocks[slot]={p,bytes};
    if(!p)return false;
    p[0]=p[bytes/4+1]=guard;
    return true;
}
extern "C" bool drop_guards() {
    for(const auto& b:blocks)if(!b.raw || b.raw[0]!=guard || b.raw[b.bytes/4+1]!=guard)return false;
#if PERF_SCANLINE
    if(scanline[0]!=guard || scanline[97]!=guard)return false;
#endif
    return true;
}
extern "C" bool drop_memory_init(uint8_t*** out) {
    if(displayRows){*out=displayRows;return true;}
    bool ok=allocate(0,92160,false) && allocate(1,92160,true) && allocate(2,65540,true)
        && allocate(3,16384,true) && allocate(4,8192,true) && allocate(5,8192,true);
    displayRows=(uint8_t**)heap_caps_malloc(240*sizeof(uint8_t*),MALLOC_CAP_INTERNAL|MALLOC_CAP_8BIT);
    if(!ok || !displayRows){
        for(auto& b:blocks){heap_caps_free(b.raw);b.raw=nullptr;}
        heap_caps_free(displayRows);displayRows=nullptr;*out=nullptr;return false;
    }
    display=blocks[0].raw+1;
    Screen_atari=blocks[1].raw+1;
#if PERF_SCANLINE
    Screen_atari=display;
    scanline[0]=scanline[97]=guard;
#endif
    MEMORY_mem=(uint8_t*)(blocks[2].raw+1);
    under_atarixl_os=(uint8_t*)(blocks[3].raw+1);
    under_cart809F=(uint8_t*)(blocks[4].raw+1);
    under_cartA0BF=(uint8_t*)(blocks[5].raw+1);
    for(int y=0;y<240;y++)displayRows[y]=(uint8_t*)display+y*384;
    *out=displayRows;
#if PERF_SCANLINE
    Serial.printf("[MEMORY_PASS] display_internal=92160 draw_internal=384 backup_psram=92160 atari_psram=65540 shadows_psram=32768 guards=%d\n",drop_guards());
#else
    Serial.printf("[MEMORY_PASS] display_internal=92160 draw_psram=92160 atari_psram=65540 shadows_psram=32768 guards=%d\n",drop_guards());
#endif
    return true;
}
void drop_publish() {
#if PERF_SCANLINE
    if(menuSaved){
        const uint32_t* saved=blocks[1].raw+1;
        for(int i=0;i<23040;i++)__atomic_store_n(display+i,saved[i],__ATOMIC_RELAXED);
        menuSaved=false;
    }
    return;
#endif
    // Read PSRAM in bursts before atomic publication. Xtensa emits MEMW even
    // for relaxed stores; interleaving each with a PSRAM read serializes the
    // external bus. The staging row stays on the internal task stack.
    uint32_t row[96];
    const uint32_t* source=Screen_atari;
    uint32_t* target=display;
    for(int y=0;y<240;y++) {
        memcpy(row,source,384);
        for(int x=0;x<96;x++)__atomic_store_n(target+x,row[x],__ATOMIC_RELAXED);
        source+=96;target+=96;
    }
}
extern "C" void drop_clear() {
#if PERF_SCANLINE
    for(int i=0;i<23040;i++)__atomic_store_n(display+i,0u,__ATOMIC_RELAXED);
    menuSaved=false;
#else
    memset(Screen_atari,0,92160);
    drop_publish();
#endif
}
static void text(const char* value,int x,int y,uint8_t fg=0x0e,uint8_t bg=0x02) {
    while(*value && x<352) {
        unsigned char ch=*value++;
        for(int row=0;row<8;row++) {
            uint8_t bits=drop_font[ch+row*256];
            uint32_t words[2]={0,0};
            for(int b=0;b<8;b++)words[b/4]|=uint32_t(bits&(0x80>>b)?fg:bg)<<((b%4)*8);
            auto target=(uint32_t*)(displayRows[y+row]+x);
            __atomic_store_n(target,words[0],__ATOMIC_RELAXED);
            __atomic_store_n(target+1,words[1],__ATOMIC_RELAXED);
        }
        x+=8;
    }
}
void drop_menu(bool active,int selected) {
    if(!active){drop_publish();return;}
#if PERF_SCANLINE
    if(!menuSaved){memcpy(blocks[1].raw+1,display,92160);menuSaved=true;}
#endif
    for(int i=0;i<92160/4;i++)__atomic_store_n(display+i,0x02020202u,__ATOMIC_RELAXED);
    text("PAUZA - DROPZONE",64,48);
    text(selected==0?"> WROC DO GRY":"  WROC DO GRY",64,88);
    text(selected==1?"> RESTART GRY":"  RESTART GRY",64,112);
    text(selected==2?"> AKTUALIZACJA WI-FI":"  AKTUALIZACJA WI-FI",64,136);
    text("A WYBIERZ   B WROC",64,176);
    text("MENU OTWORZ/ZAMKNIJ",64,200);
}
#if PERF_SCANLINE
extern "C" uint16_t* drop_scanline_begin(int){
    scanY=0;memcpy(scanline+1,display,384);return (uint16_t*)(scanline+1);
}
extern "C" uint16_t* drop_scanline_next(){
    if(scanY<240){
        auto dst=display+scanY*96;
        for(int x=0;x<96;x++)__atomic_store_n(dst+x,scanline[x+1],__ATOMIC_RELAXED);
    }
    if(++scanY<240)memcpy(scanline+1,display+scanY*96,384);
    return (uint16_t*)(scanline+1);
}
extern "C" int drop_overlay_begin(){
    // Preserve all optional upstream overlays. The ordinary joystick/XEX path
    // has no active overlay, so it needs no full-frame external-memory pass.
    bool needed=Screen_show_atari_speed || SIO_last_op_time>0 || CASSETTE_readable || CASSETTE_writable
        || Atari800_keyboard_leds || INPUT_mouse_mode==INPUT_MOUSE_PEN || INPUT_mouse_mode==INPUT_MOUSE_GUN;
    if(!needed)return 0;
    Screen_atari=blocks[1].raw+1;memcpy(Screen_atari,display,92160);return 1;
}
extern "C" void drop_overlay_end(){
    for(int i=0;i<23040;i++)__atomic_store_n(display+i,Screen_atari[i],__ATOMIC_RELAXED);
    Screen_atari=display;
}
#endif
void drop_dump(Emu* emulator) {
    const auto* rgb=emulator->rgb_palette();
    uint32_t sum=2166136261u;
    for(int i=0;i<256;i++)for(int shift=16;shift>=0;shift-=8)sum=(sum^uint8_t(rgb[i]>>shift))*16777619u;
    for(int i=0;i<92160;i++)sum=(sum^((uint8_t*)display)[i])*16777619u;
    Serial.printf("\n[FRAME_BEGIN] width=384 height=240 palette=768 pixels=92160 frames=%d fnv=%08x\n",Atari800_nframes,sum);
    Serial.flush();
    for(int i=0;i<256;i++){uint8_t c[]={uint8_t(rgb[i]>>16),uint8_t(rgb[i]>>8),uint8_t(rgb[i])};Serial.write(c,3);}
    for(int y=0;y<240;y++){Serial.write(displayRows[y],384);delay(1);}
    Serial.println("\n[FRAME_END]");Serial.flush();
}
