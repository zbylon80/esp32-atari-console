#pragma once
#include <stdint.h>

// A snapshot of an explicit scan/load, never a background SD poll.
enum class LibraryCondition : uint8_t {
    NotChecked, Ready, PartialLibrary, NoResponse, MountFailed,
    FolderMissing, NoMatchingFiles, ReadFailed, InvalidFile, NoMemory, RomRequired
};
struct LibraryStatus {
    LibraryCondition condition=LibraryCondition::NotChecked;
    int sdError=0;
    char sdErrorName[40]={};
    char file[40]={};
    uint32_t checkedMs=0;
    uint16_t availableGames=0,totalGames=0,validFiles=0,totalFiles=0;
    uint16_t missingFiles=0,invalidFiles=0;
    bool originalOs=false;
};
inline bool library_needs_attention(const LibraryStatus& s){
    return s.condition!=LibraryCondition::Ready&&s.condition!=LibraryCondition::PartialLibrary;
}
const LibraryStatus& library_status();
bool library_rescan();
void console_diagnostics_ui(uint8_t** screen,const LibraryStatus& status,
                            unsigned pads,uint32_t internalFree,uint32_t psramFree);
