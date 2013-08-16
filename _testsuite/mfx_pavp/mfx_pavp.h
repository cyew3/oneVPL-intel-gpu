// The following ifdef block is the standard way of creating macros which make exporting 
// from a DLL simpler. All files within this DLL are compiled with the MFX_PAVP_EXPORTS
// symbol defined on the command line. this symbol should not be defined on any project
// that uses this DLL. This way any other project whose source files include this file see 
// MFX_PAVP_API functions as being imported from a DLL, whereas this DLL sees symbols
// defined with this macro as being exported.
#ifdef MFX_PAVP_EXPORTS
#define MFX_PAVP_API __declspec(dllexport)
#else
#define MFX_PAVP_API __declspec(dllimport)
#endif


#include "win8ui_video_lib.h"
#include "pavp_video_lib_dx11.h"
#include "pavp_video_lib_dx9.h"

extern "C" CPAVPSession * CreateCSIGMASession(
        CCPDevice *cpDevice,
        uint32 certAPIVersion,
        const uint8 *cert, 
        uint32 certSize, 
        const void **revocationLists, 
        uint32 revocationListsCount,
        const EcDsaPrivKey privKey,
        const uint32 *seed,
        uint32 seedBits);

extern "C" CVideoProtection* CreateCVideoProtection_CS11(
    CCPDeviceD3D11Base* cpDevice);

extern "C" CPAVPVideo* CreateCPAVPVideo_D3D11(
    CCPDeviceD3D11* cpDevice, 
    CPAVPSession *session, 
    const PAVP_ENCRYPTION_MODE *decryptorMode);

extern "C" CPAVPVideo* CreateCPAVPVideo_Auxiliary9(
    CCPDeviceAuxiliary9* cpDevice, 
    CPAVPSession *session, 
    const PAVP_ENCRYPTION_MODE *encryptorMode,
    const PAVP_ENCRYPTION_MODE *decryptorMode);

extern "C" CPAVPVideo* CreateCPAVPVideo_CryptoSession9(
    CCPDeviceCryptoSession9* cpDevice, 
    CPAVPSession *session, 
    const PAVP_ENCRYPTION_MODE *decryptorMode);


// This class is exported from the mfx_pavp.dll
class MFX_PAVP_API Cmfx_pavp {
public:
    Cmfx_pavp(void);
    // TODO: add your methods here.
};

extern MFX_PAVP_API int nmfx_pavp;

MFX_PAVP_API int fnmfx_pavp(void);
