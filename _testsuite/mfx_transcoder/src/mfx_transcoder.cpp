/* ****************************************************************************** *\

INTEL CORPORATION PROPRIETARY INFORMATION
This software is supplied under the terms of a license agreement or nondisclosure
agreement with Intel Corporation and may not be copied or disclosed except in
accordance with the terms of that agreement
Copyright(c) 2008-2016 Intel Corporation. All Rights Reserved.

File Name: .h

\* ****************************************************************************** */

#include "mfx_pipeline_defs.h"
#include "mfx_encode_pipeline_config.h"
#include "mfx_default_pipeline_mgr.h"

#ifdef LUCAS_DLL

#include "lucas.h"

lucas::CLucasCtx lucas::CLucasCtx::_instance;

LUCAS_EXPORT int LucasMain(int argc, vm_char * argv[], lucas::TFMWrapperCtx *ctx)
{
  /// @todo Report error
  //if(!ctx)

  lucas::CLucasCtx::Instance().Init(*ctx);

#else // LUCAS_DLL

#if defined(_WIN32) || defined(_WIN64)
int _tmain(int argc, TCHAR *argv[])
#else
int main(int argc, char *argv[])
#endif // #if defined(_WIN32) || defined(_WIN64)
{ 

#endif // LUCAS_DLL
  int ret = 0;

  MFX_TRACE_INIT();
  {
    MFX_AUTO_LTRACE_FUNC(MFX_TRACE_LEVEL_HOTSPOTS);

    std::auto_ptr<IMFXPipelineConfig> cfg(new MFXPipelineConfigEncode(argc, argv));
    MFXPipelineManager defMgr;

    ret = defMgr.Execute(cfg.get());
  }
  MFX_TRACE_CLOSE();
  return ret;
}
