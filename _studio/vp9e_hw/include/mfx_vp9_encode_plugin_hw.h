//
// INTEL CORPORATION PROPRIETARY INFORMATION
//
// This software is supplied under the terms of a license agreement or
// nondisclosure agreement with Intel Corporation and may not be copied
// or disclosed except in accordance with the terms of that agreement.
//
// Copyright(C) 2016-2018 Intel Corporation. All Rights Reserved.
//

#pragma once

#include "mfx_common.h"
#if defined(AS_VP9E_PLUGIN)

#include "mfx_vp9_encode_hw_utils.h"
#include "mfxplugin++.h"

namespace MfxHwVP9Encode
{
    class Plugin : public MFXEncoderPlugin
    {
    public:
        static MFXEncoderPlugin* Create() {
            return new Plugin(false);
        }

        static mfxStatus CreateByDispatcher(mfxPluginUID guid, mfxPlugin* mfxPlg)
        {
            if (memcmp(&guid, &MFX_PLUGINID_VP9E_HW, sizeof(mfxPluginUID))) {
                return MFX_ERR_NOT_FOUND;
            }
            Plugin * tmp_pplg = 0;

            try
            {
            tmp_pplg = new Plugin(false);
            }

            catch (std::bad_alloc&)
            {
            return MFX_ERR_MEMORY_ALLOC;;
            }

            catch (...)
            {
            return MFX_ERR_UNKNOWN;;
            }

            *mfxPlg = tmp_pplg->m_adapter;
            tmp_pplg->m_createdByDispatcher = true;

            return MFX_ERR_NONE;
        }
        virtual mfxStatus PluginInit(mfxCoreInterface *core)
        {
            if (!core)
                 return MFX_ERR_NULL_PTR;

            m_pmfxCore = core;
            return MFX_ERR_NONE;
        }
        virtual mfxStatus PluginClose()
        {
            if (m_createdByDispatcher) {
                delete this;
            }

            return MFX_ERR_NONE;
        }

        virtual mfxStatus GetPluginParam(mfxPluginParam *par)
        {
            if (!par)
                return MFX_ERR_NULL_PTR;
            *par = m_PluginParam;
            return MFX_ERR_NONE;
        }
        virtual mfxStatus EncodeFrameSubmit(mfxEncodeCtrl * /*ctrl*/, mfxFrameSurface1 * /*surface*/, mfxBitstream * /*bs*/, mfxThreadTask * /*task*/)
        {
            return MFX_ERR_UNDEFINED_BEHAVIOR;
        }
        virtual mfxStatus ConfigTask(Task & /*task*/)
        {
            return MFX_ERR_UNDEFINED_BEHAVIOR;
        }
        virtual mfxStatus RemoveObsoleteParameters()
        {
            return MFX_ERR_UNDEFINED_BEHAVIOR;
        }
        virtual mfxStatus Execute(mfxThreadTask /*task*/, mfxU32 /*uid_p*/, mfxU32 /*uid_a*/)
        {
            return MFX_ERR_UNDEFINED_BEHAVIOR;
        }
        virtual mfxStatus FreeResources(mfxThreadTask, mfxStatus)
        {
            return MFX_ERR_NONE;
        }
        virtual mfxStatus Query(mfxVideoParam * /*in*/, mfxVideoParam * /*out*/)
        {
            return MFX_ERR_UNDEFINED_BEHAVIOR;
        }
        virtual mfxStatus QueryIOSurf(mfxVideoParam * /*par*/, mfxFrameAllocRequest * /*in*/, mfxFrameAllocRequest * /*out*/)
        {
            return MFX_ERR_UNDEFINED_BEHAVIOR;
        }
        virtual mfxStatus Init(mfxVideoParam * /*par*/)
        {
            return MFX_ERR_UNDEFINED_BEHAVIOR;
        }
        virtual mfxStatus Reset(mfxVideoParam * /*par*/)
        {
            return MFX_ERR_UNDEFINED_BEHAVIOR;
        }
        virtual mfxStatus Close()
        {
            return MFX_ERR_UNDEFINED_BEHAVIOR;
        }
        virtual mfxStatus GetVideoParam(mfxVideoParam * /*par*/)
        {
            return MFX_ERR_UNDEFINED_BEHAVIOR;
        }
        virtual void Release()
        {
            delete this;
        }

        virtual mfxStatus SetAuxParams(void*, int)
        {
            return MFX_ERR_UNKNOWN;
        }

        virtual mfxStatus UpdateBitstream(Task & /*task*/)
        {
            return MFX_ERR_UNDEFINED_BEHAVIOR;
        }

    protected:
        explicit Plugin(bool CreateByDispatcher)
            : m_PluginParam()
            , m_createdByDispatcher(CreateByDispatcher)
            , m_adapter(this)
            , m_pmfxCore(nullptr)
        {
            m_PluginParam.ThreadPolicy = MFX_THREADPOLICY_SERIAL;
            m_PluginParam.MaxThreadNum = 1;
            m_PluginParam.APIVersion.Major = MFX_VERSION_MAJOR;
            m_PluginParam.APIVersion.Minor = MFX_VERSION_MINOR;
            m_PluginParam.PluginUID = MFX_PLUGINID_VP9E_HW;
            m_PluginParam.Type = MFX_PLUGINTYPE_VIDEO_ENCODE;
            m_PluginParam.CodecId = MFX_CODEC_VP9;
            m_PluginParam.PluginVersion = 1;
        }
        virtual ~Plugin() {}

        mfxCoreInterface * m_pmfxCore;

        mfxPluginParam      m_PluginParam;
        bool                m_createdByDispatcher;
        MFXPluginAdapter<MFXEncoderPlugin> m_adapter;
        };
    }
#endif // AS_VP9E_PLUGIN