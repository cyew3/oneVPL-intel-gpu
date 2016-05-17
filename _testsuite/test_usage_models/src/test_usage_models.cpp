//
//               INTEL CORPORATION PROPRIETARY INFORMATION
//  This software is supplied under the terms of a license agreement or
//  nondisclosure agreement with Intel Corporation and may not be copied
//  or disclosed except in accordance with the terms of that agreement.
//        Copyright (c) 2010-2016 Intel Corporation. All Rights Reserved.
//

#include <stdexcept>
#include <iostream>

#include "test_statistics.h"
#include "test_usage_models_utils.h"
#include "test_usage_models_cmdline.h"
#include "transcode_model_reference.h"
#include "transcode_model_advanced.h"
#include "test_usage_models_exception.h"


TranscodeModel* CreateTranscode( AppParam &param )
{
    TranscodeModel* pModel = NULL;

    switch( param.usageModel )
    {
        case USAGE_MODEL_REFERENCE:
        {
            param.sessionMode = DECVPPENC_SESSION; // common session for all components
            pModel = (TranscodeModel*)new TranscodeModelReference( param );
            break;
        }

        case USAGE_MODEL_1:
        {
            // AYA debug
            //param.asyncDepth = 3;

            param.sessionMode = DECVPPENC_SESSION; // common session for all components
            pModel = (TranscodeModel*)new TranscodeModelAdvanced( param );
            break;
        }

        case USAGE_MODEL_2:
        {
            param.sessionMode = DEC_VPP_ENC_SESSION; // unique session per components
            pModel = (TranscodeModel*)new TranscodeModelAdvanced( param );
            break;
        }

        case USAGE_MODEL_3:
        {
            param.sessionMode = DECVPP_ENC_SESSION; 
            pModel = (TranscodeModel*)new TranscodeModelAdvanced( param );
            break;
        }

        case USAGE_MODEL_4:
        {
            param.sessionMode = DECENC_VPP_SESSION; 
            pModel = (TranscodeModel*)new TranscodeModelAdvanced( param );
            break;
        }

        case USAGE_MODEL_5:
        {
            param.sessionMode = DEC_VPPENC_SESSION; 
            pModel = (TranscodeModel*)new TranscodeModelAdvanced( param );
            break;
        }

        case USAGE_MODEL_6:
        {
            param.sessionMode = DECVPP_ENC_JOIN_SESSION; 
            pModel = (TranscodeModel*)new TranscodeModelAdvanced( param );
            break;
        }

        case USAGE_MODEL_7:
        {
            param.sessionMode = DEC_VPPENC_JOIN_SESSION; 
            pModel = (TranscodeModel*)new TranscodeModelAdvanced( param );
            break;
        }

        case USAGE_MODEL_8:
        {
            param.sessionMode = DEC_VPP_ENC_JOIN_SESSION; 
            pModel = (TranscodeModel*)new TranscodeModelAdvanced( param );
            break;
        }

        case USAGE_MODEL_JOIN_REFERENCE:
        {
            param.sessionMode = DEC_VPP_ENC_SESSION; 
            pModel = (TranscodeModel*)new TranscodeModelReference( param );
            break;
        }

        default:
        {
            pModel = NULL;
            break;
        }
    } // end switch

    return pModel;

} // TranscodeModel* CreateTranscode( AppParam &param )

#if defined(_WIN32) || defined(_WIN64)
int _tmain(int argc, const TCHAR *argv[])
#else
int main(int argc, const char *argv[])
#endif
{
    CommandLine cmd = CommandLine(argc, argv);
    Timer       statTimer;
    int framesCount = 0;
    int appSts = TUM_OK_STS;
    
    if (!cmd.IsValid())
    {
        msdk_fprintf(stderr, MSDK_STRING("FAILED\n"));
        CommandLine::PrintUsage(argv[0]);

        return TUM_ERR_STS;
    }

    AppParam  param;
    cmd.GetParam( param );

    TranscodeModel* transcode = NULL;
    try
    {
        mfxStatus sts;

        transcode = CreateTranscode(param);        

        if( NULL == transcode )
        {
            RETURN_ON_ERROR("\nFAIL on creation\n", TUM_ERR_STS);
        }        

        sts = transcode->Init( param );
        if(sts != MFX_ERR_NONE) { delete transcode; return TUM_ERR_STS; }
        
        cmd.PrintInfo();
        
        statTimer.Start();

        msdk_printf(MSDK_STRING("\nTranscoding started\n"));
        sts = transcode->Run();
        msdk_printf(MSDK_STRING("\n\nTranscoding finished\n"));

         statTimer.Stop();

        if(sts != MFX_ERR_NONE) { delete transcode; return TUM_ERR_STS; }
        
        framesCount = transcode->GetProcessedFramesCount();
        transcode->Close();
    }
    catch (std::exception &e)
    {
        std::cerr << e.what() << std::endl;
        appSts = TUM_ERR_STS;
    }

    delete transcode;

    msdk_printf(MSDK_STRING("Total time %.2f sec \n"), statTimer.OverallTiming());
    msdk_printf(MSDK_STRING("Frames per second %.3f fps \n"), framesCount / statTimer.OverallTiming());

    return appSts;

} // int _tmain(int argc, TCHAR *argv[])
/* EOF */
