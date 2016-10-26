//
// INTEL CORPORATION PROPRIETARY INFORMATION
//
// This software is supplied under the terms of a license agreement or
// nondisclosure agreement with Intel Corporation and may not be copied
// or disclosed except in accordance with the terms of that agreement.
//
// Copyright(C) 2014 Intel Corporation. All Rights Reserved.
//

#pragma once

#if 0
#define DEBUG_DUMP(x,...) printm(x, #x, __VA_ARGS__);
#else
#define DEBUG_DUMP
#endif

template <typename T>
T constexpr ABS(const T x) { return x > 0 ? x : -x; }

template <typename T>
T constexpr MAXABS(const T x, const T y) { return ABS(x) > ABS(y) ? ABS(x) : ABS(y); }
//Genereate EdgeDir 
//Template Input: edge search range, matrix size?content begin offset in IN matrix.
//input: median filtered input, 0, 2, 4, 6 are original lines while 1, 3, 5 are generated lines. 
//output: Edge direction of generated lines
//There should be borders in input to cover mindir and maxdir. For example, output is 8r16c, dir search is from -4 to 4, then input should be at least 17r24c, normally would be 17r32c to reduce intruction count
template<int MINDIR, int MAXDIR, int INOFFSET, uint INCOL, uint OUTROW, uint OUTCOL>
inline _GENX_ void CalculateEdgesIYUV_EdgeDir(matrix_ref<uchar, OUTROW * 2 + 1, INCOL> in, matrix_ref<char, OUTROW, OUTCOL> out) {
    static_assert(MAXDIR >= MINDIR, "Max search dir should be more than min dir");
    static_assert((INCOL >= (OUTCOL + MAXABS(MAXDIR, MINDIR) * 2)), "Input matrix should include output and max dir search range");
    matrix<short, OUTROW, OUTCOL> diff;
    matrix<short, OUTROW, OUTCOL> mindiff;
    matrix<ushort, OUTROW, OUTCOL> lessmask;
    mindiff = cm_abs<short>(cm_add<short>(in.template select<OUTROW, 2, OUTCOL, 1>(0, INOFFSET + MINDIR), -in.template select<OUTROW, 2, OUTCOL, 1>(2, INOFFSET - MINDIR)));
    out = MINDIR;
#pragma unroll
    for (int r = MINDIR + 1; r <= MAXDIR; r++) {
        diff = cm_abs<short>(cm_add<short>(in.template select<OUTROW, 2, OUTCOL, 1>(0, INOFFSET + r), -in.template select<OUTROW, 2, OUTCOL, 1>(2, INOFFSET - r)));
        lessmask = diff < mindiff;
        out.merge(r, lessmask);
        mindiff.merge(diff, lessmask);
    }
}

//Genereate Edge Candidate instead of edge dir. Avoid indirect access, but more operations
//Template Input: edge search range, matrix size?content begin offset in IN matrix.
//input: median filtered input, 0, 2, 4, 6 are original lines while 1, 3, 5 are generated lines. 
//output: Edge fix candidates of generated lines
//There should be borders in input to cover mindir and maxdir. For example, output is 8r16c, dir search is from -4 to 4, then input should be at least 17r24c, normally would be 17r32c to reduce intruction count
template<int MINDIR, int MAXDIR, int STEP, int INOFFSET, uint INCOL, uint OUTROW, uint OUTCOL, typename RT>
inline _GENX_ void CalculateEdgesIYUV_EdgeCandidate(matrix_ref<uchar, OUTROW * 2 + 1, INCOL> in, matrix_ref<RT, OUTROW, OUTCOL> out) {
    static_assert(MAXDIR >= MINDIR, "Max search dir should be more than min dir");
    static_assert((INCOL >= (OUTCOL + MAXABS(MAXDIR, MINDIR)*2)), "Input matrix should include output and max dir search range");
    matrix<short, OUTROW, OUTCOL> diff;
    matrix<short, OUTROW, OUTCOL> mindiff;
    matrix<ushort, OUTROW, OUTCOL> lessmask;
    matrix<short, OUTROW, OUTCOL> sum, minsum;
    mindiff = cm_abs<short>(cm_add<short>(in.template select<OUTROW, 2, OUTCOL, 1>(0, INOFFSET + MINDIR), -in.template select<OUTROW, 2, OUTCOL, 1>(2, INOFFSET - MINDIR)));
    minsum = in.template select<OUTROW, 2, OUTCOL, 1>(0, INOFFSET + MINDIR) + in.template select<OUTROW, 2, OUTCOL, 1>(2, INOFFSET - MINDIR);
#pragma unroll
    for (int r = MINDIR + STEP; r <= MAXDIR; r+=STEP) {
        diff = cm_abs<short>(cm_add<short>(in.template select<OUTROW, 2, OUTCOL, 1>(0, INOFFSET + r), -in.template select<OUTROW, 2, OUTCOL, 1>(2, INOFFSET - r)));
        lessmask = diff < mindiff;
        minsum.merge(in.template select<OUTROW, 2, OUTCOL, 1>(0, INOFFSET + r) + in.template select<OUTROW, 2, OUTCOL, 1>(2, INOFFSET - r), lessmask);
        mindiff.merge(diff, lessmask);
    }
    out = cm_shr<RT>(cm_add<short>(minsum, 1), 1);
}
#if 0 // compiler bug for constant propogation, had to manual unroll.
//Check if badmc is not empty, then calculate edge candidate and filter by it
//Template Input: edge search range, matrix size?content begin offset in IN matrix
//Input: badmc matrix, median filtered input, 0, 2, 4, 6 are original lines while 1, 3, 5 are generated lines, protection limit
//Output: updated IN image with edge fixed, content is in in.select<OUTROW, 2, OUTCOL, 1>(1, INOFFSET);
template<int MINDIR, int MAXDIR, int STEP, int INOFFSET, uint INCOL, uint OUTROW, uint OUTCOL>
_GENX_ inline void FixEdgeDirectionalIYUV(matrix_ref<ushort, OUTROW, OUTCOL> badmc, matrix_ref<uchar, OUTROW * 2 + 1, INCOL> in, uint prot) {
    static_assert(MAXDIR >= MINDIR, "Max search dir should be more than min dir");
#if 1
    matrix<short, 1, OUTCOL> candidateEdge;
    matrix<short, 1, OUTCOL> candidateEdgeMinus;
    matrix<short, 1, OUTCOL> candidateEdgePlus;
    matrix<short, 1, OUTCOL> candidateLinear;
    matrix<short, 1, OUTCOL> edgeCandidateOutProt;
#pragma unroll
    for (int i = 0; i<OUTROW; i++) {
        if (badmc.row(i).any()) {
            CalculateEdgesIYUV_EdgeCandidate<MINDIR, MAXDIR, STEP, INOFFSET>(in.template select<3, 1, INCOL, 1>(i * 2), candidateEdge.select_all());
            candidateEdgeMinus = candidateEdge - prot;
            candidateEdgePlus = candidateEdge + prot;
            candidateLinear = cm_shr<short>(cm_add<short>(in.template select<1, 1, OUTCOL, 1>(i * 2, INOFFSET), in.template select<1, 2, OUTCOL, 1>(i * 2 + 2, INOFFSET)), 1);
            edgeCandidateOutProt = ((candidateEdgeMinus > in.template select<1, 1, OUTCOL, 1>(i * 2, INOFFSET)) & (candidateEdgeMinus > in.template select<1, 2, OUTCOL, 1>(i * 2 + 2, INOFFSET)));
            candidateEdge.merge(candidateLinear, edgeCandidateOutProt);
            edgeCandidateOutProt = ((candidateEdgePlus < in.template select<1, 1, OUTCOL, 1>(i * 2, INOFFSET)) & (candidateEdgePlus < in.template select<1, 2, OUTCOL, 1>(i * 2 + 2, INOFFSET)));
            candidateEdge.merge(candidateLinear, edgeCandidateOutProt);
            in.template select<1, 1, OUTCOL, 1>(i * 2 + 1, INOFFSET).merge(candidateEdge, badmc.row(i));
        }
    }

#else
    if (badmc.any()) {
        matrix<short, OUTROW, OUTCOL> candidateEdge;
        matrix<short, OUTROW, OUTCOL> candidateEdgeMinus;
        matrix<short, OUTROW, OUTCOL> candidateEdgePlus;
        matrix<short, OUTROW, OUTCOL> candidateLinear;
        matrix<short, OUTROW, OUTCOL> edgeCandidateOutProt;
        CalculateEdgesIYUV_EdgeCandidate<MINDIR, MAXDIR, STEP, INOFFSET>(in, candidateEdge.select_all());
        candidateEdgeMinus = candidateEdge - prot; 
        candidateEdgePlus = candidateEdge + prot;
        candidateLinear = cm_shr<short>(cm_add<short>(in.template select<OUTROW, 2, OUTCOL, 1>(0, INOFFSET), in.template select<OUTROW, 2, OUTCOL, 1>(2, INOFFSET)), 1);
        edgeCandidateOutProt = (candidateEdgeMinus > in.template select<OUTROW, 2, OUTCOL, 1>(0, INOFFSET) & candidateEdgeMinus > in.template select<OUTROW, 2, OUTCOL, 1>(2, INOFFSET));
        candidateEdge.merge(candidateLinear, edgeCandidateOutProt);
        edgeCandidateOutProt = (candidateEdgePlus < in.template select<OUTROW, 2, OUTCOL, 1>(0, INOFFSET) & candidateEdgePlus < in.template select<OUTROW, 2, OUTCOL, 1>(2, INOFFSET));
        candidateEdge.merge(candidateLinear, edgeCandidateOutProt);
        in.template select<OUTROW, 2, OUTCOL, 1>(1, INOFFSET).merge(candidateEdge, badmc);
    }
#endif
}
#else
//Genereate Edge Candidate instead of edge dir. Avoid indirect access, but more operations
//Template Input: edge search range, matrix size?content begin offset in IN matrix.
//input: median filtered input, 0, 2, 4, 6 are original lines while 1, 3, 5 are generated lines. 
//output: Edge fix candidates of generated lines
//There should be borders in input to cover mindir and maxdir. For example, output is 8r16c, dir search is from -4 to 4, then input should be at least 17r24c, normally would be 17r32c to reduce intruction count
template<int INOFFSET, uint INCOL, uint OUTROW, uint OUTCOL, typename RT>
inline _GENX_ void CalculateEdgesIYUV_EdgeCandidate_unroll_2(matrix_ref<uchar, OUTROW * 2 + 1, INCOL> in, matrix_ref<RT, OUTROW, OUTCOL> out) {
    //TODO: might need to move min sum out of loop for better performance.
    matrix<short, OUTROW, OUTCOL> diff;
    matrix<short, OUTROW, OUTCOL> mindiff;
    matrix<ushort, OUTROW, OUTCOL> lessmask;
    matrix<short, OUTROW, OUTCOL> sum, minsum;
    {
        int r = -2;
        mindiff = cm_abs<short>(cm_add<short>(in.template select<OUTROW, 2, OUTCOL, 1>(0, INOFFSET + r), -in.template select<OUTROW, 2, OUTCOL, 1>(2, INOFFSET - r)));
        minsum = in.template select<OUTROW, 2, OUTCOL, 1>(0, INOFFSET + r) + in.template select<OUTROW, 2, OUTCOL, 1>(2, INOFFSET - r);
    }
    {
        int r = -1;
        diff = cm_abs<short>(cm_add<short>(in.template select<OUTROW, 2, OUTCOL, 1>(0, INOFFSET + r), - in.template select<OUTROW, 2, OUTCOL, 1>(2, INOFFSET - r)));
        lessmask = diff < mindiff;
        minsum.merge(in.template select<OUTROW, 2, OUTCOL, 1>(0, INOFFSET + r) + in.template select<OUTROW, 2, OUTCOL, 1>(2, INOFFSET - r), lessmask);
        mindiff.merge(diff, lessmask);
    }
    {
        int r = 0;
        diff = cm_abs<short>(cm_add<short>(in.template select<OUTROW, 2, OUTCOL, 1>(0, INOFFSET + r), -in.template select<OUTROW, 2, OUTCOL, 1>(2, INOFFSET - r)));
        lessmask = diff < mindiff;
        minsum.merge(in.template select<OUTROW, 2, OUTCOL, 1>(0, INOFFSET + r) + in.template select<OUTROW, 2, OUTCOL, 1>(2, INOFFSET - r), lessmask);
        mindiff.merge(diff, lessmask);
    }
    {
        int r = 1;
        diff = cm_abs<short>(cm_add<short>(in.template select<OUTROW, 2, OUTCOL, 1>(0, INOFFSET + r), -in.template select<OUTROW, 2, OUTCOL, 1>(2, INOFFSET - r)));
        lessmask = diff < mindiff;
        minsum.merge(in.template select<OUTROW, 2, OUTCOL, 1>(0, INOFFSET + r) + in.template select<OUTROW, 2, OUTCOL, 1>(2, INOFFSET - r), lessmask);
        mindiff.merge(diff, lessmask);
    }
    {
        int r = 2;
        diff = cm_abs<short>(cm_add<short>(in.template select<OUTROW, 2, OUTCOL, 1>(0, INOFFSET + r), -in.template select<OUTROW, 2, OUTCOL, 1>(2, INOFFSET - r)));
        lessmask = diff < mindiff;
        minsum.merge(in.template select<OUTROW, 2, OUTCOL, 1>(0, INOFFSET + r) + in.template select<OUTROW, 2, OUTCOL, 1>(2, INOFFSET - r), lessmask);
        mindiff.merge(diff, lessmask);
    }
    out = cm_shr<RT>(cm_add<short>(minsum, 1), 1);
}

template<int INOFFSET, uint INCOL, uint OUTROW, uint OUTCOL, typename RT>
inline _GENX_ void CalculateEdgesIYUV_EdgeCandidate_unroll_4_by2(matrix_ref<uchar, OUTROW * 2 + 1, INCOL> in, matrix_ref<RT, OUTROW, OUTCOL> out) {
    //TODO: might need to move min sum out of loop for better performance.
    matrix<short, OUTROW, OUTCOL> diff;
    matrix<short, OUTROW, OUTCOL> mindiff;
    matrix<ushort, OUTROW, OUTCOL> lessmask;
    matrix<short, OUTROW, OUTCOL> sum, minsum;
    {
        int r = -4;
        mindiff = cm_abs<short>(cm_add<short>(in.template select<OUTROW, 2, OUTCOL, 1>(0, INOFFSET + r), -in.template select<OUTROW, 2, OUTCOL, 1>(2, INOFFSET - r)));
        minsum = in.template select<OUTROW, 2, OUTCOL, 1>(0, INOFFSET + r) + in.template select<OUTROW, 2, OUTCOL, 1>(2, INOFFSET - r);
    }
    {
        int r = -2;
        diff = cm_abs<short>(cm_add<short>(in.template select<OUTROW, 2, OUTCOL, 1>(0, INOFFSET + r), - in.template select<OUTROW, 2, OUTCOL, 1>(2, INOFFSET - r)));
        lessmask = diff < mindiff;
        minsum.merge(in.template select<OUTROW, 2, OUTCOL, 1>(0, INOFFSET + r) + in.template select<OUTROW, 2, OUTCOL, 1>(2, INOFFSET - r), lessmask);
        mindiff.merge(diff, lessmask);
    }
    {
        int r = 0;
        diff = cm_abs<short>(cm_add<short>(in.template select<OUTROW, 2, OUTCOL, 1>(0, INOFFSET + r), -in.template select<OUTROW, 2, OUTCOL, 1>(2, INOFFSET - r)));
        lessmask = diff < mindiff;
        minsum.merge(in.template select<OUTROW, 2, OUTCOL, 1>(0, INOFFSET + r) + in.template select<OUTROW, 2, OUTCOL, 1>(2, INOFFSET - r), lessmask);
        mindiff.merge(diff, lessmask);
    }
    {
        int r = 2;
        diff = cm_abs<short>(cm_add<short>(in.template select<OUTROW, 2, OUTCOL, 1>(0, INOFFSET + r), -in.template select<OUTROW, 2, OUTCOL, 1>(2, INOFFSET - r)));
        lessmask = diff < mindiff;
        minsum.merge(in.template select<OUTROW, 2, OUTCOL, 1>(0, INOFFSET + r) + in.template select<OUTROW, 2, OUTCOL, 1>(2, INOFFSET - r), lessmask);
        mindiff.merge(diff, lessmask);
    }
    {
        int r = 4;
        diff = cm_abs<short>(cm_add<short>(in.template select<OUTROW, 2, OUTCOL, 1>(0, INOFFSET + r), -in.template select<OUTROW, 2, OUTCOL, 1>(2, INOFFSET - r)));
        lessmask = diff < mindiff;
        minsum.merge(in.template select<OUTROW, 2, OUTCOL, 1>(0, INOFFSET + r) + in.template select<OUTROW, 2, OUTCOL, 1>(2, INOFFSET - r), lessmask);
        mindiff.merge(diff, lessmask);
    }
    out = cm_shr<RT>(cm_add<short>(minsum, 1), 1);
}
//Genereate Edge Candidate instead of edge dir. Avoid indirect access, but more operations
//Template Input: edge search range, matrix size?content begin offset in IN matrix.
//input: median filtered input, 0, 2, 4, 6 are original lines while 1, 3, 5 are generated lines. 
//output: Edge fix candidates of generated lines
//There should be borders in input to cover mindir and maxdir. For example, output is 8r16c, dir search is from -4 to 4, then input should be at least 17r24c, normally would be 17r32c to reduce intruction count
template<int INOFFSET, uint INCOL, uint OUTROW, uint OUTCOL, typename RT>
inline _GENX_ void CalculateEdgesIYUV_EdgeCandidate_unroll_4(matrix_ref<uchar, OUTROW * 2 + 1, INCOL> in, matrix_ref<RT, OUTROW, OUTCOL> out) {
    //TODO: might need to move min sum out of loop for better performance.
    matrix<short, OUTROW, OUTCOL> diff;
    matrix<short, OUTROW, OUTCOL> mindiff;
    matrix<ushort, OUTROW, OUTCOL> lessmask;
    matrix<short, OUTROW, OUTCOL> sum, minsum;
    {
        int r = -4;
        mindiff = cm_abs<short>(cm_add<short>(in.template select<OUTROW, 2, OUTCOL, 1>(0, INOFFSET + r), -in.template select<OUTROW, 2, OUTCOL, 1>(2, INOFFSET - r)));
        minsum = in.template select<OUTROW, 2, OUTCOL, 1>(0, INOFFSET + r) + in.template select<OUTROW, 2, OUTCOL, 1>(2, INOFFSET - r);
    }
    {
        int r = -3;
        diff = cm_abs<short>(cm_add<short>(in.template select<OUTROW, 2, OUTCOL, 1>(0, INOFFSET + r), -in.template select<OUTROW, 2, OUTCOL, 1>(2, INOFFSET - r)));
        lessmask = diff < mindiff;
        minsum.merge(in.template select<OUTROW, 2, OUTCOL, 1>(0, INOFFSET + r) + in.template select<OUTROW, 2, OUTCOL, 1>(2, INOFFSET - r), lessmask);
        mindiff.merge(diff, lessmask);
    }
    {
        int r = -2;
        diff = cm_abs<short>(cm_add<short>(in.template select<OUTROW, 2, OUTCOL, 1>(0, INOFFSET + r), -in.template select<OUTROW, 2, OUTCOL, 1>(2, INOFFSET - r)));
        lessmask = diff < mindiff;
        minsum.merge(in.template select<OUTROW, 2, OUTCOL, 1>(0, INOFFSET + r) + in.template select<OUTROW, 2, OUTCOL, 1>(2, INOFFSET - r), lessmask);
        mindiff.merge(diff, lessmask);
    }
    {
        int r = -1;
        diff = cm_abs<short>(cm_add<short>(in.template select<OUTROW, 2, OUTCOL, 1>(0, INOFFSET + r), -in.template select<OUTROW, 2, OUTCOL, 1>(2, INOFFSET - r)));
        lessmask = diff < mindiff;
        minsum.merge(in.template select<OUTROW, 2, OUTCOL, 1>(0, INOFFSET + r) + in.template select<OUTROW, 2, OUTCOL, 1>(2, INOFFSET - r), lessmask);
        mindiff.merge(diff, lessmask);
    }
    {
        int r = 0;
        diff = cm_abs<short>(cm_add<short>(in.template select<OUTROW, 2, OUTCOL, 1>(0, INOFFSET + r), -in.template select<OUTROW, 2, OUTCOL, 1>(2, INOFFSET - r)));
        lessmask = diff < mindiff;
        minsum.merge(in.template select<OUTROW, 2, OUTCOL, 1>(0, INOFFSET + r) + in.template select<OUTROW, 2, OUTCOL, 1>(2, INOFFSET - r), lessmask);
        mindiff.merge(diff, lessmask);
    }
    {
        int r = 1;
        diff = cm_abs<short>(cm_add<short>(in.template select<OUTROW, 2, OUTCOL, 1>(0, INOFFSET + r), -in.template select<OUTROW, 2, OUTCOL, 1>(2, INOFFSET - r)));
        lessmask = diff < mindiff;
        minsum.merge(in.template select<OUTROW, 2, OUTCOL, 1>(0, INOFFSET + r) + in.template select<OUTROW, 2, OUTCOL, 1>(2, INOFFSET - r), lessmask);
        mindiff.merge(diff, lessmask);
    }
    {
        int r = 2;
        diff = cm_abs<short>(cm_add<short>(in.template select<OUTROW, 2, OUTCOL, 1>(0, INOFFSET + r), -in.template select<OUTROW, 2, OUTCOL, 1>(2, INOFFSET - r)));
        lessmask = diff < mindiff;
        minsum.merge(in.template select<OUTROW, 2, OUTCOL, 1>(0, INOFFSET + r) + in.template select<OUTROW, 2, OUTCOL, 1>(2, INOFFSET - r), lessmask);
        mindiff.merge(diff, lessmask);
    }
    {
        int r = 3;
        diff = cm_abs<short>(cm_add<short>(in.template select<OUTROW, 2, OUTCOL, 1>(0, INOFFSET + r), -in.template select<OUTROW, 2, OUTCOL, 1>(2, INOFFSET - r)));
        lessmask = diff < mindiff;
        minsum.merge(in.template select<OUTROW, 2, OUTCOL, 1>(0, INOFFSET + r) + in.template select<OUTROW, 2, OUTCOL, 1>(2, INOFFSET - r), lessmask);
        mindiff.merge(diff, lessmask);
    }
    {
        int r = 4;
        diff = cm_abs<short>(cm_add<short>(in.template select<OUTROW, 2, OUTCOL, 1>(0, INOFFSET + r), -in.template select<OUTROW, 2, OUTCOL, 1>(2, INOFFSET - r)));
        lessmask = diff < mindiff;
        minsum.merge(in.template select<OUTROW, 2, OUTCOL, 1>(0, INOFFSET + r) + in.template select<OUTROW, 2, OUTCOL, 1>(2, INOFFSET - r), lessmask);
        mindiff.merge(diff, lessmask);
    }

    out = cm_shr<RT>(cm_add<short>(minsum, 1), 1);
}

//Check if badmc is not empty, then calculate edge candidate and filter by it
//Template Input: edge search range, matrix size?content begin offset in IN matrix
//Input: badmc matrix, median filtered input, 0, 2, 4, 6 are original lines while 1, 3, 5 are generated lines, protection limit
//Output: updated IN image with edge fixed, content is in in.select<OUTROW, 2, OUTCOL, 1>(1, INOFFSET);
template<int MINDIR, int MAXDIR, int STEP, int INOFFSET, uint INCOL, uint OUTROW, uint OUTCOL>
_GENX_ inline void FixEdgeDirectionalIYUV(matrix_ref<ushort, OUTROW, OUTCOL> badmc, matrix_ref<uchar, OUTROW * 2 + 1, INCOL> in, short prot) {
#if 1 // for 15*32 input, 45 register, 455 instructions
    matrix<short, 1, OUTCOL> candidateEdge;
    matrix<short, 1, OUTCOL> candidateEdgeMinus;
    matrix<short, 1, OUTCOL> candidateEdgePlus;
    matrix<short, 1, OUTCOL> candidateLinear;
    matrix<short, 1, OUTCOL> edgeCandidateOutProt;
#pragma unroll
    for (int i = 0; i<OUTROW; i++) {
        if (badmc.row(i).any()) {
#if 1
            CalculateEdgesIYUV_EdgeCandidate<MINDIR, MAXDIR, STEP, INOFFSET>(in.template select<3, 1, INCOL, 1>(i * 2), candidateEdge.select_all());
#else
            if (MINDIR == -4 && MAXDIR == 4 && STEP == 1)
                CalculateEdgesIYUV_EdgeCandidate_unroll_4<INOFFSET>(in.template select<3, 1, INCOL, 1>(i * 2), candidateEdge.select_all());
            else if (MINDIR == -4 && MAXDIR == 4 && STEP == 2) //TODO: Need to check if combine UV processing is faster or not.
                CalculateEdgesIYUV_EdgeCandidate_unroll_4_by2<INOFFSET>(in.template select<3, 1, INCOL, 1>(i * 2), candidateEdge.select_all());
            else if (MINDIR == -2 && MAXDIR == 2 && STEP == 1)
                CalculateEdgesIYUV_EdgeCandidate_unroll_2<INOFFSET>(in.template select<3, 1, INCOL, 1>(i * 2), candidateEdge.select_all());
#endif
            candidateEdgeMinus = candidateEdge - prot;
            candidateEdgePlus = candidateEdge + prot;
            candidateLinear = cm_shr<short>(cm_add<short>(in.template select<1, 1, OUTCOL, 1>(i * 2, INOFFSET), in.template select<1, 2, OUTCOL, 1>(i * 2 + 2, INOFFSET)), 1);
            edgeCandidateOutProt = ((candidateEdgeMinus > in.template select<1, 1, OUTCOL, 1>(i * 2, INOFFSET)) & (candidateEdgeMinus > in.template select<1, 2, OUTCOL, 1>(i * 2 + 2, INOFFSET)));
            candidateEdge.merge(candidateLinear, edgeCandidateOutProt);
            edgeCandidateOutProt = ((candidateEdgePlus < in.template select<1, 1, OUTCOL, 1>(i * 2, INOFFSET)) & (candidateEdgePlus < in.template select<1, 2, OUTCOL, 1>(i * 2 + 2, INOFFSET)));
            candidateEdge.merge(candidateLinear, edgeCandidateOutProt);
            in.template select<1, 1, OUTCOL, 1>(i * 2 + 1, INOFFSET).merge(candidateEdge, badmc.row(i));
            //out.row(i).merge(candidateEdge, in.template select<1, 2, OUTCOL, 1>(i * 2 + 1, INOFFSET), badmc.row(i));
        }
    }
#else // for 15*32 input, 81 register, 468 instructions
    if (badmc.any()) {
        matrix<short, OUTROW, OUTCOL> candidateEdge;
        matrix<short, OUTROW, OUTCOL> candidateEdgeMinus;
        matrix<short, OUTROW, OUTCOL> candidateEdgePlus;
        matrix<short, OUTROW, OUTCOL> candidateLinear;
        matrix<short, OUTROW, OUTCOL> edgeCandidateOutProt;
        if (MINDIR == -4 && MAXDIR == 4 && STEP == 1)
            CalculateEdgesIYUV_EdgeCandidate_unroll_4<INOFFSET>(in, candidateEdge.select_all());
        else if (MINDIR == -4 && MAXDIR == 4 && STEP == 2)
            CalculateEdgesIYUV_EdgeCandidate_unroll_4_by2<INOFFSET>(in, candidateEdge.select_all());
        candidateEdgeMinus = candidateEdge - prot; 
        candidateEdgePlus = candidateEdge + prot;
        candidateLinear = cm_shr<short>(cm_add<short>(in.template select<OUTROW, 2, OUTCOL, 1>(0, INOFFSET), in.template select<OUTROW, 2, OUTCOL, 1>(2, INOFFSET)), 1);
        edgeCandidateOutProt = (candidateEdgeMinus > in.template select<OUTROW, 2, OUTCOL, 1>(0, INOFFSET) & candidateEdgeMinus > in.template select<OUTROW, 2, OUTCOL, 1>(2, INOFFSET));
        candidateEdge.merge(candidateLinear, edgeCandidateOutProt);
        edgeCandidateOutProt = (candidateEdgePlus < in.template select<OUTROW, 2, OUTCOL, 1>(0, INOFFSET) & candidateEdgePlus < in.template select<OUTROW, 2, OUTCOL, 1>(2, INOFFSET));
        candidateEdge.merge(candidateLinear, edgeCandidateOutProt);
        in.template select<OUTROW, 2, OUTCOL, 1>(1, INOFFSET).merge(candidateEdge, badmc);
        //out.merge(candidateEdge, in.template select<OUTROW, 2, OUTCOL, 1>(1, INOFFSET), badmc);
    }
#endif
}

//Check if badmc is not empty, then calculate edge candidate and filter by it
//Template Input: edge search range, matrix size?content begin offset in IN matrix
//Input: badmc matrix, median filtered input, 0, 2, 4, 6 are original lines while 1, 3, 5 are generated lines, protection limit
//Output: updated IN image with edge fixed, content is in in.select<OUTROW, 2, OUTCOL, 1>(1, INOFFSET);
template<int MINDIR, int MAXDIR, int STEP, int INOFFSET, uint INCOL, uint OUTROW, uint OUTCOLPACKED>
_GENX_ inline void FixEdgeDirectionalIYUV_PackedBadMC(matrix_ref<ushort, OUTROW, OUTCOLPACKED> badmc, matrix_ref<uchar, OUTROW * 2 + 1, INCOL> in, short prot) {
    const uint OUTCOL = OUTCOLPACKED * 16;
    matrix<short, 1, OUTCOL> candidateEdge;
    matrix<short, 1, OUTCOL> candidateEdgeMinus;
    matrix<short, 1, OUTCOL> candidateEdgePlus;
    matrix<short, 1, OUTCOL> candidateLinear;
    matrix<short, 1, OUTCOL> edgeCandidateOutProt;
#pragma unroll
    for (int i = 0; i<OUTROW; i++) {
        if (badmc.row(i).any()) {
            CalculateEdgesIYUV_EdgeCandidate<MINDIR, MAXDIR, STEP, INOFFSET>(in.template select<3, 1, INCOL, 1>(i * 2), candidateEdge.select_all());
            candidateEdgeMinus = candidateEdge - prot;
            candidateEdgePlus = candidateEdge + prot;
            candidateLinear = cm_shr<short>(cm_add<short>(in.template select<1, 1, OUTCOL, 1>(i * 2, INOFFSET), in.template select<1, 2, OUTCOL, 1>(i * 2 + 2, INOFFSET)), 1);
            edgeCandidateOutProt = ((candidateEdgeMinus > in.template select<1, 1, OUTCOL, 1>(i * 2, INOFFSET)) & (candidateEdgeMinus > in.template select<1, 2, OUTCOL, 1>(i * 2 + 2, INOFFSET)));
            candidateEdge.merge(candidateLinear, edgeCandidateOutProt);
            edgeCandidateOutProt = ((candidateEdgePlus < in.template select<1, 1, OUTCOL, 1>(i * 2, INOFFSET)) & (candidateEdgePlus < in.template select<1, 2, OUTCOL, 1>(i * 2 + 2, INOFFSET)));
            candidateEdge.merge(candidateLinear, edgeCandidateOutProt);
#pragma unroll
            for (int j = 0; j < OUTCOLPACKED; j++)
                in.template select<1, 1, 16, 1>(i * 2 + 1, INOFFSET + j*16).merge(candidateEdge, badmc[i][j]);
            //out.row(i).merge(candidateEdge, in.template select<1, 2, OUTCOL, 1>(i * 2 + 1, INOFFSET), badmc.row(i));
        }
    }
}
#endif