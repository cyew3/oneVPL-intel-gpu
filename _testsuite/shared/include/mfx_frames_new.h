/* /////////////////////////////////////////////////////////////////////////////
//
//                  INTEL CORPORATION PROPRIETARY INFORMATION
//     This software is supplied under the terms of a license agreement or
//     nondisclosure agreement with Intel Corporation and may not be copied
//     or disclosed except in accordance with the terms of that agreement.
//          Copyright(c) 2009-2016 Intel Corporation. All Rights Reserved.
//
//
//     GOP reorder buffer and waiting list implementation
//
*/

#ifndef _MFX_FRAMES_H_
#define _MFX_FRAMES_H_

#include <memory.h>
#include <string.h>
#include "mfxdefs.h"


    inline mfxStatus ReleasePlane(mfxFrameSurface1* pFrame)
    {
        if (pFrame)
        {
            //pFrame->Data.Locked= 0;
        }

        return MFX_ERR_NONE;
    }

    inline bool isReferenceFrame(mfxU16 FrameType)
    {
        return (FrameType & MFX_FRAMETYPE_I ||
                FrameType & MFX_FRAMETYPE_P ||
                FrameType & MFX_FRAMETYPE_REF);
    }

    inline bool isReferenceFrame(mfxU16 FrameType, mfxU16 FrameType2)
    {
        if(!FrameType2)
        {
            return (FrameType & MFX_FRAMETYPE_I ||
                    FrameType & MFX_FRAMETYPE_P ||
                    FrameType & MFX_FRAMETYPE_REF);

        }
        else
        {
            return ((FrameType & MFX_FRAMETYPE_I ||
                    FrameType & MFX_FRAMETYPE_P ||
                    FrameType & MFX_FRAMETYPE_REF)
                    &&
                   (FrameType2 & MFX_FRAMETYPE_I ||
                    FrameType2 & MFX_FRAMETYPE_P ||
                    FrameType2 & MFX_FRAMETYPE_REF) );
        }
    }

    inline bool isPredictedFrame(mfxU16 FrameType)
    {
         return (FrameType & MFX_FRAMETYPE_P) != 0;
    }

    inline bool isPredictedFrame(mfxU16 FrameType, mfxU16 FrameType2)
    {
         return ((FrameType & MFX_FRAMETYPE_P) || (FrameType2 & MFX_FRAMETYPE_P)) != 0;
    }

    inline bool isBPredictedFrame(mfxU16 FrameType)
    {
         return (FrameType & MFX_FRAMETYPE_B) != 0;
    }

    inline bool isBPredictedFrame(mfxU16 FrameType, mfxU16 FrameType2)
    {
        return ((FrameType & MFX_FRAMETYPE_B) || (FrameType2 & MFX_FRAMETYPE_B)) != 0;
    }

    class MFXGOP_
    {
    public:
        MFXGOP_()
            : m_maxB(0)
            , m_pInFrames(0)
            , m_pOutFrames(0)
            , m_FrameType(0)
            , m_FrameType2(0)
            , m_numBuffB(0)
            , m_iCurr(0)
        {};

        virtual ~MFXGOP_()
        {
            Close();
        }

        inline mfxU32 GetNumberOfB()
        {
            return m_numBuffB;
        }
    protected:
        mfxU32               m_maxB;
        mfxFrameSurface1**   m_pInFrames;  /* 0     - forward  reference frame,
                                          1     - backward reference frame
                                          other - B frames*/
        mfxFrameSurface1**   m_pOutFrames;
        mfxU16           *   m_FrameType; //frame or first field
        mfxU16           *   m_FrameType2;
        mfxU32               m_numBuffB;
        mfxU32               m_iCurr;

    protected:
        void Close()
        {
            if (m_pInFrames)
            {
                delete [] m_pInFrames;
                m_pInFrames = 0;
            }
            if (m_pOutFrames)
            {
               delete [] m_pOutFrames;
               m_pOutFrames = 0;
            }
            if (m_FrameType)
            {
                delete [] m_FrameType;
                m_FrameType = 0;
            }

            if (m_FrameType2)
            {
                delete [] m_FrameType2;
                m_FrameType2 = 0;
            }

            m_iCurr = 0;
            m_numBuffB = 0;
            m_maxB = 0;
        }

        inline bool NewGOP ()
        {
            mfxStatus       MFXSts = MFX_ERR_NONE;

            MFXSts = ReleasePlane(m_pInFrames[0]);
            if(MFXSts != MFX_ERR_NONE)
                return false;

            MFXSts = ReleasePlane(m_pOutFrames[0]);
            if(MFXSts != MFX_ERR_NONE)
                return false;

            m_pInFrames[0]   = m_pInFrames[1];
            m_pOutFrames[0]  = m_pOutFrames[1];
            m_FrameType[0]   = m_FrameType[1];
            m_FrameType2[0]   = m_FrameType2[1];

            m_pInFrames[1]   = 0;
            m_pOutFrames[1]  = 0;
            m_FrameType[1]   = 0;
            m_FrameType2[1]   = 0;

            m_iCurr = 1;
            m_numBuffB = 0;
            return true;
        }

        inline virtual bool AddBFrame(mfxFrameSurface1* inFrame,mfxFrameSurface1* outFrame, mfxU16 frameType)
        {
            if (m_numBuffB < m_maxB &&
                m_pInFrames[0] != 0  && m_pInFrames[1] ==0&&
                m_pOutFrames[0] != 0 && m_pOutFrames[1]==0)
            {
                m_pInFrames[m_numBuffB+2]  = inFrame;
                m_pOutFrames[m_numBuffB+2] = outFrame;
                m_FrameType[m_numBuffB+2]  = frameType;

                m_numBuffB ++;
                return true;
            }
            return false;
        }

        inline virtual bool AddBFrame(mfxFrameSurface1* inFrame, mfxFrameSurface1* outFrame, mfxU16 frameType, mfxU16 frameType2)
        {
            if (m_numBuffB < m_maxB &&
                m_pInFrames[0] != 0  && m_pInFrames[1] ==0&&
                m_pOutFrames[0] != 0 && m_pOutFrames[1]==0)
            {
                m_pInFrames[m_numBuffB+2]  = inFrame;
                m_pOutFrames[m_numBuffB+2] = outFrame;
                m_FrameType[m_numBuffB+2]  = frameType;
                m_FrameType2[m_numBuffB+2]  = frameType2;

                m_numBuffB ++;
                return true;
            }
            return false;
        }

        inline virtual bool AddReferenceFrame(mfxFrameSurface1* inFrame, mfxFrameSurface1* outFrame, mfxU16 frameType)
        {
            if (m_pInFrames[0] == 0 && m_pOutFrames[0] == 0)
            {
                m_pInFrames[0]  = inFrame;
                m_pOutFrames[0] = outFrame;
                m_FrameType[0]  = frameType;
                return true;
            }
            else if (m_pInFrames[1] == 0 && m_pOutFrames[1] == 0)
            {
                m_pInFrames[1]  = inFrame;
                m_pOutFrames[1] = outFrame;
                m_FrameType[1]  = frameType;
                return true;
            }
            return false;
        }

        inline virtual bool AddReferenceFrame(mfxFrameSurface1* inFrame, mfxFrameSurface1* outFrame, mfxU16 frameType, mfxU16 frameType2)
        {
            if (m_pInFrames[0] == 0 && m_pOutFrames[0] == 0)
            {
                m_pInFrames[0]  = inFrame;
                m_pOutFrames[0] = outFrame;
                m_FrameType[0]  = frameType;
                m_FrameType2[0]  = frameType2;
                return true;
            }
            else if (m_pInFrames[1] == 0 && m_pOutFrames[1] == 0)
            {
                m_pInFrames[1]  = inFrame;
                m_pOutFrames[1] = outFrame;
                m_FrameType[1]  = frameType;
                m_FrameType2[1]  = frameType2;
                return true;
            }
            return false;
        }

    public:

        inline bool AddFrame(mfxFrameSurface1* inFrame, mfxU16 frameType, mfxFrameSurface1* outFrame=0)
        {
            if (outFrame)
            {
                if (isReferenceFrame(frameType))
                {
                    return AddReferenceFrame(inFrame,outFrame,frameType);
                }
                else
                {
                    return AddBFrame(inFrame,inFrame,frameType);
                }
            }
            else
            {
                if (isReferenceFrame(frameType))
                {
                    return AddReferenceFrame(inFrame,inFrame,frameType);
                }
                else
                {
                    return AddBFrame(inFrame,inFrame,frameType);
                }

            }
        }

        inline bool AddFrame(mfxFrameSurface1* inFrame, mfxU16 frameType, mfxU8 frameType2, mfxFrameSurface1* outFrame=0)
        {
            if (outFrame)
            {
                if (isReferenceFrame(frameType, frameType2))
                {
                    return AddReferenceFrame(inFrame,outFrame,frameType, frameType2);
                }
                else
                {
                    return AddBFrame(inFrame,inFrame,frameType, frameType2);
                }
            }
            else
            {
                if (isReferenceFrame(frameType, frameType2))
                {
                    return AddReferenceFrame(inFrame,inFrame,frameType, frameType2);
                }
                else
                {
                    return AddBFrame(inFrame,inFrame,frameType, frameType2);
                }

            }
        }

        inline virtual mfxStatus Init(mfxU32 maxB)
        {
            Close();
            m_maxB = maxB;
            m_pInFrames = new mfxFrameSurface1* [m_maxB+2];
            if (!m_pInFrames)
                return MFX_ERR_MEMORY_ALLOC;
            m_pOutFrames =  new mfxFrameSurface1* [m_maxB+2];
            if (!m_pOutFrames)
                return MFX_ERR_MEMORY_ALLOC;
            m_FrameType =   new mfxU16 [m_maxB+2];
            if (!m_FrameType)
                return MFX_ERR_MEMORY_ALLOC;

            m_FrameType2 =   new mfxU16 [m_maxB+2];
            if (!m_FrameType2)
                return MFX_ERR_MEMORY_ALLOC;

            Reset();
            return MFX_ERR_NONE;
        }

        inline virtual void Reset()
        {
            if (m_pInFrames)
            {
                memset(m_pInFrames,0,sizeof(mfxFrameSurface1*)*(m_maxB+2));
            }

            if (m_pOutFrames)
            {
                memset(m_pOutFrames,0,sizeof(mfxFrameSurface1*)*(m_maxB+2));
            }

            if (m_FrameType)
            {
                memset(m_FrameType,0,sizeof(mfxU16)*(m_maxB+2));
            }

            if (m_FrameType2)
            {
                memset(m_FrameType2,0,sizeof(mfxU16)*(m_maxB+2));
            }

            m_iCurr = 0;
            m_numBuffB = 0;
        };

        inline mfxFrameSurface1* GetInFrameForDecoding()
        {
            return m_pInFrames[m_iCurr];
        }

        inline mfxFrameSurface1* GetOutFrameForDecoding()
        {
            return m_pOutFrames[m_iCurr];
        }

        inline mfxU16 GetFrameForDecodingType()
        {
            return m_FrameType[m_iCurr];
        }

        inline void GetFrameForDecodingType(mfxU16* frameType, mfxU16* frameType2)
        {
            *frameType  = m_FrameType[m_iCurr];
            *frameType2 = m_FrameType2[m_iCurr];
        }

        inline  virtual void ReleaseCurrentFrame()
        {
            if (!m_pInFrames[m_iCurr] || !m_pOutFrames[m_iCurr])
                return;

            if (m_iCurr>1)
            {
                ReleasePlane(m_pInFrames[m_iCurr]);
                ReleasePlane(m_pOutFrames[m_iCurr]);
                m_pInFrames[m_iCurr] = 0;
                m_pOutFrames[m_iCurr] = 0;
                m_FrameType[m_iCurr] = 0;
                m_FrameType2[m_iCurr] = 0;
            }
            m_iCurr++;
            if (m_iCurr>=m_numBuffB+2)
            {
                NewGOP();
            }
        }
        inline  virtual void CloseGop()
        {
            mfxU8 pictureType = MFX_FRAMETYPE_P  | MFX_FRAMETYPE_REF;
            mfxU8 pictureType2 = 0;

            if (m_pInFrames[1]==0 && m_numBuffB>0)
            {
                m_pInFrames[1]            = m_pInFrames[2+m_numBuffB-1];
                m_pOutFrames[1]           = m_pOutFrames[2+m_numBuffB-1];
                m_pInFrames [2 + m_numBuffB-1] = 0;
                m_pOutFrames[2 + m_numBuffB-1] = 0;

                if(m_FrameType2[2 + m_numBuffB-1])
                    pictureType2 = pictureType;

                m_FrameType[2 + m_numBuffB-1] = 0;
                m_FrameType2[2 + m_numBuffB-1] = 0;

                m_numBuffB --;
                m_FrameType[1] = pictureType;
                m_FrameType2[1] = pictureType2;

            }
        }
        inline mfxFrameSurface1* GetInReferenceFrame(bool bBackward = false)
        {
            return m_pInFrames[bBackward];
        }

        inline mfxFrameSurface1* GetOutReferenceFrame(bool bBackward = false)
        {
            return m_pOutFrames[bBackward];
        }

        inline mfxU16 GetReferenceFrameType(bool bBackward = false)
        {
            return m_FrameType[bBackward];
        }

    };


    class MFXWaitingList
    {
    public:
        MFXWaitingList()
            : m_maxN(0)
            , m_pFrame(0)
            , m_FrameType(0)
            , m_FrameType2(0)
            , m_curIndex(0)
            , m_nFrames(0)
        {};
        ~MFXWaitingList()
        {Close();}
        void Close()
        {
            if (m_pFrame)
            {
                delete [] m_pFrame;
                m_pFrame = 0;
            }

            if (m_FrameType)
            {
                delete [] m_FrameType;
                m_FrameType = 0;
            }

            if (m_FrameType2)
            {
                delete [] m_FrameType2;
                m_FrameType2 = 0;
            }

            m_maxN = 0;
            m_curIndex = 0;
            m_nFrames = 0;
        }

        mfxStatus Init(mfxU32 maxN)
        {
            Close();
            m_maxN = maxN;
            m_pFrame = new mfxFrameSurface1* [ m_maxN];
            if (!m_pFrame)
                return MFX_ERR_MEMORY_ALLOC;
            m_FrameType = new mfxU16 [ m_maxN];
            if (!m_FrameType)
                return MFX_ERR_MEMORY_ALLOC;
            m_FrameType2 = new mfxU16 [ m_maxN];
            if (!m_FrameType2)
                return MFX_ERR_MEMORY_ALLOC;
            memset (m_pFrame,0, sizeof(mfxFrameSurface1*)*m_maxN);
            memset (m_FrameType,0, sizeof(mfxU8)*m_maxN);
            memset (m_FrameType2,0, sizeof(mfxU8)*m_maxN);

            return MFX_ERR_NONE;
        }

        void Reset()
        {
            mfxU32 i = 0;
            if(m_pFrame)
                memset (m_pFrame,0, sizeof(mfxFrameSurface1*)* m_maxN);
            m_curIndex = 0;
            m_nFrames = 0;
            if(m_pFrame)
            {
                for(i = 0; i < m_maxN; i++)
                {
                     ReleasePlane(m_pFrame[i]);
                }
            }
            if (m_FrameType)
            {
                memset (m_FrameType,0, sizeof(mfxU16)*m_maxN);
            }

            if (m_FrameType2)
            {
                memset (m_FrameType2,0, sizeof(mfxU16)*m_maxN);
            }
        }

        bool AddFrame(mfxFrameSurface1* frame, mfxU16 frameType)
        {
            if (m_nFrames < m_maxN)
            {
                mfxU32 ind = (m_curIndex + m_nFrames)%m_maxN;
                m_pFrame[ind] = frame;
                m_FrameType[ind] = frameType;
                m_nFrames ++;
                return true;
            }
            return false;
        }

        bool AddFrame(mfxFrameSurface1* frame, mfxU16 frameType, mfxU16 frameType2)
        {
            if (m_nFrames < m_maxN)
            {
                mfxU32 ind = (m_curIndex + m_nFrames)%m_maxN;
                m_pFrame[ind] = frame;
                m_FrameType[ind] = frameType;
                m_FrameType2[ind] = frameType2;
                m_nFrames ++;
                return true;
            }
            return false;
        }

        mfxFrameSurface1* GetCurrFrame()
        {
            mfxFrameSurface1* frm = 0;
            if (m_nFrames>0)
            {
                frm = m_pFrame[m_curIndex];
            }
            return frm;
        }

        mfxU16 GetCurrType()
        {
            if (m_nFrames>0)
            {
                return m_FrameType[m_curIndex];
            }
            return 0;
        }

        void GetCurrType(mfxU16* FrameType, mfxU16* FrameType2)
        {
            *FrameType  = 0;
            *FrameType2 = 0;

            if (m_nFrames > 0)
            {
                *FrameType  = m_FrameType[m_curIndex];
                *FrameType2 = m_FrameType2[m_curIndex];
            }
        }

        bool MoveOnNextFrame()
        {
            if (m_nFrames>0)
            {
                m_curIndex = (1+m_curIndex)%m_maxN;
                m_nFrames--;
                return true;
            }
            return false;
        }

    private:
        mfxU32  m_maxN;
        mfxFrameSurface1** m_pFrame;
        mfxU16        * m_FrameType;
        mfxU16        * m_FrameType2;
        mfxU32  m_curIndex;
        mfxU32  m_nFrames;

    };



#endif //_MFX_FRAMES_H_
