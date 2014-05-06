/* ////////////////////////////////////////////////////////////////////////////// */
/*
//
//              INTEL CORPORATION PROPRIETARY INFORMATION
//  This software is supplied under the terms of a license  agreement or
//  nondisclosure agreement with Intel Corporation and may not be copied
//  or disclosed except in  accordance  with the terms of that agreement.
//        Copyright (c) 2013-2013 Intel Corporation. All Rights Reserved.
//
//
*/
#pragma once

#include "mfx_ibitstream_reader.h"
#include "mfx_pcp.h"
#include <memory>

#include <map>
#include <string>
#include "vm_shared_object.h"
class SOProxy
{
public:
    SOProxy() : m_handle(NULL) 
    {}
    
    void *get(std::string function_name)
    {
        if (NULL == m_handle)
            return NULL;
        std::map<std::string, void*>::iterator it = m_ptrs.find(function_name);
        if (m_ptrs.end() == it)
            return NULL;
        return it->second;
    }

    bool load(const vm_char* so_file_name, const char** exports, mfxU32 exports_length)
    {
        vm_so_handle handle = vm_so_load(so_file_name);
        if (NULL == handle)
            return false;
        std::map<std::string, void*> ptrs;
        for (mfxU32 i = 0; i < exports_length; i++)
           ptrs[std::string(exports[i])] = vm_so_get_addr(handle, exports[i]);

        unload();
        m_handle = handle;
        m_ptrs = ptrs;
        return true;
    }

    void unload()
    {
        if (NULL != m_handle)
        {
            vm_so_free(m_handle);
            m_handle = NULL;
        }
    }

    virtual ~SOProxy() {unload();}
protected:
    vm_so_handle m_handle;
    std::map<std::string, void*> m_ptrs;
};


#define SO_CALL(method, so, on_err, arguments) NULL == (so).get( #method ) ? (on_err) : (reinterpret_cast<decltype(&method)>((so).get( #method ))) arguments
#define SO_DATA(symbol, so, on_err) NULL == (so).get( #symbol ) ? (on_err) : (*reinterpret_cast<decltype(&symbol)>((so).get( #symbol )))

class BitstreamReaderEncryptor 
    : public InterfaceProxy<IBitstreamReader>
{
    typedef InterfaceProxy<IBitstreamReader> base;
public:
    typedef struct params{
        bool isFullEncrypt;
        bool sparse;
//        pcpSessionEncrDataLayout encrDataLayout;
        params():
            isFullEncrypt(false)
            ,sparse(false)
//            ,encrDataLayout(pcpSessionEncrDataLayout_packed)
        {}
    };
public :
    BitstreamReaderEncryptor(const pcpEncryptor& encryptor, IBitstreamReader *pReader, SOProxy *pavpdll, params *par);
    virtual ~BitstreamReaderEncryptor();

    virtual void      Close();
    virtual mfxStatus Init(const vm_char *strFileName);
    virtual mfxStatus PostInit();

    virtual mfxStatus ReadNextFrame(mfxBitstream2 &pBS);
    
    mfxStatus SetEncryptor(const pcpEncryptor& encryptor)
    {
        m_encryptor = encryptor;
        return SO_CALL(PCPVideoDecode_SetEncryptor, *m_pavpdll, MFX_ERR_UNSUPPORTED, (m_sessionPCP, &encryptor));
    };
protected:
    pcpEncryptor m_encryptor;
    IBitstreamReader *m_pReader;
    SOProxy *m_pavpdll;
    params m_params;

    pcpSession m_sessionPCP;
    // processed temporary stream
    mfxBitstream *m_processedBS;
    // input bit stream
    std::auto_ptr<mfxBitstream2>  m_originalBS;

    // is stream ended
    bool m_isEndOfStream;
};