#include "avc2_entropy.h"
#include "h264_cavlc_tables.h"

using namespace BS_AVC2;
using namespace BsReader2;

Bs16u CAVLC::vlc16(const Bs16u* t)
{
    Bs16u c = 0, n;

    for (Bs16u i = 0; i < 16; i++)
    {
        c = (c << 1)| b();
        n = *(t++);

        while (n--)
        {
            if (c == t[0])
                return t[1];

            t += 2;
        }
    }

    throw InvalidSyntax();
}

Bs8u mapBlkType(Bs8u t, Bs8u pm0)
{
    if (pm0 == Intra_16x16)
    {
        if(t & BLK_Cb)     return CbIntra16x16ACLevel;
        if(t & BLK_Cr)     return CrIntra16x16ACLevel;
        if(t & BLK_Chroma) return ChromaACLevel;
        return Intra16x16ACLevel;
    }

    switch (t)
    {
    case Intra16x16DCLevel:   return   LumaLevel4x4;
    case CbIntra16x16DCLevel: return CbLumaLevel4x4;
    case CrIntra16x16DCLevel: return CrLumaLevel4x4;
    default: return t;
    }
}


Bs16u getCT(CoeffTokens& ct, Bs16u blkType, Bs16u blkIdx, bool isCb)
{
    switch (blkType)
    {
    case Intra16x16DCLevel  : return ct.lumaDC;
    case CbIntra16x16DCLevel: return ct.CbDC;
    case CrIntra16x16DCLevel: return ct.CrDC;
    case Intra16x16ACLevel  : return ct.luma4x4[blkIdx];
    case LumaLevel4x4       : return ct.luma4x4[blkIdx];
    case CbIntra16x16ACLevel: return ct.Cb4x4[blkIdx];
    case CbLumaLevel4x4     : return ct.Cb4x4[blkIdx];
    case CrIntra16x16ACLevel: return ct.Cr4x4[blkIdx];
    case CrLumaLevel4x4     : return ct.Cr4x4[blkIdx];
    case ChromaDCLevel      : 
        if (isCb) return ct.CbDC;
        return ct.CrDC;
    case ChromaACLevel      : 
        if (isCb) return ct.Cb4x4[blkIdx];
        return ct.Cr4x4[blkIdx]; 
    default: return 0;
    }
}

Bs16u CAVLC::CoeffToken(Bs8u blkType, Bs8u blkIdx, bool isCb)
{
    Bs16s nC = -7;

    if (blkType == ChromaDCLevel)
        nC = -m_info->ChromaArrayType;
    else
    {
        MB& cMB = m_info->cMB();
        BlkLoc blkA, blkB;
        bool availableFlagA = false;
        bool availableFlagB = false;
        Bs16s nA = -1;
        Bs16s nB = -1;

        switch (blkType)
        {
        case Intra16x16DCLevel: blkIdx = 0;
        case Intra16x16ACLevel:
        case LumaLevel4x4:
            //6.4.11.4
            blkA = m_info->neighb4x4YblkA(cMB, blkIdx);
            blkB = m_info->neighb4x4YblkB(cMB, blkIdx);
            break;
        case CbIntra16x16DCLevel: blkIdx = 0;
        case CbIntra16x16ACLevel:
        case CbLumaLevel4x4:
            //6.4.11.6
            blkA = m_info->neighb4x4YblkA(cMB, blkIdx);
            blkB = m_info->neighb4x4YblkB(cMB, blkIdx);
            break;
        case CrIntra16x16DCLevel: blkIdx = 0;
        case CrIntra16x16ACLevel:
        case CrLumaLevel4x4:
            //6.4.11.6
            blkA = m_info->neighb4x4YblkA(cMB, blkIdx);
            blkB = m_info->neighb4x4YblkB(cMB, blkIdx);
            break;
        case ChromaACLevel:
            //6.4.11.5
            blkA = m_info->neighb4x4CblkA(cMB, blkIdx);
            blkB = m_info->neighb4x4CblkB(cMB, blkIdx);
            break;
        default:
            throw InvalidSyntax();
        }

        availableFlagA = !(
            blkA.addr < 0
            //TBD: sd part check 
            );

        availableFlagB = !(
            blkB.addr < 0
            //TBD: sd part check 
            );

        if (availableFlagA)
        {
            MB& mbN = m_info->MBRef(blkA.addr);
            Bs8u pm0 = m_info->MbPartPredMode(cMB, 0);

            if (mbN.mb_skip_flag)
                nA = 0;
            else if (I_PCM == mbN.MbType)
                nA = 16;
            else
                nA = TotalCoeff(getCT(mbN.ct, mapBlkType(blkType, pm0), blkA.idx, isCb));
        }

        if (availableFlagB)
        {
            MB& mbN = m_info->MBRef(blkB.addr);
            Bs8u pm0 = m_info->MbPartPredMode(cMB, 0);

            if (mbN.mb_skip_flag)
                nB = 0;
            else if (I_PCM == mbN.MbType)
                nB = 16;
            else
                nB = TotalCoeff(getCT(mbN.ct, mapBlkType(blkType, pm0), blkB.idx, isCb));
        }

        if (availableFlagA && availableFlagB)
            nC = ((nA + nB + 1) >> 1);
        else if (availableFlagA)
            nC = nA;
        else if (availableFlagB)
            nC = nB;
        else
            nC = 0;
    }

    if (nC < -2)
        throw InvalidSyntax();

    switch (nC)
    {
    case -2: return vlc16(vlcCoeffTokenM2);
    case -1: return vlc16(vlcCoeffTokenM1);
    case  0:
    case  1: return vlc16(vlcCoeffToken02);
    case  2:
    case  3: return vlc16(vlcCoeffToken24);
    case  4:
    case  5:
    case  6:
    case  7: return vlc16(vlcCoeffToken48);
    default: return vlc16(vlcCoeffToken8x);
    }
}

Bs16u CAVLC::LevelPrefix()
{
    Bs16u v = 0;

    while (!b())
        v++;

    return v;
}

Bs16u CAVLC::LevelSuffix(Bs16u l, Bs16u p)
{
    if (l == 0 && p == 14)
        l = 4;
    else if (p >= 15)
        l = p - 3;
    return (l ? b(l) : 0);
}

Bs16u CAVLC::TotalZeros(Bs16u maxNumCoeff, Bs16u tzVlcIndex)
{
    if (maxNumCoeff == 4)
        return vlc16(vlcTotalZeros2x2[tzVlcIndex]);

    if (maxNumCoeff == 8)
        return vlc16(vlcTotalZeros2x4[tzVlcIndex]);

    return vlc16(vlcTotalZeros4x4[tzVlcIndex]);
}

static const Bs8u meMapCat12[48][2] =
{
    { 47,  0 },    { 31, 16 },    { 15,  1 },    {  0,  2 },
    { 23,  4 },    { 27,  8 },    { 29, 32 },    { 30,  3 },
    {  7,  5 },    { 11, 10 },    { 13, 12 },    { 14, 15 },
    { 39, 47 },    { 43,  7 },    { 45, 11 },    { 46, 13 },
    { 16, 14 },    {  3,  6 },    {  5,  9 },    { 10, 31 },
    { 12, 35 },    { 19, 37 },    { 21, 42 },    { 26, 44 },
    { 28, 33 },    { 35, 34 },    { 37, 36 },    { 42, 40 },
    { 44, 39 },    {  1, 43 },    {  2, 45 },    {  4, 46 },
    {  8, 17 },    { 17, 18 },    { 18, 20 },    { 20, 24 },
    { 24, 19 },    {  6, 21 },    {  9, 26 },    { 22, 28 },
    { 25, 23 },    { 32, 27 },    { 33, 29 },    { 34, 30 },
    { 36, 22 },    { 40, 25 },    { 38, 38 },    { 41, 41 },
};

static const Bs8u meMapCat03[16][2] =
{
    { 15,  0 },    {  0,  1 },    {  7,  2 },    { 11,  4 },
    { 13,  8 },    { 14,  3 },    {  3,  5 },    {  5, 10 },
    { 10, 12 },    { 12, 15 },    {  1,  7 },    {  2, 11 },
    {  4, 13 },    {  8, 14 },    {  6,  6 },    {  9,  9 },
};

Bs8u CAVLC::CBP()
{
    Bs8u pm0 = m_info->MbPartPredMode(m_info->cMB(), 0);
    Bs8u ChromaArrayType = m_info->ChromaArrayType;
    Bs32u codeNum = m_bs.GetUE();

    if ((ChromaArrayType == 1 || ChromaArrayType == 2) && codeNum < 48)
        return meMapCat12[codeNum][pm0 >= Pred_L0];

    if ((ChromaArrayType == 0 || ChromaArrayType == 3) && codeNum < 16)
        return meMapCat03[codeNum][pm0 >= Pred_L0];

    throw InvalidSyntax();
}