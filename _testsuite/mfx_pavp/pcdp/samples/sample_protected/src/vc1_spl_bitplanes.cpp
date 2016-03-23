//* ////////////////////////////////////////////////////////////////////////////// */
//*
//
//              INTEL CORPORATION PROPRIETARY INFORMATION
//  This software is supplied under the terms of a license  agreement or
//  nondisclosure agreement with Intel Corporation and may not be copied
//  or disclosed except in  accordance  with the terms of that agreement.
//        Copyright (c) 2010-2016 Intel Corporation. All Rights Reserved.
//
//
//*/


#include "vc1_spl_header_parser.h"
#include <assert.h>
#include <string.h>

namespace ProtectedLibrary
{

void VC1_SPL_Headers::DecodeBitplane(VC1SplBitplane* pBitplane, mfxI32 width, mfxI32 height,mfxI32 offset)
{
    mfxI32 tmp;
    mfxI32 i, j;
    mfxI32 tmp_invert = 0;
    mfxI32 tmp_databits = 0;

    memset(pBitplane, 0, sizeof(VC1SplBitplane));
    if (offset == 0)
        ++SH.bp_round_count;
    if (VC1_MAX_BITPANE_CHUNCKS == SH.bp_round_count)
        SH.bp_round_count = 0;

    mfxU32 HeightMB = SH.heightMB;
    // Special case for Fields
    if (SH.IsResize &&PicH.FCM != VC1_FieldInterlace)
    {
        HeightMB -= 1;
        height -= 1;
    }

    if (SH.bp_round_count >= 0)
        pBitplane->m_databits = SH.pBitplane.m_databits + HeightMB*SH.widthMB * SH.bp_round_count + offset;
    else
        pBitplane->m_databits = SH.pBitplane.m_databits - HeightMB * SH.widthMB + offset;

    VC1_SPL_GET_BITS(1, tmp_invert);
    pBitplane->m_invert = (mfxU8)tmp_invert;

     VC1_SPL_GET_BITS(1, tmp);
     if(tmp == 1)
     {
         VC1_SPL_GET_BITS(1, tmp);
         if(tmp == 0)
         {
             pBitplane->m_imode = VC1_BITPLANE_NORM2_MODE;
         }
         else
         {
             pBitplane->m_imode = VC1_BITPLANE_NORM6_MODE;
         }
     }
     else
     {
         VC1_SPL_GET_BITS(2, tmp);
         if(tmp == 2)
         {
             pBitplane->m_imode = VC1_BITPLANE_ROWSKIP_MODE;
         }
         else if (tmp == 1)
         {
             pBitplane->m_imode = VC1_BITPLANE_DIFF2_MODE;
         }
         else if (tmp == 3)
         {
             pBitplane->m_imode = VC1_BITPLANE_COLSKIP_MODE;
         }
         else
         {
             VC1_SPL_GET_BITS(1, tmp);
             if(tmp == 1)
             {
                 pBitplane->m_imode = VC1_BITPLANE_DIFF6_MODE;
             }
             else
             {
                 pBitplane->m_imode = VC1_BITPLANE_RAW_MODE;
             }
         }

     }

    switch(pBitplane->m_imode)
    {
    case VC1_BITPLANE_RAW_MODE:
        break;
    case VC1_BITPLANE_NORM2_MODE:
        Norm2ModeDecode(pBitplane, width, height);

        if(pBitplane->m_invert)
        {
            InverseBitplane(pBitplane, width*height);
        }

        break;
    case VC1_BITPLANE_DIFF2_MODE:
        Norm2ModeDecode(pBitplane, width, height);
        InverseDiff(pBitplane, width, height);

        break;
    case VC1_BITPLANE_NORM6_MODE:
        Norm6ModeDecode(pBitplane, width, height);

        if(pBitplane->m_invert)
        {
            InverseBitplane(pBitplane, width*height);
        }
        break;
    case VC1_BITPLANE_DIFF6_MODE:
        Norm6ModeDecode(pBitplane, width, height);
        InverseDiff(pBitplane, width, height);

        break;
    case VC1_BITPLANE_ROWSKIP_MODE:
        for(i = 0; i < height; i++)
        {
            VC1_SPL_GET_BITS(1, tmp);
            if(tmp == 0)
            {
                for(j = 0; j < width; j++)
                    pBitplane->m_databits[width*i + j] = 0;
            }
            else
            {
                for(j = 0; j < width; j++)
                {
                    VC1_SPL_GET_BITS(1, tmp_databits);
                    pBitplane->m_databits[width*i + j] = (mfxU8)tmp_databits;
                }
            }
        }
        if(pBitplane->m_invert)
        {
            InverseBitplane(pBitplane, width*height);
        }

        break;
    case VC1_BITPLANE_COLSKIP_MODE:
        for(i = 0; i < width; i++)
        {
            VC1_SPL_GET_BITS(1, tmp);
            if(tmp == 0)
            {
                for(j = 0; j < height; j++)
                    pBitplane->m_databits[i + j*width] = 0;
            }
            else
            {
                for(j = 0; j < height; j++)
                {
                    VC1_SPL_GET_BITS(1, tmp_databits);
                    pBitplane->m_databits[i + j*width] = (mfxU8)tmp_databits;
                }
            }
        }
        if(pBitplane->m_invert)
        {
            InverseBitplane(pBitplane, width*height);
        }
        break;

    }
}

void VC1_SPL_Headers::InverseDiff(VC1SplBitplane* pBitplane, mfxI32 widthMB, mfxI32 heightMB)
{
    mfxI32 i, j;

    for(i = 0; i < heightMB; i++)
    {
        for(j = 0; j < widthMB; j++)
        {
            if((i == 0 && j == 0))
            {
                pBitplane->m_databits[i*widthMB + j] = pBitplane->m_databits[i*widthMB + j] ^ pBitplane->m_invert;
            }
            else if(j == 0)
            {
                pBitplane->m_databits[i*widthMB + j] = pBitplane->m_databits[i*widthMB + j] ^pBitplane->m_databits[widthMB*(i-1)];
            }
            else if(((i>0) && (pBitplane->m_databits[i*widthMB+j-1] != pBitplane->m_databits[(i-1)*widthMB+j])))
            {
                pBitplane->m_databits[i*widthMB + j] = pBitplane->m_databits[i*widthMB + j] ^ pBitplane->m_invert;
            }
            else
            {
                pBitplane->m_databits[i*widthMB + j] = pBitplane->m_databits[i*widthMB + j] ^ pBitplane->m_databits[i*widthMB + j - 1];
            }
        }
    }

}


void VC1_SPL_Headers::InverseBitplane(VC1SplBitplane* pBitplane, mfxI32 size)
{
    mfxI32 i;

    for(i = 0; i < size; i++)
    {
        pBitplane->m_databits[i] = pBitplane->m_databits[i] ^ 1;
    }
}


void VC1_SPL_Headers::Norm2ModeDecode(VC1SplBitplane* pBitplane, mfxI32 width, mfxI32 height)
{
    mfxI32 i;
    mfxI32 tmp_databits = 0;

    if((width*height) & 1)
    {
        VC1_SPL_GET_BITS(1, tmp_databits);
        pBitplane->m_databits[0] = (mfxU8)tmp_databits;
    }

    for(i = (width*height) & 1; i < (width*height/2)*2; i+=2)
    {
        mfxI32 tmp;
        VC1_SPL_GET_BITS(1, tmp);
        if(tmp == 0)
        {
            pBitplane->m_databits[i]   = 0;
            pBitplane->m_databits[i+1] = 0;
        }
        else
        {
            VC1_SPL_GET_BITS(1, tmp);
            if(tmp == 1)
            {
                pBitplane->m_databits[i]   = 1;
                pBitplane->m_databits[i+1] = 1;
            }
            else
            {
                VC1_SPL_GET_BITS(1, tmp);
                if(tmp == 0)
                {
                    pBitplane->m_databits[i]   = 1;
                    pBitplane->m_databits[i+1] = 0;
                }
                else
                {
                    pBitplane->m_databits[i]   = 0;
                    pBitplane->m_databits[i+1] = 1;
                }
            }
        }
    }
}

void VC1_SPL_Headers::Norm6ModeDecode(VC1SplBitplane* pBitplane, mfxI32 width, mfxI32 height)
{
    mfxI32 i, j;
    mfxI32 k = -1;
    mfxI32 ResidualX = 0;
    mfxI32 ResidualY = 0;
    mfxI32 tmp = 0;
    mfxU8 _2x3tiled = (((width%3)!=0)&&((height%3)==0));


    if(_2x3tiled)
    {
        mfxI32 sizeW = width/2;
        mfxI32 sizeH = height/3;
        mfxU8 *currRowTails =  pBitplane->m_databits;

        for(i = 0; i < sizeH; i++)
        {
            currRowTails =  &pBitplane->m_databits[i*3*width];
            currRowTails += width&1;

            for(j = 0; j < sizeW; j++)
            {
                VC1_SPL_GET_BITS(1, tmp);
                if(tmp == 1)
                    k = 0;
                else
                {
                    VC1_SPL_GET_BITS(3, tmp);
                    switch(tmp)
                    {
                        case 2: k = 1;   break;
                        case 3: k = 2;   break;
                        case 4: k = 4;   break;
                        case 5: k = 8;   break;
                        case 6: k = 16;  break;
                        case 7: k = 32;  break;
                        case 0:
                            VC1_SPL_GET_BITS(4, tmp);
                            switch(tmp)
                            {
                                case 1: k = 5;  break;
                                case 2: k = 6;  break;
                                case 3: k = 9;  break;
                                case 4: k= 10;  break;
                                case 5: k = 12; break;
                                case 0: k = 3;  break;
                                case 6: k = 17; break;
                                case 7: k = 18; break;
                                case 9: k = 24; break;
                                case 10: k = 33;break;
                                case 11: k = 34; break;
                                case 8: k = 20;  break;
                                case 12: k = 36; break;
                                case 13: k = 40; break;
                                case 14: k = 48; break;
                                default: assert(0); break;
                            }
                            break;
                        case 1:
                            VC1_SPL_GET_BITS(1, tmp);
                            if(tmp == 0)
                            {
                                VC1_SPL_GET_BITS(5, tmp);
                                switch(tmp)
                                {
                                    case 7: k = 7;  break;
                                    case 11: k = 11;  break;
                                    case 13: k = 13;  break;
                                    case 14: k = 14;  break;
                                    case 19: k = 19;  break;
                                    case 21: k = 21;  break;
                                    case 22: k = 22;  break;
                                    case 25: k = 25;  break;
                                    case 26: k = 26;  break;
                                    case 28: k = 28;  break;
                                    case 3: k = 35;  break;
                                    case 5: k = 37;  break;
                                    case 6: k = 38;  break;
                                    case 9: k = 41;  break;
                                    case 10: k = 42;  break;
                                    case 12: k = 44;  break;
                                    case 17: k = 49;  break;
                                    case 18: k = 50;  break;
                                    case 20: k = 52;  break;
                                    case 24: k = 56;  break;
                                    default: assert(0); break;
                                }
                            }
                            else
                            {
                                VC1_SPL_GET_BITS(1, tmp);
                                if(tmp == 1)
                                {
                                    k = 63;
                                }
                                else
                                {
                                    VC1_SPL_GET_BITS(3, tmp);
                                    switch(tmp)
                                    {
                                        case 7: k = 31;  break;
                                        case 6: k = 47;  break;
                                        case 5: k = 55;  break;
                                        case 4: k = 59;  break;
                                        case 3: k = 61;  break;
                                        case 2: k = 62;  break;
                                        case 0:
                                            VC1_SPL_GET_BITS(4, tmp);
                                            switch(tmp)
                                            {
                                                case 14: k = 15;  break;
                                                case 13: k = 23;  break;
                                                case 12: k = 27;  break;
                                                case 11: k = 29;  break;
                                                case 10: k = 30;  break;
                                                case 9: k = 39;  break;
                                                case 8: k = 43;  break;
                                                case 7: k = 45;  break;
                                                case 6: k = 46;  break;
                                                case 5: k = 51;  break;
                                                case 4: k = 53;  break;
                                                case 3: k = 54;  break;
                                                case 2: k = 57;  break;
                                                case 1: k = 58;  break;
                                                case 0: k = 60;  break;
                                                default : assert(0);  break;
                                            }
                                            break;
                                        default : assert(0);  break;
                                    }
                                }
                            }
                            break;
                        default: assert(0); break;
                    }
                }

                currRowTails[0] = (mfxU8)(k&1);
                currRowTails[1] = (mfxU8)((k&2)>>1);

                currRowTails[width + 0] = (mfxU8)((k&4)>>2);
                currRowTails[width + 1] = (mfxU8)((k&8)>>3);

                currRowTails[2*width + 0] = (mfxU8)((k&16)>>4);
                currRowTails[2*width + 1] = (mfxU8)((k&32)>>5);

                currRowTails+=2;

             }
        }
        ResidualX = width & 1;
        ResidualY = 0;
    }
    else //3x2 tiled
    {
        mfxI32 sizeW = width/3;
        mfxI32 sizeH = height/2;
        mfxU8 *currRowTails =  pBitplane->m_databits;

        for(i = 0; i < sizeH; i++)
        {
            currRowTails =  &pBitplane->m_databits[i*2*width];

            currRowTails += width%3;
            currRowTails += (height&1)*width;

            for(j = 0; j < sizeW; j++)
            {
               VC1_SPL_GET_BITS(1, tmp);
                if(tmp == 1)
                    k = 0;
                else
                {
                    VC1_SPL_GET_BITS(3, tmp);
                    switch(tmp)
                    {
                        case 2: k = 1;   break;
                        case 3: k = 2;   break;
                        case 4: k = 4;   break;
                        case 5: k = 8;   break;
                        case 6: k = 16;  break;
                        case 7: k = 32;  break;
                        case 0:
                            VC1_SPL_GET_BITS(4, tmp);
                            switch(tmp)
                            {
                                case 1: k = 5;  break;
                                case 2: k = 6;  break;
                                case 3: k = 9;  break;
                                case 4: k= 10;  break;
                                case 5: k = 12; break;
                                case 0: k = 3;  break;
                                case 6: k = 17; break;
                                case 7: k = 18; break;
                                case 9: k = 24; break;
                                case 10: k = 33;break;
                                case 11: k = 34; break;
                                case 8: k = 20;  break;
                                case 12: k = 36; break;
                                case 13: k = 40; break;
                                case 14: k = 48; break;
                                default: assert(0); break;
                            }
                            break;
                        case 1:
                            VC1_SPL_GET_BITS(1, tmp);
                            if(tmp == 0)
                            {
                                VC1_SPL_GET_BITS(5, tmp);
                                switch(tmp)
                                {
                                    case 7: k = 7;  break;
                                    case 11: k = 11;  break;
                                    case 13: k = 13;  break;
                                    case 14: k = 14;  break;
                                    case 19: k = 19;  break;
                                    case 21: k = 21;  break;
                                    case 22: k = 22;  break;
                                    case 25: k = 25;  break;
                                    case 26: k = 26;  break;
                                    case 28: k = 28;  break;
                                    case 3: k = 35;  break;
                                    case 5: k = 37;  break;
                                    case 6: k = 38;  break;
                                    case 9: k = 41;  break;
                                    case 10: k = 42;  break;
                                    case 12: k = 44;  break;
                                    case 17: k = 49;  break;
                                    case 18: k = 50;  break;
                                    case 20: k = 52;  break;
                                    case 24: k = 56;  break;
                                    default: assert(0); break;
                                }
                            }
                            else
                            {
                                VC1_SPL_GET_BITS(1, tmp);
                                if(tmp == 1)
                                {
                                    k = 63;
                                }
                                else
                                {
                                    VC1_SPL_GET_BITS(3, tmp);
                                    switch(tmp)
                                    {
                                        case 7: k = 31;  break;
                                        case 6: k = 47;  break;
                                        case 4: k = 59;  break;
                                        case 3: k = 61;  break;
                                        case 2: k = 62;  break;
                                        case 5: k = 55;  break;
                                        case 0:
                                            VC1_SPL_GET_BITS(4, tmp);
                                            switch(tmp)
                                            {
                                                case 14: k = 15;  break;
                                                case 13: k = 23;  break;
                                                case 12: k = 27;  break;
                                                case 11: k = 29;  break;
                                                case 10: k = 30;  break;
                                                case 9: k = 39;  break;
                                                case 8: k = 43;  break;
                                                case 7: k = 45;  break;
                                                case 6: k = 46;  break;
                                                case 5: k = 51;  break;
                                                case 4: k = 53;  break;
                                                case 3: k = 54;  break;
                                                case 2: k = 57;  break;
                                                case 1: k = 58;  break;
                                                case 0: k = 60;  break;
                                                default : assert(0);  break;
                                            }
                                            break;
                                        default : assert(0);  break;
                                    }
                                }
                            }
                            break;
                        default: assert(0); break;
                    }
                }

                currRowTails[0] = (mfxU8)(k&1);
                currRowTails[1] = (mfxU8)((k&2)>>1);
                currRowTails[2] = ((mfxU8)(k&4)>>2);

                currRowTails[width + 0] = (mfxU8)((k&8)>>3);
                currRowTails[width + 1] = (mfxU8)((k&16)>>4);
                currRowTails[width + 2] = (mfxU8)((k&32)>>5);

                currRowTails+=3;

             }
        }
        ResidualX = width % 3;
        ResidualY = height & 1;
    }

    for(i = 0; i < ResidualX; i++)
    {
        mfxI32 ColSkip;
        VC1_SPL_GET_BITS(1, ColSkip);

        if(1 == ColSkip)
        {
            for(j = 0; j < height; j++)
            {
                mfxI32 Value = 0;
                VC1_SPL_GET_BITS(1, Value);
                pBitplane->m_databits[i + width * j] = (mfxU8)Value;
            }
        }
        else
        {
            for(j = 0; j < height; j++)
            {
                pBitplane->m_databits[i + width * j] = 0;
            }
        }
    }

    for(j = 0; j < ResidualY; j++)
    {
        mfxI32 RowSkip;

        VC1_SPL_GET_BITS(1, RowSkip);

        if(1 == RowSkip)
        {
            for(i = ResidualX; i < width; i++)
            {
                mfxI32 Value = 0;
                VC1_SPL_GET_BITS(1, Value);
                pBitplane->m_databits[i] = (mfxU8)Value;
            }
        }
        else
        {
            for(i = ResidualX; i < width; i++)
            {
                pBitplane->m_databits[i] = 0;
            }
        }
    }
}

}