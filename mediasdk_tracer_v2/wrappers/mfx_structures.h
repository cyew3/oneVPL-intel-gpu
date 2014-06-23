//mfxstructures.h
#ifndef MFX_STRUCTURES_H_
#define MFX_STRUCTURES_H_

#include "mfxvideo.h"
#include "../loggers/timer.h"

enum  Component {
    DECODE  = 0x1,
    ENCODE  = 0x2,
    VPP     = 0x4,
};

struct TracerSyncPoint {
    mfxSyncPoint syncPoint;
    Component component;
    Timer timer;
};

#endif //MFX_STRUCTURES_H_
