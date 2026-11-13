#include "console-build-config.h"
#include <Arduino.h>
#include <Esp32AtariController.h>
#include <esp_vfs.h>
#include <errno.h>
#include <fcntl.h>
#include <sys/stat.h>
#include <atomic>
#include <freertos/queue.h>
#include <string>
#include <mbedtls/sha256.h>
#include "sd-storage.h"
#include "dropzone.h"
#include "game-catalog.h"
#include "library-status.h"
#include "TwoPadHub.h"
#include "pad-mapping.h"
using namespace esp32controller;
TwoPadHub* TwoPadHub::instance=nullptr;
static TwoPadHub hub;
static PlayerBackend backend0(hub,0),backend1(hub,1);
static XboxController controller0(backend0),controller1(backend1);
static XboxController* controllers[]={&controller0,&controller1};
struct PairInput { DropInput players[2]; };
static QueueHandle_t queue;
static QueueHandle_t padRequests,padStatuses;
static std::atomic<uint16_t> edges[2];
static std::atomic<unsigned> connected{0};
static std::atomic<uint32_t> maxPollGap{0},maxPollCost{0};
static uint32_t lastPoll,changedAt[2];
static PadMapping mapping[2];
void archon_input_metrics(uint32_t* gap,uint32_t* cost){*gap=maxPollGap.load();*cost=maxPollCost.load();}
struct Asset {const uint8_t* data;size_t size;};
static Asset* cached;
static bool* available;
static int cachedGame=-1;
static int storageError;
static LibraryStatus storageStatus;
bool library_game_available(int index);
const LibraryStatus& library_status(){return storageStatus;}
static void recordMount(){
    storageStatus.sdError=console_sd_last_error();
    snprintf(storageStatus.sdErrorName,sizeof(storageStatus.sdErrorName),"%s",console_sd_error_name());
    storageStatus.checkedMs=millis();
}
static void countAvailable(){
    storageStatus.availableGames=0;storageStatus.validFiles=0;
    if(available)for(int i=0;i<console_media_count;i++)storageStatus.validFiles+=available[i];
    for(int i=0;i<console_game_count;i++)storageStatus.availableGames+=library_game_available(i);
}
int library_storage_error(){return storageError;}
extern void console_boot_progress(int done,int total,const char* file);
extern void console_game_progress(int done,int total,const char* file);
static int mediaIndex(const char* name){
    for(int i=0;i<console_media_count;i++)if(!strcmp(name,console_media[i].file))return i;
    return -1;
}
bool library_game_available(int index){
    if(!available||index<0||index>=console_game_count)return false;
    const auto& info=game_info(index);if(info.originalOs&&!CONSOLE_HAS_ORIGINAL_OS)return false;int first=mediaIndex(info.file),second=info.disk2?mediaIndex(info.disk2):-1;
    return first>=0&&available[first]&&(!info.disk2||(second>=0&&available[second]));
}
bool library_game_cached(int index){return cachedGame==index;}
static void scanLibrary(){
    uint32_t start=millis();storageError=0;
    storageStatus=LibraryStatus{};
    storageStatus.totalGames=console_game_count;storageStatus.totalFiles=console_media_count;
    storageStatus.originalOs=CONSOLE_HAS_ORIGINAL_OS;storageStatus.checkedMs=start;
    console_boot_progress(0,console_media_count,nullptr);
    // Rescans must preserve the loaded game's buffers and every core reference.
    if(!cached)cached=(Asset*)calloc(console_media_count,sizeof(Asset));
    if(!available)available=(bool*)calloc(console_media_count,sizeof(bool));
    if(!cached||!available){storageError=2004;storageStatus.condition=LibraryCondition::NoMemory;return;}
    memset(available,0,console_media_count*sizeof(bool));
    bool mounted=console_sd_mount(4000);recordMount();
    if(!mounted){storageError=2001;storageStatus.condition=console_sd_no_response()?LibraryCondition::NoResponse:LibraryCondition::MountFailed;return;}
    struct stat root;
    if(stat(console_sd_root,&root)!=0||!S_ISDIR(root.st_mode)){
        storageError=2002;storageStatus.condition=LibraryCondition::FolderMissing;
        console_sd_unmount();return;
    }
    bool readFailed=false;
    for(int i=0;i<console_media_count;i++){
        const auto& media=console_media[i];char path[192];
        snprintf(path,sizeof(path),"%s/%s",console_sd_root,media.file);
        struct stat st;errno=0;int found=stat(path,&st);
        available[i]=found==0&&S_ISREG(st.st_mode)&&st.st_size==media.bytes;
        if(found!=0){++storageStatus.missingFiles;if(errno!=ENOENT)readFailed=true;}
        else if(!available[i]){++storageStatus.invalidFiles;snprintf(storageStatus.file,sizeof(storageStatus.file),"%s",media.file);}
        console_boot_progress(i+1,console_media_count,media.file);
    }
    console_sd_unmount();
    countAvailable();storageStatus.checkedMs=millis();
    if(readFailed){storageError=2001;storageStatus.condition=LibraryCondition::ReadFailed;}
    else if(!storageStatus.validFiles){
        storageStatus.condition=storageStatus.invalidFiles?LibraryCondition::InvalidFile:LibraryCondition::NoMatchingFiles;
        storageError=storageStatus.invalidFiles?2003:2002;
    }else storageStatus.condition=storageStatus.missingFiles||storageStatus.invalidFiles?LibraryCondition::PartialLibrary:LibraryCondition::Ready;
    Serial.printf("[LIBRARY_SCAN] available_media=%u expected=%d elapsed_ms=%u error=%d condition=%d cached_game=%d\n",unsigned(storageStatus.validFiles),console_media_count,unsigned(millis()-start),storageError,int(storageStatus.condition),cachedGame);
}
bool library_rescan(){scanLibrary();return !library_needs_attention(storageStatus);}
bool library_original_os(const char* path){
    if(strncmp(path,"/game/",6))return false;
    for(int i=0;i<console_game_count;i++)if(!strcmp(path+6,game_info(i).file))return game_info(i).originalOs&&CONSOLE_HAS_ORIGINAL_OS;
    return false;
}
static Asset asset(const char* path){
    if(cached)for(int i=0;i<console_media_count;i++)if(path[0]=='/'&&!strcmp(path+1,console_media[i].file))return cached[i];
    return {nullptr,0};
}
struct Slot {bool used;size_t pos;Asset file;};
static Slot slots[4];
bool library_prepare_game(int index){
    if(cachedGame==index)return true;
    storageError=0;
    if(game_info(index).originalOs&&!CONSOLE_HAS_ORIGINAL_OS){storageError=2007;storageStatus.condition=LibraryCondition::RomRequired;return false;}
    if(!cached||!available){storageError=2004;storageStatus.condition=LibraryCondition::NoMemory;return false;}
    // The caller must detach the previous core's cartridge/disks first.
    for(auto& slot:slots)if(slot.used){storageError=2006;return false;}
    for(int i=0;i<console_media_count;i++){free((void*)cached[i].data);cached[i]={};}
    cachedGame=-1;
    const auto& info=game_info(index);int indices[]={mediaIndex(info.file),info.disk2?mediaIndex(info.disk2):-1};
    int needed=info.disk2?2:1;size_t total=0;uint32_t start=millis();
    console_game_progress(0,needed,info.file);
    bool mounted=console_sd_mount(4000);recordMount();
    if(!mounted){storageError=2001;storageStatus.condition=console_sd_no_response()?LibraryCondition::NoResponse:LibraryCondition::MountFailed;return false;}
    for(int n=0;n<needed;n++){
        int i=indices[n];if(i<0){storageError=2002;break;}
        const auto& media=console_media[i];char path[192];snprintf(path,sizeof(path),"%s/%s",console_sd_root,media.file);
        snprintf(storageStatus.file,sizeof(storageStatus.file),"%s",media.file);
        FILE* file=fopen(path,"rb");struct stat st;
        if(!file){available[i]=false;storageError=2002;break;}
        if(fstat(fileno(file),&st)!=0||st.st_size!=media.bytes){fclose(file);available[i]=false;storageError=2003;break;}
        auto* data=(uint8_t*)heap_caps_malloc(media.bytes,MALLOC_CAP_SPIRAM|MALLOC_CAP_8BIT);
        if(!data){fclose(file);storageError=2004;break;}
        size_t count=fread(data,1,media.bytes,file);bool ok=count==media.bytes&&!ferror(file);fclose(file);
        uint8_t hash[32];char hex[65];
        if(ok){mbedtls_sha256_ret(data,media.bytes,hash,0);for(int j=0;j<32;j++)snprintf(hex+j*2,3,"%02x",hash[j]);ok=!strcmp(hex,media.sha256);}
        if(!ok){free(data);available[i]=false;storageError=2003;break;}
        available[i]=true;cached[i]={data,media.bytes};total+=media.bytes;
        Serial.printf("[SD_FILE] %s bytes=%u sha256=OK\n",media.file,unsigned(media.bytes));
        console_game_progress(n+1,needed,media.file);
    }
    console_sd_unmount();
    countAvailable();storageStatus.checkedMs=millis();
    if(!storageError){
        cachedGame=index;storageStatus.file[0]=0;
        storageStatus.condition=storageStatus.validFiles==storageStatus.totalFiles?LibraryCondition::Ready:LibraryCondition::PartialLibrary;
    }else{
        storageStatus.condition=storageError==2004?LibraryCondition::NoMemory:storageError==2002?LibraryCondition::ReadFailed:LibraryCondition::InvalidFile;
        for(int i=0;i<console_media_count;i++){free((void*)cached[i].data);cached[i]={};}
    }
    Serial.printf("[GAME_CACHE] game=%s files=%d bytes=%u elapsed_ms=%u error=%d gameplay_io=0\n",info.id,storageError?0:needed,storageError?0:unsigned(total),unsigned(millis()-start),storageError);
    return !storageError;
}
std::string to_string(int value){char s[16];snprintf(s,sizeof(s),"%d",value);return s;}
std::string get_ext(const std::string& path){auto p=path.find_last_of('.');if(p==std::string::npos)return "";auto e=path.substr(p+1);for(auto& c:e)if(c>='A'&&c<='Z')c+='a'-'A';return e;}
static int openGame(const char* path,int flags,int){
    auto file=asset(path);if(!file.data){errno=ENOENT;return -1;}
    if((flags&O_ACCMODE)!=O_RDONLY){errno=EROFS;return -1;}
    for(int i=0;i<4;i++)if(!slots[i].used){slots[i]={true,0,file};return i;}
    errno=EMFILE;return -1;
}
static ssize_t readGame(int fd,void* dst,size_t count){
    if(fd<0||fd>=4||!slots[fd].used){errno=EBADF;return -1;}
    auto& f=slots[fd];size_t available=f.pos<f.file.size?f.file.size-f.pos:0;
    if(count>available)count=available;if(!count)return 0;memcpy(dst,f.file.data+f.pos,count);f.pos+=count;return count;
}
static off_t seekGame(int fd,off_t offset,int whence){
    if(fd<0||fd>=4||!slots[fd].used){errno=EBADF;return -1;}
    int64_t base=whence==SEEK_SET?0:whence==SEEK_CUR?slots[fd].pos:whence==SEEK_END?slots[fd].file.size:-1;
    int64_t pos=base+offset;if(base<0||pos<0||pos>INT32_MAX){errno=EINVAL;return -1;}
    slots[fd].pos=size_t(pos);return off_t(pos);
}
static int closeGame(int fd){if(fd<0||fd>=4||!slots[fd].used){errno=EBADF;return -1;}slots[fd]={};return 0;}
static int statGame(const char* path,struct stat* s){auto file=asset(path);if(!file.data){errno=ENOENT;return -1;}memset(s,0,sizeof(*s));s->st_mode=S_IFREG|0444;s->st_size=file.size;return 0;}
static int fstatGame(int fd,struct stat* s){if(fd<0||fd>=4||!slots[fd].used){errno=EBADF;return -1;}memset(s,0,sizeof(*s));s->st_mode=S_IFREG|0444;s->st_size=slots[fd].file.size;return 0;}
bool drop_begin(){
    scanLibrary();
    esp_vfs_t v={};v.flags=ESP_VFS_FLAG_DEFAULT;v.open=openGame;v.read=readGame;v.lseek=seekGame;v.close=closeGame;v.stat=statGame;v.fstat=fstatGame;
    auto result=esp_vfs_register("/game",&v,nullptr);
    queue=xQueueCreate(1,sizeof(PairInput));
    padRequests=xQueueCreate(1,sizeof(PadCommand));padStatuses=xQueueCreate(1,sizeof(PadStatus));
    Serial.printf("[FILES] readonly=1 result=%d games=%d source=microSD_cache no_format=1\n",result,unsigned(console_game_count));
    return result==ESP_OK && queue && padRequests && padStatuses && hub.begin() && controller0.begin() && controller1.begin();
}
void drop_poll(){
    if(!queue)return;
    uint32_t pollStart=micros();if(lastPoll && pollStart-lastPoll>maxPollGap.load())maxPollGap=pollStart-lastPoll;lastPoll=pollStart;
    PadCommand command;
    if(xQueueReceive(padRequests,&command,0)==pdTRUE){
        hub.command(command,millis());
        for(int i=0;i<2;i++){mapping[i]=PadMapping{};edges[i]=0;}
    }
    hub.update();PairInput sample{};unsigned maskConnected=0;
    auto padStatus=hub.status(millis());static uint32_t publishedRevision=0;static uint8_t publishedSeconds=0;
    if(padStatus.revision!=publishedRevision||padStatus.seconds!=publishedSeconds){
        xQueueOverwrite(padStatuses,&padStatus);publishedRevision=padStatus.revision;publishedSeconds=padStatus.seconds;
    }
    for(int i=0;i<2;i++){
        auto change=controllers[i]->update(millis());const auto& s=controllers[i]->state();
        auto& input=sample.players[i];input.valid=s.connected&&s.inputValid;
        mapping[i].update(input.valid,s.held,s.analog.lt,s.analog.rt);
        if(change.inputChanged)changedAt[i]=micros();input.changedUs=changedAt[i];
        if(input.valid){input.held=mapping[i].held;input.lx=s.analog.lx;input.ly=s.analog.ly;input.physicalHeld=s.held;input.lt=s.analog.lt;input.rt=s.analog.rt;edges[i].fetch_or(mapping[i].pressed);}
        else edges[i]=0;
        if(s.connected)maskConnected|=1u<<i;
        static uint32_t lastLog[2]={};
        if(false && change.inputChanged && s.inputValid && (s.pressed||s.released||millis()-lastLog[i]>=250)){
            lastLog[i]=millis();Serial.printf("[PAD_INPUT] player=%d held=0x%04x lx=%d ly=%d\n",i+1,s.held,int(input.lx*100),int(input.ly*100));
        }
    }
    connected=maskConnected;xQueueOverwrite(queue,&sample);
    uint32_t cost=micros()-pollStart;if(cost>maxPollCost.load())maxPollCost=cost;
}
DropInput drop_input(int player){PairInput sample{};if(queue)xQueuePeek(queue,&sample,0);return sample.players[player];}
uint16_t drop_pressed(int player){return edges[player].exchange(0);}
bool drop_connected(){return connected.load()!=0;}
unsigned archon_connected(){return connected.load();}
PadStatus drop_pad_status(){PadStatus s{};if(padStatuses)xQueuePeek(padStatuses,&s,0);return s;}
bool drop_pad_request(PadCommand command){return padRequests&&xQueueSend(padRequests,&command,0)==pdTRUE;}
extern "C" uint8_t* library_map_file(const char* path,int len){
    if(strncmp(path,"/game/",6))return nullptr;auto f=asset(path+5);
    return f.data&&size_t(len)==f.size?const_cast<uint8_t*>(f.data):nullptr;
}
extern "C" void gui_msg(const char* msg){Serial.printf("[CORE] %s\n",msg);}
