/* ****************************************************************************** *\

Copyright (C) 2013 Intel Corporation.  All rights reserved.

Redistribution and use in source and binary forms, with or without
modification, are permitted provided that the following conditions are met:
- Redistributions of source code must retain the above copyright notice,
this list of conditions and the following disclaimer.
- Redistributions in binary form must reproduce the above copyright notice,
this list of conditions and the following disclaimer in the documentation
and/or other materials provided with the distribution.
- Neither the name of Intel Corporation nor the names of its contributors
may be used to endorse or promote products derived from this software
without specific prior written permission.

THIS SOFTWARE IS PROVIDED BY INTEL CORPORATION "AS IS" AND ANY EXPRESS OR
IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES
OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.
IN NO EVENT SHALL INTEL CORPORATION BE LIABLE FOR ANY DIRECT, INDIRECT,
INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT
NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
(INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.

File Name: mfx_plugin_hive.h

\* ****************************************************************************** */

#pragma once
#include <list>
#include <string>
#include <memory>
#include <string.h>
#include "mfx_dispatcher_defs.h"
#include "mfxplugin.h"
#include "mfx_win_reg_key.h"


namespace MFX {

    inline bool operator == (const mfxPluginUID &lhs, const mfxPluginUID & rhs) {
        return !memcmp(lhs.Data, rhs.Data, sizeof(mfxPluginUID));
    }
    
    inline bool operator != (const mfxPluginUID &lhs, const mfxPluginUID & rhs) {
        return !(lhs == rhs);
    }

    struct PluginDescriptionRecord {
        mfxU32  Type;
        mfxU32  CodecId;
        mfxPluginUID uid;
        std::basic_string<msdk_disp_char> Path;
        std::string Name;
        bool Default;
    };

    class MFXPluginHive  {
        std::list<PluginDescriptionRecord> mRecords;
    public:
        typedef std::list<PluginDescriptionRecord>::iterator iterator;
        iterator begin() {
            return mRecords.begin();
        }
        iterator end() {
            return mRecords.end();
        }
        void insert(iterator beg_iter, iterator end_iter) {
            mRecords.insert(mRecords.end(), beg_iter, end_iter);
        }
        size_t size() {
            return mRecords.size();
        }

        MFXPluginHive(mfxU32 mfxStorageID = 0);
    };
}
