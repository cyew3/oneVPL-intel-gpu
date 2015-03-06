//
//               INTEL CORPORATION PROPRIETARY INFORMATION
//  This software is supplied under the terms of a license agreement or
//  nondisclosure agreement with Intel Corporation and may not be copied
//  or disclosed except in accordance with the terms of that agreement.
//        Copyright (c) 2013-2014 Intel Corporation. All Rights Reserved.
//

#include "mux_exposed_functions_list.h"

FUNCTION(retMfxMux, mfxStatus, MFXMuxer_Init, (mfxStreamParams* par, mfxDataIO *data_io, mfxMuxer *mux), (par, data_io, mux))
FUNCTION(retMfxMux, mfxStatus, MFXMuxer_Close, (mfxMuxer mux), (mux))