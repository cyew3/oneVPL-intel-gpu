//
// INTEL CORPORATION PROPRIETARY INFORMATION
//
// This software is supplied under the terms of a license agreement or
// nondisclosure agreement with Intel Corporation and may not be copied
// or disclosed except in accordance with the terms of that agreement.
//
// Copyright(C) 2006-2012 Intel Corporation. All Rights Reserved.
//

#ifndef __BITSTREAMIN_H__
#define __BITSTREAMIN_H__

#include "umc_defs.h"
#if defined (UMC_ENABLE_MJPEG_VIDEO_DECODER)
#include "jpegbase.h"


class CBaseStreamInput;

const int DEC_DEFAULT_BUFLEN = 4096; // internal intermediate input buffer

class CBitStreamInput
{
public:
  CBitStreamInput(void);
  virtual ~CBitStreamInput(void);

  JERRCODE Attach(CBaseStreamInput* in);
  JERRCODE Detach(void);

  JERRCODE Init(int bufSize = DEC_DEFAULT_BUFLEN);
  JERRCODE FillBuffer(int nMinBytes = 0);

  Ipp8u* GetDataPtr(void) const { return m_pData; }
  int    GetDataLen(void) const { return m_DataLen; }
  Ipp8u* GetCurrPtr(void) const { return m_pData + m_currPos; }
  int    GetCurrPos(void) const { return m_currPos; }
  void   SetCurrPos(int cp)     { m_nUsedBytes += (cp - m_currPos); m_currPos = cp; }
  int    GetNumUsedBytes(void)  { return m_nUsedBytes; }

  JERRCODE Seek(long offset, int origin = UIC_SEEK_CUR);
  JERRCODE CheckByte(int pos, int* byte);
  JERRCODE ReadByte(int* byte);
  JERRCODE ReadWord(int* word);
  JERRCODE ReadDword(int* dword);

protected:
  CBaseStreamInput* m_in;

  Ipp8u*            m_pData;
  int               m_DataLen;
  int               m_currPos;
  int               m_nUsedBytes;

  int               m_eod;
};

#endif // UMC_ENABLE_MJPEG_VIDEO_DECODER
#endif // __BITSTREAMIN_H__

