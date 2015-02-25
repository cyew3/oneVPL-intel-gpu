#ifndef TRACER_H_
#define TRACER_H_


#include <string>
#include <iostream>
#include <stdlib.h>
#include <exception>
#include "mfxvideo.h"
#include "../config/config.h"
#include "../dumps/dump.h"
#include "../loggers/log.h"
#include "../loggers/timer.h"
#include "functions_table.h"


void tracer_init();
mfxStatus _MFXInitEx(mfxInitParam par, mfxSession *session);

#endif //TRACER_H_