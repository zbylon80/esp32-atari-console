#include <Arduino.h>
#include "drop-font.h"
#include "game-catalog.h"
#include "pad-management.h"
extern bool library_game_available(int);
static uint8_t** rows;
static constexpr uint8_t bg=0x02,ink=0x0e,muted=0x0a,accent=0x28;
static void rect(int x,int y,int w,int h,uint8_t color){
    uint32_t word=uint32_t(color)*0x01010101u;
    for(int dy=y;dy<y+h&&dy<240;dy++){
        int dx=x,end=min(x+w,384);
        for(;dx<end&&(dx&3);dx++)__atomic_store_n(rows[dy]+dx,color,__ATOMIC_RELAXED);
        for(;dx+4<=end;dx+=4)__atomic_store_n((uint32_t*)(rows[dy]+dx),word,__ATOMIC_RELAXED);
        for(;dx<end;dx++)__atomic_store_n(rows[dy]+dx,color,__ATOMIC_RELAXED);
    }
}
static void label(const char* value,int x,int y,bool selected=false,uint8_t color=ink){
    const int start=x;
    while(*value){unsigned char c=*value++;
        if(c=='\n'){x=start;y+=12;continue;}
        if(x>336||y>232)continue;
        uint8_t pen=selected?bg:color,paper=selected?ink:bg;
        for(int dy=0;dy<8;dy++){uint32_t words[2]={};uint8_t bits=drop_font[c+dy*256];
            for(int dx=0;dx<8;dx++)words[dx/4]|=uint32_t(bits&(128>>dx)?pen:paper)<<((dx%4)*8);
            auto dst=(uint32_t*)(rows[y+dy]+x);__atomic_store_n(dst,words[0],__ATOMIC_RELAXED);__atomic_store_n(dst+1,words[1],__ATOMIC_RELAXED);
        }x+=8;
    }
}
// Small Fuji silhouette drawn at native resolution, without bitmap RAM.
static void brand(const char* subtitle){
    for(int y=0;y<28;y++){
        int bend=y>7?(y-7)*(y-7)/32:0;
        rect(40-bend,12+y,4,1,ink);rect(56+bend,12+y,4,1,ink);
    }
    rect(48,12,4,28,ink);
    label("ATARI",80,14);label(subtitle,80,28,false,muted);
    rect(24,46,328,1,0x06);
}
static void choice(const char* text,int index,int selected,int y,bool enabled=true){
    bool chosen=index==selected;
    if(chosen){rect(24,y,328,18,ink);rect(24,y,4,18,accent);}
    label(text,36,y+5,chosen,enabled?ink:muted);
}
static void footer(const char* top,const char* bottom){
    rect(24,202,328,1,0x06);label(top,32,208);label(bottom,32,224,false,muted);
}
static void progressUi(uint8_t** screen,int done,int total,const char* title,bool loading){
    rows=screen;rect(0,0,384,240,bg);brand(loading?"WCZYTYWANIE GRY":"URUCHAMIANIE KONSOLI");
    label(loading?"ODCZYT I SPRAWDZANIE SHA256":"SPRAWDZANIE BIBLIOTEKI microSD",32,76,false,muted);
    char text[64];snprintf(text,sizeof(text),"PLIKI: %d/%d",done,total);label(text,32,104);
    rect(32,128,320,14,muted);rect(34,130,316,10,bg);
    if(total>0)rect(34,130,316*min(done,total)/total,10,accent);
    label(title?title:"PRZYGOTOWYWANIE BIBLIOTEKI",32,160,false,accent);
    footer(loading?"GRA WLACZY SIE AUTOMATYCZNIE":"MENU WLACZY SIE AUTOMATYCZNIE",loading?"TRWA ODCZYT PLIKOW Z KARTY":"SPRAWDZAM DOSTEPNOSC GIER");
}
void console_boot_ui(uint8_t** screen,int done,int total,const char* title){progressUi(screen,done,total,title,false);}
void console_load_ui(uint8_t** screen,int done,int total,const char* title){progressUi(screen,done,total,title,true);}
void console_ui(uint8_t** screen,int kind,int selected,int game,bool diskSide,int error){
    rows=screen;rect(0,0,384,240,bg);
    char text[64];
    if(kind==0){
        int first=(selected/6)*6;brand("BIBLIOTEKA GIER");
        snprintf(text,sizeof(text),"%02d / %02d",selected+1,console_game_count);label(text,272,20);
        for(int row=0;row<6&&first+row<console_game_count;row++)choice(game_info(first+row).title,first+row,selected,52+row*19,library_game_available(first+row));
        label(game_info(selected).players,32,172,false,accent);
        label(library_game_available(selected)?game_info(selected).hint:"BRAK PLIKU LUB WYMAGANEGO ROM-U",32,188,false,muted);
        footer("A GRAJ   X INSTRUKCJA","VIEW USTAWIENIA   LEWO/PRAWO STRONY");
    }else if(kind==2){
        brand("PAUZA");label(game_info(game).title,32,54,false,accent);
        const char* entries[]={"WROC DO GRY","RESTART GRY","WYBIERZ GRE","INSTRUKCJA","USTAWIENIA",diskSide?"WLOZ STRONE A":"WLOZ STRONE B"};
        int count=game_info(game).disk2?6:5;
        for(int i=0;i<count;i++)choice(entries[i],i,selected,72+i*20);
        footer("A WYBIERZ   X INSTRUKCJA","B / VIEW WROC DO GRY");
    }else if(kind==3){
        brand("USTAWIENIA");
        const char* entries[]={"KONTROLERY","AKTUALIZACJA WI-FI","WROC"};
        for(int i=0;i<3;i++)choice(entries[i],i,selected,56+i*24);
        label("VIEW (DWA KWADRATY): MENU KONSOLI",32,152);
        label("MENU (3 KRESKI): ATARI OPTION",32,168);
        label("A = LT = RT   KRZYZAK = LEWY DRAZEK",32,184,false,muted);
        footer("A WYBIERZ","B / VIEW WROC");
    }else if(kind==8){
        brand("BASIC");label("KARTA microSD NIE ODPOWIADA",32,84);
        label("URUCHAMIAM BASIC Z PAMIECI",32,116,false,accent);
        footer("VIEW: MENU KONSOLI","BASIC DZIALA BEZ KARTY");
    }else if(kind==7){
        brand("BASIC");
        choice("WROC DO BASIC",0,selected,72);choice("USTAWIENIA",1,selected,100);
        label("ALTIRRA BASIC - PAMIEC KONSOLI",32,148,false,muted);
        footer("A WYBIERZ","B / VIEW WROC DO BASIC");
    }else if(kind==6){
        brand(selected?"STEROWANIE 2/2":"CEL I ZASADY 1/2");label(game_info(game).title,32,54,false,accent);
        label(selected?game_info(game).help:game_info(game).about,32,72);
        label("A = LT = RT   KRZYZAK = LEWY DRAZEK",32,188,false,muted);
        footer(error==0?"A GRAJ   X / LEWO-PRAWO STRONA":"A WZNOW   X / LEWO-PRAWO STRONA","B / VIEW ZAMKNIJ INSTRUKCJE");
    }else{
        brand(error>=2001&&error<=2004?"BLAD BIBLIOTEKI microSD":"BLAD GRY");
        label(error==2001?"BRAK KARTY LUB BLAD ODCZYTU":error==2002?"BRAK PLIKU GRY NA KARCIE":error==2003?"PLIK GRY MA BLEDNA ZAWARTOSC":error==2007?"WYMAGANY WLASNY ROM ATARI XL":error==2004?"BRAK PAMIECI NA BIBLIOTEKE":"NIE UDALO SIE URUCHOMIC",32,68);
        label(game_info(game).title,32,92,false,accent);
        snprintf(text,sizeof(text),"BLAD EMULACJI: %d",error);label(text,32,116);
        footer("B / VIEW WYBOR GRY","MOZESZ WYBRAC INNY TYTUL");
    }
}
static const char* padState(const PadStatus& s,int p){return s.connected[p]?"POLACZONY":s.known[p]?"ZAPISANY, ROZLACZONY":"BRAK PRZYPISANIA";}
static const char* padNotice(PadNotice n){
    switch(n){
    case PadNotice::Paired:return "PAD DODANY. PUSC PRZYCISKI.";
    case PadNotice::Swapped:return "ZAMIANA GRACZY ZAPISANA";
    case PadNotice::Forgotten:return "PRZYPISANIE USUNIETE";
    case PadNotice::TimedOut:return "CZAS PAROWANIA MINAL";
    case PadNotice::SaveError:return "BLAD ZAPISU. SPROBUJ PONOWNIE.";
    case PadNotice::Occupied:return "NAJPIERW USUN OBECNE PRZYPISANIE";
    default:return "";
    }
}
void console_pads_ui(uint8_t** screen,int kind,int selected,int player,const PadStatus& s){
    rows=screen;rect(0,0,384,240,bg);char text[64];
    brand("KONTROLERY");
    if(kind==9){
        choice("GRACZ 1",0,selected,56);label(padState(s,0),40,78,false,s.connected[0]?accent:muted);
        choice("GRACZ 2",1,selected,96);label(padState(s,1),40,118,false,s.connected[1]?accent:muted);
        choice("ZAMIEN GRACZY 1 I 2",2,selected,142,s.known[0]||s.known[1]);
        choice("WROC",3,selected,164);
        label(padNotice(s.notice),32,188,false,accent);footer("A WYBIERZ","B / VIEW USTAWIENIA");
    }else if(kind==10){
        snprintf(text,sizeof(text),"GRACZ %d: %s",player+1,padState(s,player));label(text,32,64);
        choice("DODAJ PAD",0,selected,96,!s.known[player]);
        choice("USUN PRZYPISANIE",1,selected,120,s.known[player]);
        choice("WROC",2,selected,144);
        label(s.known[player]?"ABY ZASTAPIC PAD, USUN PRZYPISANIE":"DODAJ PAD DO TEGO GRACZA",32,180,false,muted);
        footer("A WYBIERZ","B / VIEW LISTA KONTROLEROW");
    }else if(kind==11){
        snprintf(text,sizeof(text),"DODAWANIE PADA: GRACZ %d",player+1);label(text,32,64,false,accent);
        label("WLACZ PAD I PRZYTRZYMAJ",32,88);label("PRZYCISK PAROWANIA OBOK USB.",32,104);
        label("GDY SIE POLACZY, NACISNIJ A.",32,128);
        if(s.seconds)snprintf(text,sizeof(text),"POZOSTALO: %u S",unsigned(s.seconds));else snprintf(text,sizeof(text),"OCZEKIWANIE NA PAD");
        label(text,32,156,false,muted);choice("ANULUJ",0,0,178);footer("A / B / VIEW ANULUJ","ZAPISANE PADY NADAL DZIALAJA");
    }else{
        snprintf(text,sizeof(text),"USUNAC PRZYPISANIE GRACZA %d?",player+1);label(text,32,68);
        label("TEN PAD ZOSTANIE ROZLACZONY.",32,92,false,muted);
        label("DRUGI PAD ZACHOWA USTAWIENIA.",32,108,false,muted);
        choice("TAK, USUN PRZYPISANIE",0,selected,140);
        choice("ANULUJ",1,selected,164);footer("A POTWIERDZ","B / VIEW ANULUJ");
    }
}
