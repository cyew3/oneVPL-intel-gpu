# Overview

This document describes HEVC extension of the Flexible Encode Infrastructure (FEI). It is not a comprehensive manual, it describes only HEVC specific functionality of FEI, mostly data structures. For complete description of FEI, including architecture details and usage models please refer to the *SDK API Reference Manual for Flexible Encode Infrastructure*. Both these manuals assume that reader is familiar with Media SDK architecture described in the *SDK API Reference Manual*.

In this manual term "AVC FEI" is often used to distinguish general FEI extension described in above mentioned manual from "HEVC FEI" extension described in this document.



## Acronyms and Abbreviations

| | |
--- | ---
**FEI** | Flexible Encode Infrastructure
**PreENC** | Pre Encoding - preliminary stage of encoding process, usually used for content analysis.
**ENC** | ENCode - first stage of encoding process that includes motion estimation and mode decision.
**PAK** | PAcK - last stage of encoding process that includes bit packing.
**MVP** | Motion Vector Predictor.



<div style="page-break-before:always" />
# Architecture

HEVC FEI is built as extension of AVC FEI. It uses the same classes of functions **ENC**, **PAK** and **ENCODE**. It has the same four kinds of calls PreENC, ENCODE, ENC and PAK. And the same major usage models "PreENC followed by ENCODE" and "ENC followed by PAK". See *Architecture* chapter of the *SDK API Reference Manual for Flexible Encode Infrastructure* for more details.

## Direct access to VA buffers

To improve performance, HEVC FEI eliminates additional data copy inside SDK library (see Figure 1) by allowing direct access to VA buffers, as illustrated in Figure 2.

###### Figure 1: mfxExtBuffer mapping to VA buffers

![mfxExtBuffer mapping to VA buffers](./pic/ext_buffers_mapping.png)

###### Figure 2: Direct access to VA buffers

![Direct access to VA buffers](./pic/va_buffers_direct_access.png)

The application manages extension buffer allocation through VA API for Linux*. In order to do that, it is recommended to implement a buffer allocator and use it across the entire application. Example 1 shows the pseudo code of the buffer allocator implementation and its usage.
Note that only extension buffers with field `VaBufferID` support direct access to VA buffers. Others must be allocated in system memory.

###### Example 1: Buffer Allocator Pseudo Code
```C
#include <va/va.h>
#include "mfxfeihevc.h"

class mfxBufferAllocator
{
    Alloc(mfxExtBuffer buffer)
    {
        vaCreateBuffer(VADisplay, VAContextID, VABufferType, buffer_size, num_elem, NULL, buffer.VaBufferID);
        buffer.DataSize = buffer_size * num_elem;
        buffer.Data = NULL;
    }
    Free(mfxExtBuffer buffer)
    {
        vaDestroyBuffer(VADisplay, buffer.VaBufferID);
    }
    Lock(mfxExtBuffer buffer)
    {
        vaMapBuffer(VADisplay, buffer.VaBufferID, buffer.Data);
    }
    Unlock(mfxExtBuffer buffer)
    {
        vaUnmapBuffer(VADisplay, buffer.VaBufferID);
        buffer.Data = NULL;
    }
}

mfxBufferAllocator allocator(VADisplay);
mfxExtFeiHevcEncQP qp;
allocator.Alloc(qp, num_ctu);

for (;;)
{
    allocator.Lock(qp);
    FillInQpBuffer(qp);
    allocator.Unlock(qp);

    EncodeFrame(qp);
}
allocator.Free(qp);
```

Please refer to Appendix E in the *SDK API Reference Manual* for more details about working directly with VA API.


<div style="page-break-before:always" />
# Programming Guide

To build HEVC FEI based application next header files should be included
- mfxenc.h - for PreENC and ENC functionality
- mfxfei.h - for basic FEI functionality and PreENC
- mfxhevcfei.h - for HEVC FEI extension
- mfxvideo.h - -for the rest of Media SDK functionality

HEVC FEI does not extend PreENC functionality. Exactly the same functions and data structures should be used for HEVC pre-processing.

In most other use cases, command flow for HEVC FEI is the same as for AVC FEI. The same functions but different data structures should be used. See [Structure Reference](#Structure_Reference) below for description of new structures.

The SDK distinguishes between AVC and HEVC FEI by `CodecId` parameter in `mfxVideoParam` provided during initialization. The rest of initialization remains the same.



<div style="page-break-before:always" />
# <a id='Structure_Reference'>Structure Reference</a>

In the following structures all reserved fields must be zeroed by application if structure is used as input, and should not be modified if structure is passed between different SDK components.


## <a id='mfxExtFeiHevcEncFrameCtrl'>mfxExtFeiHevcEncFrameCtrl</a>

**Definition**

```
typedef struct {
    mfxExtBuffer    Header;

    mfxU16    SearchPath;
    mfxU16    LenSP;
    mfxU16    RefWidth;
    mfxU16    RefHeight;
    mfxU16    SearchWindow;

    mfxU16    NumMvPredictors[2];
    mfxU16    MultiPred[2];

    mfxU16    SubPelMode;
    mfxU16    AdaptiveSearch;
    mfxU16    MVPredictor;

    mfxU16    PerCuQp;
    mfxU16    PerCtuInput;
    mfxU16    ForceCtuSplit;
    mfxU16    NumFramePartitions;

    mfxU16    reserved0[108];
} mfxExtFeiHevcEncFrameCtrl;
```

**Description**

This extension buffer specifies frame level control for ENCODE and ENC usage models. It is used during runtime and should be attached to the `mfxEncodeCtrl` structure for ENCODE usage model and to the `mfxENCInput` for ENC.

**Members**

| | |
--- | ---
`Header.BufferId` | Buffer ID, must be `MFX_EXTBUFF_HEVCFEI_ENC_CTRL`.
`SearchPath` | This value specifies search path.<br><br> 0x00 - default <br> 0x01 - diamond search<br> 0x02 - exhaustive, aka full search
`LenSP` | This value defines number of search units in search path. If adaptive search is enabled it starts after this number has been reached. Valid range [1,63].
`RefWidth, RefHeight` | These values specify width and height of search region in pixels. They should be multiple of 4. Maximum allowed region is 64x32 for one direction and 32x32 for bidirectional search.
`SearchWindow` | This value specifies one of the predefined search path and window size.<br><br> 0x00 - not use predefined search window<br> 0x01 - Tiny – 4 SUs 24x24 window diamond search<br> 0x02 - Small – 9 SUs 28x28 window diamond search<br> 0x03 - Diamond – 16 SUs 48x40 window diamond search<br> 0x04 - Large Diamond – 32 SUs 48x40 window diamond search<br> 0x05 - Exhaustive – 48 SUs 48x40 window full search<br> 0x06 - Diamond – 16 SUs 64x32 window diamond search<br> 0x07 - Large Diamond – 32 SUs 64x32 window diamond search<br> 0x08 - Exhaustive – 48 SUs 64x32 window full search
`NumMvPredictors[2]` | Number of L0/L1 MV predictors provided by the application. Up to four predictors are supported.
`MultiPred[2]` | If this value is equal to zero, then no internal MV predictors will be used. Set it to 1 to enable additional (spatial) MV predictors from neighbor CUs. Note, that disabling internal MV predictors can severely degrade video quality.
`SubPelMode` | This value specifies sub pixel precision for motion estimation.<br><br> 0x00 - integer motion estimation<br> 0x01 - half-pixel motion estimation<br> 0x03 - quarter-pixel motion estimation<br>
`AdaptiveSearch` | If set, adaptive search is enabled.
`MVPredictor` | If this value is not equal to zero, then usage of MV predictors is enabled and the application should attach [mfxExtFeiHevcEncMVPredictors](#mfxExtFeiHevcEncMVPredictors) structure to the `mfxEncodeCtrl` structure at runtime. This value also specifies predictor block size: <br><br> 0x00 - MVPs are disabled <br> 0x01 - MVPs are enabled for 16x16 block <br> 0x02 - MVPs are enabled for 32x32 block <br> 0x07 - MVPs are enabled, block size is defined by `BlockSize` variable in [mfxFeiHevcEncMVPredictors](#mfxExtFeiHevcEncMVPredictors) structure.
`PerCuQp` | If this value is not equal to zero, then CU level QPs are used during encoding and [mfxExtFeiHevcEncQP](#mfxExtFeiHevcEncQP) structure should be attached to the `mfxEncodeCtrl` structure at runtime.
`PerCtuInput` | If this value is not equal to zero, then CTU level control is enabled and [mfxExtFeiHevcEncCtuCtrl](#mfxExtFeiHevcEncCtuCtrl) structure should be attached to the `mfxEncodeCtrl` structure at runtime.
`ForceCtuSplit` | If this value is not equal to zero, then each CTU in frame is split at least once. This is performance/quality trade-off flag, setting it improves performance but reduces quality.
`NumFramePartitions` | This value specifies number of partitions in frame that encoder processes concurrently. Valid numbers are {1, 2, 4, 8, 16}. This is performance/quality trade-off parameter. The smaller the number of partitions the better quality, the worse performance.


**Change History**

This structure is available since SDK API 1.25



## <a id='mfxExtFeiHevcEncMVPredictors'>mfxExtFeiHevcEncMVPredictors</a>
**Definition**

```
typedef struct {
    struct {
        mfxU8   RefL0 : 4;
        mfxU8   RefL1 : 4;
    } RefIdx[4];

    mfxU32     BlockSize : 2;
    mfxU32     reserved0 : 30;

    mfxI16Pair MV[4][2];
} mfxFeiHevcEncMVPredictors;


typedef struct {
    mfxExtBuffer  Header;
    mfxU32        VaBufferID;
    mfxU32        Pitch;
    mfxU32        Height;
    mfxU16        reserved0[54];

    mfxFeiHevcEncMVPredictors *Data;
} mfxExtFeiHevcEncMVPredictors;
```

**Description**
This extension buffer specifies MV predictors for ENCODE and ENC usage models. To enable usage of this buffer the application should set `MVPredictor` field in the [mfxExtFeiHevcEncFrameCtrl](#mfxExtFeiHevcEncFrameCtrl) structure to non-zero value.

This structure is used during runtime and should be attached to the `mfxEncodeCtrl` structure for ENCODE usage model and to the `mfxENCInput` for ENC.

**Members**

| | |
--- | ---
`Header.BufferId` | Buffer ID, must be `MFX_EXTBUFF_HEVCFEI_ENC_MV_PRED`.
`VaBufferID` | VA buffer ID. It is used by buffer allocator and SDK encoder and should not be directly set or changed by application.
`Pitch`<br>`Height` | Pitch and height of `Data` buffer in elements. `Pitch` may be bigger than picture width divided by CTU size, and `Height` may be bigger than picture height divided by CTU size due to alignment requirements of underlying HW implementation. This value is set by buffer allocator and should not be directly set or changed by application.<br><br>To access an element located in specified row and column next code may be used: <br> `mfxFeiHevcEncMVPredictors *mvp = buf.Data + row * buf.Pitch + col;`
`Data` | Buffer that holds actual MV predictors.
`RefIdx[4]` | Array of reference indexes for each MV predictor. Index in the array is a predictor number.
`RefL0, RefL1` | L0 and L1 reference indexes.
`BlockSize` | Block size for which MV predictors are specified. <br><br> 0x0 - disable MVPs for current block<br> 0x1 - MVPs for 16x16 blocks<br> 0x2 - MVPs for 32x32 block<br><br>It is used only if `MVPredictor` variable in [mfxExtFeiHevcEncFrameCtrl](#mfxExtFeiHevcEncFrameCtrl) structure is set to 0x07.
`MV[4][2]` | Up to 4 MV predictors per CTU. First index is predictor number, second is 0 for L0 reference and 1 for L1 reference.<br><br>0x8000 value should be used for intra CTUs.<br><br>Number of actual predictors is defined by `NumMVPredictors[2]` value in the [mfxExtFeiHevcEncFrameCtrl](#mfxExtFeiHevcEncFrameCtrl) structure. Unused MV predictors are ignored.

**Change History**

This structure is available since SDK API 1.25



## <a id='mfxExtFeiHevcEncQP'>mfxExtFeiHevcEncQP</a>

**Definition**

```
typedef struct {
    mfxExtBuffer  Header;
    mfxU32        VaBufferID;
    mfxU32        Pitch;
    mfxU32        Height;
    mfxU16        reserved[6];

    mfxU8    *Data;
} mfxExtFeiHevcEncQP;
```

**Description**

This extension buffer specifies per CU QP values for ENCODE and ENC usage models. To enable its usage for ENCODE and ENC set `PerCuQp` value in the [mfxExtFeiHevcEncFrameCtrl](#mfxExtFeiHevcEncFrameCtrl) structure.

This structure is used during runtime and should be attached to the `mfxEncodeCtrl` structure for ENCODE usage model and to the `mfxENCInput` for ENC.

**TODO: add layout description here.**

Note, that depending on HW capabilities some limitations on QP values may apply. Please refer to release notes for further details.

**Members**

| | |
--- | ---
`Header.BufferId` | Buffer ID, must be `MFX_EXTBUFF_HEVCFEI_ENC_QP`.
`VaBufferID` | VA buffer ID. It is used by buffer allocator and SDK encoder and should not be directly set or changed by application.
`Pitch`<br>`Height` | Pitch and height of `Data` buffer in elements. This value is set by buffer allocator and should not be directly set or changed by application.<br><br>To access an element located in specified row and column next code may be used: <br> `mfxU8 *qp = buf.Data + row * buf.Pitch + col;`
`Data` | Buffer that holds per CU QP values.

**Change History**

This structure is available since SDK API 1.25



## <a id='mfxExtFeiHevcEncCtuCtrl'>mfxExtFeiHevcEncCtuCtrl</a>

**Definition**

```
typedef struct {
    mfxU32    ForceToIntra : 1;
    mfxU32    ForceToInter : 1;
    mfxU32    reserved0    : 30;

    mfxU32    reserved1[3];
} mfxFeiHevcEncCtuCtrl;


typedef struct {
    mfxExtBuffer  Header;
    mfxU32        VaBufferID;
    mfxU32        Pitch;
    mfxU32        Height;
    mfxU16        reserved0[54];

    mfxFeiHevcEncCtuCtrl *Data;
} mfxExtFeiHevcEncCtuCtrl;
```

**Description**

This structure specifies CTU level control parameters for ENCODE and ENC usage models. To enable its usage for ENCODE and ENC set `PerCtuInput` value in the [mfxExtFeiHevcEncFrameCtrl](#mfxExtFeiHevcEncFrameCtrl) structure.

This structure is used during runtime and should be attached to the `mfxEncodeCtrl` structure for ENCODE usage model and to the `mfxENCInput` for ENC.

**Members**

| | |
--- | ---
`Header.BufferId` | Buffer ID, must be 'MFX_EXTBUFF_HEVCFEI_ENC_CTU_CTRL'.
`VaBufferID` | VA buffer ID. It is used by buffer allocator and SDK encoder and should not be directly set or changed by application.
`Pitch`<br>`Height` | Pitch and height of `Data` buffer in elements. `Pitch` may be bigger than picture width divided by CTU size, and `Height` may be bigger than picture height divided by CTU size due to alignment requirements of underlying HW implementation. This value is set by buffer allocator and should not be directly set or changed by application.<br><br>To access an element located in specified row and column next code may be used: <br> `mfxFeiHevcEncCtuCtrl *ctrl = buf.Data + row * buf.Pitch + col;`
`Data` | Buffer that holds per CTU control parameters.
`ForceToIntra` <br>`ForceToInter` | If one of these values is set to 1, then current CTU encoded accordingly, as Intra or Inter. If more than one value is set or all values are zero, then encoder decides CTU type.

**Change History**

This structure is available since SDK API 1.25
