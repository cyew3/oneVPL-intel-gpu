//
// INTEL CORPORATION PROPRIETARY INFORMATION
//
// This software is supplied under the terms of a license agreement or
// nondisclosure agreement with Intel Corporation and may not be copied
// or disclosed except in accordance with the terms of that agreement.
//
// Copyright(C) 2007-2008 Intel Corporation. All Rights Reserved.
//

#if 0
#include "umc_me_m2.h"


//*******************************************************************************************************
namespace UMC
{


MeMPEG2::MeMPEG2()
{
        m_PredCalc = 0;
}

MeMPEG2::~MeMPEG2()
{
}

bool MeMPEG2::Init(MeInitParams *par)
{
  return MeBase::Init(par);
}

#define VARMEAN_INTRA(pblock, step, vardiff, meandiff, _vardiff)                 \
  ippiVarSum8x8_8u32s_C1R(pblock             , step, &vardiff[0], &meandiff[0]); \
  ippiVarSum8x8_8u32s_C1R(pblock + 8         , step, &vardiff[1], &meandiff[1]); \
  ippiVarSum8x8_8u32s_C1R(pblock + 8*step    , step, &vardiff[2], &meandiff[2]); \
  ippiVarSum8x8_8u32s_C1R(pblock + 8*step + 8, step, &vardiff[3], &meandiff[3]); \
  _vardiff = vardiff[0] + vardiff[1] + vardiff[2] + vardiff[3]

#define VARMEAN_INTER(pDiff, step, vardiff, meandiff, _vardiff)              \
  ippiVarSum8x8_16s32s_C1R(pDiff         , step, &vardiff[0], &meandiff[0]); \
  ippiVarSum8x8_16s32s_C1R(pDiff+8       , step, &vardiff[1], &meandiff[1]); \
  ippiVarSum8x8_16s32s_C1R(pDiff+4*step  , step, &vardiff[2], &meandiff[2]); \
  ippiVarSum8x8_16s32s_C1R(pDiff+4*step+8, step, &vardiff[3], &meandiff[3]); \
  _vardiff = vardiff[0] + vardiff[1] + vardiff[2] + vardiff[3]

#define TRY_MV(_bestmv, _bestcost, mvx, mvy) { \
  Ipp32s bcost; \
  ippiSAD16x16_8u32s(src, sstep, ref+((mvx)>>1)+((mvy)>>1)*rstep, rstep, \
    &bcost, (((mvx) & 1) << 3) | (((mvy) & 1) << 2)); \
  if(bcost < _bestcost) { \
    _bestcost = bcost; \
    _bestmv.x = mvx; _bestmv.y = mvy; \
    changed = 1; \
  } \
}

#define MV_IN_RANGE(mvx, mvy) \
  (bx*bw*2+((mvx)>>0)>=ll && (bx+1)*bw*2+((mvx+0)>>0) <= lr && \
   by*bh*2+((mvy)>>0)>=lt && (by+1)*bh*2+((mvy+0)>>0) <= lb )

void MeMPEG2::me_block(const Ipp8u* src, Ipp32s sstep, const Ipp8u* ref, Ipp32s rstep,
                       Ipp32s bh, Ipp32s bx, Ipp32s by,
                       Ipp32s ll, Ipp32s lt, Ipp32s lr, Ipp32s lb,
                       MeMV* mv, Ipp32s* cost, MeMV* mvleft, MeMV* mvtop)
{
  const Ipp32s bw = 16; // can be a param
  MeMV bestmv;
  Ipp32s bestcost = IPP_MAX_32S;
  Ipp32s changed = 0;
  Ipp32s step = (((bx - by) & 7) != 1) ? 2 :
    (IPP_MAX(m_par->SearchRange.x, m_par->SearchRange.y)>>3<<1);

  TRY_MV(bestmv, bestcost, 0, 0);
  mv->x &= ~1; // to full pixel
  mv->y &= ~1;
  if(MV_IN_RANGE(mv->x, mv->y)) {
    TRY_MV(bestmv, bestcost, mv->x, mv->y);
  }
  // try left and top
  if(mvleft) {
    if(MV_IN_RANGE(mvleft->x & ~1, mvleft->y & ~1)) {
      TRY_MV(bestmv, bestcost, mvleft->x & ~1, mvleft->y & ~1);
    }
  }
  if(mvtop) {
    if(MV_IN_RANGE(mvtop->x & ~1, mvtop->y & ~1)) {
      TRY_MV(bestmv, bestcost, mvtop->x & ~1, mvtop->y & ~1);
    }
  }

  while(step>=1) {
    changed = 1;
    while(changed) {
      MeMV localmv = bestmv;
      changed = 0;
      if(MV_IN_RANGE(bestmv.x+step, bestmv.y)) {
        TRY_MV(localmv, bestcost, bestmv.x+step, bestmv.y);
      }
      if(MV_IN_RANGE(bestmv.x-step, bestmv.y)) {
        TRY_MV(localmv, bestcost, bestmv.x-step, bestmv.y);
      }
      if(MV_IN_RANGE(bestmv.x, bestmv.y+step)) {
        TRY_MV(localmv, bestcost, bestmv.x, bestmv.y+step);
      }
      if(MV_IN_RANGE(bestmv.x, bestmv.y-step)) {
        TRY_MV(localmv, bestcost, bestmv.x, bestmv.y-step);
      }
      if(changed)
        bestmv = localmv;
    }
    step = (step==2) ? 1 : ((step+1)>>2<<1); // keep even till the end
  }

  *mv = bestmv;
  *cost = bestcost;

}

bool MeMPEG2::EstimateFrame(MeParams *par)
{
    MeFrame *res = par->pSrc;
    Ipp32s x, y;
    Ipp8u *src, *ref[2];
    Ipp16s bufDiff[3][256];
    Ipp32s varintra, varinter, var[4], mean[4];

    //check and save input
    m_par = par;

    if(!CheckParams())
        return false;

    //estimate

    for( y=0; y<m_HeightMB; y++){
        for( x=0; x<m_WidthMB; x++){
            Ipp32s bestcost;
            src = res->ptr[0] + res->step[0] * y * 16 + x*16;
            ref[0] = par->pRefF[0]->ptr[0] + par->pRefF[0]->step[0] * y * 16 + x*16;
            if(par->SearchDirection == ME_BidirSearch)
              ref[1] = par->pRefB[0]->ptr[0] + par->pRefB[0]->step[0] * y * 16 + x*16;
            else
              ref[1] = 0;
            m_adr = m_WidthMB*y + x;
            //m_ResMB[m_adr].skipped = 0;

            if(x>0 && x+1 < m_WidthMB) { // try to skip (MPEG only)
              if(par->SearchDirection == ME_ForwardSearch) { //P-picture
                ippiGetDiff16x16_8u16s_C1(src, res->step[0],
                  ref[0], par->pRefF[0]->step[0],
                  bufDiff[0], 32, 0, 0, 0, ippRndZero);
                VARMEAN_INTER(bufDiff[0], 32, var, mean, varinter);
                //if(varinter <= par->Quant) {
                if (var[0] <= par->Quant && abs(mean[0])<par->Quant*8 &&
                    var[1] <= par->Quant && abs(mean[1])<par->Quant*8 &&
                    var[2] <= par->Quant && abs(mean[2])<par->Quant*8 &&
                    var[3] <= par->Quant && abs(mean[3])<par->Quant*8 ) {
                  //m_ResMB[m_adr].skipped = 1;
                  m_ResMB[m_adr].MbType = ME_MbFrwSkipped;
                  m_ResMB[m_adr].MbCosts[0] = varinter;
                  m_ResMB[m_adr].MV[0][0].x = 0;
                  m_ResMB[m_adr].MV[0][0].y = 0;
                  continue;
                }
              } else if (par->SearchDirection == ME_BidirSearch) { // B picture
                if ( (m_ResMB[m_adr-1].MbType != ME_MbIntra)
                  && (!(m_ResMB[m_adr-1].MbType == ME_MbFrw || m_ResMB[m_adr-1].MbType == ME_MbBidir)
                      || x*16+(m_ResMB[m_adr-1].MV[0][0].x>>1)+16 < m_WidthMB*16)
                  && (!(m_ResMB[m_adr-1].MbType == ME_MbBkw || m_ResMB[m_adr-1].MbType == ME_MbBidir)
                      || x*16+(m_ResMB[m_adr-1].MV[1][0].x>>1)+16 < m_WidthMB*16) )
                {
                  MeMbType skiptype;
                  if (m_ResMB[m_adr-1].MbType == ME_MbFrw) {
                    ippiGetDiff16x16_8u16s_C1(src, res->step[0],
                      ref[0]+(m_ResMB[m_adr-1].MV[0][0].x>>1)+(m_ResMB[m_adr-1].MV[0][0].y>>1)*par->pRefF[0]->step[0], par->pRefF[0]->step[0],
                      bufDiff[0], 32, 0, 0,
                      ((m_ResMB[m_adr-1].MV[0][0].x & 1) << 3) | ((m_ResMB[m_adr-1].MV[0][0].y & 1) << 2), ippRndZero);
                    skiptype = ME_MbFrwSkipped;
                  } else if (m_ResMB[m_adr-1].MbType == ME_MbBkw) {
                    ippiGetDiff16x16_8u16s_C1(src, res->step[0],
                      ref[0]+(m_ResMB[m_adr-1].MV[1][0].x>>1)+(m_ResMB[m_adr-1].MV[1][0].y>>1)*par->pRefF[0]->step[0], par->pRefF[0]->step[0],
                      bufDiff[0], 32, 0, 0,
                      ((m_ResMB[m_adr-1].MV[1][0].x & 1) << 3) | ((m_ResMB[m_adr-1].MV[1][0].y & 1) << 2), ippRndZero);
                    skiptype = ME_MbBkwSkipped;
                  } else if (m_ResMB[m_adr-1].MbType == ME_MbBidir) {
                    ippiGetDiff16x16B_8u16s_C1(src, res->step[0],
                      ref[0]+(m_ResMB[m_adr-1].MV[0][0].x>>1)+(m_ResMB[m_adr-1].MV[0][0].y>>1)*par->pRefF[0]->step[0], par->pRefF[0]->step[0],
                      ((m_ResMB[m_adr-1].MV[0][0].x & 1) << 3) | ((m_ResMB[m_adr-1].MV[0][0].y & 1) << 2),
                      ref[1]+(m_ResMB[m_adr-1].MV[1][0].x>>1)+(m_ResMB[m_adr-1].MV[1][0].y>>1)*par->pRefF[0]->step[0], par->pRefF[0]->step[0],
                      ((m_ResMB[m_adr-1].MV[1][0].x & 1) << 3) | ((m_ResMB[m_adr-1].MV[1][0].y & 1) << 2),
                      bufDiff[0], 32, ippRndZero);
                    skiptype = ME_MbBidirSkipped;
                  } else {
                    VM_ASSERT(0);
                  }
                  VARMEAN_INTER(bufDiff[0], 32, var, mean, varinter);
                  //if(varinter <= par->Quant) {
                  if (var[0] <= par->Quant && abs(mean[0])<par->Quant*8 &&
                      var[1] <= par->Quant && abs(mean[1])<par->Quant*8 &&
                      var[2] <= par->Quant && abs(mean[2])<par->Quant*8 &&
                      var[3] <= par->Quant && abs(mean[3])<par->Quant*8 ) {
                    //m_ResMB[m_adr].skipped = 1;
                    m_ResMB[m_adr].MbType = skiptype;
                    m_ResMB[m_adr].MbCosts[0] = varinter;
                    m_ResMB[m_adr].MV[0][0] = m_ResMB[m_adr-1].MV[0][0];
                    m_ResMB[m_adr].MV[1][0] = m_ResMB[m_adr-1].MV[1][0];
                    continue;
                  }
                }
              }
            }
            m_BestMV[0].x = m_BestMV[0].y = 0;
            me_block(src, res->step[0], ref[0], par->pRefF[0]->step[0],
              16, x, y,
              IPP_MAX(par->PicRange.top_left.x*2, x*16*2 - par->SearchRange.x*2),
              IPP_MAX(par->PicRange.top_left.y*2, y*16*2 - par->SearchRange.y*2),
              IPP_MIN(par->PicRange.bottom_right.x*2, (x+1)*16*2 + par->SearchRange.x*2 -1),
              IPP_MIN(par->PicRange.bottom_right.y*2, (y+1)*16*2 + par->SearchRange.y*2 -1),
              &(m_BestMV[0]), &(m_BestCost[0]),
              x>0 ? &(m_ResMB[m_adr-1].MV[0][0]) : 0,
              y>0 ? &(m_ResMB[m_adr-m_HeightMB].MV[0][0]) : 0);
            m_ResMB[m_adr].MV[0][0] = m_BestMV[0];

            VARMEAN_INTRA(src, res->step[0], var, mean, varintra);

            ippiGetDiff16x16_8u16s_C1(src, res->step[0],
              ref[0]+(m_BestMV[0].x>>1)+(m_BestMV[0].y>>1)*par->pRefF[0]->step[0], par->pRefF[0]->step[0],
              bufDiff[0], 32, 0, 0,
              ((m_BestMV[0].x & 1) << 3) | ((m_BestMV[0].y & 1) << 2), ippRndZero);
            VARMEAN_INTER(bufDiff[0], 32, var, mean, varinter);

            if(varinter <= varintra) {
              m_ResMB[m_adr].MbType = ME_MbFrw;
              bestcost = varinter;
            } else {
              m_ResMB[m_adr].MbType = ME_MbIntra;
              bestcost = varintra;
            }
            m_ResMB[m_adr].MbCosts[0] = bestcost;
            if(m_ResMB[m_adr].MbType == ME_MbFrw) {
              if (var[0] <= par->Quant && abs(mean[0])<par->Quant &&
                  var[1] <= par->Quant && abs(mean[1])<par->Quant &&
                  var[2] <= par->Quant && abs(mean[2])<par->Quant &&
                  var[3] <= par->Quant && abs(mean[3])<par->Quant ) {
                continue;
              }
            }
            if(par->SearchDirection != ME_BidirSearch) {
              continue;
            }

            m_BestMV[1].x = -m_BestMV[0].x; // need to be scaled
            m_BestMV[1].y = -m_BestMV[0].y; // need to be scaled

            me_block(src, res->step[0], ref[1], par->pRefF[0]->step[0],
              16, x, y,
              IPP_MAX(par->PicRange.top_left.x*2, x*16*2 - par->SearchRange.x*2),
              IPP_MAX(par->PicRange.top_left.y*2, y*16*2 - par->SearchRange.y*2),
              IPP_MIN(par->PicRange.bottom_right.x*2, (x+1)*16*2 + par->SearchRange.x*2 -1),
              IPP_MIN(par->PicRange.bottom_right.y*2, (y+1)*16*2 + par->SearchRange.y*2 -1),
              &(m_BestMV[1]), &(m_BestCost[1]),
              x>0 ? &(m_ResMB[m_adr-1].MV[1][0]) : 0,
              y>0 ? &(m_ResMB[m_adr-m_HeightMB].MV[1][0]) : 0);
            m_ResMB[m_adr].MV[1][0] = m_BestMV[1];

            ippiGetDiff16x16_8u16s_C1(src, res->step[0],
              ref[1]+(m_BestMV[1].x>>1)+(m_BestMV[1].y>>1)*par->pRefF[0]->step[0], par->pRefF[0]->step[0],
              bufDiff[1], 32, 0, 0,
              ((m_BestMV[1].x & 1) << 3) | ((m_BestMV[1].y & 1) << 2), ippRndZero);
            VARMEAN_INTER(bufDiff[1], 32, var, mean, varinter);
// both MVs are stored in their positions!!!

            m_ResMB[m_adr].MV[1][0] = m_BestMV[1];
            if(varinter < bestcost) {
              m_ResMB[m_adr].MbType = ME_MbBkw;
              bestcost = varinter;
            }

            // try bidir now
            ippiGetDiff16x16B_8u16s_C1(src, res->step[0],
              ref[0]+(m_BestMV[0].x>>1)+(m_BestMV[0].y>>1)*par->pRefF[0]->step[0], par->pRefF[0]->step[0],
              ((m_BestMV[0].x & 1) << 3) | ((m_BestMV[0].y & 1) << 2),
              ref[1]+(m_BestMV[1].x>>1)+(m_BestMV[1].y>>1)*par->pRefF[0]->step[0], par->pRefF[0]->step[0],
              ((m_BestMV[1].x & 1) << 3) | ((m_BestMV[1].y & 1) << 2),
              bufDiff[2], 32, ippRndZero);
            VARMEAN_INTER(bufDiff[2], 32, var, mean, varinter);
            m_BestCost[2] = varinter;

            if(varinter*17 < bestcost*16) {
              m_ResMB[m_adr].MbType = ME_MbBidir;
              bestcost = varinter;
            }
            m_ResMB[m_adr].MbCosts[0] = bestcost;
         }
    }

    //save results
    res->NumOfMVs = m_NumOfMVs;
    res->MBs = m_ResMB;

    return true;
}

void MeMPEG2::Close()
{
  return MeBase::Close();
}

}
#endif
