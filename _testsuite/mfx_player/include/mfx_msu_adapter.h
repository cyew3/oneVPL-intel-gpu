#pragma once

//TODO: should we copy this code to remove rependencies???
#define CYUVReader  CYUVReader2
#define main main2
#include "metrics_calc_lite.cpp"

//adaptee to msu interface for metrics calc
class CReaderFromMfxSurfaces
    : public CReader
{
public:
    //custom data push
    virtual void PutSurface(mfxFrameSurface1 * pSurface)
    {
        m_pSurface = pSurface;
    }
    //msu interface
    virtual int  OpenReadFile(std::string /*name*/, unsigned int /*w*/, unsigned int /*h*/, ESequenceType /*type*/, int /*order*/)
    {
        return 0;
    }
    virtual bool ReadRawFrame(unsigned int /*field*/)
    {
        return NULL != m_pSurface;
    }
    virtual void GetFrame(int /*idx*/, SImage *dst)
    {
        
    }
protected:
    mfxFrameSurface1 *m_pSurface;

};

