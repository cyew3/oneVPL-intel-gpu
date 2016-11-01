/* /////////////////////////////////////////////////////////////////////////////
//
//                  INTEL CORPORATION PROPRIETARY INFORMATION
//     This software is supplied under the terms of a license agreement or
//     nondisclosure agreement with Intel Corporation and may not be copied
//     or disclosed except in accordance with the terms of that agreement.
//          Copyright(c) 2016 Intel Corporation. All Rights Reserved.
//
*/

#include "ts_vpp.h"
#include "ts_ext_buffers.h"

//#include "mfx_ext_buffers.h"

#define MFX_EXTBUFF_GPU_HANG MFX_MAKEFOURCC('H','A','N','G')
typedef struct {
    mfxExtBuffer Header;
} mfxExtIntGPUHang;

namespace vpp_gpu_hang
{

class TestSuite
    : public tsVideoVPP
{
    mfxU32                     m_frames_submitted;
    mfxU32                     m_frames_synced;
    mfxU32                     m_frame_to_hang;
    tsExtBufType<mfxFrameData> m_trigger;

public:

    TestSuite(mfxU32 frame_to_hang = 1)
        : tsVideoVPP()
        , m_frames_submitted(0) 
        , m_frames_synced(0)
        , m_frame_to_hang(frame_to_hang)
    {
        m_trigger.AddExtBuffer(MFX_EXTBUFF_GPU_HANG, sizeof(mfxExtIntGPUHang));
    }

    int RunTest(unsigned int)
    {
        TS_START;

        m_impl = MFX_IMPL_HARDWARE;
        MFXInit();

        m_par.IOPattern = MFX_IOPATTERN_IN_VIDEO_MEMORY|MFX_IOPATTERN_OUT_VIDEO_MEMORY;
        m_par.AsyncDepth = 1;

        CreateAllocators();

        SetFrameAllocator();
        SetHandle();

        AllocSurfaces();

        Init(m_session, m_pPar);

        ProcessFrames(3);

        //disable subsequent triggers
        m_frame_to_hang = -1;

        //check flush - should be MFX_ERR_NONE regardless of GPU hang status
        g_tsStatus.expect(MFX_ERR_NONE);
        m_pSurfOut = m_pSurfPoolOut->GetSurface();
        auto sts =
            RunFrameVPPAsync(m_session, 0, m_pSurfOut, 0, m_pSyncPoint);
        if (sts != MFX_ERR_MORE_DATA)
            g_tsStatus.check(sts);

        Close();
        Init(m_session, m_pPar);

        //check if all is OK after reset
        g_tsStatus.expect(MFX_ERR_NONE);
        ProcessFrames(3);

        TS_END;
        return 0;
    }

    static const unsigned int n_cases;

private:

    mfxStatus RunFrameVPPAsync(mfxSession  session, mfxFrameSurface1 *in, mfxFrameSurface1 *out, mfxExtVppAuxData *aux, mfxSyncPoint *syncp)
    {
        if (m_frames_submitted == m_frame_to_hang)
        {
            in->Data.NumExtParam = m_trigger.NumExtParam;
            in->Data.ExtParam    = m_trigger.ExtParam;

            g_tsLog << "Frame #" << m_frames_submitted << " - Fire GPU hang trigger\n";
        }

        if (m_frames_synced > m_frame_to_hang)
            //check subsequent calls will return hang status until Close/Init
            g_tsStatus.expect(MFX_ERR_GPU_HANG);

        mfxStatus sts =
            tsVideoVPP::RunFrameVPPAsync(session, in, out, aux, syncp);

        if (m_frames_submitted == m_frame_to_hang)
        {
            //clear GPU hang trigger to avoid subsequently triggers
            in->Data.NumExtParam = 0;
            in->Data.ExtParam    = 0;
        }

        ++m_frames_submitted;
        return sts;
    }

    mfxStatus SyncOperation(mfxSyncPoint syncp)
    {
        mfxStatus sts =
            tsVideoVPP::SyncOperation(syncp);

        ++m_frames_synced;
        if (m_frames_synced == m_frame_to_hang)
            g_tsStatus.expect(MFX_ERR_GPU_HANG);

        return sts;
    }
};

const unsigned int TestSuite::n_cases = 1;

TS_REG_TEST_SUITE_CLASS(vpp_gpu_hang);

}
