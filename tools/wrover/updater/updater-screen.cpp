#include <Arduino.h>
#include <atomic>
#define EMU_ATARI 1
#define EMU_NES 2
#define EMU_NES6 4
#define EMU_SMS 3
#define IRE(x) ((uint32_t)(((x)+40)*255/3.3/147.5)<<8)
#define SYNC_LEVEL IRE(-40)
#define BLANKING_LEVEL IRE(0)
#define BLACK_LEVEL IRE(7.5)
#define GRAY_LEVEL IRE((100-7.5)/2)
#define WHITE_LEVEL IRE(100)
#include "video_out.h"
#include "test-palettes.h"
#include "drop-font.h"
// Only the 48 text rows need unique storage; other scanlines share a blank row.
static uint32_t pixels[48][96],blank[96],palette[256];
static uint8_t* rows[240];
static bool started;
static void line(const char* str,int row){
    int x=40;
    while(*str&&x<=336){unsigned char ch=*str++;
        for(int y=0;y<8;y++){
            uint8_t bits=drop_font[ch+y*256];
            uint32_t words[2]={};for(int b=0;b<8;b++)words[b/4]|=uint32_t(bits&(0x80>>b)?0x0e:0)<<((b%4)*8);
            __atomic_store_n(&pixels[row+y][x/4],words[0],__ATOMIC_RELAXED);
            __atomic_store_n(&pixels[row+y][x/4+1],words[1],__ATOMIC_RELAXED);
        }x+=8;
    }
}
void updater_screen_begin(){
    memcpy(palette,test_ntsc,sizeof(palette));
    for(int y=0;y<240;y++)rows[y]=(uint8_t*)blank;
    for(int n=0;n<6;n++)for(int y=0;y<8;y++)rows[40+n*28+y]=(uint8_t*)pixels[n*8+y];
    _lines=rows;video_init(4,EMU_ATARI,palette,1);started=true;
}
void updater_message(const char* title,const char* detail){
    if(!started)return;
    for(auto& row:pixels)for(auto& p:row)__atomic_store_n(&p,0u,__ATOMIC_RELAXED);
    line("ATARI - TRYB AKTUALIZACJI",0);line(title,8);line(detail,16);
    line("GRA JEST ZATRZYMANA",24);line("WGRYWANIE Z KOMPUTERA",32);line("RESET = POWROT DO GRY",40);
}
