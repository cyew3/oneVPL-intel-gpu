/*//////////////////////////////////////////////////////////////////////////////
//
//                  INTEL CORPORATION PROPRIETARY INFORMATION
//     This software is supplied under the terms of a license agreement or
//     nondisclosure agreement with Intel Corporation and may not be copied
//     or disclosed except in accordance with the terms of that agreement.
//          Copyright(c) 2003-2016 Intel Corporation. All Rights Reserved.
//
*/
//  MPEG-2 is a international standard promoted by ISO/IEC and
//  other organizations. Implementations of this standard, or the standard
//  enabled platforms may require licenses from various entities, including
//  Intel Corporation.

#pragma once

#include "umc_defs.h"
#if defined (UMC_ENABLE_MPEG2_VIDEO_DECODER)

#ifdef UMC_VA_LINUX
//LibVA headers
#   include <va/va.h>
#endif

//IPP headers
#include "ipps.h"
#include "ippi.h"
#include "ippvc.h"

//VM headers
#include "vm_debug.h"
#include "vm_thread.h"
#include "vm_event.h"

//UMC headers
#include "umc_structures.h"
#include "umc_video_decoder.h"
#include "umc_media_data_ex.h"
#include "umc_cyclic_buffer.h"
//MPEG-2
#include "umc_mpeg2_dec_bstream.h"
#include "umc_mpeg2_dec_defs.h"
#include "umc_mpeg2_dec.h"
#include "umc_va_base.h"

#ifdef UMC_VA_DXVA
#include "umc_va_dxva2_protected.h"
#endif

#include "mfxstructures.h"
#define ELK
//#define OTLAD

#define MPEG2_VIRTUAL

#if defined (__ICL)
//non-pointer conversion from "unsigned __int64" to "Ipp32s={signed int}" may lose significant bits
//#pragma warning(disable:2259)
void ownvc_Average8x16_8u_C1R(const Ipp8u *pSrc, Ipp32s srcStep, Ipp8u *pDst, Ipp32s dstStep);
void ownvc_Average8x16HP_HF0_8u_C1R(const Ipp8u *pSrc, Ipp32s srcStep, Ipp8u *pDst, Ipp32s dstStep);
void ownvc_Average8x16HP_FH0_8u_C1R(const Ipp8u *pSrc, Ipp32s srcStep, Ipp8u *pDst, Ipp32s dstStep);
void ownvc_Average8x16HP_HH0_8u_C1R(const Ipp8u *pSrc, Ipp32s srcStep, Ipp8u *pDst, Ipp32s dstStep);
template<class TSrc, class TDst>
void GeneralCopy(TSrc* src, Ipp32u srcStep, TDst* dst, Ipp32u dstStep, IppiSize roi);
#endif

namespace UMC
{

#define pack_l      pack_w

#if defined(UMC_VA_DXVA) || defined(UMC_VA_LINUX)

#define UMC_VA


class PackVA
{
public:
    PackVA()
    {
        m_va = NULL;
        is_analyzer = false;
        va_mode = VA_NO;
        bNeedNewFrame = false;
        IsProtectedBS = false;
        memset(init_count, 0, sizeof(init_count));
        bs = 0;
        curr_encryptedData = 0;
        curr_bs_encryptedData = 0;
        add_to_slice_start = 0;
        memset(add_bytes,0,16);
        overlap = 0;
        vpPictureParam = NULL;
        pMBControl0 = NULL;
        pSliceInfo = NULL;
        pSliceInfoBuffer = NULL;
        pQmatrixData = NULL;
    };
    virtual ~PackVA();
    bool SetVideoAccelerator(VideoAccelerator * va);
    Status InitBuffers(int size_bf = 0, int size_sl = 0);
    Status SetBufferSize(
        Ipp32s          numMB,
        MPEG2FrameType  picture_coding_type,
        int             size_bs=0,
        int             size_sl=0);
    Status SaveVLDParameters(
        sSequenceHeader*    sequenceHeader,
        sPictureHeader*     PictureHeader,
        Ipp8u*              bs_start_ptr,
        sFrameBuffer*       frame_buffer,
        Ipp32s              task_num,
        Ipp32s              source_mb_height = 0);

#if defined(UMC_VA_DXVA)
    Status GetStatusReport(DXVA_Status_VC1 *pStatusReport);
#endif
    
    VideoAccelerationProfile va_mode;
    VideoAccelerator         *m_va;

    bool   bNeedNewFrame;
    bool   is_analyzer;
    Ipp32s totalNumCoef;
    Ipp8u  *pBitsreamData;
    Ipp32s va_index;
    Ipp32s field_buffer_index;
    Ipp8u  *pSliceStart;
    Ipp32s bs_size;
    Ipp32s bs_size_getting;
    Ipp32s slice_size_getting;
    bool   IsFrameSkipped;
    Ipp32s m_SliceNum;
    //protected content:
    Ipp8u  add_bytes[16];
    bool   IsProtectedBS;
    Ipp32u init_count[4];
    mfxBitstream * bs;
    Ipp32s add_to_slice_start;
    bool   is_bs_aligned_16;
    mfxEncryptedData * curr_encryptedData;
    mfxEncryptedData * curr_bs_encryptedData;
    Ipp32u overlap;

#ifdef UMC_VA_DXVA
    DXVA_PictureParameters  *vpPictureParam;
    void                    *pMBControl0;
    DXVA_TCoefSingle        *pResidual;
    DXVA_SliceInfo          *pSliceInfo;
    DXVA_SliceInfo          *pSliceInfoBuffer;
    DXVA_QmatrixData        *pQmatrixData;
    DXVA_QmatrixData        QmatrixData;
#elif defined UMC_VA_LINUX
    VAPictureParameterBufferMPEG2   *vpPictureParam;
    VAMacroblockParameterBufferMPEG2 *pMBControl0;
    VASliceParameterBufferMPEG2     *pSliceInfo;
    VASliceParameterBufferMPEG2     *pSliceInfoBuffer;
    VAIQMatrixBufferMPEG2           *pQmatrixData;
    VAIQMatrixBufferMPEG2           QmatrixData;
#endif

#ifdef UMC_STREAM_ANALYZER
    LPExtraMBAnalyzer MBAnalyzerData;
    LPExtraPictureAnalyzer PicAnalyzerData;
#endif
};
#endif
    static const Ipp8u Mpeg2MbSize[3][2][2] = {
        { {16, 16}, { 8,  8} }, //4:2:0
        { {16, 16}, { 8, 16} }, //4:2:2
        { {16, 16}, {16, 16} }  //4:4:4
    };
    class Mpeg2MbContext
    {
    public:
        void ResetSequenceLayer(Ipp32s heightL, Ipp32s pitchL, Ipp32s pitchC, Ipp8u chromaFmt)
        {
            m_blkOffsets[0][0] = 0;
            m_blkOffsets[0][1] = 8;
            m_blkOffsets[0][2] = 8 * pitchL;
            m_blkOffsets[0][3] = 8 * pitchL + 8;
            m_blkOffsets[0][4] = 0;
            m_blkOffsets[0][5] = 0;
            m_blkOffsets[0][6] = 8 * pitchC;
            m_blkOffsets[0][7] = 8 * pitchC;
            m_blkOffsets[1][0] = 0;
            m_blkOffsets[1][1] = 8;
            m_blkOffsets[1][2] = pitchL;
            m_blkOffsets[1][3] = pitchL + 8;
            m_blkOffsets[1][4] = 0;
            m_blkOffsets[1][5] = 0;
            m_blkOffsets[1][6] = pitchC;
            m_blkOffsets[1][7] = pitchC;
            m_blkOffsets[2][0] = 0;
            m_blkOffsets[2][1] = 8;
            m_blkOffsets[2][2] = 16 * pitchL;
            m_blkOffsets[2][3] = 16 * pitchL + 8;
            m_blkOffsets[2][4] = 0;
            m_blkOffsets[2][5] = 0;
            m_blkOffsets[2][6] = 16 * pitchC;
            m_blkOffsets[2][7] = 16 * pitchC;

            m_heightL = heightL;
            m_chromaFmt = chromaFmt;
            m_blkNum = ChromaFmt2NumBlk(chromaFmt);

            m_blkPitches[0][0] = pitchL;
            m_blkPitches[1][0] = 2 * pitchL;
            m_blkPitches[2][0] = 2 * pitchL;
            m_blkPitches[0][1] = pitchC;
            m_blkPitches[1][1] = (m_blkNum == 6) ? pitchC : 2 * pitchC;
            m_blkPitches[2][1] = 2 * pitchC;
        }

        void ResetPictureLayer(MPEG2FrameType picType, Ipp8u picStruct, Ipp8u fieldCnt)
        {
            m_picType = picType;
            m_secondFieldFlag = fieldCnt;
            m_fieldPicFlag = picStruct != (Ipp8u)FRAME_PICTURE ? (Ipp8u)1 : (Ipp8u)0;
            m_bottomFlag = picStruct == (Ipp8u)BOTTOM_FIELD ? (Ipp8u)1 : (Ipp8u)0;
        }

        Ipp32s MbOffset(Ipp32u mbX, Ipp32u mbY, Ipp32u chromaFlag) const
        {
            Ipp32u mbW = Mpeg2MbSize[m_chromaFmt - 1][chromaFlag][0];
            Ipp32u mbH = Mpeg2MbSize[m_chromaFmt - 1][chromaFlag][1];
            return mbX * mbW + (mbY * mbH * (m_fieldPicFlag + 1) + m_bottomFlag) * (chromaFlag ? PitchC() : PitchL());
        }

        bool IsCurPicRef(Ipp32s fieldSel) const
        {
            if (m_picType == MPEG2_B_PICTURE)
                return false;
            if (m_secondFieldFlag && (m_bottomFlag != fieldSel))
                return true;
            return false;
        }

        Ipp8u MbWidth(Ipp32u cFlag) const { return Mpeg2MbSize[m_chromaFmt - 1][cFlag][0]; }
        Ipp8u MbHeight(Ipp32u cFlag) const { return Mpeg2MbSize[m_chromaFmt - 1][cFlag][1]; }
        Ipp32u MbX2PixX(Ipp32u mbX, Ipp32u cFlag) const { return mbX * Mpeg2MbSize[m_chromaFmt - 1][cFlag][0]; }
        Ipp32u MbY2PixY(Ipp32u mbY, Ipp32u cFlag) const { return mbY * Mpeg2MbSize[m_chromaFmt - 1][cFlag][1]; }
        Ipp32u NumBlk() const { return m_blkNum; }
        Ipp32s BlkPitch(Ipp32u blk, Ipp8u dctType) const { return m_blkPitches[dctType][blk < 4 ? 0 : 1]; }
        Ipp32s BlkOffset(Ipp32u blk, Ipp8u dctType) const { return m_blkOffsets[dctType][blk]; }
        Ipp32s PitchL() const { return m_blkPitches[0][0]; }
        Ipp32s PitchC() const { return m_blkPitches[0][1]; }
        Ipp32s MbPitchL() const { return PitchL() * (m_fieldPicFlag ? 2 : 1); }
        Ipp32s MbPitchC() const { return PitchC() * (m_fieldPicFlag ? 2 : 1); }
        Ipp32u HeightL() const { return m_heightL; }
        bool IsBPic() const { return m_picType == MPEG2_B_PICTURE; }
        bool IsSecondField() const { return m_secondFieldFlag == 1; }
        bool IsBottomField() const { return m_bottomFlag == 1; }

    protected:
        Ipp32u ChromaFmt2NumBlk(Ipp8u chromaFmt) const { return chromaFmt * (chromaFmt - 1) + 6; }

        Ipp32s m_blkOffsets[3][8];
        Ipp32s m_blkPitches[3][2];
        Ipp32u m_blkNum;
        Ipp32u m_heightL;
        MPEG2FrameType m_picType;
        Ipp8u m_chromaFmt;
        Ipp8u m_fieldPicFlag;
        Ipp8u m_bottomFlag;
        Ipp8u m_secondFieldFlag;
    };

    typedef struct _SeqHeaderMask
    {
        Ipp8u *memMask;
        Ipp16u memSize;

    } SeqHeaderMask;

    typedef struct _SignalInfo
    {
        mfxU16          VideoFormat;
        mfxU16          VideoFullRange;
        mfxU16          ColourDescriptionPresent;
        mfxU16          ColourPrimaries;
        mfxU16          TransferCharacteristics;
        mfxU16          MatrixCoefficients;

    } SignalInfo;


    class MPEG2VideoDecoderBase : public VideoDecoder
    {
    public:
        ///////////////////////////////////////////////////////
        /////////////High Level public interface///////////////
        ///////////////////////////////////////////////////////
        // Default constructor
        MPEG2VideoDecoderBase(void);

        // Default destructor
        MPEG2_VIRTUAL ~MPEG2VideoDecoderBase(void);

        // Initialize for subsequent frame decoding.
        MPEG2_VIRTUAL Status Init(BaseCodecParams *init);

        // Get next frame
        MPEG2_VIRTUAL Status GetFrame(MediaData* in, MediaData* out);

        // Close  decoding & free all allocated resources
        MPEG2_VIRTUAL Status Close(void);

        // Reset decoder to initial state
        MPEG2_VIRTUAL Status Reset(void);

        // Get video stream information, valid after initialization
        MPEG2_VIRTUAL Status GetInfo(BaseCodecParams* info);

        Status          GetPerformance(Ipp64f *perf);

        //reset skip frame counter
        Status          ResetSkipCount(){return UMC_OK;};

        // increment skip frame counter
        Status          SkipVideoFrame(Ipp32s){return UMC_OK;};

        // get skip frame counter statistic
        Ipp32u          GetNumOfSkippedFrames();

        //access to the latest decoded frame
        //Status PreviewLastFrame(VideoData *out, BaseCodec *pPostProcessing = NULL);

        // returns closed capture data from gop user data
        MPEG2_VIRTUAL Status GetUserData(MediaData* pCC);

        MPEG2_VIRTUAL Status  SetParams(BaseCodecParams* params);
//additional functions for UMC-MFX interaction
        Status GetCCData(Ipp8u* ptr, Ipp32u *size, Ipp64u *time, Ipp32u bufsize);
        Status GetPictureHeader(MediaData* input, int task_num, int prev_task_num);
        Ipp32s GetCurrDecodingIndex(int task_num);
        Ipp32s GetNextDecodingIndex(int index);
        Ipp32s GetPrevDecodingIndex(int index);
        
        void SetCorruptionFlag(int task_num);
        bool GetCorruptionFlag(int index);

        Ipp32s GetFrameType(int index);
        Ipp32s GetDisplayIndex();
        Ipp32s GetRetBufferLen();
        void SetOutputPointers(MediaData *output, int task_num);
        Status PostProcessFrame(int display_index, int task_num);
        Status PostProcessUserData(int display_index);
        Status ProcessRestFrame(int task_num);
        bool IsFramePictureStructure(int task_num);
        bool IsFrameSkipped();
        bool IsSHDecoded();
        void SetSkipMode(Ipp32s SkipMode);
        Ipp64f GetCurrDecodedTime(int index);
        bool isOriginalTimeStamp(int index);
        void SetFieldIndex(int task_num, int index);
        void LockTask(int index);
        void UnLockTask(int index);
        int FindFreeTask();
        Ipp32s GetCurrThreadsNum(int task_num);
        DecodeSpec m_Spec;

        Status GetSequenceHeaderMemoryMask(Ipp8u *buffer, Ipp16u &size);
        Status GetSignalInfoInformation(mfxExtVideoSignalInfo *buffer);

#if defined(UMC_VA_DXVA) || defined(UMC_VA_LINUX)
        PackVA pack_w;
#endif

    // for BSD and DEC
    public:
        bool InitTables();

        void DeleteHuffmanTables();

        const sSequenceHeader& GetSequenceHeader() const { return sequenceHeader; }
        const sPictureHeader& GetPictureHeader(int task_num) const { return PictureHeader[task_num]; }

        Ipp32s GetAspectRatioW() {return m_ClipInfo.aspect_ratio_width;}
        Ipp32s GetAspectRatioH() {return m_ClipInfo.aspect_ratio_height;}
        Status DoDecodeSlices(Ipp32s threadID, int task_num);
        Status SaveDecoderState();
        Status RestoreDecoderState();
        Status RestoreDecoderStateAndRemoveLastField();

    protected:

        Status          UpdateFrameBuffer(int task_num);
        Status          PrepareBuffer(MediaData* data, int task_num);
        Status          FlushBuffer(MediaData* data, int task_num);

        typedef struct {
            Ipp64f time;
            bool   isOriginal;
        } TimeStampInfo;

        Ipp64f          m_dPlaybackRate;
        Ipp64f          m_InputTime;
        TimeStampInfo   m_dTime[2 * DPB_SIZE];
        Ipp64f          m_dTimeCalc;
        MPEG2FrameType  m_picture_coding_type_save;


        enum SkipLevel
        {
            SKIP_NONE = 0,
            SKIP_B,
            SKIP_PB,
            SKIP_ALL
        };
        Ipp32s m_SkipLevel;

    protected:
        //The purpose of protected interface to have controlled
        //code in derived UMC MPEG2 decoder classes

        ///////////////////////////////////////////////////////
        /////////////Level 1 protected interface///////////////
        ///////////////////////////////////////////////////////
        //Level 1 can call level 2 functions or re-implement it

        // Decode next frame
       // Status DecodeFrame(IppVideoContext  *video,
                         //  Ipp64f           currentTime,
                         //  MediaData        *output);

        //Sequence Header search. Stops after header start code
        Status FindSequenceHeader(IppVideoContext *video);

        //Sequence Header decode
        MPEG2_VIRTUAL Status DecodeSequenceHeader(IppVideoContext *video, int task_num);

        //Picture Header decode and picture
        MPEG2_VIRTUAL Status DecodePicture();

        // Is current picture to be skipped
        bool IsPictureToSkip(int task_num);


        ///////////////////////////////////////////////////////
        /////////////Level 2 protected interface///////////////
        ///////////////////////////////////////////////////////
        //Level 2 can call level 3 functions or re-implement it

        //Picture Header decode
        MPEG2_VIRTUAL Status DecodePictureHeader(int task_num);
        MPEG2_VIRTUAL Status FindSliceStartCode();

        MPEG2_VIRTUAL Status DecodeSlices(Ipp32s threadID, int task_num);

        //Slice decode, includes MB decode
        MPEG2_VIRTUAL Status DecodeSlice(IppVideoContext *video, int task_num);

        // decode all headers but slice, starts right after startcode
        MPEG2_VIRTUAL Status DecodeHeader(Ipp32s startcode, IppVideoContext *video, int task_num);

        ///////////////////////////////////////////////////////
        /////////////Level 3 protected interface///////////////
        ///////////////////////////////////////////////////////
        //Level 3 can call level 4 functions or re-implement it

        //Slice Header decode
        MPEG2_VIRTUAL Status DecodeSliceHeader(IppVideoContext *video, int task_num);

        ///////////////////////////////////////////////////////
        /////////////Level 4 protected interface///////////////
        ///////////////////////////////////////////////////////
        //Level 4 is the lowest level chunk of code
        //can be used as is for basic codec to implement standard
        //or overloaded for special purposes like HWMC, smart decode

        MPEG2_VIRTUAL Status DecodeSlice_MPEG1(IppVideoContext *video, int task_num);

 protected:

        SampleBuffer*             m_pCCData;
        SampleBuffer*             m_pCCDataTS;
        sVideoFrameBuffer::UserDataVector m_user_data;
        std::vector< std::pair<Ipp64f, size_t> > m_user_ts_data;
        MediaData                 m_ccCurrData;
        MediaData                 m_ccCurrDataTS;
        sSequenceHeader           sequenceHeader;
        sSHSavedPars              sHSaved;

        SeqHeaderMask             shMask;
        SignalInfo                m_signalInfo;

        sPictureHeader            PictureHeader[2*DPB_SIZE];

        mp2_VLCTable              vlcMBAdressing;
        mp2_VLCTable              vlcMBType[3];
        mp2_VLCTable              vlcMBPattern;
        mp2_VLCTable              vlcMotionVector;

        int                       task_locked[DPB_SIZE*2];
        sVideoFrameBuffer::UserDataVector frame_user_data_v[DPB_SIZE*2];
        sFrameBuffer              frame_buffer;
        sFrameBuffer              frameBuffer_backup_previous;
        sFrameBuffer              frameBuffer_backup;
        Ipp32s                    b_curr_number_backup;
        Ipp32s                    first_i_occure_backup;
        Ipp32s                    frame_count_backup;

        IppVideoContext**         Video[2*DPB_SIZE];

        Ipp32u                    m_lFlags;

        Ipp32s                    m_nNumberOfThreads;
        Ipp32s                    m_nNumberOfThreadsTask[2*DPB_SIZE];
        Ipp32s                    m_nNumberOfAllocatedThreads;
        bool                      m_IsFrameSkipped;
        bool                      m_IsSHDecoded;
        bool                      m_FirstHeaderAfterSequenceHeaderParsed;
        bool                      m_IsDataFirst;
        bool                      m_IsLastFrameProcessed;
        bool                      m_isFrameRateFromInit;
        bool                      m_isAspectRatioFromInit;
        VideoStreamInfo           m_InitClipInfo;                         // (VideoStreamInfo) Init clip info
        class THREAD_ID
        {
        public:
            Ipp32u                m_nNumber;
            void*                 m_lpOwner;
        };

       // THREAD_ID*                 m_lpThreadsID;
        //MediaDataEx::_MediaDataEx* m_pStartCodesData;
        //MPEG2_VIRTUAL Status    DecodeBegin(Ipp64f time, sVideoStreamInfo * info);
        //Status                  Macroblock_444(IppVideoContext *video) { return UMC_ERR_INVALID_STREAM; }

        Status                  ThreadingSetup(Ipp32s maxThreads);
        bool                    DeleteTables();
        MPEG2_VIRTUAL void      CalculateFrameTime(Ipp64f in_time, Ipp64f * out_time, bool * isOriginal, int task_num, bool buffered);
        bool                    PictureStructureValid(Ipp32u picture_structure);

        Status                  DecodeSlice_FrameI_420(IppVideoContext *video, int task_num);
        Status                  DecodeSlice_FrameI_422(IppVideoContext *video, int task_num);
        Status                  DecodeSlice_FramePB_420(IppVideoContext *video, int task_num);
        Status                  DecodeSlice_FramePB_422(IppVideoContext *video, int task_num);
        Status                  DecodeSlice_FieldPB_420(IppVideoContext *video, int task_num);
        Status                  DecodeSlice_FieldPB_422(IppVideoContext *video, int task_num);

        Status                  mv_decode(Ipp32s r, Ipp32s s, IppVideoContext *video, int task_num);
        Status                  mv_decode_dp(IppVideoContext *video, int task_num);
        Status                  update_mv(Ipp16s *pval, Ipp32s s, IppVideoContext *video, int task_num);

        Status                  mc_frame_forward_420(IppVideoContext *video, int task_num);
        Status                  mc_frame_forward_422(IppVideoContext *video, int task_num);
        Status                  mc_field_forward_420(IppVideoContext *video, int task_num);
        Status                  mc_field_forward_422(IppVideoContext *video, int task_num);

        Status                  mc_frame_backward_420(IppVideoContext *video, int task_num);
        Status                  mc_frame_backward_422(IppVideoContext *video, int task_num);
        Status                  mc_field_backward_420(IppVideoContext *video, int task_num);
        Status                  mc_field_backward_422(IppVideoContext *video, int task_num);

        Status                  mc_frame_backward_add_420(IppVideoContext *video, int task_num);
        Status                  mc_frame_backward_add_422(IppVideoContext *video, int task_num);
        Status                  mc_field_backward_add_420(IppVideoContext *video, int task_num);
        Status                  mc_field_backward_add_422(IppVideoContext *video, int task_num);

        Status                  mc_fullpel_forward(IppVideoContext *video, int task_num);
        Status                  mc_fullpel_backward(IppVideoContext *video, int task_num);
        Status                  mc_fullpel_backward_add(IppVideoContext *video, int task_num);

        void                    mc_frame_forward0_420(IppVideoContext *video, int task_num);
        void                    mc_frame_forward0_422(IppVideoContext *video, int task_num);
        void                    mc_field_forward0_420(IppVideoContext *video, int task_num);
        void                    mc_field_forward0_422(IppVideoContext *video, int task_num);

        Status                  mc_dualprime_frame_420(IppVideoContext *video, int task_num);
        Status                  mc_dualprime_frame_422(IppVideoContext *video, int task_num);
        Status                  mc_dualprime_field_420(IppVideoContext *video, int task_num);
        Status                  mc_dualprime_field_422(IppVideoContext *video, int task_num);

        Status                  mc_mp2_420b_skip(IppVideoContext *video, int task_num);
        Status                  mc_mp2_422b_skip(IppVideoContext *video, int task_num);
        Status                  mc_mp2_420_skip(IppVideoContext *video, int task_num);
        Status                  mc_mp2_422_skip(IppVideoContext *video, int task_num);

        Ipp32s blkOffsets[3][8];
        Ipp32s blkPitches[3][2];


 private:
         void                   sequence_display_extension(int task_num);
         void                   sequence_scalable_extension(int task_num);
         void                   picture_temporal_scalable_extension(int task_num);
         void                   picture_spartial_scalable_extension(int task_num);
         void                   picture_display_extension(int task_num);
         void                   copyright_extension(int task_num);
         void                   quant_matrix_extension(int task_num);

         void                   ReadCCData(int task_num);

protected:

         Status BeginVAFrame(int task_num);
    };
}
#pragma warning(default: 4324)

#endif // UMC_ENABLE_MPEG2_VIDEO_DECODER
