# Overview

This document describes HEVC extension of the Flexible Encode Infrastructure for fine-tuning of hardware encoding pipeline. Please refer to the *SDK API Reference Manual* and to the *SDK API Reference Manual for Flexible Encode Infrastructure* for a complete description of the API.



## Acronyms and Abbreviations

| | |
--- | ---
**FEI** | Flexible Encode Infrastructure
**ENC** | ENCode first stage of encoding process that include motion estimation and CTB mode decision.
**PAK** | PAcK  last stage of encoding process that include bit packing.
**PreENC** | Pre Encoding
**MV** | Motion Vector
**MB** | Macro Block
**SPS** | Sequence Parameter Set
**PPS** | Picture Parameter Set


<div style="page-break-before:always" />
# Architecture

HEVC FEI has similar to AVC FEI functionality. This manual covers HEVC specific extension only.

# Programming Guide

This chapter describes the concepts used in programming the FEI extension for SDK.


<div style="page-break-before:always" />
# Function Reference

This section describes SDK functions and their operations.

In each function description, only commonly used status codes are documented. The function may return additional status codes, such as **MFX_ERR_INVALID_HANDLE** or **MFX_ERR_NULL_PTR**, in certain case. See the **mfxStatus** enumerator for a list of all status codes.

## <a id='MFXVideo_xxx'>MFXVideo_xxx</a>

**Syntax**

```
mfxStatus MFXVideo_xxx(mfxSession session, mfxVideoParam *par);
```

**Parameters**

| | |
--- | ---
`session` | SDK session handle
`par` | Pointer to the mfxVideoParam structure

**Description**

This function initializes **xxx** class of functions. [mfxFeiFunction](#_mfxFeiFunction) should be
attached to the **mfxVideoParam** to select required usage model.

**Return Status**

| | |
--- | ---
`MFX_ERR_NONE` | The function completed successfully.

**Change History**

This function is available since SDK API 1.xxx.


<div style="page-break-before:always" />
# Structure Reference

In the following structures all reserved fields must be zero.


## <a id='_mfxExtFeiHevcEncFrameCtrl'></a>mfxExtFeiHevcEncFrameCtrl

**Definition**

```
typedef struct {
    mfxExtBuffer    Header;

    mfxU16    SearchPath;
    mfxU16    LenSP;
    mfxU16    RefWidth;
    mfxU16    RefHeight;
    mfxU16    SearchWindow;

    mfxU16    NumMvPredictorsL0;
    mfxU16    NumMvPredictorsL1;
    mfxU16    MultiPredL0;
    mfxU16    MultiPredL1;

    mfxU16    SubPelMode;
    mfxU16    AdaptiveSearch;
    mfxU16    MVPredictor;

    mfxU16    PerCtbQp;
    mfxU16    PerCtbInput;
    mfxU16    CoLocatedCtbDistortion;
    mfxU16    ForceLcuSplit;

    mfxU16    reserved0[108];
} mfxExtFeiHevcEncFrameCtrl;
```

**Description**

This extension buffer specifies frame level control for ENCODE and ENC usage models. It is used
during runtime and should be attached to the **mfxEncodeCtrl** structure for ENCODE usage model and
to the **mfxENCInput** for ENC.

To better utilize HW capability, motion estimation is performed on group of search locations, so
called search unit (SU). The number of locations in one SU depends on the block size. For example,
for 16x16 macroblock, SU consists of 4x4 locations, i.e. 16 motion vectors are estimated at once, in
one SU. See the figure on the left.

These SUs are arranged in search path (SP). This is predefined set of search units, for example,
diamond shaped path. Motion estimation will go along this path until **LenSP** SUs will be checked.

If all SUs in SP have been processed and adaptive search has been enabled, motion estimation
continues for neighbor SUs, until local minimum will be found or number of processed SUs reached
**MaxLenSP** (not controllable by application) or boundary of search window will be reached.

Note, that though search window size is rather small, just 48 by 40 pixels, actual motion vectors
may be much longer, because this search window is specified relative to the motion vector predictor.
And that in turn may be of any valid length.

**Members**

| | |
--- | ---
`Header.BufferId` | Buffer ID, must be **MFX_EXTBUFF_HEVCFEI_ENC_CTRL**.
`SearchPath` | reserved and must be zero<br><br>This value specifies search path.<br><br>0 - exhaustive aka full search<br><br>1 - diamond search
`LenSP` | reserved and must be zero<br><br>This value defines number of search units in search path. If adaptive search is enabled it starts after this number has been reached. Valid range [1,63].
`RefWidth, RefHeight` | reserved and must be zero<br><br>These values specify width and height of search region in pixels. They should be multiple of 4. Maximum allowed region is 64x32 for one direction and 32x32 for bidirectional search.
`SearchWindow` | This value specifies one of the predefined search path and window size.<br><br> 0 : not use predefined search window<br> 1 : Tiny – 4 SUs 24x24 window diamond search<br> 2 : Small – 9 SUs 28x28 window diamond search<br> 3 : Diamond – 16 SUs 48x40 window diamond search<br> 4 : Large Diamond – 32 SUs 48x40 window diamond search<br> 5 : Exhaustive – 48 SUs 48x40 window full search<br> 6 : Diamond – 16 SUs 64x32 window diamond search<br> 7 : Large Diamond – 32 SUs 64x32 window diamond search<br> 8 : Exhaustive – 48 SUs 64x32 window full search
`NumMvPredictorsL0` | Number of L0 MV predictors provided by the application. Up to four L0 predictors are supported.
`NumMvPredictorsL1` | Number of L1 MV predictors provided by the application. Up to four L1 predictors are supported.
`MultiPredL0, MultiPredL1` | If this value is not equal to zero, then MVs from neighbor CTBs will be used as predictors.
`SubPelMode` | This value specifies sub pixel precision for motion estimation.<br><br>0x00 - integer motion estimation<br><br>0x01 - half-pixel motion estimation<br><br>0x03 - quarter-pixel motion estimation<br><br>
`AdaptiveSearch` | If set, adaptive search is enabled.
`MVPredictor` | If this value is not equal to zero, then usage of MV predictors is enabled and the application should attach [mfxExtFeiHevcEncMVPredictors](#_mfxExtFeiHevcEncMVPredictors) structure to the **mfxEncodeCtrl** structure at runtime.
`PerCtbQp` | If this value is not equal to zero, then CTB level QPs are used during encoding and [mfxExtFeiHevcEncQP](#_mfxExtFeiHevcEncQP) structure should be attached to the **mfxEncodeCtrl** structure at runtime.
`PerCtbInput` | If this value is not equal to zero, then CTB level control is enabled and [mfxExtFeiHevcEncCtbCtrl](#_mfxExtFeiHevcEncCtbCtrl) structure should be attached to the **mfxEncodeCtrl** structure at runtime.
`CoLocatedCtbDistortion` | reserved and must be zero<br><br>If set, this field enables calculation of **ColocatedCtbDistortion** value in the [mfxExtFeiHevcEncCtbStat](#_mfxExtFeiHevcEncCtbStat) structure.
`PerCtbQp` | If this value is not equal to zero, then CTB level QPs are used during encoding and [mfxExtFeiHevcEncQP](#_mfxExtFeiHevcEncQP) structure should be attached to the **mfxEncodeCtrl** structure at runtime.
`SubPelMode`| This value specifies sub pixel precision for motion estimation.<br> <table><tr> <td>00b </td> <td>integer mode searching</td> </tr><tr> <td>01b</td> <td>half-pel mode searching</td> </tr><tr> <td>10b</td> <td>reserved</td> </tr><tr> <td>11b</td> <td>quarter-pel mode searching</td> </tr></table>
`AdaptiveSearch`|If set, adaptive search is enabled.

**Change History**

This structure is available since SDK API 1..



## <a id='_mfxFeiHevcEncMVPredictors'></a>mfxFeiHevcEncMVPredictors

**Definition**

```
typedef struct {
    struct {
        mfxU8   RefL0 : 4;
        mfxU8   RefL1 : 4;
    } RefIdx[4]; /* index is predictor number */

    mfxU32     BlockSize : 2;
    mfxU32     reserved0 : 30;

    mfxI16Pair MV[4][2]; /* first index is predictor number, second is 0 for L0 and 1 for L1 */
} mfxFeiHevcEncMVPredictors;
```

**Description**

This structure specifies MV predictors for ENCODE and ENC usage models.

**Members**

| | |
--- | ---
`RefIdx[4]` | Array of reference indexes for each MV predictor.
`RefL0, RefL1` | **L0** and **L1** reference indexes.
`BlockSize` |
`MV[4][2]` | Up to 4 MV predictors per CTB. First index is predictor number, second is 0 for L0 (past) reference and 1 for L1 (future) reference.<br><br>**0x8000** value should be used for intra MBs.<br><br>Number of actual predictors is defined by **NumMVPredictors[]** value in the [mfxExtFeiHevcEncFrameCtrl](#_mfxExtFeiHevcEncFrameCtrl) structure.<br><br>MV predictor is for the whole 16x16 MB.

**Change History**

This structure is available since SDK API 1..

## <a id='_mfxExtFeiHevcEncMVPredictors'></a>mfxExtFeiHevcEncMVPredictors

**Definition**

```
typedef struct {
    mfxExtBuffer  Header;
    mfxU32        VaBufferID;
    mfxU32        DataSize;
    mfxU16        reserved0[56];

    mfxFeiHevcEncMVPredictors *Data;
} mfxExtFeiHevcEncMVPredictors;
```

**Description**

This extension buffer specifies MV predictors for ENCODE and ENC usage models. To enable usage of
this buffer the application should set **MVPredictor** field in the
[mfxExtFeiHevcEncFrameCtrl](#_mfxExtFeiHevcEncFrameCtrl) structure to none zero value.

This structure is used during runtime and should be attached to the **mfxEncodeCtrl** structure for
ENCODE usage model and to the **mfxENCInput** for ENC.

**Members**

| | |
--- | ---
`Header.BufferId` | Buffer ID, must be **MFX_EXTBUFF_HEVCFEI_ENC_MV_PRED**.
`VaBufferID` |
`DataSize` |
`Data` |

**Change History**

This structure is available since SDK API 1..


## <a id='_mfxExtFeiHevcEncQPs'></a><a id='_mfxExtFeiHevcEncQP'></a>mfxExtFeiHevcEncQP

**Definition**

```
typedef struct {
    mfxExtBuffer  Header;
    mfxU32        VaBufferID;
    mfxU32        DataSize;
    mfxU16        reserved[8];

    mfxU8    *Data;
} mfxExtFeiHevcEncQP;
```

**Description**

This extension buffer specifies per CTB QP values for PreENC, ENCODE and ENC usage models. To enable
its usage for PreENC, set **mfxExtFeiHevcPreEncCtrl**::**CtbQp** value, for ENCODE and ENC set
**mfxExtFeiHevcEncFrameCtrl::PerCtbQp** value.

This structure is used during runtime and should be attached to the [mfxENCInput](#_mfxENCInput) or
**mfxEncodeCtrl** structure.

**Members**

| | |
--- | ---
`Header.BufferId` | Buffer ID, must be **MFX_EXTBUFF_HEVCFEI_ENC_QP**.
`VaBufferID` |
`DataSize` |
`Data` |

**Change History**

This structure is available since SDK API 1..

## <a id='_mfxFeiHevcEncCtbCtrl'></a>mfxFeiHevcEncCtbCtrl

**Definition**

```
typedef struct {
    mfxU32    ForceToIntra : 1;
    mfxU32    reserved0    : 31;

    mfxU32    reserved1;
    mfxU32    reserved2;
    mfxU32    reserved3;
} mfxFeiHevcEncCtbCtrl;
```

**Description**

This structure specifies CTB level parameters for ENCODE and ENC usage models.

**Members**

| | |
--- | ---
`ForceToIntra` | If this value is set to ‘1’, then current CTB is encoded as intra CTB, otherwise encoder decides CTB type.

**Change History**

This structure is available since SDK API 1..



## <a id='_mfxExtFeiHevcEncCtbCtrl'></a>mfxExtFeiHevcEncCtbCtrl

**Definition**

```
typedef struct {
    mfxExtBuffer  Header;
    mfxU32        VaBufferID;
    mfxU32        DataSize;
    mfxU16        reserved0[56];

    mfxFeiHevcEncCtbCtrl *Data;
} mfxExtFeiHevcEncCtbCtrl;
```

**Description**

This extension buffer specifies CTB level parameters for ENCODE and ENC usage models. To enable usage
of this buffer the application should set **PerCtbInput** field in the
[mfxExtFeiHevcEncFrameCtrl](#_mfxExtFeiHevcEncFrameCtrl) structure to none zero value.

This structure is used during runtime and should be attached to the **mfxEncodeCtrl** structure for
ENCODE usage model and to the **mfxENCInput** for ENC.

**Members**

| | |
--- | ---
`Header.BufferId` | Buffer ID, must be **MFX_EXTBUFF_HEVCFEI_ENC_CTB_CTRL**.
`VaBufferID` |
`DataSize` |
`Data` |

**Change History**

This structure is available since SDK API 1..


<div style="page-break-before:always" />
# Enumerator Reference

## <a id='_mfxFeiFunction'></a>mfxFeiFunction

**Description**

The `mfxFeiFunction` enumerator specifies FEI usage models of **ENCODE**, **ENC** and **PAK** classes of functions.

**Name/Description**

| | |
--- | ---
`xxx` | xxx usage models.

**Change History**

This enumerator is available since SDK API 1.xxx.
