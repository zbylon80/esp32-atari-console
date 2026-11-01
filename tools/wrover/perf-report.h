#pragma once
#include <stdint.h>
struct PerfReport {
    uint32_t magic,request,frames,elapsedMs,coreUs,copyUs,audioUs,maxUs;
    uint32_t guards,connected,frameHash,ramHash,pc,a,x,y,s,p,internalFree;
    char variant[32];
};
static constexpr uint32_t perfMagic=0x50455231;
