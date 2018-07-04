/* /////////////////////////////////////////////////////////////////////////////
//
//                  INTEL CORPORATION PROPRIETARY INFORMATION
//     This software is supplied under the terms of a license agreement or
//     nondisclosure agreement with Intel Corporation and may not be copied
//     or disclosed except in accordance with the terms of that agreement.
//          Copyright(c) 2018 Intel Corporation. All Rights Reserved.
//
*/

#pragma once

#include "ts_encoder.h"

namespace avce_mfe_async_encode
{

/*!\brief Flush modes */
enum {
    FlushModeNone = 0,
    FlushModeEnc1 = 1,
    FlushModeEnc2 = 2,
    FlushModeEnc3 = 3,
};

/*!\brief Structure of test suite parameters*/
typedef struct
{
    mfxU16 mfMode;       ///< MFE MFMode parameter
    mfxU16 maxNumFrames; ///< MFE MaxNumFrames parameter
    mfxU16 asyncDepth;   ///< video parameter AsyncDepth
    mfxU16 gopRefDist;   ///< video parameter GopRefDist
    mfxU16 bRefType;     ///< coding option2 parameter BRefType
    mfxU16 flushMode;    ///< flush mode
    mfxU16 frameToFlush; ///< frame to flush
} tc_struct;

class AsyncEncodeTest
{
public:
    /*! \brief A constructor
     *  \param fei_enabled - FEI is enabled if true
     */
    AsyncEncodeTest(bool fei_enabled);

    //! \brief A destructor
    ~AsyncEncodeTest() {}

    //! \brief Main method. Runs test case
    //! \param id - test case number
    int RunTest(unsigned int id);

    //! The number of test cases
    static const unsigned int n_cases;

    //! \brief Set of test cases
    static const tc_struct test_case[];

private:
    //! First encoder (parent)
    tsVideoEncoder m_enc1;
    //! Second encoder (child)
    tsVideoEncoder m_enc2;
    //! Third  encoder (child)
    tsVideoEncoder m_enc3;

    //! Flag indicating whether FEI is enabled or not
    bool m_fei_enabled;

    //! The number of frames to encode
    static const int frames_to_encode;

    /*! \brief Set video parameters
     *  \param enc - pointer to encoder
     *  \tc - test case parameters
     */
    void SetParams(tsVideoEncoder& enc, const tc_struct& tc);

    /*! \brief Initialize SDK session
     *
     *  Unlike to MFXInit() this function does not set handle for initialized
     *  session
     *  \param enc - pointer to encoder
     */
    void InitSession(tsVideoEncoder& enc);

    /*! \brief Set handle for the SDK session
     *  \param enc - pointer to encoder
     *  \param hdl - handle
     *  \param hdl_type - handle type
     */
    void SetHandle(tsVideoEncoder& enc, mfxHDL &hdl, mfxHandleType &hdl_type);

    //! \brief Initialize encoders
    //! \tc - test case parameters
    void Init(const tc_struct& tc);

    //! \brief Wipe bitstreams associated with encoders
    void WipeBitsreams();

    //! \brief Flush of MFE internal buffer
    //! \param enc - pointer to encoder
    void Flush(tsVideoEncoder& enc);
};

};
