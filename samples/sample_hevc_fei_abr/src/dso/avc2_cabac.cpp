#include "avc2_entropy.h"
#include "h264_cabac_tables.h"

using namespace BS_AVC2;
using namespace BsReader2;

// ADE ////////////////////////////////////////////////////////////////////////
static const Bs8u renormTab[32] =
{
    6, 5, 4, 4, 3, 3, 3, 3,
    2, 2, 2, 2, 2, 2, 2, 2,
    1, 1, 1, 1, 1, 1, 1, 1,
    1, 1, 1, 1, 1, 1, 1, 1
};

Bs8u ADE::DecodeDecision(Bs8u& ctxState)
{
#if (BS_AVC2_ADE_MODE == 1)
    Bs32u vMPS = (ctxState & 1);
    Bs32u bin = vMPS;
    Bs32u state = (ctxState >> 1);
    Bs32u rLPS = rangeTabLPS[state][(m_range >> 6) & 3];

    m_range -= rLPS;

    if (m_val < (m_range << m_bits))
    {
        ctxState = (transIdxMPS[state] << 1) | vMPS;

        if (m_range >= 256)
            return bin;

        m_range <<= 1;
        m_bits--;
    }
    else
    {
        Bs8u renorm = renormTab[(rLPS>>3) & 0x1F];
        m_val -= (m_range << m_bits);
        m_range = (rLPS << renorm);
        m_bits -= renorm;

        bin ^= 1;
        if (!state)
            vMPS ^= 1;

        ctxState = (transIdxLPS[state] << 1) | vMPS;
    }

    if (m_bits > 0)
        return bin;

    m_val   = (m_val << 16) | B(2);
    m_bits += 16;

    return bin;
#else
    Bs8u  binVal = 0;
    Bs8u  valMPS = (ctxState & 1);
    Bs8u  pStateIdx = (ctxState >> 1);
    Bs16u codIRangeLPS = rangeTabLPS[pStateIdx][(m_codIRange >> 6) & 3];

    m_codIRange -= codIRangeLPS;

    if (m_codIOffset >= m_codIRange)
    {
        binVal = !valMPS;
        m_codIOffset -= m_codIRange;
        m_codIRange = codIRangeLPS;

        if (pStateIdx == 0)
            valMPS = !valMPS;

        pStateIdx = transIdxLPS[pStateIdx];

    }
    else
    {
        binVal = valMPS;
        pStateIdx = transIdxMPS[pStateIdx];
    }

    ctxState = (pStateIdx << 1) | valMPS;

    while (m_codIRange < 256)
    {
        m_codIRange <<= 1;
        m_codIOffset = (m_codIOffset << 1) | b();
    }

    return binVal;
#endif
}

Bs8u ADE::DecodeBypass()
{
#if (BS_AVC2_ADE_MODE == 1)
    if (!--m_bits)
    {
        m_val  = (m_val << 16) | B(2);
        m_bits = 16;
    }

    Bs32s val = m_val - (m_range << m_bits);

    if (val < 0)
        return 0;

    m_val = Bs32u(val);
    return 1;
#else
    m_codIOffset = (m_codIOffset << 1) | b();
    //nBits++;

    if (m_codIOffset >= m_codIRange)
    {
        m_codIOffset -= m_codIRange;
        return 1;
    }
    return 0;
#endif
}

Bs8u ADE::DecodeTerminate()
{
#if (BS_AVC2_ADE_MODE == 1)
    Bs32u range = m_range - 2;
    Bs32s val = m_val - (range << m_bits);

    if (val < 0)
    {
        if (range >= 256)
        {
            m_range = range;
            return 0;
        }

        m_range = (range << 1);

        if (--m_bits)
            return 0;

        m_val  = (m_val << 16) | B(2);
        m_bits = 16;

        return 0;
    }

    m_range = range;

    return 1;
#else
    m_codIRange -= 2;

    if (m_codIOffset >= m_codIRange)
        return 1;

    while (m_codIRange < 256)
    {
        m_codIRange <<= 1;
        m_codIOffset = (m_codIOffset << 1) | b();
    }

    return 0;
#endif
}

// Context ////////////////////////////////////////////////////////////////////
#define Clip3(_min, _max, _x) BS_MIN(BS_MAX(_min, _x), _max)

inline void InitCtx(Bs8u& dst, const Bs8s* src, const Bs16u SliceQPy)
{
    Bs16s m = src[0];
    Bs16s n = src[1];
    Bs16s preCtxState = Clip3(1, 126, ((m * Clip3(0, 51, SliceQPy)) >> 4) + n);

    if (preCtxState <= 63)
        dst = ((63 - preCtxState) << 1);
    else
        dst = ((preCtxState - 64) << 1) | 1;
}

void CABAC::Init(Slice& s)
{
    Bs16u SliceQPy = 26 + s.pps->pic_init_qp_minus26 + s.slice_qp_delta;
    Bs16u InitIdc = s.cabac_init_idc + !((s.slice_type % 5) == SLICE_I || (s.slice_type % 5) == SLICE_SI);
    Bs16u i = 0, j = 0;

    if (InitIdc > 4 || s.cabac_init_idc > 2)
        throw InvalidSyntax();

    for (; i <= 10; i++)
        InitCtx(m_ctxState[i], CtxInitTable_0_10_x1[i], SliceQPy);

    for (j = 0; i <= 59; i++, j++)
        InitCtx(m_ctxState[i], CtxInitTable_11_59_x3[j][s.cabac_init_idc], SliceQPy);

    for (j = 0; i <= 69; i++, j++)
        InitCtx(m_ctxState[i], CtxInitTable_60_69_x1[j], SliceQPy);

    for (j = 0; i <= 275; i++, j++)
        InitCtx(m_ctxState[i], CtxInitTable_70_275_x4[j][InitIdc], SliceQPy);

    m_ctxState[i++] = (63 << 1);

    for (j = 0; i <= 1023; i++, j++)
        InitCtx(m_ctxState[i], CtxInitTable_277_1023_x4[j][InitIdc], SliceQPy);

    InitADE();

    BinCount = 0;
}


// Binarization ///////////////////////////////////////////////////////////////

struct Bin
{
    Bs32u b;
    Bs32u n;
};

struct BinPair
{
    Bin     b[2];
    Bs32s   v;
};

Bin& operator+= (Bin& b, Bs8u v)
{
    b.b = (b.b << 1) | !!v;
    b.n++;
    return b;
}

inline void UBin(Bin& b, Bs32u v)
{
    b.n = v + 1;
    b.b = ((0xFFFFFFFF >> (32 - b.n)) << 1);
}

inline void TUBin(Bin& b, Bs32u v, Bs32u cMax)
{
    if (v == cMax)
    {
        b.n = cMax;
        b.b = (0xFFFFFFFF >> (32 - b.n));
        return;
    }

    UBin(b, v);
}

bool UEGkP(Bin& p, Bs32s synElVal, Bs32u uCoff, bool signedValFlag)
{
    TUBin(p, std::min<Bs32u>(uCoff, BS_ABS(synElVal)), uCoff);

    if (!signedValFlag && !(p.n == uCoff && (p.b & 1)))
        return false;

    if (signedValFlag && p.n == 1 && p.b == 0)
        return false;

    return true;
}

void UEGkS(Bin& s, Bs8u k, Bs32s synElVal, Bs32u uCoff, bool signedValFlag)
{
    s.b = 0;
    s.n = 0;

    if ((Bs32u)BS_ABS(synElVal) >= uCoff)
    {
        Bs32s sufS = BS_ABS(synElVal) - uCoff;

        for (;;)
        {
            if (sufS >= (1 << k))
            {
                s += 1;
                sufS = sufS - (1 << k);
                k++;
            }
            else
            {
                s += 0;
                while (k--)
                    s += ((sufS >> k) & 1);
                break;
            }
        }
    }

    if (signedValFlag && synElVal != 0)
        s += (synElVal <= 0);
}

inline void UEGk(BinPair& bp, Bs8u k, Bs32s synElVal, Bs32u uCoff, bool signedValFlag)
{
    bp.v = synElVal;
    if (UEGkP(bp.b[0], synElVal, uCoff, signedValFlag))
        UEGkS(bp.b[1], k, synElVal, uCoff, signedValFlag);
    else
        bp.b[1].b = bp.b[1].n = 0;
}

const BinPair BinFL1[2] =
{
    { { { 0x00000000,  1 }, { 0x00000000,  0 } },        0 },
    { { { 0x00000001,  1 }, { 0x00000000,  0 } },        1 },
};

const BinPair BinRefIdxLx[32] =
{
    { { { 0x00000000,  1 }, { 0x00000000,  0 } },        0 },
    { { { 0x00000002,  2 }, { 0x00000000,  0 } },        1 },
    { { { 0x00000006,  3 }, { 0x00000000,  0 } },        2 },
    { { { 0x0000000E,  4 }, { 0x00000000,  0 } },        3 },
    { { { 0x0000001E,  5 }, { 0x00000000,  0 } },        4 },
    { { { 0x0000003E,  6 }, { 0x00000000,  0 } },        5 },
    { { { 0x0000007E,  7 }, { 0x00000000,  0 } },        6 },
    { { { 0x000000FE,  8 }, { 0x00000000,  0 } },        7 },
    { { { 0x000001FE,  9 }, { 0x00000000,  0 } },        8 },
    { { { 0x000003FE, 10 }, { 0x00000000,  0 } },        9 },
    { { { 0x000007FE, 11 }, { 0x00000000,  0 } },       10 },
    { { { 0x00000FFE, 12 }, { 0x00000000,  0 } },       11 },
    { { { 0x00001FFE, 13 }, { 0x00000000,  0 } },       12 },
    { { { 0x00003FFE, 14 }, { 0x00000000,  0 } },       13 },
    { { { 0x00007FFE, 15 }, { 0x00000000,  0 } },       14 },
    { { { 0x0000FFFE, 16 }, { 0x00000000,  0 } },       15 },
    { { { 0x0001FFFE, 17 }, { 0x00000000,  0 } },       16 },
    { { { 0x0003FFFE, 18 }, { 0x00000000,  0 } },       17 },
    { { { 0x0007FFFE, 19 }, { 0x00000000,  0 } },       18 },
    { { { 0x000FFFFE, 20 }, { 0x00000000,  0 } },       19 },
    { { { 0x001FFFFE, 21 }, { 0x00000000,  0 } },       20 },
    { { { 0x003FFFFE, 22 }, { 0x00000000,  0 } },       21 },
    { { { 0x007FFFFE, 23 }, { 0x00000000,  0 } },       22 },
    { { { 0x00FFFFFE, 24 }, { 0x00000000,  0 } },       23 },
    { { { 0x01FFFFFE, 25 }, { 0x00000000,  0 } },       24 },
    { { { 0x03FFFFFE, 26 }, { 0x00000000,  0 } },       25 },
    { { { 0x07FFFFFE, 27 }, { 0x00000000,  0 } },       26 },
    { { { 0x0FFFFFFE, 28 }, { 0x00000000,  0 } },       27 },
    { { { 0x1FFFFFFE, 29 }, { 0x00000000,  0 } },       28 },
    { { { 0x3FFFFFFE, 30 }, { 0x00000000,  0 } },       29 },
    { { { 0x7FFFFFFE, 31 }, { 0x00000000,  0 } },       30 },
    { { { 0xFFFFFFFE, 32 }, { 0x00000000,  0 } },       31 },
};

const BinPair BinTU3[4] =
{
    { { { 0x00000000,  1 }, { 0x00000000,  0 } },        0 },
    { { { 0x00000002,  2 }, { 0x00000000,  0 } },        1 },
    { { { 0x00000006,  3 }, { 0x00000000,  0 } },        2 },
    { { { 0x00000007,  3 }, { 0x00000000,  0 } },        3 },
};

const BinPair BinFL7[8] =
{
    { { { 0x00000000,  3 }, { 0x00000000,  0 } },        0 },
    { { { 0x00000004,  3 }, { 0x00000000,  0 } },        1 },
    { { { 0x00000002,  3 }, { 0x00000000,  0 } },        2 },
    { { { 0x00000006,  3 }, { 0x00000000,  0 } },        3 },
    { { { 0x00000001,  3 }, { 0x00000000,  0 } },        4 },
    { { { 0x00000005,  3 }, { 0x00000000,  0 } },        5 },
    { { { 0x00000003,  3 }, { 0x00000000,  0 } },        6 },
    { { { 0x00000007,  3 }, { 0x00000000,  0 } },        7 },
};

const BinPair BinMbTypeI[26] =
{
    { { { 0x00000000,  1 }, { 0x00000000,  0 } },        0 },
    { { { 0x00000003,  2 }, { 0x00000000,  0 } },       25 },
    { { { 0x00000020,  6 }, { 0x00000000,  0 } },        1 },
    { { { 0x00000021,  6 }, { 0x00000000,  0 } },        2 },
    { { { 0x00000022,  6 }, { 0x00000000,  0 } },        3 },
    { { { 0x00000023,  6 }, { 0x00000000,  0 } },        4 },
    { { { 0x00000028,  6 }, { 0x00000000,  0 } },       13 },
    { { { 0x00000029,  6 }, { 0x00000000,  0 } },       14 },
    { { { 0x0000002A,  6 }, { 0x00000000,  0 } },       15 },
    { { { 0x0000002B,  6 }, { 0x00000000,  0 } },       16 },
    { { { 0x00000048,  7 }, { 0x00000000,  0 } },        5 },
    { { { 0x00000049,  7 }, { 0x00000000,  0 } },        6 },
    { { { 0x0000004A,  7 }, { 0x00000000,  0 } },        7 },
    { { { 0x0000004B,  7 }, { 0x00000000,  0 } },        8 },
    { { { 0x0000004C,  7 }, { 0x00000000,  0 } },        9 },
    { { { 0x0000004D,  7 }, { 0x00000000,  0 } },       10 },
    { { { 0x0000004E,  7 }, { 0x00000000,  0 } },       11 },
    { { { 0x0000004F,  7 }, { 0x00000000,  0 } },       12 },
    { { { 0x00000058,  7 }, { 0x00000000,  0 } },       17 },
    { { { 0x00000059,  7 }, { 0x00000000,  0 } },       18 },
    { { { 0x0000005A,  7 }, { 0x00000000,  0 } },       19 },
    { { { 0x0000005B,  7 }, { 0x00000000,  0 } },       20 },
    { { { 0x0000005C,  7 }, { 0x00000000,  0 } },       21 },
    { { { 0x0000005D,  7 }, { 0x00000000,  0 } },       22 },
    { { { 0x0000005E,  7 }, { 0x00000000,  0 } },       23 },
    { { { 0x0000005F,  7 }, { 0x00000000,  0 } },       24 },
};

const BinPair BinMbTypeSI[27] =
{
    { { { 0x00000000,  1 }, { 0x00000000,  0 } },        0 },
    { { { 0x00000001,  1 }, { 0x00000000,  1 } },        1 },
    { { { 0x00000001,  1 }, { 0x00000003,  2 } },       26 },
    { { { 0x00000001,  1 }, { 0x00000020,  6 } },        2 },
    { { { 0x00000001,  1 }, { 0x00000021,  6 } },        3 },
    { { { 0x00000001,  1 }, { 0x00000022,  6 } },        4 },
    { { { 0x00000001,  1 }, { 0x00000023,  6 } },        5 },
    { { { 0x00000001,  1 }, { 0x00000028,  6 } },       14 },
    { { { 0x00000001,  1 }, { 0x00000029,  6 } },       15 },
    { { { 0x00000001,  1 }, { 0x0000002A,  6 } },       16 },
    { { { 0x00000001,  1 }, { 0x0000002B,  6 } },       17 },
    { { { 0x00000001,  1 }, { 0x00000048,  7 } },        6 },
    { { { 0x00000001,  1 }, { 0x00000049,  7 } },        7 },
    { { { 0x00000001,  1 }, { 0x0000004A,  7 } },        8 },
    { { { 0x00000001,  1 }, { 0x0000004B,  7 } },        9 },
    { { { 0x00000001,  1 }, { 0x0000004C,  7 } },       10 },
    { { { 0x00000001,  1 }, { 0x0000004D,  7 } },       11 },
    { { { 0x00000001,  1 }, { 0x0000004E,  7 } },       12 },
    { { { 0x00000001,  1 }, { 0x0000004F,  7 } },       13 },
    { { { 0x00000001,  1 }, { 0x00000058,  7 } },       18 },
    { { { 0x00000001,  1 }, { 0x00000059,  7 } },       19 },
    { { { 0x00000001,  1 }, { 0x0000005A,  7 } },       20 },
    { { { 0x00000001,  1 }, { 0x0000005B,  7 } },       21 },
    { { { 0x00000001,  1 }, { 0x0000005C,  7 } },       22 },
    { { { 0x00000001,  1 }, { 0x0000005D,  7 } },       23 },
    { { { 0x00000001,  1 }, { 0x0000005E,  7 } },       24 },
    { { { 0x00000001,  1 }, { 0x0000005F,  7 } },       25 },
};

const BinPair BinMbTypeP[30] =
{
    { { { 0x00000001,  1 }, { 0x00000000,  1 } },        5 },
    { { { 0x00000001,  1 }, { 0x00000003,  2 } },       30 },
    { { { 0x00000001,  1 }, { 0x00000020,  6 } },        6 },
    { { { 0x00000001,  1 }, { 0x00000021,  6 } },        7 },
    { { { 0x00000001,  1 }, { 0x00000022,  6 } },        8 },
    { { { 0x00000001,  1 }, { 0x00000023,  6 } },        9 },
    { { { 0x00000001,  1 }, { 0x00000028,  6 } },       18 },
    { { { 0x00000001,  1 }, { 0x00000029,  6 } },       19 },
    { { { 0x00000001,  1 }, { 0x0000002A,  6 } },       20 },
    { { { 0x00000001,  1 }, { 0x0000002B,  6 } },       21 },
    { { { 0x00000001,  1 }, { 0x00000048,  7 } },       10 },
    { { { 0x00000001,  1 }, { 0x00000049,  7 } },       11 },
    { { { 0x00000001,  1 }, { 0x0000004A,  7 } },       12 },
    { { { 0x00000001,  1 }, { 0x0000004B,  7 } },       13 },
    { { { 0x00000001,  1 }, { 0x0000004C,  7 } },       14 },
    { { { 0x00000001,  1 }, { 0x0000004D,  7 } },       15 },
    { { { 0x00000001,  1 }, { 0x0000004E,  7 } },       16 },
    { { { 0x00000001,  1 }, { 0x0000004F,  7 } },       17 },
    { { { 0x00000001,  1 }, { 0x00000058,  7 } },       22 },
    { { { 0x00000001,  1 }, { 0x00000059,  7 } },       23 },
    { { { 0x00000001,  1 }, { 0x0000005A,  7 } },       24 },
    { { { 0x00000001,  1 }, { 0x0000005B,  7 } },       25 },
    { { { 0x00000001,  1 }, { 0x0000005C,  7 } },       26 },
    { { { 0x00000001,  1 }, { 0x0000005D,  7 } },       27 },
    { { { 0x00000001,  1 }, { 0x0000005E,  7 } },       28 },
    { { { 0x00000001,  1 }, { 0x0000005F,  7 } },       29 },
    { { { 0x00000000,  3 }, { 0x00000000,  0 } },        0 },
    { { { 0x00000003,  3 }, { 0x00000000,  0 } },        1 },
    { { { 0x00000002,  3 }, { 0x00000000,  0 } },        2 },
    { { { 0x00000001,  3 }, { 0x00000000,  0 } },        3 },
};

const BinPair BinMbTypeB[49] =
{
    { { { 0x00000000,  1 }, { 0x00000000,  0 } },        0 },
    { { { 0x00000004,  3 }, { 0x00000000,  0 } },        1 },
    { { { 0x00000005,  3 }, { 0x00000000,  0 } },        2 },
    { { { 0x00000030,  6 }, { 0x00000000,  0 } },        3 },
    { { { 0x00000031,  6 }, { 0x00000000,  0 } },        4 },
    { { { 0x00000032,  6 }, { 0x00000000,  0 } },        5 },
    { { { 0x00000033,  6 }, { 0x00000000,  0 } },        6 },
    { { { 0x00000034,  6 }, { 0x00000000,  0 } },        7 },
    { { { 0x00000035,  6 }, { 0x00000000,  0 } },        8 },
    { { { 0x00000036,  6 }, { 0x00000000,  0 } },        9 },
    { { { 0x00000037,  6 }, { 0x00000000,  0 } },       10 },
    { { { 0x0000003E,  6 }, { 0x00000000,  0 } },       11 },
    { { { 0x0000003F,  6 }, { 0x00000000,  0 } },       22 },
    { { { 0x0000003D,  6 }, { 0x00000000,  1 } },       23 },
    { { { 0x0000003D,  6 }, { 0x00000003,  2 } },       48 },
    { { { 0x0000003D,  6 }, { 0x00000020,  6 } },       24 },
    { { { 0x0000003D,  6 }, { 0x00000021,  6 } },       25 },
    { { { 0x0000003D,  6 }, { 0x00000022,  6 } },       26 },
    { { { 0x0000003D,  6 }, { 0x00000023,  6 } },       27 },
    { { { 0x0000003D,  6 }, { 0x00000028,  6 } },       36 },
    { { { 0x0000003D,  6 }, { 0x00000029,  6 } },       37 },
    { { { 0x0000003D,  6 }, { 0x0000002A,  6 } },       38 },
    { { { 0x0000003D,  6 }, { 0x0000002B,  6 } },       39 },
    { { { 0x0000003D,  6 }, { 0x00000048,  7 } },       28 },
    { { { 0x0000003D,  6 }, { 0x00000049,  7 } },       29 },
    { { { 0x0000003D,  6 }, { 0x0000004A,  7 } },       30 },
    { { { 0x0000003D,  6 }, { 0x0000004B,  7 } },       31 },
    { { { 0x0000003D,  6 }, { 0x0000004C,  7 } },       32 },
    { { { 0x0000003D,  6 }, { 0x0000004D,  7 } },       33 },
    { { { 0x0000003D,  6 }, { 0x0000004E,  7 } },       34 },
    { { { 0x0000003D,  6 }, { 0x0000004F,  7 } },       35 },
    { { { 0x0000003D,  6 }, { 0x00000058,  7 } },       40 },
    { { { 0x0000003D,  6 }, { 0x00000059,  7 } },       41 },
    { { { 0x0000003D,  6 }, { 0x0000005A,  7 } },       42 },
    { { { 0x0000003D,  6 }, { 0x0000005B,  7 } },       43 },
    { { { 0x0000003D,  6 }, { 0x0000005C,  7 } },       44 },
    { { { 0x0000003D,  6 }, { 0x0000005D,  7 } },       45 },
    { { { 0x0000003D,  6 }, { 0x0000005E,  7 } },       46 },
    { { { 0x0000003D,  6 }, { 0x0000005F,  7 } },       47 },
    { { { 0x00000070,  7 }, { 0x00000000,  0 } },       12 },
    { { { 0x00000071,  7 }, { 0x00000000,  0 } },       13 },
    { { { 0x00000072,  7 }, { 0x00000000,  0 } },       14 },
    { { { 0x00000073,  7 }, { 0x00000000,  0 } },       15 },
    { { { 0x00000074,  7 }, { 0x00000000,  0 } },       16 },
    { { { 0x00000075,  7 }, { 0x00000000,  0 } },       17 },
    { { { 0x00000076,  7 }, { 0x00000000,  0 } },       18 },
    { { { 0x00000077,  7 }, { 0x00000000,  0 } },       19 },
    { { { 0x00000078,  7 }, { 0x00000000,  0 } },       20 },
    { { { 0x00000079,  7 }, { 0x00000000,  0 } },       21 },
};

const BinPair BinSubMbTypeP[4] =
{
    { { { 0x00000001,  1 }, { 0x00000000,  0 } },        0 },
    { { { 0x00000000,  2 }, { 0x00000000,  0 } },        1 },
    { { { 0x00000003,  3 }, { 0x00000000,  0 } },        2 },
    { { { 0x00000002,  3 }, { 0x00000000,  0 } },        3 },
};

const BinPair BinSubMbTypeB[13] =
{
    { { { 0x00000000,  1 }, { 0x00000000,  0 } },        0 },
    { { { 0x00000004,  3 }, { 0x00000000,  0 } },        1 },
    { { { 0x00000005,  3 }, { 0x00000000,  0 } },        2 },
    { { { 0x00000018,  5 }, { 0x00000000,  0 } },        3 },
    { { { 0x00000019,  5 }, { 0x00000000,  0 } },        4 },
    { { { 0x0000001A,  5 }, { 0x00000000,  0 } },        5 },
    { { { 0x0000001B,  5 }, { 0x00000000,  0 } },        6 },
    { { { 0x0000001E,  5 }, { 0x00000000,  0 } },       11 },
    { { { 0x0000001F,  5 }, { 0x00000000,  0 } },       12 },
    { { { 0x00000038,  6 }, { 0x00000000,  0 } },        7 },
    { { { 0x00000039,  6 }, { 0x00000000,  0 } },        8 },
    { { { 0x0000003A,  6 }, { 0x00000000,  0 } },        9 },
    { { { 0x0000003B,  6 }, { 0x00000000,  0 } },       10 },
};

const BinPair BinCBPtrCatEq03[16] =
{
    { { { 0x00000000,  4 }, { 0x00000000,  0 } },        0 },
    { { { 0x00000008,  4 }, { 0x00000000,  0 } },        1 },
    { { { 0x00000004,  4 }, { 0x00000000,  0 } },        2 },
    { { { 0x0000000C,  4 }, { 0x00000000,  0 } },        3 },
    { { { 0x00000002,  4 }, { 0x00000000,  0 } },        4 },
    { { { 0x0000000A,  4 }, { 0x00000000,  0 } },        5 },
    { { { 0x00000006,  4 }, { 0x00000000,  0 } },        6 },
    { { { 0x0000000E,  4 }, { 0x00000000,  0 } },        7 },
    { { { 0x00000001,  4 }, { 0x00000000,  0 } },        8 },
    { { { 0x00000009,  4 }, { 0x00000000,  0 } },        9 },
    { { { 0x00000005,  4 }, { 0x00000000,  0 } },       10 },
    { { { 0x0000000D,  4 }, { 0x00000000,  0 } },       11 },
    { { { 0x00000003,  4 }, { 0x00000000,  0 } },       12 },
    { { { 0x0000000B,  4 }, { 0x00000000,  0 } },       13 },
    { { { 0x00000007,  4 }, { 0x00000000,  0 } },       14 },
    { { { 0x0000000F,  4 }, { 0x00000000,  0 } },       15 },
};

const BinPair BinCBPtrCatNe03[48] =
{
    { { { 0x00000000,  4 }, { 0x00000000,  1 } },        0 },
    { { { 0x00000008,  4 }, { 0x00000000,  1 } },        1 },
    { { { 0x00000004,  4 }, { 0x00000000,  1 } },        2 },
    { { { 0x0000000C,  4 }, { 0x00000000,  1 } },        3 },
    { { { 0x00000002,  4 }, { 0x00000000,  1 } },        4 },
    { { { 0x0000000A,  4 }, { 0x00000000,  1 } },        5 },
    { { { 0x00000006,  4 }, { 0x00000000,  1 } },        6 },
    { { { 0x0000000E,  4 }, { 0x00000000,  1 } },        7 },
    { { { 0x00000001,  4 }, { 0x00000000,  1 } },        8 },
    { { { 0x00000009,  4 }, { 0x00000000,  1 } },        9 },
    { { { 0x00000005,  4 }, { 0x00000000,  1 } },       10 },
    { { { 0x0000000D,  4 }, { 0x00000000,  1 } },       11 },
    { { { 0x00000003,  4 }, { 0x00000000,  1 } },       12 },
    { { { 0x0000000B,  4 }, { 0x00000000,  1 } },       13 },
    { { { 0x00000007,  4 }, { 0x00000000,  1 } },       14 },
    { { { 0x0000000F,  4 }, { 0x00000000,  1 } },       15 },
    { { { 0x00000000,  4 }, { 0x00000002,  2 } },       16 },
    { { { 0x00000008,  4 }, { 0x00000002,  2 } },       17 },
    { { { 0x00000004,  4 }, { 0x00000002,  2 } },       18 },
    { { { 0x0000000C,  4 }, { 0x00000002,  2 } },       19 },
    { { { 0x00000002,  4 }, { 0x00000002,  2 } },       20 },
    { { { 0x0000000A,  4 }, { 0x00000002,  2 } },       21 },
    { { { 0x00000006,  4 }, { 0x00000002,  2 } },       22 },
    { { { 0x0000000E,  4 }, { 0x00000002,  2 } },       23 },
    { { { 0x00000001,  4 }, { 0x00000002,  2 } },       24 },
    { { { 0x00000009,  4 }, { 0x00000002,  2 } },       25 },
    { { { 0x00000005,  4 }, { 0x00000002,  2 } },       26 },
    { { { 0x0000000D,  4 }, { 0x00000002,  2 } },       27 },
    { { { 0x00000003,  4 }, { 0x00000002,  2 } },       28 },
    { { { 0x0000000B,  4 }, { 0x00000002,  2 } },       29 },
    { { { 0x00000007,  4 }, { 0x00000002,  2 } },       30 },
    { { { 0x0000000F,  4 }, { 0x00000002,  2 } },       31 },
    { { { 0x00000000,  4 }, { 0x00000003,  2 } },       32 },
    { { { 0x00000008,  4 }, { 0x00000003,  2 } },       33 },
    { { { 0x00000004,  4 }, { 0x00000003,  2 } },       34 },
    { { { 0x0000000C,  4 }, { 0x00000003,  2 } },       35 },
    { { { 0x00000002,  4 }, { 0x00000003,  2 } },       36 },
    { { { 0x0000000A,  4 }, { 0x00000003,  2 } },       37 },
    { { { 0x00000006,  4 }, { 0x00000003,  2 } },       38 },
    { { { 0x0000000E,  4 }, { 0x00000003,  2 } },       39 },
    { { { 0x00000001,  4 }, { 0x00000003,  2 } },       40 },
    { { { 0x00000009,  4 }, { 0x00000003,  2 } },       41 },
    { { { 0x00000005,  4 }, { 0x00000003,  2 } },       42 },
    { { { 0x0000000D,  4 }, { 0x00000003,  2 } },       43 },
    { { { 0x00000003,  4 }, { 0x00000003,  2 } },       44 },
    { { { 0x0000000B,  4 }, { 0x00000003,  2 } },       45 },
    { { { 0x00000007,  4 }, { 0x00000003,  2 } },       46 },
    { { { 0x0000000F,  4 }, { 0x00000003,  2 } },       47 },
};

//const BinPair BinMvdLx[65535] = UEGk(3,  synElVal, 9, 1))
const BinPair BinMvdLx[65] =
{
    { { { 0x00000000,  1 }, { 0x00000000,  0 } },        0 },
    { { { 0x00000002,  2 }, { 0x00000000,  1 } },        1 },
    { { { 0x00000002,  2 }, { 0x00000001,  1 } },       -1 },
    { { { 0x00000006,  3 }, { 0x00000000,  1 } },        2 },
    { { { 0x00000006,  3 }, { 0x00000001,  1 } },       -2 },
    { { { 0x0000000E,  4 }, { 0x00000000,  1 } },        3 },
    { { { 0x0000000E,  4 }, { 0x00000001,  1 } },       -3 },
    { { { 0x0000001E,  5 }, { 0x00000000,  1 } },        4 },
    { { { 0x0000001E,  5 }, { 0x00000001,  1 } },       -4 },
    { { { 0x0000003E,  6 }, { 0x00000000,  1 } },        5 },
    { { { 0x0000003E,  6 }, { 0x00000001,  1 } },       -5 },
    { { { 0x0000007E,  7 }, { 0x00000000,  1 } },        6 },
    { { { 0x0000007E,  7 }, { 0x00000001,  1 } },       -6 },
    { { { 0x000000FE,  8 }, { 0x00000000,  1 } },        7 },
    { { { 0x000000FE,  8 }, { 0x00000001,  1 } },       -7 },
    { { { 0x000001FE,  9 }, { 0x00000000,  1 } },        8 },
    { { { 0x000001FE,  9 }, { 0x00000001,  1 } },       -8 },
    { { { 0x000001FF,  9 }, { 0x00000000,  5 } },        9 },
    { { { 0x000001FF,  9 }, { 0x00000001,  5 } },       -9 },
    { { { 0x000001FF,  9 }, { 0x00000002,  5 } },       10 },
    { { { 0x000001FF,  9 }, { 0x00000003,  5 } },      -10 },
    { { { 0x000001FF,  9 }, { 0x00000004,  5 } },       11 },
    { { { 0x000001FF,  9 }, { 0x00000005,  5 } },      -11 },
    { { { 0x000001FF,  9 }, { 0x00000006,  5 } },       12 },
    { { { 0x000001FF,  9 }, { 0x00000007,  5 } },      -12 },
    { { { 0x000001FF,  9 }, { 0x00000008,  5 } },       13 },
    { { { 0x000001FF,  9 }, { 0x00000009,  5 } },      -13 },
    { { { 0x000001FF,  9 }, { 0x0000000A,  5 } },       14 },
    { { { 0x000001FF,  9 }, { 0x0000000B,  5 } },      -14 },
    { { { 0x000001FF,  9 }, { 0x0000000C,  5 } },       15 },
    { { { 0x000001FF,  9 }, { 0x0000000D,  5 } },      -15 },
    { { { 0x000001FF,  9 }, { 0x0000000E,  5 } },       16 },
    { { { 0x000001FF,  9 }, { 0x0000000F,  5 } },      -16 },
    { { { 0x000001FF,  9 }, { 0x00000040,  7 } },       17 },
    { { { 0x000001FF,  9 }, { 0x00000041,  7 } },      -17 },
    { { { 0x000001FF,  9 }, { 0x00000042,  7 } },       18 },
    { { { 0x000001FF,  9 }, { 0x00000043,  7 } },      -18 },
    { { { 0x000001FF,  9 }, { 0x00000044,  7 } },       19 },
    { { { 0x000001FF,  9 }, { 0x00000045,  7 } },      -19 },
    { { { 0x000001FF,  9 }, { 0x00000046,  7 } },       20 },
    { { { 0x000001FF,  9 }, { 0x00000047,  7 } },      -20 },
    { { { 0x000001FF,  9 }, { 0x00000048,  7 } },       21 },
    { { { 0x000001FF,  9 }, { 0x00000049,  7 } },      -21 },
    { { { 0x000001FF,  9 }, { 0x0000004A,  7 } },       22 },
    { { { 0x000001FF,  9 }, { 0x0000004B,  7 } },      -22 },
    { { { 0x000001FF,  9 }, { 0x0000004C,  7 } },       23 },
    { { { 0x000001FF,  9 }, { 0x0000004D,  7 } },      -23 },
    { { { 0x000001FF,  9 }, { 0x0000004E,  7 } },       24 },
    { { { 0x000001FF,  9 }, { 0x0000004F,  7 } },      -24 },
    { { { 0x000001FF,  9 }, { 0x00000050,  7 } },       25 },
    { { { 0x000001FF,  9 }, { 0x00000051,  7 } },      -25 },
    { { { 0x000001FF,  9 }, { 0x00000052,  7 } },       26 },
    { { { 0x000001FF,  9 }, { 0x00000053,  7 } },      -26 },
    { { { 0x000001FF,  9 }, { 0x00000054,  7 } },       27 },
    { { { 0x000001FF,  9 }, { 0x00000055,  7 } },      -27 },
    { { { 0x000001FF,  9 }, { 0x00000056,  7 } },       28 },
    { { { 0x000001FF,  9 }, { 0x00000057,  7 } },      -28 },
    { { { 0x000001FF,  9 }, { 0x00000058,  7 } },       29 },
    { { { 0x000001FF,  9 }, { 0x00000059,  7 } },      -29 },
    { { { 0x000001FF,  9 }, { 0x0000005A,  7 } },       30 },
    { { { 0x000001FF,  9 }, { 0x0000005B,  7 } },      -30 },
    { { { 0x000001FF,  9 }, { 0x0000005C,  7 } },       31 },
    { { { 0x000001FF,  9 }, { 0x0000005D,  7 } },      -31 },
    { { { 0x000001FF,  9 }, { 0x0000005E,  7 } },       32 },
    { { { 0x000001FF,  9 }, { 0x0000005F,  7 } },      -32 },
};

//const BinPair BinCoeffAbsLvl[65536] = UEGk(0, synElVal, 14, 0)
const BinPair BinCoeffAbsLvl[32] =
{
    { { { 0x00000000,  1 }, { 0x00000000,  0 } },        0 },
    { { { 0x00000002,  2 }, { 0x00000000,  0 } },        1 },
    { { { 0x00000006,  3 }, { 0x00000000,  0 } },        2 },
    { { { 0x0000000E,  4 }, { 0x00000000,  0 } },        3 },
    { { { 0x0000001E,  5 }, { 0x00000000,  0 } },        4 },
    { { { 0x0000003E,  6 }, { 0x00000000,  0 } },        5 },
    { { { 0x0000007E,  7 }, { 0x00000000,  0 } },        6 },
    { { { 0x000000FE,  8 }, { 0x00000000,  0 } },        7 },
    { { { 0x000001FE,  9 }, { 0x00000000,  0 } },        8 },
    { { { 0x000003FE, 10 }, { 0x00000000,  0 } },        9 },
    { { { 0x000007FE, 11 }, { 0x00000000,  0 } },       10 },
    { { { 0x00000FFE, 12 }, { 0x00000000,  0 } },       11 },
    { { { 0x00001FFE, 13 }, { 0x00000000,  0 } },       12 },
    { { { 0x00003FFE, 14 }, { 0x00000000,  0 } },       13 },
    { { { 0x00003FFF, 14 }, { 0x00000000,  1 } },       14 },
    { { { 0x00003FFF, 14 }, { 0x00000004,  3 } },       15 },
    { { { 0x00003FFF, 14 }, { 0x00000005,  3 } },       16 },
    { { { 0x00003FFF, 14 }, { 0x00000018,  5 } },       17 },
    { { { 0x00003FFF, 14 }, { 0x00000019,  5 } },       18 },
    { { { 0x00003FFF, 14 }, { 0x0000001A,  5 } },       19 },
    { { { 0x00003FFF, 14 }, { 0x0000001B,  5 } },       20 },
    { { { 0x00003FFF, 14 }, { 0x00000070,  7 } },       21 },
    { { { 0x00003FFF, 14 }, { 0x00000071,  7 } },       22 },
    { { { 0x00003FFF, 14 }, { 0x00000072,  7 } },       23 },
    { { { 0x00003FFF, 14 }, { 0x00000073,  7 } },       24 },
    { { { 0x00003FFF, 14 }, { 0x00000074,  7 } },       25 },
    { { { 0x00003FFF, 14 }, { 0x00000075,  7 } },       26 },
    { { { 0x00003FFF, 14 }, { 0x00000076,  7 } },       27 },
    { { { 0x00003FFF, 14 }, { 0x00000077,  7 } },       28 },
    { { { 0x00003FFF, 14 }, { 0x000001E0,  9 } },       29 },
    { { { 0x00003FFF, 14 }, { 0x000001E1,  9 } },       30 },
    { { { 0x00003FFF, 14 }, { 0x000001E2,  9 } },       31 },
};

// DECODE /////////////////////////////////////////////////////////////////////

enum CTX_FLAGS
{
      ERROR = -4
    , EXTERNAL
    , TERMINATE
    , BYPASS
};

bool CABAC::mbSkipFlag()
{
    const Bs16s ctxIdxOffset[2] = { 11, 24 };
    const Bs16s off = ctxIdxOffset[m_info->isB];
    const Bs16u s = 0; //no suffix for FL bins
    MB* mbA = m_info->getMB(m_info->neighbMbA(m_info->cMB()));
    MB* mbB = m_info->getMB(m_info->neighbMbB(m_info->cMB()));
    Bs16s inc = (mbA && !mbA->mb_skip_flag) + (mbB && !mbB->mb_skip_flag);
    BinPair res = {};

    for (Bs16u i = 0; i < 2; i++)
    {
        Bin const & expected = BinFL1[i].b[s];
        Bin & decoded = res.b[s];

        while (decoded.n < expected.n)
            decoded += DecodeBin(off, inc);

        if (decoded.b == expected.b)
            return !!BinFL1[i].v;
    }

    throw InvalidSyntax();
}

bool CABAC::mbFieldDecFlag()
{
    const Bs16s off = 70;
    const Bs16u s = 0; //no suffix for FL bins
    BinPair res = {};
    MB& cMB = m_info->cMB();
    Bs16s inc =
        (m_info->AvailableA(cMB) && m_info->getMB(m_info->mbAddrA(cMB))->mb_field_decoding_flag) +
        (m_info->AvailableB(cMB) && m_info->getMB(m_info->mbAddrB(cMB))->mb_field_decoding_flag);

    for (Bs16u i = 0; i < 2; i++)
    {
        Bin const & expected = BinFL1[i].b[s];
        Bin & decoded = res.b[s];

        while (decoded.n < expected.n)
            decoded += DecodeBin(off, inc);

        if (decoded.b == expected.b)
            return !!BinFL1[i].v;
    }

    throw InvalidSyntax();
}

const Bs8s ctxIdxIncMbType[4][2][7] =
{
/*B */{{ EXTERNAL,        3, EXTERNAL,        5,        5,        5,        5},
       {        0,TERMINATE,        1,        2, EXTERNAL,        3,        3},},

/*P */{{        0,        1, EXTERNAL,    ERROR,    ERROR,    ERROR,    ERROR},
       {        0,TERMINATE,        1,        2, EXTERNAL,        3,        3},},

/*I */{{ EXTERNAL,TERMINATE,        3,        4, EXTERNAL, EXTERNAL,        7},
       {    ERROR,    ERROR,    ERROR,    ERROR,    ERROR,    ERROR,    ERROR},},

/*SI*/{{ EXTERNAL,    ERROR,    ERROR,    ERROR,    ERROR,    ERROR,    ERROR},
       { EXTERNAL,TERMINATE,        3,        4, EXTERNAL, EXTERNAL,        7},},
};

Bs16s mbTypeInc(Info& info, Bs32u idx, Bin& bin, bool isSuffix)
{
    Bs16s inc = ctxIdxIncMbType[idx][isSuffix][BS_MIN(bin.n, 6)];

    if (inc != EXTERNAL)
        return inc;

    if (bin.n == 0)
    {
        MB& cMB = info.cMB();
        MB* mbA = info.getMB(info.neighbMbA(cMB));
        MB* mbB = info.getMB(info.neighbMbB(cMB));
        bool condTermFlagA = !(!mbA
            || (info.isI  && mbA->MbType == I_NxN)
            || (info.isB && (mbA->MbType == B_Skip || mbA->MbType == B_Direct_16x16))
            || (info.isSI && mbA->MbType == MBTYPE_SI));
        bool condTermFlagB = !(!mbB
            || (info.isI  && mbB->MbType == I_NxN)
            || (info.isB && (mbB->MbType == B_Skip || mbB->MbType == B_Direct_16x16))
            || (info.isSI && mbB->MbType == MBTYPE_SI));
        return condTermFlagA + condTermFlagB;
    }

    if (info.isB)
    {
        if (!isSuffix && bin.n == 2)
            return 5 - (bin.b & 1);

        if (isSuffix && bin.n == 4)
            return 3 - (bin.b & 1);
    }

    if (info.isP || info.isSP)
    {
        if (!isSuffix && bin.n == 2)
            return 2 + (bin.b & 1);

        if (isSuffix && bin.n == 4)
            return 3 - (bin.b & 1);
    }

    if (info.isI || (isSuffix && info.isSI))
    {
        if (bin.n == 4)
            return 6 - (bin.b & 1);

        if (bin.n == 5)
            return 7 - !!(bin.b & 2);
    }

    throw InvalidSyntax();
}

Bs8u CABAC::mbType()
{
    const BinPair* bins;
    Bs16u numBins;
    Bs16s offset[2] = {ERROR, ERROR};
    BinPair res = {};
    Bs16u idx;

    if (m_info->isB)
    {
        bins = BinMbTypeB;
        numBins = sizeof(BinMbTypeB) / sizeof(BinPair);
        offset[0] = 27;
        offset[1] = 32;
        idx = 0;
    }
    else if (m_info->isP || m_info->isSP)
    {
        bins = BinMbTypeP;
        numBins = sizeof(BinMbTypeP) / sizeof(BinPair);
        offset[0] = 14;
        offset[1] = 17;
        idx = 1;
    }
    else if (m_info->isI)
    {
        bins = BinMbTypeI;
        numBins = sizeof(BinMbTypeI) / sizeof(BinPair);
        offset[0] = 3;
        idx = 2;
    }
    else if (m_info->isSI)
    {
        bins = BinMbTypeSI;
        numBins = sizeof(BinMbTypeSI) / sizeof(BinPair);
        offset[0] = 0;
        offset[1] = 3;
        idx = 3;
    }
    else
        throw InvalidSyntax();

    for (Bs16u i = 0; i < numBins; i++)
    {
        Bs16u match = 0;

        for (Bs16s s = 0; s < 2; s++)
        {
            Bin const & expected = bins[i].b[s];
            Bin & decoded = res.b[s];

            while (decoded.n < expected.n)
                decoded += DecodeBin(offset[s], mbTypeInc(*m_info, idx, decoded, !!s));

            if (decoded.b != expected.b)
                break;

            match++;
        }

        if (match == 2)
            return m_info->MbType((Bs8u)bins[i].v);
    }

    throw InvalidSyntax();
}

bool CABAC::transformSize8x8Flag()
{
    MB* mbA = m_info->getMB(m_info->neighbMbA(m_info->cMB()));
    MB* mbB = m_info->getMB(m_info->neighbMbB(m_info->cMB()));
    const Bs16u s = 0; //no suffix for FL bins
    const Bs16s off = 399;
    Bs16s inc = (mbA && mbA->transform_size_8x8_flag) +
                (mbB && mbB->transform_size_8x8_flag);
    Bin decoded = {};

    for (Bs16u i = 0; i < 2; i++)
    {
        Bin const & expected = BinFL1[i].b[s];

        while (decoded.n < expected.n)
            decoded += DecodeBin(off, inc);

        if (decoded.b == expected.b)
            return !!BinFL1[i].v;
    }

    throw InvalidSyntax();
}

Bs16s cbpInc(Info& info, Bs32u , Bin& bin, bool isSuffix)
{
    MB& cMB = info.cMB();
    MB *mbA, *mbB;
    bool condTermFlagA, condTermFlagB;

    if (!isSuffix)
    {
        BlkLoc blkA = info.neighb8x8YblkA(cMB, bin.n);
        BlkLoc blkB = info.neighb8x8YblkB(cMB, bin.n);

        mbA = info.getMB(blkA.addr);
        mbB = info.getMB(blkB.addr);

        condTermFlagA = !(
              !mbA
            || (mbA->MbType == I_PCM)
            || (mbA != &cMB && !mbA->mb_skip_flag && ((info.cbpY(*mbA) >> blkA.idx) & 1))
            || (mbA == &cMB && ((bin.b >> (bin.n - blkA.idx - 1)) & 1)));

        condTermFlagB = !(
              !mbB
            || (mbB->MbType == I_PCM)
            || (mbB != &cMB && !mbB->mb_skip_flag && ((info.cbpY(*mbB) >> blkB.idx) & 1))
            || (mbB == &cMB && ((bin.b >> (bin.n - blkB.idx - 1)) & 1)));

        return condTermFlagA + 2 * condTermFlagB;
    }

    mbA = info.getMB(info.neighbMbA(cMB, false));
    mbB = info.getMB(info.neighbMbB(cMB, false));

    condTermFlagA = (
        (mbA && mbA->MbType == I_PCM)
        || !((!mbA || mbA->mb_skip_flag)
            || (bin.n == 0 && info.cbpC(*mbA) == 0)
            || (bin.n == 1 && info.cbpC(*mbA) != 2)));

    condTermFlagB = (
        (mbB && mbB->MbType == I_PCM)
        || !((!mbB || mbB->mb_skip_flag)
        || (bin.n == 0 && info.cbpC(*mbB) == 0)
        || (bin.n == 1 && info.cbpC(*mbB) != 2)));

    return condTermFlagA + 2 * condTermFlagB + ((bin.n == 1) ? 4 : 0);
}

Bs8u CABAC::CBP()
{
    const Bs16s offset[2] = { 73, 77 };
    const BinPair* bins;
    Bs16u numBins;
    BinPair res = {};

    if (   m_info->cSPS().chroma_format_idc == 0
        || m_info->cSPS().chroma_format_idc == 3)
    {
        bins = BinCBPtrCatEq03;
        numBins = sizeof(BinCBPtrCatEq03) / sizeof(BinPair);
    }
    else
    {
        bins = BinCBPtrCatNe03;
        numBins = sizeof(BinCBPtrCatNe03) / sizeof(BinPair);
    }

    for (Bs16u i = 0; i < numBins; i++)
    {
        Bs16u match = 0;

        for (Bs16s s = 0; s < 2; s++)
        {
            Bin const & expected = bins[i].b[s];
            Bin & decoded = res.b[s];

            while (decoded.n < expected.n)
                decoded += DecodeBin(offset[s], cbpInc(*m_info, 0, decoded, !!s));

            if (decoded.b != expected.b)
                break;

            match++;
        }

        if (match == 2)
            return (Bs8u)bins[i].v;
    }

    throw InvalidSyntax();
}

Bs8s CABAC::mbQpDelta()
{
    const Bs16s off = 60;
    Bs16s inc[3] = { EXTERNAL, 2, 3 };
    MB* prevMB = m_info->getMB(m_info->cMB().Addr - 1);
    Bs8s b;

    inc[0] = !( !prevMB
        || prevMB->mb_skip_flag
        || prevMB->MbType == I_PCM
        || (m_info->MbPartPredMode(*prevMB, 0) != Intra_16x16 && prevMB->coded_block_pattern == 0)
        || prevMB->mb_qp_delta == 0 );

    for (b = 0; DecodeDecision(m_ctxState[off + inc[BS_MIN(b, 2)]]); b++);

    BinCount += b + 1;

    return (b & 1) ? ((b + 1) >> 1) : -((b + 1) >> 1);
}


const Bs8s ctxIdxIncSubMbType[2][7] =
{
/*P_SP */{ 0, 1, 2, ERROR, ERROR, ERROR, ERROR},
/*B    */{ 0, 1, EXTERNAL, 3, 3, 3, ERROR }
};

Bs16s subMbTypeInc(Info& /*info*/, Bs32u idx, Bin& bin, bool /*isSuffix*/)
{
    Bs16s inc = ctxIdxIncSubMbType[idx][BS_MIN(6, bin.n)];

    if (inc == EXTERNAL)
        return 3 - (bin.b & 1);

    return inc;
}

Bs8u CABAC::subMbType()
{
    const BinPair* bins;
    const Bs16u s = 0; // no suffix
    Bs16u numBins;
    BinPair res = {};
    Bs16s off;

    if (m_info->isB)
    {
        bins = BinSubMbTypeB;
        numBins = sizeof(BinSubMbTypeB) / sizeof(BinPair);
        off = 36;
    }
    else if (m_info->isP || m_info->isSP)
    {
        bins = BinSubMbTypeP;
        numBins = sizeof(BinSubMbTypeP) / sizeof(BinPair);
        off = 21;
    }
    else
        throw InvalidSyntax();

    for (Bs16u i = 0; i < numBins; i++)
    {
        Bin const & expected = bins[i].b[s];
        Bin & decoded = res.b[s];

        while (decoded.n < expected.n)
            decoded += DecodeBin(off, subMbTypeInc(*m_info, m_info->isB, decoded, !!s));

        if (decoded.b == expected.b)
        {
            if (m_info->isB)
                return SubMbTblB[bins[i].v][0];

            return SubMbTblP[bins[i].v][0];
        }
    }

    throw InvalidSyntax();
}

bool ridxCondTermFlagN(Info& info, Bs32u idx, bool isA)
{
    bool X = (idx & 1);
    Bs8u mbPartIdx = (idx >> 1);
    PartLoc loc = isA ? info.neighbPartA(info.cMB(), mbPartIdx, 0)
        : info.neighbPartB(info.cMB(), mbPartIdx, 0);
    MB* mbN = info.getMB(loc.mbAddr);

    if (!mbN || mbN->mb_skip_flag || info.MbPartPredMode(*mbN, 0) < Pred_L0)
        return false;

    bool refIdxZeroFlagN = false;

    if (   info.MbaffFrameFlag
        && !info.cMB().mb_field_decoding_flag
        && mbN->mb_field_decoding_flag)
        refIdxZeroFlagN = (mbN->pred.ref_idx_lX[X][loc.mbPartIdx] <= 1);
    else
        refIdxZeroFlagN = (mbN->pred.ref_idx_lX[X][loc.mbPartIdx] == 0);

    if (refIdxZeroFlagN)
        return false;

    Bs8u t = mbN->MbType;

    if (t == B_Direct_16x16 || t == B_Skip)
        return false;

    Bs8u pm = (t == P_8x8 || t == B_8x8) ? info.SubMbPredMode(*mbN, loc.mbPartIdx)
        : info.MbPartPredMode(*mbN, loc.mbPartIdx);

    return (pm == Pred_L0 || pm == Pred_L1 || pm == BiPred);
}

const Bs8s ctxIdxIncRefIdxLX[7] = { EXTERNAL, 4, 5, 5, 5, 5, 5};

Bs16s refIdxLXInc(Info& info, Bs32u idx, Bin& bin, bool /*isSuffix*/)
{
    Bs16s inc = ctxIdxIncRefIdxLX[BS_MIN(6, bin.n)];

    if (inc == EXTERNAL)
        return (ridxCondTermFlagN(info, idx, true) + 2 * ridxCondTermFlagN(info, idx, false));

    return inc;
}

Bs8u CABAC::refIdxLX(Bs16u mbPartIdx, bool X)
{
    const BinPair* bins = BinRefIdxLx;
    const Bs16u numBins = sizeof(BinRefIdxLx) / sizeof(BinPair);
    const Bs16u s = 0; // no suffix
    const Bs16u off = 54;
    BinPair res = {};
    Bs32u idx = (mbPartIdx << 1) | Bs32u(X);

    for (Bs16u i = 0; i < numBins; i++)
    {
        Bin const & expected = bins[i].b[s];
        Bin & decoded = res.b[s];

        while (decoded.n < expected.n)
            decoded += DecodeBin(off, refIdxLXInc(*m_info, idx, decoded, !!s));

        if (decoded.b == expected.b)
            return (Bs8u)bins[i].v;
    }

    throw InvalidSyntax();
}

Bs16u absMvdCompN(Info& info, Bs32u idx, bool isA)
{
    bool X = (idx & 1);
    bool compIdx = !!(idx & 2);
    Bs8u subMbPartIdx = ((idx >> 2) & 3);
    Bs8u mbPartIdx = (idx >> 4);
    PartLoc loc = isA ? info.neighbPartA(info.cMB(), mbPartIdx, subMbPartIdx)
        : info.neighbPartB(info.cMB(), mbPartIdx, subMbPartIdx);
    MB* mbN = info.getMB(loc.mbAddr);

    if (!mbN || mbN->mb_skip_flag || info.MbPartPredMode(*mbN, 0) < Pred_L0)
        return false;

    bool predModeEqualFlagN;
    Bs8s pm;

    switch (mbN->MbType)
    {
    case B_Direct_16x16:
    case B_Skip:
        predModeEqualFlagN = false;
        break;
    case P_8x8:
    case B_8x8:
        pm = info.SubMbPredMode(*mbN, loc.mbPartIdx);
        predModeEqualFlagN = (pm == Pred_L0 || pm == Pred_L1 || pm == BiPred);
        break;
    default:
        pm = info.MbPartPredMode(*mbN, loc.mbPartIdx);
        predModeEqualFlagN = (pm == Pred_L0 || pm == Pred_L1 || pm == BiPred);
        break;
    }

    if (!predModeEqualFlagN)
        return 0;

    if (    compIdx
        && info.MbaffFrameFlag
        && info.cMB().mb_field_decoding_flag == 0
        && mbN->mb_field_decoding_flag == 1)
        return BS_ABS(mbN->pred.mvd_lX[X][loc.mbPartIdx][loc.subMbPartIdx][compIdx]) * 2;

    if (   compIdx
        && info.MbaffFrameFlag
        && info.cMB().mb_field_decoding_flag == 1
        && mbN->mb_field_decoding_flag == 0)
        return BS_ABS(mbN->pred.mvd_lX[X][loc.mbPartIdx][loc.subMbPartIdx][compIdx]) / 2;

    return BS_ABS(mbN->pred.mvd_lX[X][loc.mbPartIdx][loc.subMbPartIdx][compIdx]);
}

const Bs8s ctxIdxIncMvdLX[7] = { EXTERNAL, 3, 4, 5, 6, 6, 6 };

Bs16s mvdLXInc(Info& info, Bs32u idx, Bin& bin, bool isSuffix)
{
    if (isSuffix)
        return ERROR;

    Bs16s inc = ctxIdxIncMvdLX[BS_MIN(6, bin.n)];

    if (inc != EXTERNAL)
        return inc;

    Bs16u sum = absMvdCompN(info, idx, true) + absMvdCompN(info, idx, false);

    if (sum <  3) return 0;
    if (sum > 32) return 2;
    return 1;
}

Bs16s CABAC::mvdLX(Bs16u mbPartIdx, Bs16u subMbPartIdx, bool compIdx, bool X)
{
    const BinPair* bins = BinMvdLx;
    const Bs16u numBins = sizeof(BinMvdLx) / sizeof(BinPair);
    const Bs16s off[2][2] = { { 40, BYPASS }, {47, BYPASS} };
    Bs32u idx = (mbPartIdx << 4) | (subMbPartIdx << 2) | (Bs32u(compIdx) << 1) | Bs32u(X);
    BinPair res = {};

    for (Bs16u i = 0; i < numBins; i++)
    {
        Bs16u match = 0;

        for (Bs16s s = 0; s < 2; s++)
        {
            Bin const & expected = bins[i].b[s];
            Bin & decoded = res.b[s];

            while (decoded.n < expected.n)
                decoded += DecodeBin(off[!!compIdx][s], mvdLXInc(*m_info, idx, decoded, !!s));

            if (decoded.b != expected.b)
                break;

            match++;
        }

        if (match == 2)
            return (Bs16s)bins[i].v;
    }

    if (res.b[0].b != bins[numBins-1].b[0].b)
        throw InvalidSyntax();

    Bin expected;
    Bin & decoded = res.b[1];

    for (Bs16u vabs = BS_ABS(bins[numBins - 1].v) + 1; vabs <= 0x7FFF; vabs++)
    {
        for (Bs16s vsign = 1; vsign > -2; vsign -= 2)
        {
            UEGkS(expected, 3, vsign * vabs, 9, true);

            while (decoded.n < expected.n)
            {
                decoded += DecodeBypass();
                BinCount++;
            }

            if (decoded.b == expected.b)
                return vsign * vabs;
        }
    }

    throw InvalidSyntax();
}

bool CABAC::prevIntraPredModeFlag()
{
    const Bs16s off = 68;
    const Bs16s inc = 0;
    const Bs16u s = 0; //no suffix for FL bins
    Bin decoded = {};

    for (Bs16u i = 0; i < 2; i++)
    {
        Bin const & expected = BinFL1[i].b[s];

        while (decoded.n < expected.n)
            decoded += DecodeBin(off, inc);

        if (decoded.b == expected.b)
            return !!BinFL1[i].v;
    }

    throw InvalidSyntax();
}

Bs8u CABAC::remIntraPredMode()
{
    const Bs16s off = 69;
    const Bs16s inc = 0;
    const Bs16u s = 0; //no suffix for FL bins
    Bin decoded = {};

    for (Bs16u i = 0; i < 8; i++)
    {
        Bin const & expected = BinFL7[i].b[s];

        while (decoded.n < expected.n)
            decoded += DecodeBin(off, inc);

        if (decoded.b == expected.b)
            return BinFL7[i].v;
    }

    throw InvalidSyntax();
}

Bs8u CABAC::intraChromaPredMode()
{
    const Bs16s off = 64;
    const Bs16u s = 0; //no suffix
    MB* mbA = m_info->getMB(m_info->neighbMbA(m_info->cMB()));
    MB* mbB = m_info->getMB(m_info->neighbMbB(m_info->cMB()));
    Bin decoded = {};
    Bs16s inc[2] =
    {
        (Bs16s)(!(!mbA
                || m_info->MbPartPredMode(*mbA, 0) >= Pred_L0
                || mbA->MbType == I_PCM
                || !mbA->pred.intra_chroma_pred_mode) +
                !(!mbB
                || m_info->MbPartPredMode(*mbB, 0) >= Pred_L0
                || mbB->MbType == I_PCM
                || !mbB->pred.intra_chroma_pred_mode)),
                3
    };

    for (Bs16u i = 0; i < 4; i++)
    {
        Bin const & expected = BinTU3[i].b[s];

        while (decoded.n < expected.n)
            decoded += DecodeBin(off, inc[!!decoded.n]);

        if (decoded.b == expected.b)
            return BinTU3[i].v;
    }

    throw InvalidSyntax();
}

Bs8u ctxBlockCatIdx(Bs8u c)
{
    if (c == 5 || c == 9 || c == 13)
        return 3;

    if (c <  5) return 0;
    if (c <  9) return 1;
    if (c < 13) return 2;

    throw InvalidSyntax();
}

inline bool isSkipPCM(MB* p) { return !p || p->mb_skip_flag || p->MbType == I_PCM; }

bool cbfCondTermFlagN(Info& info, Bs8u blkIdx, Bs8u ctxBlockCat, bool isCb, bool isA)
{
    MB& cMB = info.cMB();
    MB* mbN = 0;
    Bs16s tbCBF = -1;
    BlkLoc loc;

    switch (ctxBlockCat)
    {
    case  0:
    case  6:
    case 10:
        mbN = info.getMB(isA ? info.neighbMbA(cMB) : info.neighbMbB(cMB));

        if (!mbN || info.MbPartPredMode(*mbN, 0) != Intra_16x16)
            break;

             if (ctxBlockCat == 0) tbCBF = mbN->cbf.lumaDC;
        else if (ctxBlockCat == 6) tbCBF = mbN->cbf.CbDC;
        else                       tbCBF = mbN->cbf.CrDC;
        break;
    case 1:
    case 2:
        loc = isA ? info.neighb4x4YblkA(cMB, blkIdx) : info.neighb4x4YblkB(cMB, blkIdx);
        mbN = info.getMB(loc.addr);

        if (!isSkipPCM(mbN) && ((info.cbpY(*mbN) >> (loc.idx >> 2)) & 1))
            tbCBF = mbN->transform_size_8x8_flag
                ? ((mbN->cbf.luma8x8 >> (loc.idx >> 2)) & 1)
                : ((mbN->cbf.luma4x4 >> loc.idx) & 1);
        break;
    case 3:
        mbN = info.getMB(isA ? info.neighbMbA(cMB) : info.neighbMbB(cMB));

        if (!isSkipPCM(mbN) && info.cbpC(*mbN))
            tbCBF = isCb ? mbN->cbf.CbDC : mbN->cbf.CrDC;
        break;
    case 4:
        loc = isA ? info.neighb4x4CblkA(cMB, blkIdx) : info.neighb4x4CblkB(cMB, blkIdx);
        mbN = info.getMB(loc.addr);

        if (!isSkipPCM(mbN) && info.cbpC(*mbN) == 2)
            tbCBF = isCb
                ? ((mbN->cbf.Cb4x4 >> loc.idx) & 1)
                : ((mbN->cbf.Cr4x4 >> loc.idx) & 1);
        break;
    case 5:
        loc = isA ? info.neighb8x8YblkA(cMB, blkIdx) : info.neighb8x8YblkB(cMB, blkIdx);
        mbN = info.getMB(loc.addr);

        if (!isSkipPCM(mbN) && mbN->transform_size_8x8_flag && ((info.cbpY(*mbN) >> blkIdx) & 1))
            tbCBF = ((mbN->cbf.luma8x8 >> loc.idx) & 1);
        break;
    case 7:
    case 8:
        loc = isA ? info.neighb4x4CblkA(cMB, blkIdx) : info.neighb4x4CblkB(cMB, blkIdx);
        mbN = info.getMB(loc.addr);

        if (!isSkipPCM(mbN) && ((info.cbpY(*mbN) >> (loc.idx >> 2)) & 1))
            tbCBF = mbN->transform_size_8x8_flag
                ? ((mbN->cbf.Cb8x8 >> (loc.idx >> 2)) & 1)
                : ((mbN->cbf.Cb4x4 >> loc.idx) & 1);
        break;
    case 9:
        loc = isA ? info.neighb8x8CblkA(cMB, blkIdx) : info.neighb8x8CblkB(cMB, blkIdx);
        mbN = info.getMB(loc.addr);

        if (!isSkipPCM(mbN) && mbN->transform_size_8x8_flag && ((info.cbpY(*mbN) >> blkIdx) & 1))
            tbCBF = ((mbN->cbf.Cb8x8 >> loc.idx) & 1);
        break;
    case 11:
    case 12:
        loc = isA ? info.neighb4x4CblkA(cMB, blkIdx) : info.neighb4x4CblkB(cMB, blkIdx);
        mbN = info.getMB(loc.addr);

        if (!isSkipPCM(mbN) && ((info.cbpY(*mbN) >> (loc.idx >> 2)) & 1))
            tbCBF = mbN->transform_size_8x8_flag
            ? ((mbN->cbf.Cr8x8 >> (loc.idx >> 2)) & 1)
            : ((mbN->cbf.Cr4x4 >> loc.idx) & 1);
        break;
    case 13:
        loc = isA ? info.neighb8x8CblkA(cMB, blkIdx) : info.neighb8x8CblkB(cMB, blkIdx);
        mbN = info.getMB(loc.addr);

        if (!isSkipPCM(mbN) && mbN->transform_size_8x8_flag && ((info.cbpY(*mbN) >> blkIdx) & 1))
            tbCBF = ((mbN->cbf.Cr8x8 >> loc.idx) & 1);
        break;
    default:
        throw InvalidSyntax();
    }

    Bs8u pm0 = info.MbPartPredMode(cMB, 0);

    if (   (!mbN && pm0 >= Pred_L0)
        || (mbN && tbCBF == -1 && mbN->MbType != I_PCM)
        /*TBD: sd partitioning check*/)
        return false;

    if (  (!mbN && pm0 < Pred_L0)
        || (mbN && mbN->MbType == I_PCM))
        return true;

    return tbCBF < 0 ? false : !!tbCBF;
}

bool CABAC::codedBlockFlag(Bs8u blkIdx, Bs8u ctxBlockCat, bool isCb)
{
    const Bs16s off[4] = { 85, 460, 472, 1012 }; //<5, <9, <13, 5 9 13
    const Bs16u s = 0; //no suffix for FL bins
    const Bs16u idx = ctxBlockCatIdx(ctxBlockCat);
    const Bs16u inc = ctxIdxBlockCatOffset[0][ctxBlockCat]
        + cbfCondTermFlagN(*m_info, blkIdx, ctxBlockCat, isCb, true)
        + 2 * cbfCondTermFlagN(*m_info, blkIdx, ctxBlockCat, isCb, false);
    Bin decoded = {};

    for (Bs16u i = 0; i < 2; i++)
    {
        Bin const & expected = BinFL1[i].b[s];

        while (decoded.n < expected.n)
            decoded += DecodeBin(off[idx], inc);

        if (decoded.b == expected.b)
            return !!BinFL1[i].v;
    }

    throw InvalidSyntax();
}

Bs16s scfCtxIdxInc(
    Info& info,
    Bs8u ctxBlockCat,
    Bs8u levelListIdx,
    bool last)
{
    bool frame = !info.cMB().mb_field_decoding_flag;

    if (ctxBlockCat == 3)
        return BS_MIN(levelListIdx / info.NumC8x8, 2);

    if (ctxBlockCat == 5 || ctxBlockCat == 9 || ctxBlockCat == 13)
        return ctxIdxIncScf8x8[levelListIdx][last ? 2 : !frame];

    return levelListIdx;
}

Bs16u scfOffIdx(Bs8u ctxBlockCat, bool field)
{
    Bs16u idx = 0;

         if (ctxBlockCat <   5) idx = 0;
    else if (ctxBlockCat ==  5) idx = 1;
    else if (ctxBlockCat <   9) idx = 2;
    else if (ctxBlockCat ==  9) idx = 3;
    else if (ctxBlockCat <  13) idx = 4;
    else if (ctxBlockCat == 13) idx = 5;
    else throw InvalidSyntax();

    return idx * 2 + field;
}

bool CABAC::SCF(Bs8u ctxBlockCat, Bs8u levelListIdx)
{
    const Bs16u off[12] = {105, 277, 402, 436, 484, 776, 690, 675, 528, 908, 718, 733};
    const Bs16u idx = scfOffIdx(ctxBlockCat, m_info->cMB().mb_field_decoding_flag);
    const Bs16u inc = ctxIdxBlockCatOffset[1][ctxBlockCat]
        + scfCtxIdxInc(*m_info, ctxBlockCat, levelListIdx, false);
    Bin decoded = {};

    for (Bs16u i = 0; i < 2; i++)
    {
        Bin const & expected = BinFL1[i].b[0];

        while (decoded.n < expected.n)
            decoded += DecodeBin(off[idx], inc);

        if (decoded.b == expected.b)
            return !!BinFL1[i].v;
    }

    throw InvalidSyntax();
}

bool CABAC::LSCF(Bs8u ctxBlockCat, Bs8u levelListIdx)
{
    const Bs16u off[12] = {166, 338, 417, 451, 572, 864, 690, 699, 616, 908, 748, 757};
    const Bs16u idx = scfOffIdx(ctxBlockCat, m_info->cMB().mb_field_decoding_flag);
    const Bs16u inc = ctxIdxBlockCatOffset[2][ctxBlockCat]
        + scfCtxIdxInc(*m_info, ctxBlockCat, levelListIdx, true);
    Bin decoded = {};

    for (Bs16u i = 0; i < 2; i++)
    {
        Bin const & expected = BinFL1[i].b[0];

        while (decoded.n < expected.n)
            decoded += DecodeBin(off[idx], inc);

        if (decoded.b == expected.b)
            return !!BinFL1[i].v;
    }

    throw InvalidSyntax();
}

inline Bs16s coeffAbsLevelInc(Bin& bin, Bs8u ctxBlockCat, Bs8u gt1, Bs8u eq1)
{
    if (!bin.n)
        return ctxIdxBlockCatOffset[3][ctxBlockCat] + (gt1 ? 0 : BS_MIN(4, 1 + eq1));

    return ctxIdxBlockCatOffset[3][ctxBlockCat] + 5 + BS_MIN(gt1, 4 - (ctxBlockCat == 3));
}

Bs16u CABAC::coeffAbsLevel(Bs8u ctxBlockCat, Bs8u gt1, Bs8u eq1)
{
    const Bs16s off[6][2] = { { 227, BYPASS }, { 426, BYPASS }, { 952, BYPASS }, { 708, BYPASS }, { 982, BYPASS }, { 766, BYPASS } };
    const Bs16u idx = scfOffIdx(ctxBlockCat, false) / 2;
    const BinPair* bins = BinCoeffAbsLvl;
    const Bs16u numBins = sizeof(BinCoeffAbsLvl) / sizeof(BinPair);
    BinPair res = {};

    for (Bs16u i = 0; i < numBins; i++)
    {
        Bs16u match = 0;

        for (Bs16s s = 0; s < 2; s++)
        {
            Bin const & expected = bins[i].b[s];
            Bin & decoded = res.b[s];

            while (decoded.n < expected.n)
                decoded += DecodeBin(off[idx][s], coeffAbsLevelInc(decoded, ctxBlockCat, gt1, eq1));

            if (decoded.b != expected.b)
                break;

            match++;
        }

        if (match == 2)
            return (Bs16u)bins[i].v + 1;
    }

    if (res.b[0].b != bins[numBins - 1].b[0].b)
        throw InvalidSyntax();

    Bin expected;
    Bin & decoded = res.b[1];

    for (Bs16u vabs = bins[numBins - 1].v + 1; vabs < 0xFFFF; vabs++)
    {
        UEGkS(expected, 0, vabs, 14, false);

        while (decoded.n < expected.n)
        {
            decoded += DecodeBypass();
            BinCount++;
        }

        if (decoded.b == expected.b)
            return vabs + 1;
    }

    throw InvalidSyntax();
}

bool CABAC::coeffSignFlag()
{
    Bin decoded = {};

    for (Bs16u i = 0; i < 2; i++)
    {
        Bin const & expected = BinFL1[i].b[0];

        while (decoded.n < expected.n)
        {
            decoded += DecodeBypass();
            BinCount++;
        }

        if (decoded.b == expected.b)
            return !!BinFL1[i].v;
    }

    throw InvalidSyntax();
}