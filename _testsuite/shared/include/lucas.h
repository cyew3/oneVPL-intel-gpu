/* ////////////////////////////////////////////////////////////////////////////// */
/*
//
//              INTEL CORPORATION PROPRIETARY INFORMATION
//  This software is supplied under the terms of a license  agreement or
//  nondisclosure agreement with Intel Corporation and may not be copied
//  or disclosed except in  accordance  with the terms of that agreement.
//        Copyright (c) 2008-2009 Intel Corporation. All Rights Reserved.
//
//
*/

/**
 * @file   Lucas.h
 * 
 * @brief  LucasCtx class declaration
 * 
 * 
 */

#ifdef LUCAS_DLL
#ifndef LUCASCTX_H
#define LUCASCTX_H

#include "framemodewrapperctx.h"

#ifdef WIN32 
    #define LUCAS_EXPORT extern "C" __declspec(dllexport)
#else
    #define LUCAS_EXPORT extern "C"
#endif

/**
* @brief Namespace that contains data necessary to integrate MediaSDK with Lucas project
*/
namespace lucas {
  
  class CLucasCtx {
  public:
    static CLucasCtx &Instance()
    { 
      return _instance;
    }

    void Init(TFMWrapperCtx &ctx)
    {
      _ctx = &ctx;
      _ctx->ctxLen = sizeof(lucas::TFMWrapperCtx);
    }
    TFMWrapperCtx *Get()
    {
      /// @todo Report error
//      if(!_ctx)
      return _ctx;
    }
    TFMWrapperCtx &operator*() { return *Get(); }
    TFMWrapperCtx *operator->() { return Get(); }

  private:
    static CLucasCtx _instance;
    TFMWrapperCtx *_ctx;
    
    CLucasCtx() : _ctx(0) {};
    CLucasCtx(const CLucasCtx &);             /**< @brief Disallowed */
    CLucasCtx &operator=(const CLucasCtx &);  /**< @brief Disallowed */
    ~CLucasCtx() {};
  };

} // namespace lucas

#endif /* LUCASCTX_H */
#endif /* LUCAS_DLL */

