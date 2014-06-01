#if defined(_WIN32) || defined(_WIN64)

#include <string>
#include "mfxvideo.h"
#include "../loggers/log.h"
#include "../dumps/dump.h"
#include "functions_table.h"

mfxStatus MFXInit(mfxIMPL impl, mfxVersion *ver, mfxSession *session)
{
    return MFX_ERR_NOT_FOUND;
}

mfxStatus MFXClose(mfxSession session)
{
    return MFX_ERR_NOT_FOUND;
}

void DLLMain()
{

}

#endif // #if defined(_WIN32) || defined(_WIN64)
