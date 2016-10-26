//
// INTEL CORPORATION PROPRIETARY INFORMATION
//
// This software is supplied under the terms of a license agreement or
// nondisclosure agreement with Intel Corporation and may not be copied
// or disclosed except in accordance with the terms of that agreement.
//
// Copyright(C) 2014 Intel Corporation. All Rights Reserved.
//

#include <cm/cm.h>
#include "deinterlace_genx.h"
#include "common_genx.h"
#pragma warning (disable:2415)


_GENX_MAIN_ void CalculateEdgesIYUV_EdgeDir_Test() {
    int count = 0;
    {
        printf("Case: %d---------------------------------------------------------------------------------------------\n", count++);
        matrix<uchar, 8, 32> in = 3;
        matrix<char, 3, 16> out = 0;
        matrix<char, 3, 16> expectout = -4;

        CalculateEdgesIYUV_EdgeDir<-4, 4, 4>(in.select<7, 1, 32, 1>(0, 0), out.select_all());

        (out == expectout).all() ? printf("Pass!") : printm(out);
        printf("\n");
    }
    {
        printf("Case: %d---------------------------------------------------------------------------------------------\n", count++);
        matrix<uchar, 8, 32> in = 3;
        matrix<char, 3, 16> out = 0;
        matrix<char, 3, 16> expectout = -4;

        in[0][5] = 0;
        in[2][5] = 0;
        expectout[0][5] = -3;
        expectout[1][5] = -3;
        CalculateEdgesIYUV_EdgeDir<-4, 4, 4>(in.select<7, 1, 32, 1>(0, 0), out.select_all());

        (out == expectout).all() ? printf("Pass!") : printm(out);
        printf("\n");
    }
    {
        printf("Case: %d---------------------------------------------------------------------------------------------\n", count++);
        matrix<uchar, 8, 32> in = 3;
        matrix<char, 3, 16> out = 0;
        matrix<char, 3, 16> expectout = -4;

        in.row(2) = 0;
        in[2][9] = 3;
        in[0][10] = 0;
        expectout[0][2] = -3;
        expectout[0][3] = -2;
        expectout[0][4] = -1;
        expectout[0][5] = 0;
        expectout[0][6] = 0;
        expectout[0][7] = -1;
        expectout[0][8] = -2;
        expectout[0][9] = -3;

        expectout[1][1] = 4;
        expectout[1][2] = 3;
        expectout[1][3] = 2;
        expectout[1][4] = 1;
        expectout[1][5] = 0;
        expectout[1][6] = -1;
        expectout[1][7] = -2;
        expectout[1][8] = -3;
        expectout[1][9] = -4;

        CalculateEdgesIYUV_EdgeDir<-4, 4, 4>(in.select<7, 1, 32, 1>(0, 0), out.select_all());

        (out == expectout).all() ? printf("Pass!") : printm(out);
        printf("\n");
    }
    {
        printf("Case: %d---------------------------------------------------------------------------------------------\n", count++);
        matrix<uchar, 8, 32> in = 3;
        matrix<char, 3, 16> out = 0;
        matrix<char, 3, 16> expectout = -2;

        CalculateEdgesIYUV_EdgeDir<-2, 2, 2>(in.select<7, 1, 32, 1>(0, 0), out.select_all());

        (out == expectout).all() ? printf("Pass!") : printm(out);
        printf("\n");
    }
    {
        printf("Case: %d---------------------------------------------------------------------------------------------\n", count++);
        matrix<uchar, 8, 32> in = 3;
        matrix<char, 3, 16> out = 0;
        matrix<char, 3, 16> expectout = -2;

        in[0][5] = 0;
        in[2][5] = 0;
        expectout[0][1] = -1;
        expectout[0][5] = -1;
        expectout[1][5] = -1;
        CalculateEdgesIYUV_EdgeDir<-2, 2, 2>(in.select<7, 1, 32, 1>(0, 0), out.select_all());

        (out == expectout).all() ? printf("Pass!") : printm(out);
        printf("\n");
    }
    {
        printf("Case: %d---------------------------------------------------------------------------------------------\n", count++);
        matrix<uchar, 8, 32> in = 3;
        matrix<char, 3, 16> out = 0;
        matrix<char, 3, 16> expectout = -2;

        in.row(2) = 0;
        in[2][9] = 3;
        in[0][10] = 0;
        in[0][5] = 0;
        expectout[0][1] = 2;
        expectout[0][2] = 1;
        expectout[0][3] = 0;
        expectout[0][4] =-1;
        expectout[0][5] =-2;
        expectout[0][6] =-1;
        expectout[0][7] = 0;
        expectout[0][8] = 0;
        expectout[0][9] =-1;

        expectout[1][5] = +2;
        expectout[1][6] = +1;
        expectout[1][7] = +0;
        expectout[1][8] = -1;
        expectout[1][9] = -2;

        CalculateEdgesIYUV_EdgeDir<-2, 2, 2>(in.select<7, 1, 32, 1>(0, 0), out.select_all());

        (out == expectout).all() ? printf("Pass!") : printm(out);
        printf("\n");
    }
}

_GENX_MAIN_ void CalculateEdgesIYUV_EdgeCandidate_Test() {
    int count = 0;
    {
        printf("Case: %d---------------------------------------------------------------------------------------------\n", count++);
        matrix<uchar, 8, 32> in = 3;
        matrix<uchar, 3, 16> out = 0;
        matrix<uchar, 3, 16> expectout = 3;

        CalculateEdgesIYUV_EdgeCandidate<-4, 4, 1, 4>(in.select<7, 1, 32, 1>(0, 0), out.select_all());

        (out == expectout).all() ? printf("Pass!") : printm(out);
        printf("\n");
    }
    {
        printf("Case: %d---------------------------------------------------------------------------------------------\n", count++);
        matrix<uchar, 8, 32> in = 3;
        matrix<uchar, 3, 16> out = 0;
        matrix<uchar, 3, 16> expectout = 3;

        in[0][5] = 0;
        in[2][5] = 0;
        CalculateEdgesIYUV_EdgeCandidate<-4, 4, 1, 4>(in.select<7, 1, 32, 1>(0, 0), out.select_all());

        (out == expectout).all() ? printf("Pass!") : printm(out);
        printf("\n");
    }
    {
        printf("Case: %d---------------------------------------------------------------------------------------------\n", count++);
        matrix<uchar, 8, 32> in = 3;
        matrix<uchar, 3, 16> out = 0;
        matrix<uchar, 3, 16> expectout = 2;

        in.row(2) = 0;
        in[0][10] = 0;
        in[2][9] = 3;
        expectout[0][1] = 3;
        expectout[0][2] = 3;
        expectout[0][3] = 3;
        expectout[0][4] = 3;
        expectout[0][5] = 3;
        expectout[0][6] = 0;
        expectout[0][7] = 0;
        expectout[0][8] = 0;
        expectout[0][9] = 0;
        expectout[0][10] = 0;

        expectout[1][1] = 3;
        expectout[1][2] = 3;
        expectout[1][3] = 3;
        expectout[1][4] = 3;
        expectout[1][5] = 3;
        expectout[1][6] = 3;
        expectout[1][7] = 3;
        expectout[1][8] = 3;
        expectout[1][9] = 3;
        expectout.row(2) = 3;

        CalculateEdgesIYUV_EdgeCandidate<-4, 4, 1, 4>(in.select<7, 1, 32, 1>(0, 0), out.select_all());

        (out == expectout).all() ? printf("Pass!") : printm(out);
        printf("\n");
    }
    {
        printf("Case: %d---------------------------------------------------------------------------------------------\n", count++);
        matrix<uchar, 8, 32> in = 3;
        matrix<uchar, 3, 16> out = 0;
        matrix<uchar, 3, 16> expectout = 3;

        CalculateEdgesIYUV_EdgeCandidate<-2, 2, 1, 2>(in.select<7, 1, 32, 1>(0, 0), out.select_all());

        (out == expectout).all() ? printf("Pass!") : printm(out);
        printf("\n");
    }
    {
        printf("Case: %d---------------------------------------------------------------------------------------------\n", count++);
        matrix<uchar, 8, 32> in = 3;
        matrix<uchar, 3, 16> out = 0;
        matrix<uchar, 3, 16> expectout = 3;

        in[0][5] = 0;
        in[2][5] = 0;

        CalculateEdgesIYUV_EdgeCandidate<-2, 2, 1, 2>(in.select<7, 1, 32, 1>(0, 0), out.select_all());

        (out == expectout).all() ? printf("Pass!") : printm(out);
        printf("\n");
    }
    {
        printf("Case: %d---------------------------------------------------------------------------------------------\n", count++);
        matrix<uchar, 8, 32> in = 3;
        matrix<uchar, 3, 16> out = 0;
        matrix<uchar, 3, 16> expectout = 2;

        in.row(2) = 0;
        in[2][9] = 3;
        in[0][10] = 0;
        in[0][5] = 0;
        expectout[0][1] = 0;
        expectout[0][2] = 0;
        expectout[0][3] = 0;
        expectout[0][4] = 0;
        expectout[0][6] = 3;
        expectout[0][7] = 3;
        expectout[0][8] = 0;
        expectout[0][9] = 0;
        expectout[0][10] = 0;

        expectout[1][5] = 3;
        expectout[1][6] = 3;
        expectout[1][7] = 3;
        expectout[1][8] = 3;
        expectout[1][9] = 3;
        expectout.row(2) = 3;

        CalculateEdgesIYUV_EdgeCandidate<-2, 2, 1, 2>(in.select<7, 1, 32, 1>(0, 0), out.select_all());

        (out == expectout).all() ? printf("Pass!") : printm(out);
        printf("\n");
    }
}

_GENX_MAIN_ void FixEdgeDirectionalIYUV_Test() {
    const ushort badmc_init[3][16] = { { 1, 0, 0, 0, 1, 1, 1, 0 }, { 1, 0, 0, 0, 1, 1, 1, 0 }, { 0 } };
    const ushort original_out_init[3][16] = { 
        { 3, 3, 0, 0, 0, 0, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3 },
        { 3, 0, 0, 0, 0, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3 },
        { 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3 }
    };
    int count = 0;
    {
        printf("Case: %d---------------------------------------------------------------------------------------------\n", count++);
        matrix<uchar, 8, 32> in = 3;
        matrix<char, 3, 16> originalout(original_out_init);
        matrix<ushort, 3, 16> badmc(badmc_init);
        auto outref = in.select<3, 2, 16, 1>(1, 4);
        outref = originalout;
        const char expect_out_init[3][16] = {
            { 3, 3, 0, 0, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3 },
            { 3, 0, 0, 0, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3 },
            { 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3 }
        };
        matrix<char, 3, 16> expectout(expect_out_init);
        int prot = 1;

        FixEdgeDirectionalIYUV<-4, 4, 1, 4>(badmc.select_all(), in.select<7, 1, 32, 1>(0, 0), prot);

        (outref == expectout).all() ? printf("Pass!") : printm(outref);
        printf("\n");
    }
    {
        printf("Case: %d---------------------------------------------------------------------------------------------\n", count++);
        matrix<uchar, 8, 32> in = 3;
        matrix<char, 3, 16> originalout(original_out_init);
        matrix<ushort, 3, 16> badmc(badmc_init);
        auto outref = in.select<3, 2, 16, 1>(1, 4);
        outref = originalout;
        const char expect_out_init[3][16] = {
            { 1, 3, 0, 0, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3 },
            { 3, 0, 0, 0, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3 },
            { 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3 }
        };
        matrix<char, 3, 16> expectout(expect_out_init);
        int prot = 1;

        in[0][5] = 0;
        in[2][5] = 0;
        in[0][4] = 1;
        in[2][4] = 1;
        FixEdgeDirectionalIYUV<-4, 4, 1, 4>(badmc.select_all(), in.select<7, 1, 32, 1>(0, 0), prot);

        (outref == expectout).all() ? printf("Pass!") : printm(outref);
        printf("\n");
    }
    {
        printf("Case: %d---------------------------------------------------------------------------------------------\n", count++);
        matrix<uchar, 8, 32> in = 3;
        matrix<char, 3, 16> originalout(original_out_init);
        matrix<ushort, 3, 16> badmc(badmc_init);
        auto outref = in.select<3, 2, 16, 1>(1, 4);
        outref = originalout;
        const char expect_out_init[3][16] = {
            { 1, 3, 0, 0, 1, 4, 2, 3, 3, 3, 3, 3, 3, 3, 3, 3 },
            { 2, 0, 0, 0, 2, 2, 2, 3, 3, 3, 3, 3, 3, 3, 3, 3 },
            { 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3 }
        };
        matrix<char, 3, 16> expectout(expect_out_init);
        int prot = 1;

        in.row(2) = 1;
        in[2][9] = 5;
        in[0][4] = 1;
        in[0][5] = 0;
        in[0][10] = 2;

        FixEdgeDirectionalIYUV<-4, 4, 1, 4>(badmc.select_all(), in.select<7, 1, 32, 1>(0, 0), prot);

        (outref == expectout).all() ? printf("Pass!") : printm(outref);
        printf("\n");
    }
    {
        printf("Case: %d---------------------------------------------------------------------------------------------\n", count++);
        matrix<uchar, 8, 32> in = 3;
        matrix<char, 3, 16> originalout(original_out_init);
        matrix<ushort, 3, 16> badmc(badmc_init);
        auto outref = in.select<3, 2, 16, 1>(1, 2);
        outref = originalout;
        const char expect_out_init[3][16] = {
            { 3, 3, 0, 0, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3 },
            { 3, 0, 0, 0, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3 },
            { 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3 }
        };
        matrix<char, 3, 16> expectout(expect_out_init);
        int prot = 1;

        FixEdgeDirectionalIYUV<-2, 2, 1, 2>(badmc.select_all(), in.select<7, 1, 32, 1>(0, 0), prot);

        (outref == expectout).all() ? printf("Pass!") : printm(outref);
        printf("\n");
    }
    {
        printf("Case: %d---------------------------------------------------------------------------------------------\n", count++);
        matrix<uchar, 8, 32> in = 3;
        matrix<char, 3, 16> originalout(original_out_init);
        matrix<ushort, 3, 16> badmc(badmc_init);
        auto outref = in.select<3, 2, 16, 1>(1, 2);
        outref = originalout;
        const char expect_out_init[3][16] = {
            { 3, 3, 0, 0, 1, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3 },
            { 3, 0, 0, 0, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3 },
            { 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3 }
        };
        matrix<char, 3, 16> expectout(expect_out_init);
        int prot = 1;

        in[0][6] = 1;
        in[2][6] = 1;

        FixEdgeDirectionalIYUV<-2, 2, 1, 2>(badmc.select_all(), in.select<7, 1, 32, 1>(0, 0), prot);

        (outref == expectout).all() ? printf("Pass!") : printm(outref);
        printf("\n");
    }
    {
        printf("Case: %d---------------------------------------------------------------------------------------------\n", count++);
        matrix<uchar, 8, 32> in = 3;
        matrix<char, 3, 16> originalout(original_out_init);
        matrix<ushort, 3, 16> badmc(badmc_init);
        auto outref = in.select<3, 2, 16, 1>(1, 2);
        outref = originalout;
        const char expect_out_init[3][16] = {
            { 1, 3, 0, 0, 1, 4, 2, 3, 3, 3, 3, 3, 3, 3, 3, 3 },
            { 2, 0, 0, 0, 2, 2, 2, 3, 3, 3, 3, 3, 3, 3, 3, 3 },
            { 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3 }
        };
        matrix<char, 3, 16> expectout(expect_out_init);
        int prot = 1;

        in.row(2) = 1;
        in[2][7] = 5;
        in[0][4] = 1;
        in[0][5] = 0;
        in[0][8] = 2;

        FixEdgeDirectionalIYUV<-2, 2, 1, 2>(badmc.select_all(), in.select<7, 1, 32, 1>(0, 0), prot);

        (outref == expectout).all() ? printf("Pass!") : printm(outref);
        printf("\n");
    }
}


#if 0
int main(int argc, char *argv[]) {
    CalculateEdgesIYUV_EdgeDir_Test();
    CalculateEdgesIYUV_EdgeCandidate_Test();
    FixEdgeDirectionalIYUV_Test();
}
#endif