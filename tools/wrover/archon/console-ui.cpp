#include <Arduino.h>
#include "drop-font.h"
#include "game-catalog.h"
#include "pad-management.h"
extern bool library_game_available(int);
static bool english;
void console_set_language(bool enabled){english=enabled;}
static const char* tr(const char* pl,const char* en){return english?en:pl;}

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
    rows=screen;rect(0,0,384,240,bg);brand(loading?tr("WCZYTYWANIE GRY","LOADING GAME"):tr("URUCHAMIANIE KONSOLI","STARTING CONSOLE"));
    label(loading?tr("ODCZYT I SPRAWDZANIE SHA256","READING AND CHECKING SHA256"):tr("SPRAWDZANIE BIBLIOTEKI microSD","CHECKING microSD LIBRARY"),32,76,false,muted);
    char text[64];snprintf(text,sizeof(text),tr("PLIKI: %d/%d","FILES: %d/%d"),done,total);label(text,32,104);
    rect(32,128,320,14,muted);rect(34,130,316,10,bg);
    if(total>0)rect(34,130,316*min(done,total)/total,10,accent);
    label(title?title:tr("PRZYGOTOWYWANIE BIBLIOTEKI","PREPARING LIBRARY"),32,160,false,accent);
    footer(loading?tr("GRA WLACZY SIE AUTOMATYCZNIE","GAME WILL START AUTOMATICALLY"):tr("MENU WLACZY SIE AUTOMATYCZNIE","MENU WILL OPEN AUTOMATICALLY"),loading?tr("TRWA ODCZYT PLIKOW Z KARTY","READING FILES FROM CARD"):tr("SPRAWDZAM DOSTEPNOSC GIER","CHECKING AVAILABLE GAMES"));
}
void console_boot_ui(uint8_t** screen,int done,int total,const char* title){progressUi(screen,done,total,title,false);}
void console_load_ui(uint8_t** screen,int done,int total,const char* title){progressUi(screen,done,total,title,true);}
void console_ui(uint8_t** screen,int kind,int selected,int game,bool diskSide,int error){
    rows=screen;rect(0,0,384,240,bg);
    char text[64];
    if(kind==0){
        int first=(selected/6)*6;brand(tr("BIBLIOTEKA GIER","GAME LIBRARY"));
        snprintf(text,sizeof(text),"%02d / %02d",selected+1,console_game_count);label(text,272,20);
        for(int row=0;row<6&&first+row<console_game_count;row++)choice(game_info(first+row).title,first+row,selected,52+row*19,library_game_available(first+row));
        label(english?game_info(selected).playersEn:game_info(selected).players,32,172,false,accent);
        label(library_game_available(selected)?game_info(selected).hint:tr("BRAK PLIKU LUB WYMAGANEGO ROM-U","GAME FILE OR REQUIRED ROM MISSING"),32,188,false,muted);
        footer(tr("A GRAJ   X INSTRUKCJA","A PLAY   X HELP"),tr("VIEW USTAWIENIA   LEWO/PRAWO STRONY","VIEW SETTINGS   LEFT/RIGHT PAGE"));
    }else if(kind==2){
        brand(tr("PAUZA","PAUSED"));label(game_info(game).title,32,54,false,accent);
        const char* entries[]={tr("WROC DO GRY","RESUME GAME"),tr("RESTART GRY","RESTART GAME"),tr("WYBIERZ GRE","CHOOSE GAME"),tr("INSTRUKCJA","HELP"),tr("USTAWIENIA","SETTINGS"),diskSide?tr("WLOZ STRONE A","INSERT SIDE A"):tr("WLOZ STRONE B","INSERT SIDE B")};
        int count=game_info(game).disk2?6:5;
        for(int i=0;i<count;i++)choice(entries[i],i,selected,72+i*20);
        footer(tr("A WYBIERZ   X INSTRUKCJA","A SELECT   X HELP"),tr("B / VIEW WROC DO GRY","B / VIEW RESUME"));
    }else if(kind==3){
        brand(tr("USTAWIENIA","SETTINGS"));
        const char* entries[]={tr("KONTROLERY","CONTROLLERS"),tr("AKTUALIZACJA WI-FI","WI-FI UPDATE"),tr("JEZYK: POLSKI","LANGUAGE: ENGLISH"),tr("WROC","BACK")};
        for(int i=0;i<4;i++)choice(entries[i],i,selected,54+i*22);
        label(tr("VIEW (DWA KWADRATY): MENU KONSOLI","VIEW (TWO SQUARES): CONSOLE MENU"),32,152);
        label("LT START  LB SELECT  RB OPTION",32,168);
        label(tr("FIRE: A / RT   RUCH: KRZYZAK/DRAZEK","FIRE: A / RT   MOVE: D-PAD/STICK"),32,184,false,muted);
        footer(tr("A WYBIERZ","A SELECT"),tr("B / VIEW WROC","B / VIEW BACK"));
    }else if(kind==8){
        brand("BASIC");label(tr("KARTA microSD NIE ODPOWIADA","microSD CARD NOT RESPONDING"),32,84);
        label(tr("URUCHAMIAM BASIC Z PAMIECI","STARTING BUILT-IN BASIC"),32,116,false,accent);
        footer(tr("VIEW: MENU KONSOLI","VIEW: CONSOLE MENU"),tr("BASIC DZIALA BEZ KARTY","BASIC WORKS WITHOUT A CARD"));
    }else if(kind==7){
        brand("BASIC");
        choice(tr("WROC DO BASIC","RESUME BASIC"),0,selected,72);choice(tr("USTAWIENIA","SETTINGS"),1,selected,100);
        label(tr("ALTIRRA BASIC - PAMIEC KONSOLI","BUILT-IN ALTIRRA BASIC"),32,148,false,muted);
        footer(tr("A WYBIERZ","A SELECT"),tr("B / VIEW WROC DO BASIC","B / VIEW RESUME BASIC"));
    }else if(kind==6){
        brand(selected?tr("STEROWANIE 2/2","CONTROLS 2/2"):tr("CEL I ZASADY 1/2","GOAL AND RULES 1/2"));label(game_info(game).title,32,54,false,accent);
        label(english?(selected?game_info(game).helpEn:game_info(game).aboutEn):(selected?game_info(game).help:game_info(game).about),32,72);
        label(tr("FIRE: A / RT   RUCH: KRZYZAK/DRAZEK","FIRE: A / RT   MOVE: D-PAD/STICK"),32,188,false,muted);
        footer(error==0?tr("A GRAJ   X / LEWO-PRAWO STRONA","A PLAY   X / LEFT-RIGHT PAGE"):tr("A WZNOW   X / LEWO-PRAWO STRONA","A RESUME   X / LEFT-RIGHT PAGE"),tr("B / VIEW ZAMKNIJ INSTRUKCJE","B / VIEW CLOSE HELP"));
    }else{
        brand(error>=2001&&error<=2004?tr("BLAD BIBLIOTEKI microSD","microSD LIBRARY ERROR"):tr("BLAD GRY","GAME ERROR"));
        label(error==2001?tr("BRAK KARTY LUB BLAD ODCZYTU","NO CARD OR READ ERROR"):error==2002?tr("BRAK PLIKU GRY NA KARCIE","GAME FILE MISSING FROM CARD"):error==2003?tr("PLIK GRY MA BLEDNA ZAWARTOSC","GAME FILE FAILED VERIFICATION"):error==2007?tr("WYMAGANY WLASNY ROM ATARI XL","ATARI XL ROM REQUIRED"):error==2004?tr("BRAK PAMIECI NA BIBLIOTEKE","NOT ENOUGH MEMORY FOR LIBRARY"):tr("NIE UDALO SIE URUCHOMIC","COULD NOT START GAME"),32,68);
        label(game_info(game).title,32,92,false,accent);
        snprintf(text,sizeof(text),tr("BLAD EMULACJI: %d","EMULATION ERROR: %d"),error);label(text,32,116);
        footer(tr("B / VIEW WYBOR GRY","B / VIEW GAME LIBRARY"),tr("MOZESZ WYBRAC INNY TYTUL","SELECT ANOTHER GAME"));
    }
}
static const char* padState(const PadStatus& s,int p){return s.connected[p]?tr("POLACZONY","CONNECTED"):s.known[p]?tr("ZAPISANY, ROZLACZONY","SAVED, DISCONNECTED"):tr("BRAK PRZYPISANIA","UNASSIGNED");}
static const char* padNotice(PadNotice n){
    switch(n){
    case PadNotice::Paired:return tr("PAD DODANY. PUSC PRZYCISKI.","PAD ADDED. RELEASE THE BUTTONS.");
    case PadNotice::Swapped:return tr("ZAMIANA GRACZY ZAPISANA","PLAYER SWAP SAVED");
    case PadNotice::Forgotten:return tr("PRZYPISANIE USUNIETE","ASSIGNMENT REMOVED");
    case PadNotice::TimedOut:return tr("CZAS PAROWANIA MINAL","PAIRING TIMED OUT");
    case PadNotice::SaveError:return tr("BLAD ZAPISU. SPROBUJ PONOWNIE.","SAVE FAILED. PLEASE TRY AGAIN.");
    case PadNotice::Occupied:return tr("NAJPIERW USUN OBECNE PRZYPISANIE","REMOVE THE CURRENT ASSIGNMENT FIRST");
    default:return "";
    }
}
void console_pads_ui(uint8_t** screen,int kind,int selected,int player,const PadStatus& s){
    rows=screen;rect(0,0,384,240,bg);char text[64];
    brand(tr("KONTROLERY","CONTROLLERS"));
    if(kind==9){
        choice(tr("GRACZ 1","PLAYER 1"),0,selected,56);label(padState(s,0),40,78,false,s.connected[0]?accent:muted);
        choice(tr("GRACZ 2","PLAYER 2"),1,selected,96);label(padState(s,1),40,118,false,s.connected[1]?accent:muted);
        choice(tr("ZAMIEN GRACZY 1 I 2","SWAP PLAYERS 1 AND 2"),2,selected,142,s.known[0]||s.known[1]);
        choice(tr("WROC","BACK"),3,selected,164);
        label(padNotice(s.notice),32,188,false,accent);footer(tr("A WYBIERZ","A SELECT"),tr("B / VIEW USTAWIENIA","B / VIEW SETTINGS"));
    }else if(kind==10){
        snprintf(text,sizeof(text),tr("GRACZ %d: %s","PLAYER %d: %s"),player+1,padState(s,player));label(text,32,64);
        choice(tr("DODAJ PAD","ADD CONTROLLER"),0,selected,96,!s.known[player]);
        choice(tr("USUN PRZYPISANIE","REMOVE ASSIGNMENT"),1,selected,120,s.known[player]);
        choice(tr("WROC","BACK"),2,selected,144);
        label(s.known[player]?tr("ABY ZASTAPIC PAD, USUN PRZYPISANIE","REMOVE ASSIGNMENT TO REPLACE PAD"):tr("DODAJ PAD DO TEGO GRACZA","ADD A PAD FOR THIS PLAYER"),32,180,false,muted);
        footer(tr("A WYBIERZ","A SELECT"),tr("B / VIEW LISTA KONTROLEROW","B / VIEW CONTROLLERS"));
    }else if(kind==11){
        snprintf(text,sizeof(text),tr("DODAWANIE PADA: GRACZ %d","PAIRING PAD FOR PLAYER %d"),player+1);label(text,32,64,false,accent);
        label(tr("WLACZ PAD I PRZYTRZYMAJ","TURN ON THE PAD AND HOLD"),32,88);label(tr("PRZYCISK PAROWANIA OBOK USB.","THE PAIR BUTTON NEXT TO USB."),32,104);
        label(tr("GDY SIE POLACZY, NACISNIJ A.","PRESS A WHEN CONNECTED."),32,128);
        if(s.seconds)snprintf(text,sizeof(text),tr("POZOSTALO: %u S","TIME LEFT: %u S"),unsigned(s.seconds));else snprintf(text,sizeof(text),tr("OCZEKIWANIE NA PAD","WAITING FOR CONTROLLER"));
        label(text,32,156,false,muted);choice(tr("ANULUJ","CANCEL"),0,0,178);footer(tr("A / B / VIEW ANULUJ","A / B / VIEW CANCEL"),tr("ZAPISANE PADY NADAL DZIALAJA","SAVED CONTROLLERS STILL WORK"));
    }else{
        snprintf(text,sizeof(text),tr("USUNAC PRZYPISANIE GRACZA %d?","REMOVE PLAYER %d ASSIGNMENT?"),player+1);label(text,32,68);
        label(tr("TEN PAD ZOSTANIE ROZLACZONY.","THIS PAD WILL BE DISCONNECTED."),32,92,false,muted);
        label(tr("DRUGI PAD ZACHOWA USTAWIENIA.","THE OTHER PAD KEEPS ITS SETTINGS."),32,108,false,muted);
        choice(tr("TAK, USUN PRZYPISANIE","YES, REMOVE ASSIGNMENT"),0,selected,140);
        choice(tr("ANULUJ","CANCEL"),1,selected,164);footer(tr("A POTWIERDZ","A CONFIRM"),tr("B / VIEW ANULUJ","B / VIEW CANCEL"));
    }
}
