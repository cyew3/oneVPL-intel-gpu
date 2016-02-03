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
#include "mfxstructures.h"
//#include "cm_rt.h"
#include "cmutility.h"
#include "cmassert.h"
#include "iostream"
#include "fstream"
#include "vector"
using namespace std;

class CmDeviceEx
{
  friend class CmKernelEx;
  friend class CmBufferEx;
  friend class CmBufferUPEx;
  friend class CmQueueEx;
  friend class CmSurface2DEx;
  friend class CmSurface2DUPEx;
  template<typename T>
  friend class CmSurface2DUPExT;
  template<typename T>
  friend class CmSurface2DExT;

public:
  CmDeviceEx(const unsigned char * programm, int size, mfxHandleType _mfxDeviceType, mfxHDL _mfxDeviceHdl, bool jit = false)
  {
    mfxDeviceType = _mfxDeviceType;
    mfxDeviceHdl = _mfxDeviceHdl;
    CMUT_ASSERT(programm != NULL && size > 0);

    UINT version = 0;
    int result = CM_FAILURE;//CreateCmDevice(pDevice, version);
#if defined(_WIN32) || defined(_WIN64)
    if(MFX_HANDLE_DIRECT3D_DEVICE_MANAGER9 == mfxDeviceType)
        result = CreateCmDevice(pDevice,version,(IDirect3DDeviceManager9*) mfxDeviceHdl);
    else if(MFX_HANDLE_D3D11_DEVICE == mfxDeviceType)
        result = CreateCmDevice(pDevice,version,(ID3D11Device*) mfxDeviceHdl);
#elif defined(LINUX32) || defined (LINUX64)
    if(MFX_HANDLE_VA_DISPLAY == mfxDeviceType)
        result = CreateCmDevice(pDevice,version, (VADisplay) mfxDeviceHdl);
#endif

    CM_FAIL_IF(result != CM_SUCCESS || pDevice == NULL, result);

    CmProgram * pProgram = NULL;
    if(jit)
        result = ::ReadProgramJit(pDevice, pProgram, programm, size);
    else
        result = ::ReadProgram(pDevice, pProgram, programm, size);  
    CM_FAIL_IF(result != CM_SUCCESS || pProgram == NULL, result);

    this->programs.push_back(pProgram);

  }

  ~CmDeviceEx()
  {
    for (auto program : programs) {
      pDevice->DestroyProgram(program);
    }

    DestroyCmDevice(pDevice);
  }

  CmDevice * operator -> () { return pDevice; }
  //CmProgram * Program () const { return pProgram; }

  void GetCaps (CM_DEVICE_CAP_NAME capName, size_t & capValueSize, void * pCapValue)
  {
    int result = pDevice->GetCaps(capName, capValueSize, pCapValue);
    CM_FAIL_IF(result != CM_SUCCESS, result);
  }

  CmProgram * Program(int i)
  {
    return programs[i];
  }

  void CreateThreadSpace(UINT width, UINT height, CmThreadSpace* & pThreadSpace)
  {
    int result = pDevice->CreateThreadSpace(width, height, pThreadSpace);
    CM_FAIL_IF(result != CM_SUCCESS || pThreadSpace == NULL, result);
  }

protected:
  int CCCreateCmDevice(CmDevice *& pDevice, UINT version)
  {
#if defined(_WIN32) || defined(_WIN64)
      if(MFX_HANDLE_DIRECT3D_DEVICE_MANAGER9 == mfxDeviceType)
          return ::CreateCmDevice(pDevice,version,(IDirect3DDeviceManager9*) mfxDeviceHdl);
      //else if(MFX_HANDLE_D3D11_DEVICE == mfxDeviceType)
      //    return ::CreateCmDevice(pDevice,version,(ID3D11Device*) mfxDeviceHdl);
#elif defined(LINUX32) || defined (LINUX64)
      if(MFX_HANDLE_VA_DISPLAY == mfxDeviceType)
          return ::CreateCmDevice(pDevice,version, (VADisplay) mfxDeviceHdl);
#endif
      else
          return CM_FAILURE;
  }

  CmSurface2D * CreateSurface2D(UINT width, UINT height, CM_SURFACE_FORMAT format)
  {
    CmSurface2D * pCmSurface2D = NULL;

    int result = pDevice->CreateSurface2D(width, height, format, pCmSurface2D);
    if(result != CM_SUCCESS)
        std::cout << "CM FAILURE!\n";
    CM_FAIL_IF(result != CM_SUCCESS || pCmSurface2D == NULL, result);

    return pCmSurface2D;
  }  
  
  CmBuffer * CreateBuffer(UINT size)
  {
    CmBuffer * pCmBuffer = NULL;

    int result = pDevice->CreateBuffer (size, pCmBuffer);
    CM_FAIL_IF(result != CM_SUCCESS || pCmBuffer == NULL, result);

    return pCmBuffer;
  }

  CmBufferUP * CreateBufferUP(const unsigned char * pData, UINT size)
  {
    CmBufferUP * pCmBuffer = NULL;

    int result = pDevice->CreateBufferUP(size, (void *)pData, pCmBuffer);
    CM_FAIL_IF(result != CM_SUCCESS || pCmBuffer == NULL, result);

    return pCmBuffer;
  }

  CmSurface2DUP * CreateSurface2DUP(unsigned char * pData, UINT width, UINT height, CM_SURFACE_FORMAT format)
  {
    CmSurface2DUP * pCmSurface = NULL;

    int result = pDevice->CreateSurface2DUP (width, height, format, (void *)pData, pCmSurface);
    CM_FAIL_IF(result != CM_SUCCESS || pCmSurface == NULL, result);

    return pCmSurface;
  }

#ifdef CMRT_EMU
  CmKernel * CreateKernel(const char * pKernelName, const void * fncPnt)
  {
    CmKernel * pKernel = NULL;

    for (auto program : programs) {
      int result = pDevice->CreateKernel (program, pKernelName, fncPnt, pKernel);
      if (result == CM_SUCCESS) {
        return pKernel;     
      }
    }

    throw CMUT_EXCEPTION("fail");
  }
#else
  CmKernel * CreateKernel(const char * pKernelName)
  {
    CmKernel * pKernel = NULL;

    for (auto program : programs) {
      int result = pDevice->CreateKernel(program, pKernelName, pKernel);
      if (result == CM_SUCCESS) {
        return pKernel;
      }
    }

    throw CMUT_EXCEPTION("fail");
  }
#endif

  void DestroyKernel(CmKernel * pKernel)
  {
    pDevice->DestroyKernel(pKernel);
  }

protected:
  CmDevice * pDevice;
  std::vector<CmProgram *> programs;

  mfxHandleType mfxDeviceType;
  mfxHDL        mfxDeviceHdl;
};
