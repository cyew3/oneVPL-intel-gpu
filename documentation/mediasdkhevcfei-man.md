# Overview

This document describes HEVC extension of the Flexible Encode Infrastructure for fine-tuning of hardware encoding pipeline. Please refer to the *SDK API Reference Manual* and to the *SDK API Reference Manual for Flexible Encode Infrastructure* for a complete description of the API.



## Acronyms and Abbreviations

| | |
--- | ---
**FEI** | Flexible Encode Infrastructure
**ENC** | ENCode first stage of encoding process that include motion estimation and MB mode decision.
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

## <a id='_mfxExtFeiPreEncCtrl'></a>mfxExtFeiPreEncCtrl

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

This extension buffer specifies frame level control for ENCODE usage model. It is used during runtime and should be attached to the **mfxEncodeCtrl** structure.

**Members**

| | |
--- | ---
`Header.BufferId` | Buffer ID, must be **MFX_EXTBUFF_HEVCFEI_ENC_CTRL**.
`SearchWindow` | This value specifies one of the predefined search path and window size.<br><br> 0 : not use predefined search window<br> 1 : Tiny – 4 SUs 24x24 window diamond search<br> 2 : Small – 9 SUs 28x28 window diamond search<br> 3 : Diamond – 16 SUs 48x40 window diamond search<br> 4 : Large Diamond – 32 SUs 48x40 window diamond search<br> 5 : Exhaustive – 48 SUs 48x40 window full search<br> 6 : Diamond – 16 SUs 64x32 window diamond search<br> 7 : Large Diamond – 32 SUs 64x32 window diamond search<br> 8 : Exhaustive – 48 SUs 64x32 window full search 
`SubPelMode`| This value specifies sub pixel precision for motion estimation.<br> <table><tr> <td>00b </td> <td>integer mode searching</td> </tr><tr> <td>01b</td> <td>half-pel mode searching</td> </tr><tr> <td>10b</td> <td>reserved</td> </tr><tr> <td>11b</td> <td>quarter-pel mode searching</td> </tr></table>
`AdaptiveSearch`|If set, adaptive search is enabled.

**Change History**

This structure is available since SDK API 1.24.


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
