"""Render the production UI C++ on Windows; no emulator or desktop app is opened."""
from pathlib import Path
import json,re,subprocess,sys
ROOT=Path(__file__).resolve().parents[2]
OUT=ROOT/'.build/menu-preview';OUT.mkdir(exist_ok=True)
sys.path.insert(0,str(ROOT/'.tools/python-libs'))
from PIL import Image
games=json.loads((ROOT/'tools/wrover/library.json').read_text())['games']
font=ROOT/'.build/wrover-archon/WroverArchon/src/drop-font.h'
(OUT/'drop-font.h').write_bytes(font.read_bytes())
(OUT/'Arduino.h').write_text('#pragma once\n#include <cstdint>\n#include <cstdio>\n#include <algorithm>\nusing std::min;\n#define __ATOMIC_RELAXED 0\n#define __atomic_store_n(p,v,o) (*(p)=(v))\n')
entries=[]
for g in games:
 fields=[json.dumps(g[k]) for k in ('id','title','file','hint')]
 entries.append('{'+','.join(fields)+',GameProfile::'+g['profile']+',0,nullptr,0,'+json.dumps(g['players'])+','+('"sideB"' if g.get('disk2') else 'nullptr')+',false,'+json.dumps('\n'.join(g['help']))+','+json.dumps('\n'.join(g['about']))+','+json.dumps(g['players_en'])+','+json.dumps('\n'.join(g['help_en']))+','+json.dumps('\n'.join(g['about_en']))+'}')
source='''#include <Arduino.h>
#include <cassert>
#include <cstring>
#include "game-catalog.h"
#include "pad-management.h"
#include "library-status.h"
#include "system-controls.h"
void console_set_system_bindings(const SystemBindings&,bool);
const GameInfo console_games[]={ENTRIES};
const int console_game_count=COUNT;
static bool allAvailable=true;
bool library_game_available(int index){return allAvailable||index%2==0;}
static LibraryStatus storage;
const LibraryStatus& library_status(){return storage;}
void console_ui(uint8_t**,int,int,int,bool,int);
void console_set_language(bool);
void console_boot_ui(uint8_t**,int,int,const char*);
void console_load_ui(uint8_t**,int,int,const char*);
int main(){
 uint8_t data[240][416],*rows[240];
 for(int y=0;y<240;y++)rows[y]=data[y]+16;
 for(bool english: {false,true}){
 console_set_language(english);
 storage=LibraryStatus{};storage.condition=LibraryCondition::Ready;
 allAvailable=true;
 for(int kind: {0,2,3,6,5})for(int g=0;g<console_game_count;g++)for(int page=0;page<(kind==6?2:1);page++){
  memset(data,0xcd,sizeof(data));
  console_ui(rows,kind,kind==0?g:kind==2?3:page,g,false,kind==6?0:2);
  for(int y=0;y<240;y++)for(int x=0;x<16;x++)assert(data[y][x]==0xcd&&data[y][400+x]==0xcd);
  char name[84];snprintf(name,sizeof(name),page?"ui-%d-%02d-p1.raw":"ui-%d-%02d.raw",kind,g);
  if(english){char localized[88];snprintf(localized,sizeof(localized),"en-%s",name);strcpy(name,localized);}
  FILE* f=fopen(name,"wb");assert(f);for(auto row:rows)fwrite(row,1,384,f);fclose(f);
 }
 for(int done: {0,1,7,20,21}){
  memset(data,0xcd,sizeof(data));
  console_boot_ui(rows,done,21,done?"Montezuma's Revenge":nullptr);
  for(int y=0;y<240;y++)for(int x=0;x<16;x++)assert(data[y][x]==0xcd&&data[y][400+x]==0xcd);
  char name[84];snprintf(name,sizeof(name),"boot-%02d.raw",done);
  if(english){char localized[88];snprintf(localized,sizeof(localized),"en-%s",name);strcpy(name,localized);}
  FILE* f=fopen(name,"wb");assert(f);for(auto row:rows)fwrite(row,1,384,f);fclose(f);
 }
 for(int item: {0,1,2}){
  memset(data,0xcd,sizeof(data));console_ui(rows,7,item,0,false,0);
  for(int y=0;y<240;y++)for(int x=0;x<16;x++)assert(data[y][x]==0xcd&&data[y][400+x]==0xcd);
 }
 for(int condition=0;condition<=int(LibraryCondition::RomRequired);condition++){
  storage=LibraryStatus{};storage.condition=LibraryCondition(condition);
  storage.totalGames=33;storage.availableGames=27;storage.totalFiles=34;storage.validFiles=28;
  storage.originalOs=condition%2==0;
  strcpy(storage.sdErrorName,condition==int(LibraryCondition::NoResponse)?"ESP_ERR_TIMEOUT":"ESP_OK");
  strcpy(storage.file,"sek-smisja-side-b.atr");
  memset(data,0xcd,sizeof(data));console_diagnostics_ui(rows,storage,3,169864,3670016);
  for(int y=0;y<240;y++)for(int x=0;x<16;x++)assert(data[y][x]==0xcd&&data[y][400+x]==0xcd);
  char name[84];snprintf(name,sizeof(name),english?"en-diagnostics-%02d.raw":"diagnostics-%02d.raw",condition);
  FILE* f=fopen(name,"wb");assert(f);for(auto row:rows)fwrite(row,1,384,f);fclose(f);
  memset(data,0xcd,sizeof(data));console_ui(rows,0,0,0,false,0);
  for(int y=0;y<240;y++)for(int x=0;x<16;x++)assert(data[y][x]==0xcd&&data[y][400+x]==0xcd);
  snprintf(name,sizeof(name),english?"en-storage-library-%02d.raw":"storage-library-%02d.raw",condition);
  f=fopen(name,"wb");assert(f);for(auto row:rows)fwrite(row,1,384,f);fclose(f);
 }
 storage.condition=LibraryCondition::Ready;
 for(int done: {0,1,2}){
  memset(data,0xcd,sizeof(data));console_load_ui(rows,done,2,"Seksmisja - A.D. 2044");
  for(int y=0;y<240;y++)for(int x=0;x<16;x++)assert(data[y][x]==0xcd&&data[y][400+x]==0xcd);
 }
 allAvailable=false;
 for(int selected: {0,1}){
  memset(data,0xcd,sizeof(data));console_ui(rows,0,selected,0,false,0);
  for(int y=0;y<240;y++)for(int x=0;x<16;x++)assert(data[y][x]==0xcd&&data[y][400+x]==0xcd);
 }
 for(int selected: {0,1,2,3,4,5,6}){
  memset(data,0xcd,sizeof(data));console_ui(rows,3,selected,0,false,0);
  for(int y=0;y<240;y++)for(int x=0;x<16;x++)assert(data[y][x]==0xcd&&data[y][400+x]==0xcd);
 }
 uint8_t permutation[]={0,1,2};int layout=0;
 do{
  SystemBindings bindings;for(int i=0;i<3;i++)bindings.buttons[i]=permutation[i];
  for(bool error: {false,true}){
   console_set_system_bindings(bindings,error);
   for(int item=0;item<5;item++){
    memset(data,0xcd,sizeof(data));console_ui(rows,14,item,0,false,0);
    for(int y=0;y<240;y++)for(int x=0;x<16;x++)assert(data[y][x]==0xcd&&data[y][400+x]==0xcd);
    char name[84];snprintf(name,sizeof(name),english?"en-controls-%d-%d-%d.raw":"controls-%d-%d-%d.raw",layout,int(error),item);
    FILE* f=fopen(name,"wb");assert(f);for(auto row:rows)fwrite(row,1,384,f);fclose(f);
   }
  }
  for(int g=0;g<console_game_count;g++){
   memset(data,0xcd,sizeof(data));console_ui(rows,6,1,g,false,0);
   for(int y=0;y<240;y++)for(int x=0;x<16;x++)assert(data[y][x]==0xcd&&data[y][400+x]==0xcd);
  }
  ++layout;
 }while(std::next_permutation(permutation,permutation+3));
 console_set_system_bindings(SystemBindings{},false);
 for(int kind: {9,10,11,12})for(int state=0;state<4;state++)for(int player=0;player<2;player++)for(int selected=0;selected<(kind==9?4:kind==10?3:kind==11?1:2);selected++){
  PadStatus s{};s.known[0]=state>0;s.connected[0]=state>0;s.known[1]=state>0;s.connected[1]=state==2;
  if(state==2){s.pairingPlayer=1;s.seconds=42;}
  if(state==3)s.notice=PadNotice::SaveError;
  memset(data,0xcd,sizeof(data));console_pads_ui(rows,kind,selected,player,s);
  for(int y=0;y<240;y++)for(int x=0;x<16;x++)assert(data[y][x]==0xcd&&data[y][400+x]==0xcd);
  char name[84];snprintf(name,sizeof(name),"pads-%d-%d-%d-%d.raw",kind,state,player,selected);
  if(english){char localized[88];snprintf(localized,sizeof(localized),"en-%s",name);strcpy(name,localized);}
  FILE* f=fopen(name,"wb");assert(f);for(auto row:rows)fwrite(row,1,384,f);fclose(f);
 }
 }
 puts("PASS Polish and English UI states; row guards intact");
}
'''.replace('ENTRIES',','.join(entries)).replace('COUNT',str(len(games)))
(OUT/'render.cpp').write_text(source)
vswhere=Path(r'C:/Program Files (x86)/Microsoft Visual Studio/Installer/vswhere.exe')
vs=subprocess.check_output([str(vswhere),'-latest','-products','*','-requires','Microsoft.VisualStudio.Component.VC.Tools.x86.x64','-property','installationPath'],text=True).strip()
assert vs,'MSVC tools required'
batch='@echo off\ncall "'+str(Path(vs)/'VC/Auxiliary/Build/vcvars64.bat')+'" >nul\nif errorlevel 1 exit /b 1\n'
batch+='cl /nologo /std:c++17 /EHsc /I"'+str(OUT)+'" /I"'+str(ROOT/'src')+'" /I"'+str(ROOT/'tools/wrover/archon')+'" "'+str(OUT/'render.cpp')+'" "'+str(ROOT/'tools/wrover/archon/console-ui.cpp')+'" /Fe:"'+str(OUT/'render.exe')+'"\n'
batch+='if errorlevel 1 exit /b 1\nrender.exe\n'
(OUT/'build.cmd').write_text(batch)
subprocess.run(['cmd','/d','/c',str(OUT/'build.cmd')],cwd=OUT,check=True)
cpp=(ROOT/'.build/wrover-archon/WroverArchon/src/emu_atari800.cpp').read_text()
section=cpp.split('const uint32_t atari_palette_rgb[256] = {')[1].split('};')[0]
palette=[]
for color in re.findall(r'0x[0-9a-fA-F]+',section):
 v=int(color,16);palette.extend(((v>>16)&255,(v>>8)&255,v&255))
assert len(palette)==768
for raw in list(OUT.glob('ui-*.raw'))+list(OUT.glob('controls-*.raw'))+list(OUT.glob('boot-*.raw'))+list(OUT.glob('pads-*.raw'))+list(OUT.glob('diagnostics-*.raw'))+list(OUT.glob('storage-library-*.raw'))+list(OUT.glob('en-*.raw')):
 im=Image.frombytes('P',(384,240),raw.read_bytes());im.putpalette(palette);im.convert('RGB').save(raw.with_suffix('.png'))
sheet=Image.new('RGB',(768,480))
for name,pos in [('ui-6-15',(0,0)),('ui-6-15-p1',(384,0)),('ui-6-01',(0,240)),('ui-6-01-p1',(384,240))]:
 sheet.paste(Image.open(OUT/(name+'.png')),pos)
sheet.save(OUT/'preview.png')
print('Preview:',OUT/'preview.png')
sheet=Image.new('RGB',(768,480))
for name,pos in [('boot-00',(0,0)),('boot-01',(384,0)),('boot-07',(0,240)),('boot-21',(384,240))]:
 sheet.paste(Image.open(OUT/(name+'.png')),pos)
sheet.save(OUT/'boot-preview.png')
print('Boot preview:',OUT/'boot-preview.png')
sheet=Image.new('RGB',(768,480))
for name,pos in [('pads-9-1-0-0',(0,0)),('pads-10-1-1-1',(384,0)),('pads-11-2-1-0',(0,240)),('pads-12-1-1-1',(384,240))]:
 sheet.paste(Image.open(OUT/(name+'.png')),pos)
sheet.save(OUT/'controllers-preview.png')
print('Controllers preview:',OUT/'controllers-preview.png')
