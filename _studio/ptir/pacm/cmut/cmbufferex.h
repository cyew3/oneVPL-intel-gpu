/**             
***
*** Copyright  (C) 1985-2016 Intel Corporation. All rights reserved.
***
*** The information and source code contained herein is the exclusive
*** property of Intel Corporation. and may not be disclosed, examined
*** or reproduced in whole or in part without explicit written authorization
*** from the company.
***
*** ----------------------------------------------------------------------------
**/ 
#pragma once

#include "cmrt_cross_platform.h"
//#include "cm_rt.h"
#include "cmdeviceex.h"
//#include "cm_rt.h"
#include "cmassert.h"
#include "assert.h"
#include "ptir_memory.h"

class CmBufferEx
{
public:
  CmBufferEx(CmDeviceEx & cmDeviceEx, int size)
    : cmDeviceEx(cmDeviceEx)
  {
#ifdef _DEBUG
    //cout << "CmBufferEx (" << size << ")" << endl;
#endif
    pCmBuffer = cmDeviceEx.CreateBuffer(size);
  }

  CmBufferEx(CmDeviceEx & cmDeviceEx, const unsigned char * pData, int size)
    : cmDeviceEx(cmDeviceEx)
  {
#ifdef _DEBUG
    //cout << "CmBufferEx (" << size << ")" << endl;
#endif
    assert(pData != NULL);

    pCmBuffer = cmDeviceEx.CreateBuffer(size);
    Write(pData);
  }

  ~CmBufferEx() 
  {
    if (pCmBuffer != NULL) {
      cmDeviceEx->DestroySurface(pCmBuffer);
    }
  }

  operator CmBuffer & () { return *pCmBuffer; }
  CmBuffer * operator -> () { return pCmBuffer; }

  void Read(void * pSysMem, CmEvent * pEvent = NULL)
  {
    assert(pSysMem != NULL);

    int result = pCmBuffer->ReadSurface((unsigned char *)pSysMem, pEvent);
    CM_FAIL_IF(result != CM_SUCCESS, result);
  }

  void Write(const unsigned char * pSysMem)
  {
    assert(pSysMem != NULL);
    int result = pCmBuffer->WriteSurface(pSysMem, NULL);
    CM_FAIL_IF(result != CM_SUCCESS, result);
  }

protected:
  CmBuffer * pCmBuffer;
  CmDeviceEx & cmDeviceEx;

private:
  //prohobit copy constructor
  CmBufferEx(const CmBufferEx& that);
  //prohibit assignment operator
  CmBufferEx& operator=(const CmBufferEx&);
};

class CmBufferUPEx
{
public:
  CmBufferUPEx(CmDeviceEx & cmDeviceEx, const unsigned char * pData, int size)
    : cmDeviceEx(cmDeviceEx)
  {
#ifdef _DEBUG
    //cout << "CmBufferUPEx (0x" << std::hex << (unsigned int)pData << ", " << std::dec << size << ")" << endl;
#endif
    assert (pData != NULL);

    pCmBuffer = cmDeviceEx.CreateBufferUP(pData, size);
  }

  ~CmBufferUPEx() 
  {
    if (pCmBuffer != NULL) {
      cmDeviceEx->DestroyBufferUP(pCmBuffer);
    }
  }

  operator CmBufferUP & () { return *pCmBuffer; }
  CmBufferUP * operator -> () { return pCmBuffer; }

protected:
  CmBufferUP * pCmBuffer;
  CmDeviceEx & cmDeviceEx;

private:
  //prohobit copy constructor
  CmBufferUPEx(const CmBufferUPEx& that);
  //prohibit assignment operator
  CmBufferUPEx& operator=(const CmBufferUPEx&);
};

class CmSurface2DEx
{
public:
  CmSurface2DEx(CmDeviceEx & cmDeviceEx, UINT width, UINT height, CM_SURFACE_FORMAT format)
      : cmDeviceEx(cmDeviceEx), isCreated(true)
  {
#ifdef _DEBUG
//    cout << "CmSurface2DEx (" << width << ", " << height << ")" << endl;
#endif
    pCmSurface2D = cmDeviceEx.CreateSurface2D(width, height, format);
  }

  CmSurface2DEx(CmDeviceEx & cmDeviceEx, UINT width, UINT height, CM_SURFACE_FORMAT format, CmSurface2D* psurf)
      : cmDeviceEx(cmDeviceEx), isCreated(false)
  {
#ifdef _DEBUG
//    cout << "CmSurface2DEx (" << width << ", " << height << ")" << endl;
#endif
    //assert(psurf != NULL);
      pCmSurface2D = psurf;
  }

  CmSurface2DEx(CmDeviceEx & cmDeviceEx, const unsigned char * pData, UINT width, UINT height, CM_SURFACE_FORMAT format)
      : cmDeviceEx(cmDeviceEx), isCreated(true)
  {
#ifdef _DEBUG
//    cout << "CmSurface2DEx (" << width << ", " << height << ")" << endl;
#endif
    assert(pData != NULL);

    pCmSurface2D = cmDeviceEx.CreateSurface2D(width, height, format);
    Write (pData);
  }

  ~CmSurface2DEx() 
  {
    if (pCmSurface2D != NULL && isCreated) {
      cmDeviceEx->DestroySurface(pCmSurface2D);
    }
  }

  operator CmSurface2D & () { return *pCmSurface2D; }
  CmSurface2D * operator -> () { return pCmSurface2D; }

  void Read(void * pSysMem, CmEvent* pEvent = NULL)
  {
    assert (pSysMem != NULL);
    int result = pCmSurface2D->ReadSurface((unsigned char *)pSysMem, pEvent);
    CM_FAIL_IF(result != CM_SUCCESS, result);
  }

  void Read(void * pSysMem, const UINT stride, CmEvent* pEvent = NULL)
  {
    assert (pSysMem != NULL);
    int result = pCmSurface2D->ReadSurfaceStride((unsigned char *)pSysMem, pEvent, stride);
    CM_FAIL_IF(result != CM_SUCCESS, result);
  }

  void Write(const unsigned char * pSysMem)
  {
    assert (pSysMem != NULL);
    int result = pCmSurface2D->WriteSurface(pSysMem, NULL);
    CM_FAIL_IF(result != CM_SUCCESS, result);
  }
  CmSurface2D * pCmSurface2D;
protected:

  CmDeviceEx & cmDeviceEx;
  bool isCreated;

private:
  //prohobit copy constructor
  CmSurface2DEx(const CmSurface2DEx& that);
  //prohibit assignment operator
  CmSurface2DEx& operator=(const CmSurface2DEx&);
};

class CmSurface2DUPEx
{
public:
  CmSurface2DUPEx(CmDeviceEx & deviceEx, unsigned int width, unsigned int height, CM_SURFACE_FORMAT format)
    : deviceEx(deviceEx), pData(NULL), width(width), height(height), pitch(0), format(format), malloced(false)
  {
#ifdef _DEBUG
    //std::cout << "CmSurface2DUPEx (" << width << ", " << height << ")" << std::endl;
#endif
    int result = deviceEx->GetSurface2DInfo(width, height, format, pitch, physicalSize);
    CM_FAIL_IF(result != CM_SUCCESS, result);

    pData = CM_ALIGNED_MALLOC(physicalSize, 0x1000);
    if (pData == NULL) {
      // Can't throw a bad_alloc with a string in the constructor
      // Throw a derived variant instead
      throw CMUT_EXCEPTION2("fail", cm_bad_alloc);
    }
    memset(pData,0,physicalSize);
    malloced = true;

    pCmSurface2D = deviceEx.CreateSurface2DUP((unsigned char *)pData, width, height, format);
  }

  ~CmSurface2DUPEx()
  {
    if (pCmSurface2D != NULL) {
      deviceEx->DestroySurface2DUP(pCmSurface2D);
    }  
    
    if (malloced && pData) {
      CM_ALIGNED_FREE(pData);
      pData = 0;
    }
  }

  operator CmSurface2DUP & () { return *pCmSurface2D; }
  CmSurface2DUP * operator -> () { return pCmSurface2D; }

  const unsigned int Pitch() const { return pitch; }
  const unsigned int Height() const { return height; }
  const unsigned int Width() const { return width; }

  void * DataPtr() const { return pData; }
  void * DataPtr(int row) const { return (unsigned char *)pData + row * pitch; }

  void Read(void * pSysMem, CmEvent* pEvent = NULL)
  {
    assert (pSysMem != NULL);
    for (int row = 0; row < actualHeight(); row++)
    ptir_memcpy((char*)pSysMem + actualWidth() *row, DataPtr(row), actualWidth());
  }

  void Write(const unsigned char * pSysMem)
  {
    assert (pSysMem != NULL);
    for (int row = 0; row < actualHeight(); row++)
    ptir_memcpy(DataPtr(row), (char*)pSysMem + actualWidth()*row, actualWidth());
  }

protected:
  CmSurface2DUP * pCmSurface2D;
  CmDeviceEx & deviceEx;

  void * pData;

  unsigned int width;// in elements
  unsigned int height;
  unsigned int pitch;// in bytes
  unsigned int physicalSize;
  const CM_SURFACE_FORMAT format;

  bool malloced;

  int actualWidth() const {
      switch (format) {
      case CM_SURFACE_FORMAT_A8R8G8B8:
      case CM_SURFACE_FORMAT_X8R8G8B8:
      case CM_SURFACE_FORMAT_R32F:
          return 4 * width;
      case CM_SURFACE_FORMAT_V8U8:
      //case CM_SURFACE_FORMAT_R16_SINT:
      case CM_SURFACE_FORMAT_UYVY:
      case CM_SURFACE_FORMAT_YUY2:
          return 2 * width;
      case CM_SURFACE_FORMAT_A8:
      case CM_SURFACE_FORMAT_P8:
      //case CM_SURFACE_FORMAT_R8_UINT:
          return 1 * width;
      case CM_SURFACE_FORMAT_NV12:
          return 1 * width;
      default:
          break;
      }
      throw CMUT_EXCEPTION("Unkown Format Code");
  }
  int actualHeight() const {
      switch (format) {
      case CM_SURFACE_FORMAT_A8R8G8B8:
      case CM_SURFACE_FORMAT_X8R8G8B8:
      case CM_SURFACE_FORMAT_R32F:
          return 1 * height;
      case CM_SURFACE_FORMAT_V8U8:
      //case CM_SURFACE_FORMAT_R16_SINT:
      case CM_SURFACE_FORMAT_UYVY:
      case CM_SURFACE_FORMAT_YUY2:
          return 1 * height;
      case CM_SURFACE_FORMAT_A8:
      case CM_SURFACE_FORMAT_P8:
      //case CM_SURFACE_FORMAT_R8_UINT:
          return 1 * height;
      case CM_SURFACE_FORMAT_NV12:
          return 3 * height / 2;
      default:
          break;
      }
      throw CMUT_EXCEPTION("Unkown Format Code");
  }

private:
  //prohobit copy constructor
  CmSurface2DUPEx(const CmSurface2DUPEx& that);
  //prohibit assignment operator
  CmSurface2DUPEx& operator=(const CmSurface2DUPEx&);

};

