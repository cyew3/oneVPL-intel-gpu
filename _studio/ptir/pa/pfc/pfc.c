/* ****************************************************************************** *\

INTEL CORPORATION PROPRIETARY INFORMATION
This software is supplied under the terms of a license agreement or nondisclosure
agreement with Intel Corporation and may not be copied or disclosed except in
accordance with the terms of that agreement
Copyright(c) 2014 Intel Corporation. All Rights Reserved.

File Name: api.h

\* ****************************************************************************** */

#include "pfc.h"
#include "../telecine/telecine.h"
#include <assert.h> 

#if (defined(LINUX32) || defined(LINUX64))
#ifndef min
#define min(a,b)            (((a) < (b)) ? (a) : (b))
#endif
#ifndef max
#define max(a,b)            (((a) > (b)) ? (a) : (b))
#endif
#endif

static int DetectInterlace_I420(unsigned char *frame, unsigned char *prev_frame, int pels, int lines, int pitch)
{
    unsigned int
        image_size = (unsigned int)(pels * lines),
        interlaced_blocks = 0,
        progressive_blocks = 0,
        step1v_tot = 0,
        step2v_tot = 0,
        step1h_tot = 0,
        step2h_tot = 0;
    unsigned char
        *fp,
        *pp,
        *f0,
        *f1,
        *f2,
        *f3;
    int
        i,
        j,
        k,
        tmp,
        d0,
        d1;

    for (i = 8; i < lines - 8; i += 8)
    {
        fp = frame + i * pitch + (i & (8 * (SKIP_FACTOR - 1))) + 8;
        pp = prev_frame + i * pitch + (i & (8 * (SKIP_FACTOR - 1))) + 8;

        for (j = 8; j < pels - 8; j += 8 * SKIP_FACTOR)
        {
            f0 = fp;
            f1 = f0 + pitch;
            f2 = pp;
            f3 = f2 + pitch;

            d0 = d1 = 0;

            // Calculate SAD for even and odd lines of 8x8 block.
            // We're doing a partial SAD here for speed.
            for (k = 0; k < 4; k++)
            {
                d0 += MABS(f0[0] - f2[0]);
                d0 += MABS(f0[2] - f2[2]);
                d0 += MABS(f0[4] - f2[4]);
                d0 += MABS(f0[6] - f2[6]);
                d1 += MABS(f1[1] - f3[1]);
                d1 += MABS(f1[3] - f3[3]);
                d1 += MABS(f1[5] - f3[5]);
                d1 += MABS(f1[7] - f3[7]);

                f0 += 2 * pitch;
                f1 += 2 * pitch;
                f2 += 2 * pitch;
                f3 += 2 * pitch;
            }
            d0 <<= 1;
            d1 <<= 1;
            
            // If there is enough difference to determine movement,
            // check this region for indications of interlaced artifacts
            if (d0 > MOVEMENT_THRESH || d1 > MOVEMENT_THRESH)
            {
                int step1v_corr = 0;
                int step2v_corr = 0;
                int step1h_corr = 0;
                int step2h_corr = 0;

                f0 = fp;
                f1 = f0 + pitch;
                f2 = f1 + pitch;

                // Determine 1 and 2 step pixel differences for 8x8 region
                for (k = 0; k < 4; k++)
                {
                    tmp = f0[1] - f1[1];
                    step1v_corr += MSQUARED(tmp);
                    tmp = f0[1] - f2[1];
                    step2v_corr += MSQUARED(tmp);
                    tmp = f1[0] - f1[1];
                    step1h_corr += MSQUARED(tmp);
                    tmp = f1[0] - f1[2];
                    step2h_corr += MSQUARED(tmp);

                    tmp = f0[3] - f1[3];
                    step1v_corr += MSQUARED(tmp);
                    tmp = f0[3] - f2[3];
                    step2v_corr += MSQUARED(tmp);
                    tmp = f1[2] - f1[3];
                    step1h_corr += MSQUARED(tmp);
                    tmp = f1[2] - f1[4];
                    step2h_corr += MSQUARED(tmp);

                    tmp = f0[5] - f1[5];
                    step1v_corr += MSQUARED(tmp);
                    tmp = f0[5] - f2[5];
                    step2v_corr += MSQUARED(tmp);
                    tmp = f1[4] - f1[5];
                    step1h_corr += MSQUARED(tmp);
                    tmp = f1[4] - f1[6];
                    step2h_corr += MSQUARED(tmp);

                    tmp = f0[7] - f1[7];
                    step1v_corr += MSQUARED(tmp);
                    tmp = f0[7] - f2[7];
                    step2v_corr += MSQUARED(tmp);
                    tmp = f1[6] - f1[7];
                    step1h_corr += MSQUARED(tmp);
                    tmp = f1[6] - f1[8];
                    step2h_corr += MSQUARED(tmp);

                    f0 += 2 * pitch;
                    f1 += 2 * pitch;
                    f2 += 2 * pitch;
                }

                if (step1v_corr > step2v_corr && step1h_corr < (40*40*4*4) && step2v_corr < (40*40*4*4))
                    interlaced_blocks++;

                if (step1v_corr < 2 * step2v_corr && step1h_corr < (40*40*4*4) && step1v_corr < (40*40*4*4))
                    progressive_blocks++;

                step1v_tot += step1v_corr;
                step2v_tot += step2v_corr;
                step1h_tot += step1h_corr;
                step2h_tot += step2h_corr;
            }

            fp += 8 * SKIP_FACTOR;
            pp += 8 * SKIP_FACTOR;
        }
    }

#if (SKIP_FACTOR != 1)
    interlaced_blocks *= SKIP_FACTOR;
    progressive_blocks *= SKIP_FACTOR;

    step1v_tot *= SKIP_FACTOR;
    step2v_tot *= SKIP_FACTOR;
    step1h_tot *= SKIP_FACTOR;
    step2h_tot *= SKIP_FACTOR;
#endif

    // Scale these values. 
    // (Done to prevent overflow during later multiplies)
    step1v_tot /= image_size;
    step2v_tot /= image_size;
    step1h_tot /= image_size;
    step2h_tot /= image_size;

    if (interlaced_blocks > progressive_blocks && interlaced_blocks > (image_size >> 12))
        return INTL_WEAK_INTERLACE;

    if (step1v_tot * step2h_tot > step1h_tot * step2v_tot && step1v_tot > step2v_tot && 2 * interlaced_blocks > progressive_blocks && interlaced_blocks > (image_size >> 10))
        return INTL_WEAK_INTERLACE;

    if (progressive_blocks > (image_size >> 10) && progressive_blocks > (interlaced_blocks << 4))
        return INTL_WEAK_PROGRESSIVE;

    return INTL_NO_DETECTION;
}
static void Deinterlace_I420_Fast(unsigned char *frame, unsigned char *prev_frame, int pels, int lines, int pitch)
{
    int line, pel;
    unsigned char *fp1,*pfp1;
    unsigned char *fp2,*pfp2;
    unsigned char *fp3,*pfp3;

    for (line = 1; line < lines - 2; line += 2)
    {
        unsigned int a,b,c,d,e,f;
        int c_nw,c_ne,c_se,c_sw;
        int diff,tmp;
        int prev_mode = INTL_LINE_REMOVE_AVG;
        int mode = INTL_LINE_REMOVE_AVG;
        int next_mode = INTL_LINE_REMOVE_AVG;

        // current frame
        fp2 = frame + line * pitch;
        fp1 = fp2 - pitch;
        fp3 = fp2 + pitch;

        // previous frame
        if (prev_frame != 0)
        {
            pfp2 = prev_frame + line * pitch;
            pfp1 = pfp2 - pitch;
            pfp3 = pfp2 + pitch;
        }
        else
        {
            pfp2 = fp2;
            pfp1 = fp1;
            pfp3 = fp3;
        }

        // initialize
        for (pel = 0; pel < pels - 4; pel += 4)
        {
            // Load *next* 4 pels
            a = ((unsigned int *)fp1)[1];
            b = ((unsigned int *)fp2)[1];
            c = ((unsigned int *)fp3)[1];

            // Get corners
            c_nw = (a >> 24);
            c_ne = (a & 0xff);
            c_sw = (c >> 24);
            c_se = (c & 0xff);

            // Edge detect
            tmp = c_nw + c_ne - c_sw - c_se;
            if ((tmp < 100) && (tmp > -100))
            {
                next_mode = INTL_LINE_REMOVE_AVG;
                goto proc;
            }

            tmp = c_ne + c_se - c_nw - c_sw;
            if (tmp > 100)
            {
                next_mode = INTL_LINE_REMOVE_AVG;
                goto proc;
            }
            if (tmp < -100)
            {
                next_mode = INTL_LINE_REMOVE_AVG;
                goto proc;
            }

            // Load previous pels
            d = ((unsigned int *)pfp1)[1];
            e = ((unsigned int *)pfp2)[1];
            f = ((unsigned int *)pfp3)[1];

            // Diff with previous pels
            tmp = c_nw;
            tmp -= (d >> 24);
            diff = tmp ^ (tmp >> 31);

            tmp = c_ne;
            tmp -= (d & 0xff);
            diff += tmp ^ (tmp >> 31);

            if (diff > 100)
            {
                next_mode = INTL_LINE_REMOVE_AVG;
                goto proc;
            }

            tmp = c_sw;
            tmp -= (f >> 24);
            diff += tmp ^ (tmp >> 31);

            tmp = c_se;
            tmp -= (f & 0xff);
            diff += tmp ^ (tmp >> 31);

            if (diff > 200)
            {
                next_mode = INTL_LINE_REMOVE_AVG;
                goto proc;
            }

            tmp = (b >> 24);
            tmp -= (e >> 24);
            diff += tmp ^ (tmp >> 31);
            tmp = (b & 0xff);
            tmp -= (e & 0xff);
            diff += tmp ^ (tmp >> 31);

            if (diff > 300)
            {
                next_mode = INTL_LINE_REMOVE_AVG;
                goto proc;
            }

            next_mode = INTL_LINE_REMOVE_MEDIAN;

proc:
            if (mode == INTL_LINE_REMOVE_MEDIAN || prev_mode == INTL_LINE_REMOVE_MEDIAN || next_mode == INTL_LINE_REMOVE_MEDIAN)
            {
                // median
                fp2[0] = MEDIAN_3(fp1[0],fp2[0],fp3[0]);
                fp2[1] = MEDIAN_3(fp1[1],fp2[1],fp3[1]);
                fp2[2] = MEDIAN_3(fp1[2],fp2[2],fp3[2]);
                fp2[3] = MEDIAN_3(fp1[3],fp2[3],fp3[3]);
            }
            else
            {
                // average
                a = ((unsigned int *)fp1)[0];
                c = ((unsigned int *)fp3)[0];
                ((unsigned int*)fp2)[0] = (((a^c)>>1) & 0x7F7F7F7F) + (a&c);
            }

            prev_mode = mode;
            mode = next_mode;

            fp1 += 4;
            fp2 += 4;
            fp3 += 4;

            pfp1 += 4;
            pfp2 += 4;
            pfp3 += 4;
        }

        // last 4 pels
        if (mode == INTL_LINE_REMOVE_MEDIAN || prev_mode == INTL_LINE_REMOVE_MEDIAN)
        {
            // median
            fp2[0] = MEDIAN_3(fp1[0],fp2[0],fp3[0]);
            fp2[1] = MEDIAN_3(fp1[1],fp2[1],fp3[1]);
            fp2[2] = MEDIAN_3(fp1[2],fp2[2],fp3[2]);
            fp2[3] = MEDIAN_3(fp1[3],fp2[3],fp3[3]);
        }
        else
        {
            // average
            a = ((unsigned int *)fp1)[0];
            c = ((unsigned int *)fp3)[0];
            ((unsigned int*)fp2)[0] = (((a^c)>>1) & 0x7F7F7F7F) + (a&c);
        }
    }
}
int Deinterlace(Frame *frameIn, Frame *frameRef, int pels, int lines, unsigned int *mode, unsigned int *detection_measure)
{
    unsigned char *frame = frameIn->ucMem,
                  *prev_frame = frameRef->ucMem;
    if ((*mode) == INTL_MODE_CERTAIN)
    {
        //Deinterlace_I420_Fast(frame, prev_frame, pels, lines, pels);
        Extract_Fields_I420(prev_frame, frameIn, TRUE);

        return 1;
    }
    else if ((*mode) == INTL_MODE_SMART_CHECK)
    {
        int res = DetectInterlace_I420(frame, prev_frame, pels, lines, pels);

        // Update the "detection measure" based on result of detection
        switch (res)
        {
        case INTL_STRONG_INTERLACE:
            (*detection_measure) = (31 * (*detection_measure) + 256) >> 5;
            break;
        case INTL_WEAK_INTERLACE:
            (*detection_measure) = (31 * (*detection_measure) + 256) >> 5;
            break;
        case INTL_WEAK_PROGRESSIVE:
            (*detection_measure) = (31 * (*detection_measure) + 0) >> 5;
            break;
        case INTL_STRONG_PROGRESSIVE:
            (*detection_measure) = 0;
            break;
        }

        // If the detection measure is above this threshold, we should
        // de-interlace this frame. Otherwise, return without de-interlacing
        if ((*detection_measure) > 128)
            (*mode) = INTL_MODE_CERTAIN;
        else if ((*detection_measure) > 16)
            (*mode) = INTL_MODE_SMART_CHECK;
        else
        {
            (*mode) = INTL_MODE_NONE;
            return 0;
        }
        
        //Deinterlace_I420_Fast(frame, prev_frame, pels, lines, pels);
        Extract_Fields_I420(prev_frame, frameIn, TRUE);
        return 1;
    }
    else
    {
        return 0;
    }
}
/*
static int InvTelecineDetect(UCHAR *data, UCHAR *prevData, double frameRate, ULONG32 timestamp, ULONG32 pels, ULONG32 lines, BOOL bDeInterlaced, T_INVTELE_STATE *state)
{
    unsigned int i, j, k, ll;
    ULONG32 patternStart, histLength;
    LONG32 *pNew, *pOld;
    float temp;
    float sumEven = 0.0f, sumOdd = 0.0f;
    float sumOld = 0.0f, sumNew = 0.0f;
    float sumAll = 0.0f;

    float    inGroupMeanEven[5],outGroupMeanEven[5];
    float    inGroupStdEven[5],outGroupStdEven[5];
    ULONG32 inGroupCountEven,outGroupCountEven;
    float    outMinEven[5],outMaxEven[5];
    float    inMaxEven[5],inMinEven[5];
    BOOL    groupValidFlagEven[5];

    float    inGroupMeanOdd[5],outGroupMeanOdd[5];
    float    inGroupStdOdd[5],outGroupStdOdd[5];
    ULONG32    inGroupCountOdd,outGroupCountOdd;
    float    outMinOdd[5],outMaxOdd[5];
    float    inMaxOdd[5],inMinOdd[5];
    BOOL    groupValidFlagOdd[5];

    float    inGroupMean[5], outGroupMean[5];
    float    inGroupStd[5], outGroupStd[5];
    ULONG32 inGroupCount,outGroupCount;
    float    outMin[5],outMax[5],inMax[5];
    BOOL    groupValidFlag[5];

    ULONG32 timeSinceLastFrame;
    BOOL    obviousPatternFlag = FALSE;
    BOOL    sceneChangeFlag = FALSE;

    if (state->firstFrame == TRUE)
    {
        // Initialize history timestamps with this first timestamp
        for (i = 0; i < PULLDOWN_HIST_LEN; i++)
        {
            state->pulldownTimeHist[i] = timestamp;
        }
        state->lastRemovedTimestamp = timestamp;
        state->firstFrame = FALSE;
        goto TOO_EARLY;
    }

    // Calculate Sum of Differences for even and odd lines...
    // If we know that the frame is de-interlaced, then the stats
    // for the "odd" lines are invalid.  So just measure "even"
    // lines then copy into "odd" SAD (so the rest of the algorithm
    // will work).
    ll = (pels >> 2);

    if (bDeInterlaced)
    {
        pNew = (LONG32 *)data;
        pOld = (LONG32 *)prevData;
        ll = (pels >> 2);

        for (i = 0; i < lines; i += 2)  //  only do the luma.
        {
            for (j = 0; j < ll; j++)
            {
                temp = (float)((pNew[j]&0xff00) - (pOld[j]&0xFF00));
                sumEven += ((float)(1./(256.*256.)))*(temp*temp);
            }

            pOld += 2*ll;
            pNew += 2*ll;
        }
        sumOdd = sumEven;
    }
    else
    {
        pNew = (LONG32 *)data;
        pOld = (LONG32 *)prevData;

        for (i = 0; i < lines; i += 2)  //  only do the luma.
        {
            for (j = 0; j < ll; j++)
            {
                temp = (float)((pNew[j]&0xff00) - (pOld[j]&0xFF00));
                sumEven += ((float)(1./(256.*256.)))*(temp*temp);
                temp = (float)((pNew[j+ll]&0xFF00) - (pOld[j+ll]&0xFF00));
                sumOdd += ((float)(1./(256.*256.)))*(temp*temp);
            }

            pOld += 2*ll;
            pNew += 2*ll;
        }
        sumAll = sumEven + sumOdd;
        sumEven /= (ll * (lines>>1));
        sumOdd /= (ll * (lines>>1));
        sumAll /= (ll * lines);
    }

    sceneChangeFlag = (sumAll > 7500)?(TRUE):(FALSE);
    if (sumEven > 100) sumEven=100;
    if (sumOdd > 100) sumOdd=100;
    if (sumAll > 100) sumAll=100;

    // Compensate for 30 vs 29.97fps captures.
    if ((sumEven == 0) && (sumOdd == 0) && 
        (state->NTSCTrackingFrameCounter > 500) && (frameRate > 29.98))
    {
        state->pulldownTimeHist[PULLDOWN_HIST_LEN-1] = timestamp;
        state->NTSCTrackingFrameCounter = 0;
         goto REMOVE_FRAME;
    }
    state->NTSCTrackingFrameCounter++;

    // In case we dropped a frame
    timeSinceLastFrame = CALCULATE_ELAPSED_TICKS(state->pulldownTimeHist[PULLDOWN_HIST_LEN-1], timestamp);

    while(timeSinceLastFrame > 50)
    {
        for(i=0;i<PULLDOWN_HIST_LEN-1;i++)
        {
            state->pulldownSadHistEven[i] = state->pulldownSadHistEven[i+1];
            state->pulldownSadHistOdd[i] = state->pulldownSadHistOdd[i+1];
            state->pulldownSadHistAll[i] = state->pulldownSadHistAll[i+1];
            state->pulldownTimeHist[i] = state->pulldownTimeHist[i+1];
        }

        state->pulldownSadHistEven[i] = MISSING_SAD;
        state->pulldownSadHistOdd[i] = MISSING_SAD;
        state->pulldownSadHistAll[i] = MISSING_SAD;
        state->pulldownTimeHist[i] = timestamp;
        timeSinceLastFrame -= 33;
        state->frameRemovalPattern--;
        if (state->frameRemovalPattern < 0)
            state->frameRemovalPattern = 4;
        state->frameCountMod--;
        if (state->frameCountMod < 0)
            state->frameCountMod = 4;
    }

    //  Update the history 
    for(i = 0; i < PULLDOWN_HIST_LEN - 1; i++)
    {
        state->pulldownSadHistEven[i] = state->pulldownSadHistEven[i+1];
        state->pulldownSadHistOdd[i] = state->pulldownSadHistOdd[i+1];
        state->pulldownSadHistAll[i] = state->pulldownSadHistAll[i+1];
        state->pulldownTimeHist[i] = state->pulldownTimeHist[i+1];
    }
    state->frameRemovalPattern--;
    if (state->frameRemovalPattern < 0)
        state->frameRemovalPattern = 4;
    state->frameCountMod--;
    if (state->frameCountMod < 0)
        state->frameCountMod = 4;

    state->pulldownSadHistEven[i] = sumEven;
    state->pulldownSadHistOdd[i] = sumOdd;
    state->pulldownSadHistAll[i] = sumAll;
    state->pulldownTimeHist[i] = timestamp;

    // If removal on, and we have no pattern established, wait a while.
    if ((state->pulldownSadHistEven[PULLDOWN_HIST_LEN - 5] == UN_INIT_SAD) || 
        (state->pulldownSadHistOdd[PULLDOWN_HIST_LEN - 5] == UN_INIT_SAD) || 
        (state->pulldownSadHistAll[PULLDOWN_HIST_LEN - 5] == UN_INIT_SAD))
    {
        goto TOO_EARLY;
    }

    histLength = 0;

    while (state->pulldownSadHistAll[histLength] == UN_INIT_SAD)
    {
        histLength += 5;
        if (histLength > PULLDOWN_HIST_LEN - 10)
            goto TOO_EARLY;
    }

    if ((bDeInterlaced) || (state->ulPulldownActiveTimerProg > 90 && state->ulPulldownActiveTimerIntl < 10))
    {
        // Skip interlaced telecine tests
        goto PROGRESSIVE_TESTS;
    }

    // Gather statistics from SAD history
    // Run through tests looking at the last 20, then 15, then 10, then 5 frames
    for (patternStart = histLength; patternStart < PULLDOWN_HIST_LEN; patternStart += 5)
    {
        for (i = 0; i < 5;i++)
        {
            // Stats for "even" lines
            inGroupMeanEven[i]  = 0;
            outGroupMeanEven[i] = 0;
            inGroupStdEven[i]   = 0;
            outGroupStdEven[i]  = 0;
            inGroupCountEven    = 0;
            outGroupCountEven   = 0;
            outMinEven[i] = 255*255;
            inMinEven[i]  = 255*255;
            outMaxEven[i] = 0;
            inMaxEven[i]  = 0;
            for (j = patternStart + i; j < PULLDOWN_HIST_LEN; j++)
            {
                if (state->pulldownSadHistEven[j] == MISSING_SAD ||
                    state->pulldownSadHistEven[j] == UN_INIT_SAD)
                    continue;

                if (((j - i) % 5) == 0)
                {
                    inGroupMeanEven[i] += state->pulldownSadHistEven[j];
                    inGroupStdEven[i] += state->pulldownSadHistEven[j]*state->pulldownSadHistEven[j];
                    if (inMaxEven[i] < state->pulldownSadHistEven[j])
                        inMaxEven[i] = state->pulldownSadHistEven[j];
                    if (inMinEven[i] > state->pulldownSadHistOdd[j])
                        inMinEven[i] = state->pulldownSadHistOdd[j];
                    inGroupCountEven++;
                }
                else
                {
                    outGroupMeanEven[i]+= state->pulldownSadHistEven[j];
                    outGroupStdEven[i]+= state->pulldownSadHistEven[j]*state->pulldownSadHistEven[j];
                    if (outMinEven[i] > state->pulldownSadHistEven[j])
                        outMinEven[i] = state->pulldownSadHistEven[j];
                    if (outMaxEven[i] < state->pulldownSadHistEven[j])
                        outMaxEven[i] = state->pulldownSadHistEven[j];
                    outGroupCountEven++;
                }
            }
            // Is there enough valid data to analyze?
            if ((inGroupCountEven > 1) && (outGroupCountEven > 3))
            {
                inGroupMeanEven[i] = inGroupMeanEven[i]/inGroupCountEven;
                if ((inGroupStdEven[i]/inGroupCountEven)-(inGroupMeanEven[i]*inGroupMeanEven[i]) > 0.0f)
                    inGroupStdEven[i] = (float)sqrt((inGroupStdEven[i]/inGroupCountEven)-(inGroupMeanEven[i]*inGroupMeanEven[i]));
                else
                    inGroupStdEven[i] = 0.0f;

                outGroupMeanEven[i] = outGroupMeanEven[i]/outGroupCountEven;
                if ((outGroupStdEven[i]/outGroupCountEven)-(outGroupMeanEven[i]*outGroupMeanEven[i]) > 0.0f)
                    outGroupStdEven[i] = (float)sqrt((outGroupStdEven[i]/outGroupCountEven)-(outGroupMeanEven[i]*outGroupMeanEven[i]));
                else
                    outGroupStdEven[i] = 0.0f;

                groupValidFlagEven[i] = TRUE;
            }
            else
            {
                inGroupMeanEven[i] = 0;
                outGroupMeanEven[i] = 0;
                inGroupStdEven[i] = 1;
                outGroupStdEven[i] = 1;
                groupValidFlagEven[i] = FALSE;
            }

            // Stats for "odd" lines
            inGroupMeanOdd[i]  = 0;
            outGroupMeanOdd[i] = 0;
            inGroupStdOdd[i]   = 0;
            outGroupStdOdd[i]  = 0;
            inGroupCountOdd    = 0;
            outGroupCountOdd   = 0;
            outMinOdd[i] = 255*255;
            inMinOdd[i]  = 255*255;
            outMaxOdd[i] = 0;
            inMaxOdd[i]  = 0;
            for (j = patternStart + i; j < PULLDOWN_HIST_LEN; j++)
            {
                if (state->pulldownSadHistOdd[j] == MISSING_SAD ||
                    state->pulldownSadHistOdd[j] == UN_INIT_SAD)
                    continue;

                if (((j - i) % 5) == 0)
                {
                    inGroupMeanOdd[i] += state->pulldownSadHistOdd[j];
                    inGroupStdOdd[i] += state->pulldownSadHistOdd[j]*state->pulldownSadHistOdd[j];
                    if (inMaxOdd[i] < state->pulldownSadHistOdd[j])
                        inMaxOdd[i] = state->pulldownSadHistOdd[j];
                    if (inMinOdd[i] > state->pulldownSadHistOdd[j])
                        inMinOdd[i] = state->pulldownSadHistOdd[j];
                    inGroupCountOdd++;
                }
                else
                {
                    outGroupMeanOdd[i] += state->pulldownSadHistOdd[j];
                    outGroupStdOdd[i] += state->pulldownSadHistOdd[j]*state->pulldownSadHistOdd[j];
                    if (outMinOdd[i] > state->pulldownSadHistOdd[j])
                        outMinOdd[i] = state->pulldownSadHistOdd[j];
                    if (outMaxOdd[i] < state->pulldownSadHistOdd[j])
                        outMaxOdd[i] = state->pulldownSadHistOdd[j];
                    outGroupCountOdd++;
                }
            }
            // Is there enough valid data to analyze?
            if ((inGroupCountOdd > 1) && (outGroupCountOdd > 3))
            {
                inGroupMeanOdd[i] = inGroupMeanOdd[i]/inGroupCountOdd;
                if ((inGroupStdOdd[i]/inGroupCountOdd)-(inGroupMeanOdd[i]*inGroupMeanOdd[i]) > 0.0f)
                    inGroupStdOdd[i] = (float)sqrt((inGroupStdOdd[i]/inGroupCountOdd)-(inGroupMeanOdd[i]*inGroupMeanOdd[i]));
                else
                    inGroupStdOdd[i] = 0.0f;

                outGroupMeanOdd[i] = outGroupMeanOdd[i]/outGroupCountOdd;
                if ((outGroupStdOdd[i]/outGroupCountOdd)-(outGroupMeanOdd[i]*outGroupMeanOdd[i]) > 0.0f)
                    outGroupStdOdd[i] = (float)sqrt((outGroupStdOdd[i]/outGroupCountOdd)-(outGroupMeanOdd[i]*outGroupMeanOdd[i]));
                else
                    outGroupStdOdd[i] = 0.0f;

                groupValidFlagOdd[i] = TRUE;
            }
            else
            {
                inGroupMeanOdd[i] = 0;
                outGroupMeanOdd[i] = 0;
                inGroupStdOdd[i] = 1;
                outGroupStdOdd[i] = 1;
                groupValidFlagOdd[i] = FALSE;
            }
        }

        // Do we have a clear pattern? Always trust this test.
        for (i = 0; i < 5; i++)
        {
            if (groupValidFlagEven[i] == FALSE)
                continue;
            if (groupValidFlagOdd[i] == FALSE)
                continue;
            if (groupValidFlagEven[(i+2)%5] == FALSE)
                continue;
            if (groupValidFlagOdd[(i+2)%5] == FALSE)
                continue;

            if ((inGroupMeanEven[i]+inThresh[patternStart]*inGroupStdEven[i] < outGroupMeanEven[i] - outThresh[patternStart]*outGroupStdEven[i]) &&
                (inGroupMeanOdd[(i+2)%5]+inThresh[patternStart]*inGroupStdOdd[(i+2)%5] < outGroupMeanOdd[(i+2)%5] - outThresh[patternStart]*outGroupStdOdd[(i+2)%5]) &&
                (inGroupMeanEven[i]+inGroupStdEven[i] < inGroupMeanOdd[i]) &&
                (inGroupMeanOdd[(i+2)%5]+inGroupStdOdd[(i+2)%5] < inGroupMeanEven[(i+2)%5]))
            {
                // Set the removal pattern phase
                state->frameRemovalPattern = i;

                // If this is the right frame remove it!
                if (i == (PULLDOWN_HIST_LEN - 1) % 5)
                {
                    // Set a counter that goes up if we have a consistent pattern 80+% of the time, down otherwise
                    if ((timestamp - state->lastRemovedTimestamp > 145) &&
                        (timestamp - state->lastRemovedTimestamp < 175))
                    {
                        if (state->ulPulldownActiveTimerIntl < 95)
                            state->ulPulldownActiveTimerIntl += 5;
                        else
                        {
                            state->ulPulldownActiveTimerIntl = 100;
                            state->bInterlacedTelecineSeen = TRUE;
                        }

                        if (state->ulPulldownActiveTimerProg > 5)
                            state->ulPulldownActiveTimerProg -= 5;
                        else
                            state->ulPulldownActiveTimerProg = 0;
                    }
                    else if (timestamp - state->lastRemovedTimestamp < 300)
                    {
                        if (state->ulPulldownActiveTimerIntl > 5)
                            state->ulPulldownActiveTimerIntl -= 5;
                        else
                            state->ulPulldownActiveTimerIntl = 0;
                    }
                    state->lastRemovedTimestamp = timestamp;

                    goto REMOVE_FRAME;
                }
                else if (i == ((PULLDOWN_HIST_LEN - 2) % 5))
                {
                    obviousPatternFlag = TRUE;
                    goto INTERLEAVE_ODD;
                }
                else 
                    goto DO_NOTHING;
            }

            if ((inGroupMeanOdd[i]+inThresh[patternStart]*inGroupStdOdd[i] < outGroupMeanOdd[i] - outThresh[patternStart]*outGroupStdOdd[i]) &&
                (inGroupMeanEven[(i+2)%5]+inThresh[patternStart]*inGroupStdEven[(i+2)%5] < outGroupMeanEven[(i+2)%5] - outThresh[patternStart]*outGroupStdEven[(i+2)%5]) &&
                (inGroupMeanOdd[i]+inGroupStdOdd[i] < inGroupMeanEven[i]) &&
                (inGroupMeanEven[(i+2)%5]+inGroupStdEven[(i+2)%5] < inGroupMeanOdd[(i+2)%5]))
            {
                // Set the removal pattern phase
                state->frameRemovalPattern=i;

                // If this is the right frame remove it!
                if (i == (PULLDOWN_HIST_LEN - 1) % 5)
                {
                    // Set a counter that goes up if we have a consistent pattern 80+% of the time, down otherwise
                    if ((timestamp - state->lastRemovedTimestamp > 145) &&
                        (timestamp - state->lastRemovedTimestamp < 175))
                    {
                        if (state->ulPulldownActiveTimerIntl < 95)
                            state->ulPulldownActiveTimerIntl += 5;
                        else
                        {
                            state->ulPulldownActiveTimerIntl = 100;
                            state->bInterlacedTelecineSeen = TRUE;
                        }

                        if (state->ulPulldownActiveTimerProg > 5)
                            state->ulPulldownActiveTimerProg -= 5;
                        else
                            state->ulPulldownActiveTimerProg = 0;
                    }
                    else if (timestamp - state->lastRemovedTimestamp < 300)
                    {
                        if (state->ulPulldownActiveTimerIntl > 5)
                            state->ulPulldownActiveTimerIntl -= 5;
                        else
                            state->ulPulldownActiveTimerIntl = 0;
                    }
                    state->lastRemovedTimestamp = timestamp;

                    goto REMOVE_FRAME;
                }
                else if (i == ((PULLDOWN_HIST_LEN - 2) % 5))
                {
                    obviousPatternFlag = TRUE;
                    goto INTERLEAVE_EVEN;
                }
                else 
                    goto DO_NOTHING;
            }
        }

        // Do we have a pretty clear pattern? Only trust this test 
        // if we have succeeded with the strongest test a couple of times
        for (i = 0; i < 5; i++)
        {
            if (groupValidFlagEven[i] == FALSE)
                continue;
            if (groupValidFlagOdd[i] == FALSE)
                continue;
            if (groupValidFlagEven[(i+2)%5] == FALSE)
                continue;
            if (groupValidFlagOdd[(i+2)%5] == FALSE)
                continue;

            if ((state->ulPulldownActiveTimerIntl > 10) &&
                (inGroupMeanEven[i]      + inThresh[patternStart]*inGroupStdEven[i]      < outMinEven[i]     ) &&
                (inGroupMeanOdd[(i+2)%5] + inThresh[patternStart]*inGroupStdOdd[(i+2)%5] < outMinOdd[(i+2)%5]) &&
                (inGroupMeanEven[i]      + inGroupStdEven[i]      < inGroupMeanOdd[i]       ) &&
                (inGroupMeanOdd[(i+2)%5] + inGroupStdOdd[(i+2)%5] < inGroupMeanEven[(i+2)%5]))
            {
                // Set the removal pattern phase
                state->frameRemovalPattern=i;

                // If this is the right frame remove it!
                if(i == (PULLDOWN_HIST_LEN - 1) % 5)
                {
                    // Set a counter that goes up if we have a consistent pattern 80+% of the time, down otherwise
                    if ((timestamp - state->lastRemovedTimestamp > 145) &&
                        (timestamp - state->lastRemovedTimestamp < 175))
                    {
                        if (state->ulPulldownActiveTimerIntl < 95)
                            state->ulPulldownActiveTimerIntl += 5;
                        else
                        {
                            state->ulPulldownActiveTimerIntl = 100;
                            state->bInterlacedTelecineSeen = TRUE;
                        }

                        if (state->ulPulldownActiveTimerProg > 5)
                            state->ulPulldownActiveTimerProg -= 5;
                        else
                            state->ulPulldownActiveTimerProg = 0;
                    }
                    else if (timestamp - state->lastRemovedTimestamp < 300)
                    {
                        if (state->ulPulldownActiveTimerIntl > 5)
                            state->ulPulldownActiveTimerIntl -= 5;
                        else
                            state->ulPulldownActiveTimerIntl = 0;
                    }
                    state->lastRemovedTimestamp = timestamp;

                    goto REMOVE_FRAME;
                }
                else if (i == ((PULLDOWN_HIST_LEN - 2) % 5))
                {
                    obviousPatternFlag = TRUE;
                    goto INTERLEAVE_ODD;
                }
                else 
                    goto DO_NOTHING;
            }

            if ((state->ulPulldownActiveTimerIntl > 10) &&
                (inGroupMeanOdd[i]        + inThresh[patternStart]*inGroupStdOdd[i]        < outMinOdd[i]       ) &&
                (inGroupMeanEven[(i+2)%5] + inThresh[patternStart]*inGroupStdEven[(i+2)%5] < outMinEven[(i+2)%5]) &&
                (inGroupMeanOdd[i]        + inGroupStdOdd[i]        < inGroupMeanEven[i]     ) &&
                (inGroupMeanEven[(i+2)%5] + inGroupStdEven[(i+2)%5] < inGroupMeanOdd[(i+2)%5]))
            {
                // Set the removal pattern phase
                state->frameRemovalPattern=i;

                // If this is the right frame remove it!
                if(i == (PULLDOWN_HIST_LEN - 1) % 5)
                {
                    // Set a counter that goes up if we have a consistent pattern 80+% of the time, down otherwise
                    if ((timestamp - state->lastRemovedTimestamp > 145) && 
                        (timestamp - state->lastRemovedTimestamp < 175))
                    {
                        if (state->ulPulldownActiveTimerIntl < 95)
                            state->ulPulldownActiveTimerIntl += 5;
                        else
                        {
                            state->ulPulldownActiveTimerIntl = 100;
                            state->bInterlacedTelecineSeen = TRUE;
                        }

                        if (state->ulPulldownActiveTimerProg > 5)
                            state->ulPulldownActiveTimerProg -= 5;
                        else
                            state->ulPulldownActiveTimerProg = 0;
                    }
                    else if (timestamp - state->lastRemovedTimestamp < 300)
                    {
                        if (state->ulPulldownActiveTimerIntl > 5)
                            state->ulPulldownActiveTimerIntl -= 5;
                        else
                            state->ulPulldownActiveTimerIntl = 0;
                    }
                    state->lastRemovedTimestamp = timestamp;

                    goto REMOVE_FRAME;
                }
                else if (i == ((PULLDOWN_HIST_LEN - 2) % 5))
                {
                    obviousPatternFlag = TRUE;
                    goto INTERLEAVE_EVEN;
                }
                else 
                    goto DO_NOTHING;
            }
        }
    }

    // Rule that maintains the pattern
    // No pattern, but things are VERY quiet, and we have been seeing a 3:2 pattern 
    if ((state->frameRemovalPattern == ((PULLDOWN_HIST_LEN-1)%5)) &&
        (state->ulPulldownActiveTimerIntl > 50) &&
        (inMaxEven [(PULLDOWN_HIST_LEN-1)%5] < 13) && 
        (inMaxOdd  [(PULLDOWN_HIST_LEN-1)%5] < 13) && 
        (outMaxOdd [(PULLDOWN_HIST_LEN-1)%5] < 13) &&
        (outMaxEven[(PULLDOWN_HIST_LEN-1)%5] < 13) && 
        (groupValidFlagEven[(PULLDOWN_HIST_LEN-1)%5] == TRUE) &&
        (groupValidFlagOdd[(PULLDOWN_HIST_LEN-1)%5] == TRUE))
    {
        state->lastRemovedTimestamp = timestamp;
        state->checkNextFrameForInterlace = TRUE;

        goto REMOVE_FRAME;
    }

    // Rule that maintains the pattern
    // If we have been seeing consistent pulldown, 
    // and the last frame looks a bit interlaced, but it is in the right place.
    // This is a weak test.
    if ((state->frameRemovalPattern == ((PULLDOWN_HIST_LEN-1)%5)) &&
        (state->ulPulldownActiveTimerIntl > 50) &&
        (!state->checkNextFrameForInterlace) &&
        (state->pulldownSadHistEven[(PULLDOWN_HIST_LEN-1)] < (.6*state->pulldownSadHistOdd[(PULLDOWN_HIST_LEN-1)]) ||
         state->pulldownSadHistOdd[(PULLDOWN_HIST_LEN-1)] < (.6*state->pulldownSadHistEven[(PULLDOWN_HIST_LEN-1)])))
    {
        state->lastRemovedTimestamp = timestamp;
        state->checkNextFrameForInterlace = TRUE;
        goto REMOVE_FRAME;
    }

    // If we have been seeing consistent pulldown, and the last frame looks very interlaced.
    // This is a weak test
    if ((state->ulPulldownActiveTimerIntl > 50) &&
        (!state->checkNextFrameForInterlace) &&
        (
         (state->pulldownSadHistEven[(PULLDOWN_HIST_LEN-1)] < (state->pulldownSadHistOdd[(PULLDOWN_HIST_LEN-1)]*.3)) &&
         (state->pulldownSadHistOdd[(PULLDOWN_HIST_LEN-1)] > 9)
        ) || (
         (state->pulldownSadHistOdd[(PULLDOWN_HIST_LEN-1)] < (state->pulldownSadHistEven[(PULLDOWN_HIST_LEN-1)]*.3)) &&
         (state->pulldownSadHistEven[(PULLDOWN_HIST_LEN-1)] > 9)
        )
       )
    {
        state->lastRemovedTimestamp = timestamp;
        state->checkNextFrameForInterlace = TRUE;
        goto REMOVE_FRAME;
    }

    if(state->checkNextFrameForInterlace)
    {
        state->checkNextFrameForInterlace = FALSE;
        if(state->interleaveEvenFlag)
            goto INTERLEAVE_EVEN;
        if(state->interleaveOddFlag)
            goto INTERLEAVE_ODD;
    }

    // Otherwise no pulldown!
    if ((inMaxEven[(PULLDOWN_HIST_LEN-1)%5] > 30) || 
        (outMaxEven[(PULLDOWN_HIST_LEN-1)%5] > 30))
    {
        if (state->ulPulldownActiveTimerIntl > 0)
            state->ulPulldownActiveTimerIntl--;
    }

    // should we attempt progressive patterns?
    if ((lines > 242) || 
        (state->ulPulldownActiveTimerIntl > 90 && state->ulPulldownActiveTimerProg < 10))
    {
        goto NO_PATTERN;
    }

PROGRESSIVE_TESTS:
    // Now test to see if there is an entire repeated frame
    for (patternStart = histLength; patternStart < PULLDOWN_HIST_LEN; patternStart += 5)
    {
        for (i = 0; i < 5; i++)
        {
            inGroupMean[i]  = 0;
            outGroupMean[i] = 0;
            inGroupStd[i]   = 0;
            outGroupStd[i]  = 0;
            inGroupCount    = 0;
            outGroupCount   = 0;
            outMin[i] = 255*255;
            outMax[i] = 0;
            inMax[i]  = 0;

            for (j = patternStart + i; j < PULLDOWN_HIST_LEN; j++)
            {
                if (state->pulldownSadHistAll[j] == MISSING_SAD ||
                    state->pulldownSadHistAll[j] == UN_INIT_SAD)
                    continue;

                if (((j - i) % 5) == 0)
                {
                    inGroupMean[i] += state->pulldownSadHistAll[j];
                    inGroupStd[i] += state->pulldownSadHistAll[j] * state->pulldownSadHistAll[j];
                    if (inMax[i] < state->pulldownSadHistAll[j])
                        inMax[i] = state->pulldownSadHistAll[j];
                    inGroupCount++;
                }
                else
                {
                    outGroupMean[i] += state->pulldownSadHistAll[j];
                    outGroupStd[i] += state->pulldownSadHistAll[j] * state->pulldownSadHistAll[j];
                    if (outMin[i] > state->pulldownSadHistAll[j])
                        outMin[i] = state->pulldownSadHistAll[j];
                    if (outMax[i] < state->pulldownSadHistAll[j])
                        outMax[i] = state->pulldownSadHistAll[j];
                    outGroupCount++;
                }
            }
            if ((inGroupCount > 1) && (outGroupCount > 3))
            {
                groupValidFlag[i] = TRUE;

                inGroupMean[i] = inGroupMean[i]/inGroupCount;
                if ((inGroupStd[i]/inGroupCount)-(inGroupMean[i]*inGroupMean[i]) > 0.0f)
                    inGroupStd[i] = (float)sqrt((inGroupStd[i]/inGroupCount)-(inGroupMean[i]*inGroupMean[i]));
                else
                    inGroupStd[i] = 0.0f;

                outGroupMean[i] = outGroupMean[i]/outGroupCount;
                if ((outGroupStd[i]/outGroupCount)-(outGroupMean[i]*outGroupMean[i]) > 0.0f)
                    outGroupStd[i] = (float)sqrt((outGroupStd[i]/outGroupCount)-(outGroupMean[i]*outGroupMean[i]));
                else
                    outGroupStd[i] = 0.0f;
            }
            else
            {
                groupValidFlag[i] = FALSE;
                inGroupMean[i] = 0;
                outGroupMean[i] = 0;
                inGroupStd[i] = 1;
                outGroupStd[i] = 1;
            }
        }

        // Do we have a clear pattern? Always trust this test.
        for (i = 0;i < 5; i++)
        {
            if ((inGroupMean[i]+inThresh[patternStart]*inGroupStd[i] < outGroupMean[i] - outThresh[patternStart]*outGroupStd[i]) &&
                (groupValidFlag[i] == TRUE))
            {
                // If this is the right frame remove it!
                state->frameRemovalPattern=i;
                if(i==(PULLDOWN_HIST_LEN-1)%5)
                {
                    // Set a counter that goes up if we have a consistent pattern 80+% of the time, down otherwise
                    if ((timestamp - state->lastRemovedTimestamp > 145) &&
                        (timestamp - state->lastRemovedTimestamp < 175))
                    {
                        if (state->ulPulldownActiveTimerProg < 95)
                            state->ulPulldownActiveTimerProg += 5;
                        else
                        {
                            state->ulPulldownActiveTimerProg = 100;
                            state->bProgressiveTelecineSeen = TRUE;
                        }

                        if (state->ulPulldownActiveTimerIntl > 5)
                            state->ulPulldownActiveTimerIntl -= 5;
                        else
                            state->ulPulldownActiveTimerIntl = 0;
                    }
                    else if (timestamp - state->lastRemovedTimestamp < 300)
                    {
                        if (state->ulPulldownActiveTimerProg > 5)
                            state->ulPulldownActiveTimerProg -= 5;
                        else
                            state->ulPulldownActiveTimerProg = 0;
                    }
                    state->lastRemovedTimestamp = timestamp;
                    goto REMOVE_FRAME;
                }
                else 
                    goto DO_NOTHING;
            }
        }

        // Do we have a pretty clear pattern? Only trust this test if we have succeeded with the strongest test a couple of times
        for(i = 0; i < 5; i++)
        {
            if ((inGroupMean[i]+inThresh[patternStart]*inGroupStd[i] < outMin[i]) &&
                (state->ulPulldownActiveTimerProg > 10) &&
                (groupValidFlag[i] == TRUE))
            {
                // If this is the right frame remove it!
                state->frameRemovalPattern = i;
                if (i == (PULLDOWN_HIST_LEN-1)%5)
                {
                    // Set a counter that goes up if we have a consistent pattern 80+% of the time, down otherwise
                    if ((timestamp - state->lastRemovedTimestamp > 145) && 
                        (timestamp - state->lastRemovedTimestamp < 175))
                    {
                        if (state->ulPulldownActiveTimerProg < 95)
                            state->ulPulldownActiveTimerProg += 5;
                        else
                        {
                            state->ulPulldownActiveTimerProg = 100;
                            state->bProgressiveTelecineSeen = TRUE;
                        }

                        if (state->ulPulldownActiveTimerIntl > 5)
                            state->ulPulldownActiveTimerIntl -= 5;
                        else
                            state->ulPulldownActiveTimerIntl = 0;
                    }
                    else if (timestamp - state->lastRemovedTimestamp < 300)
                    {
                        if (state->ulPulldownActiveTimerProg > 5)
                            state->ulPulldownActiveTimerProg -= 5;
                        else
                            state->ulPulldownActiveTimerProg = 0;
                    }
                    state->lastRemovedTimestamp = timestamp;
                    goto REMOVE_FRAME;
                }
                else 
                    goto DO_NOTHING;
            }
        }

        // No pattern, but things are VERY quiet, and we have been seeing a 3:2 pattern 
        if ((inMax[(PULLDOWN_HIST_LEN-1)%5] < 9) && (state->ulPulldownActiveTimerProg > 50) &&
            (outMax[(PULLDOWN_HIST_LEN-1)%5] < 9) && (groupValidFlag[(PULLDOWN_HIST_LEN-1)%5] == TRUE) &&
            (state->frameRemovalPattern==((PULLDOWN_HIST_LEN-1)%5))
           )
        {
            state->lastRemovedTimestamp = timestamp;
            goto REMOVE_FRAME;
        }
    }

    // If we have been seeing consistent pulldown, and the last two frames have been quiet frames.
    // This is our weakest test
    if (((state->pulldownSadHistAll[(PULLDOWN_HIST_LEN-1)] < 9) ||
        (state->pulldownSadHistAll[(PULLDOWN_HIST_LEN-1)] < state->pulldownSadHistAll[(PULLDOWN_HIST_LEN-2)])) &&
        (state->ulPulldownActiveTimerProg > 50) &&
        (groupValidFlag[(PULLDOWN_HIST_LEN-1)%5] == TRUE)&&(state->frameRemovalPattern==((PULLDOWN_HIST_LEN-1)%5))
       )
    {
        state->lastRemovedTimestamp = timestamp;
        goto REMOVE_FRAME;
    }


    // If we have been seeing consistent pulldown, and the last frame looks very interlaced.
    // This is a weak test
    if ((state->pulldownSadHistAll[(PULLDOWN_HIST_LEN-1)] < state->pulldownSadHistAll[(PULLDOWN_HIST_LEN-2)] * 0.5) &&
        (state->ulPulldownActiveTimerProg > 50) &&
        (groupValidFlag[(PULLDOWN_HIST_LEN-1)%5] == TRUE))
    {
        state->frameRemovalPattern = (PULLDOWN_HIST_LEN-1)%5;
        state->lastRemovedTimestamp = timestamp;
        goto REMOVE_FRAME;
    }

    // Otherwise no pulldown!

    if((inMax[(PULLDOWN_HIST_LEN-1)%5] > 30) || (outMax[(PULLDOWN_HIST_LEN-1)%5] >30))
        if (state->ulPulldownActiveTimerProg > 0)
            state->ulPulldownActiveTimerProg--;

NO_PATTERN:
    // If there was no clear pattern, do nothing
    state->checkNextFrameForProgressive = FALSE;
    return INVTELE_RESULT_NO_PATTERN;

INTERLEAVE_ODD:
    state->checkNextFrameForProgressive = FALSE;

    // Test if data should be re-interleaved.
    // Look at cross difference of old vs. re-stitched. Only replace if better
    if (obviousPatternFlag == FALSE || sceneChangeFlag == TRUE)
    {
        pNew = (LONG32 *)(data);
        pOld = (LONG32 *)(prevData);
        for (i = 0; i < lines; i += 2)  //  only do the luma.
        {
            for(j = 0; j < ll; j++)
            {
                temp = (float)((pNew[j]&0xff00) - (pNew[j+ll]&0xff00));
                sumOld += (temp*temp);
                temp = (float)((pNew[j]&0xff00) - (pOld[j+ll]&0xff00));
                sumNew += (temp*temp);
            }
            pOld += 2*ll;
            pNew += 2*ll;
        }

        if (sumNew > sumOld)
        {
            return INVTELE_RESULT_FRAME_OK;
        }
    }

    state->interleaveOddFlag = TRUE;

    // Re-interleave
    pNew = ((LONG32 *)data)+ll;
    pOld = ((LONG32 *)prevData)+ll;

    for (k = 1; k < lines; k += 2)  //  only do the luma.
    {
        ptir_memcpy(pNew,pOld,sizeof(ULONG32)*ll);
        pOld+=2*ll;
        pNew+=2*ll;
    }
    return INVTELE_RESULT_FRAME_OK;

INTERLEAVE_EVEN:
    state->checkNextFrameForProgressive = FALSE;

    // Test if data should be re-interleaved.
    // Look at cross difference of old vs. re-stitched. Only replace if better

    if (obviousPatternFlag == FALSE || sceneChangeFlag == TRUE)
    {
        pNew = (LONG32 *)(data);
        pOld = (LONG32 *)(prevData);
        for (i = 0; i < lines; i += 2)  //  only do the luma.
        {
            for(j = 0; j < ll; j++)
            {
                temp = (float)((pNew[j]&0xff00) - (pNew[j+ll]&0xff00));
                sumOld += (temp*temp);
                temp = (float)((pNew[j+ll]&0xff00) - (pOld[j]&0xff00));
                sumNew += (temp*temp);
            }
            pOld+=2*ll;
            pNew+=2*ll;
        }

        if (sumNew > sumOld)
        {
            return INVTELE_RESULT_FRAME_OK;
        }
    }

    state->interleaveEvenFlag = TRUE;

    // Re-interleave
    pNew = ((LONG32 *)data);
    pOld = ((LONG32 *)prevData);

    for (k = 0; k < lines; k += 2)  //  only do the luma.
    {
        ptir_memcpy(pNew,pOld,sizeof(ULONG32)*ll);
        pOld+=2*ll;
        pNew+=2*ll;
    }
    return INVTELE_RESULT_FRAME_OK;

DO_NOTHING:
    // One final check in this case
    // If we are here, we think the frame is progressive.
    // If we think the previous frame was also progressive, then
    // the odd and even SADs should be somewhat similar.  If not,
    // we're in trouble.
    if (state->checkNextFrameForProgressive)
    {
        if (state->ulPulldownActiveTimerProg < 10 &&
            state->ulPulldownActiveTimerIntl > 90 &&
            (sumEven > 8 * sumOdd || sumOdd > 8 * sumEven))
        {
            goto NO_PATTERN;
        }

        if (sceneChangeFlag == TRUE)
        {
            // Just return here because we want to be sure
            // state->checkNextFrameForProgressive is TRUE
            return INVTELE_RESULT_NO_PATTERN;
        }
    }
    state->checkNextFrameForProgressive = TRUE;
    return INVTELE_RESULT_FRAME_OK;

REMOVE_FRAME:
    state->checkNextFrameForProgressive = FALSE;
    return INVTELE_RESULT_DROP_FRAME;

TOO_EARLY:
    state->checkNextFrameForProgressive = FALSE;
    return INVTELE_RESULT_TOO_EARLY;
}
//*/
int InvTelecine(unsigned char *frame, unsigned char *prev_frame, int pels, int lines)
{
    return INVTELE_RESULT_FRAME_OK;
}

static const float pi = 3.14159265f;
static const float sF0[] = {0.0f, 0.0f, 1.0f, 0.0f, 0.0f, 0.0f};
static const float sF1[] = {0.127323954f, -0.212206591f, 0.636619772f, 0.636619772f, -0.212206591f, 0.127323954f};

static const int CCIR[] = {-29,0,88,138,88,0,-29,32767,65536};

#define CLIP(i) min(max(i, 0), 255)

void Filter_CCIR601(Plane *plaOut, Plane *plaIn, int *data)
{
    int    i, j, k;

    for (j = 0; j < (int)plaIn->uiHeight; ++j)
    {
        for (i = 0; i < (int)plaOut->uiWidth; ++i)
        {
            int Qi = 2 * i + j * plaIn->uiStride;
            int Qo = i + (j + 3) * plaOut->uiWidth;
            int total = 0;
            for (k = 0; k < 7; k++)
                total += plaIn->ucCorner[Qi + k - 3] * CCIR[k];
            data[Qo] = total;
        }
    }

    for (j = 0; j < (int)plaOut->uiHeight; ++j)
    {
        for (i = 0; i < (int)plaOut->uiWidth; ++i)
        {
            int Qi = i + 2 * j * plaOut->uiWidth;
            int total = 0;
            for (k = 0; k < 7; k++)
                total += data[Qi + k * plaOut->uiWidth] * CCIR[k];

            plaOut->ucCorner[i + j * plaOut->uiStride] = (BYTE)CLIP((total + CCIR[7]) / CCIR[8]);
        }
    }
}
static __inline float sinc(float x)
{
    return (x == 0) ? 1.0f : sinf(pi * x) / (pi * x);
}
static void Filter_sinc(Plane *plaOut, Plane *plaIn)
{
    int    i, j, m, n, r = 3;
    float scaleX = (float)plaIn->uiWidth / (float)plaOut->uiWidth;
    float scaleY = (float)plaIn->uiHeight / (float)plaOut->uiHeight;
    int mode = 0;
    if (scaleX == 1.5f && scaleY == 1.5f)
        mode = 1;

    for (j = 0; j < (int)plaOut->uiHeight; ++j)
    {
        for (i = 0; i < (int)plaOut->uiWidth; ++i)
        {
            int qOut = i + j * (int)plaOut->uiStride;
            float x = (float)i * scaleX;
            float y = (float)j * scaleY;
            int X = (int)x;
            int Y = (int)y;
            int sX = max(0, X - r + 1);
            int sY = max(0, Y - r + 1);
            int tX = min(X + r + 1, (int)plaIn->uiWidth);
            int tY = min(Y + r + 1, (int)plaIn->uiHeight);
            float total = 0.0f;
            float totalc = 0.0f;
            if (mode)
            {
                float *sincFX = (float *)sF1;
                float *sincFY = (float *)sF1;
                if (x == X)
                    sincFX = (float *)sF0;
                if (y == Y)
                    sincFY = (float *)sF0;

                for (n = sY; n < tY; ++n)
                {
                    for (m = sX; m < tX; ++m)
                    {
                        float coeff = sincFX[m - X + 2] * sincFY[n - Y + 2];
                        total += plaIn->ucCorner[m + n * (int)plaIn->uiStride] * coeff;
                        totalc += coeff;
                    }
                }
            }
            else
            {
                for (n = sY; n < tY; ++n)
                {
                    for (m = sX; m < tX; ++m)
                    {
                        float coeff = sinc((x - (float)m)) * sinc((y - (float)n));
                        total += plaIn->ucCorner[m + n * (int)plaIn->uiStride] * coeff;
                        totalc += coeff;
                    }
                }
            }
            plaOut->ucCorner[qOut] = (unsigned char)(max(0, min(total / totalc, 255)));
        }
    }
}
static void Filter_bilinear(Plane *plaOut, Plane *plaIn)
{
    int    i, j;
    for (j = 0; j < (int)plaOut->uiHeight; ++j)
    {
        for (i = 0; i < (int)plaOut->uiWidth; ++i)
        {
            int qOut = i + j * (int)plaOut->uiStride;
            float x = (float)plaIn->uiWidth * (float)i / (float)plaOut->uiWidth;
            float y = (float)plaIn->uiHeight * (float)j / (float)plaOut->uiHeight;
            int X = (int)x;
            int Y = (int)y;
            float s = x - X;
            float t = y - Y;
            int qIn = X + Y * (int)plaIn->uiStride;
            if (X + 1 < (int)plaIn->uiWidth && Y + 1 < (int)plaIn->uiHeight)
            {
                plaOut->ucCorner[qOut] = (unsigned char)(
                    plaIn->ucCorner[qIn] * (1.0f - s) * (1.0f - t) + 
                    plaIn->ucCorner[qIn + 1] * (s) * (1.0f - t) + 
                    plaIn->ucCorner[qIn + (int)plaIn->uiStride] * (1.0f - s) * (t) + 
                    plaIn->ucCorner[qIn + (int)plaIn->uiStride + 1] * (s) * (t));
            }
            else
            {
                if (X + 1 < (int)plaIn->uiWidth)
                {
                    plaOut->ucCorner[qOut] = (unsigned char)(
                        plaIn->ucCorner[qIn] * (1.0f - s) + 
                        plaIn->ucCorner[qIn + 1] * (s));
                }
                else if (Y + 1 < (int)plaIn->uiHeight)
                {
                    plaOut->ucCorner[qOut] = (unsigned char)(
                        plaIn->ucCorner[qIn] * (1.0f - t) + 
                        plaIn->ucCorner[qIn + (int)plaIn->uiStride] * (t));
                }
                else
                    plaOut->ucCorner[qOut] = plaIn->ucCorner[qIn];
            }
        }
    }
}
static void Filter_block(Plane *plaOut, Plane *plaIn)
{
    int    i, j, m, n;
    for (j = 0; j < (int)plaOut->uiHeight; ++j)
    {
        for (i = 0; i < (int)plaOut->uiWidth; ++i)
        {
            int qOut = i + j * (int)plaOut->uiStride;
            int x1 = (int)((float)plaIn->uiWidth * (float)i / (float)plaOut->uiWidth);
            int y1 = (int)((float)plaIn->uiHeight * (float)j / (float)plaOut->uiHeight);
            int x2 = (int)((float)plaIn->uiWidth * (float)(i+1) / (float)plaOut->uiWidth);
            int y2 = (int)((float)plaIn->uiHeight * (float)(j+1) / (float)plaOut->uiHeight);
            int total = 0;

            if (x2 == x1) x2 = x1 + 1;
            if (y2 == y1) y2 = y1 + 1;

            for (n = y1; n < min(y2, (int)plaIn->uiHeight); ++n)
            {
                for (m = x1; m < min(x2, (int)plaIn->uiWidth); ++m)
                {
                    int qIn = m + n * (int)plaIn->uiStride;
                    total += plaIn->ucCorner[qIn];
                }
            }
            plaOut->ucCorner[qOut] = total / ((x2 - x1) * (y2 - y1));
        }
    }
}
static __inline float lerp(float P0, float P1, float P2, float P3, float s, float is)
{
    return is * is * is * P0 + 3 * is * s * (is * P1 + s * P2) + s * s * s * P3;
}
static void Filter_gradient(Plane *plaOut, Plane *plaIn)
{
    const float w[] = {4.0f, 2.0f, 1.0f};
    const float tw = 1.0f / (7.0f * 3.0f);

    int    i, j;
    int s = plaIn->uiWidth * plaIn->uiHeight * sizeof(float);
    float *gradX = 0;
    float *gradY = 0;
    float rx = 0;
    float ry = 0;
    gradX = (float *)malloc(s);
    if(!gradX)
        return;
    gradY = (float *)malloc(s);
    if(!gradY)
    {
        free(gradX);
        return;
    }

    rx = (float)plaIn->uiWidth / (float)plaOut->uiWidth;
    ry = (float)plaIn->uiHeight / (float)plaOut->uiHeight;
    assert(gradX != NULL);
    assert(gradY != NULL);
    memset(gradX, 0, s);
    memset(gradY, 0, s);

    for (j = 3; j < (int)plaIn->uiHeight - 3; ++j)
    {
        for (i = 3; i < (int)plaIn->uiWidth - 3; ++i)
        {
            int qIn = i + j * (int)plaIn->uiWidth;
            int k;
            float sx = 0, sy = 0;
            for (k = 3; k > 0; --k)
            {
                sy -= w[k - 1] * plaIn->ucCorner[(i) + (j - k) * (int)plaIn->uiWidth];
                sx -= w[k - 1] * plaIn->ucCorner[(i - k) + (j) * (int)plaIn->uiWidth];
                sx += w[k - 1] * plaIn->ucCorner[(i + k) + (j) * (int)plaIn->uiWidth];
                sy += w[k - 1] * plaIn->ucCorner[(i) + (j + k) * (int)plaIn->uiWidth];
            }

            gradX[qIn] = sx * tw * rx;
            gradY[qIn] = sy * tw * ry;
        }
    }

    for (j = 0; j < (int)plaOut->uiHeight; ++j)
    {
        for (i = 0; i < (int)plaOut->uiWidth; ++i)
        {

            int qOut = i + j * (int)plaOut->uiStride;
            float x = rx * (float)i;
            float y = ry * (float)j;
            int X = (int)x;
            int Y = (int)y;
            int qIn = X + Y * (int)plaIn->uiStride;

            float s = x - X;
            float is = 1 - s;
            float t = y - Y;
            float it = 1 - t;

            float result1, result2, result;
            float P0, P1, P2, P3;
            if (X < (int)plaIn->uiWidth - 1 && Y < (int)plaIn->uiHeight - 1)
            {
                P0 = (float)plaIn->ucCorner[qIn];
                P3 = (float)plaIn->ucCorner[qIn + 1];
                P1 = P0 + gradX[qIn];
                P2 = P3 - gradX[qIn + 1];

                result1 = lerp(P0, P1, P2, P3, s, is);

                P0 = (float)plaIn->ucCorner[qIn + (int)plaIn->uiStride];
                P3 = (float)plaIn->ucCorner[qIn + 1 + (int)plaIn->uiStride];
                P1 = P0 + gradX[qIn + (int)plaIn->uiStride];
                P2 = P3 - gradX[qIn + 1 + (int)plaIn->uiStride];

                result2 = lerp(P0, P1, P2, P3, s, is);

                P0 = result1;
                P3 = result2;
                P1 = P0 + is * gradY[qIn] + s * gradY[qIn + 1];
                P2 = P3 - is * gradY[qIn + (int)plaIn->uiStride] - s * gradY[qIn + 1 + (int)plaIn->uiStride];

                result = lerp(P0, P1, P2, P3, t, it);
            }
            else if (X < (int)plaIn->uiWidth - 1)
            {
                P0 = (float)plaIn->ucCorner[qIn];
                P3 = (float)plaIn->ucCorner[qIn + 1];
                P1 = P0 + gradX[qIn];
                P2 = P3 - gradX[qIn + 1];

                result1 = lerp(P0, P1, P2, P3, s, is);

                P0 = result1;
                P1 = P0 + 3.0f * (is * gradY[qIn] + s * gradY[qIn + 1]);

                result = it * P0 + t * P1;
            }
            else if (Y < (int)plaIn->uiHeight - 1)
            {
                P0 = (float)plaIn->ucCorner[qIn];
                P1 = P0 + 3.0f * gradX[qIn];

                result1 = is * P0 + s * P1;

                P0 = (float)plaIn->ucCorner[qIn + (int)plaIn->uiStride];
                P1 = P0 + 3.0f * gradX[qIn + (int)plaIn->uiStride];

                result2 = is * P0 + s * P1;

                P0 = result1;
                P3 = result2;
                P1 = P0 + gradY[qIn];
                P2 = P3 - gradY[qIn + (int)plaIn->uiStride];

                result = lerp(P0, P1, P2, P3, t, it);
            }
            else
            {
                P0 = (float)plaIn->ucCorner[qIn];
                P1 = P0 + 3.0f * gradX[qIn];

                result1 = is * P0 + s * P1;

                P0 = result1;
                P1 = P0 + 3.0f * gradY[qIn];

                result = it * P0 + t * P1;
            }

            plaOut->ucCorner[qOut] = (unsigned char)max(0, min(result + 0.5f, 255));
        }
    }

    free(gradX);
    free(gradY);
}
static void Filter_cos(Plane *plaOut, Plane *plaIn)
{
    const float pi = 3.141592f;
    int    i, j;

    float scaleX = (float)plaIn->uiWidth / (float)plaOut->uiWidth;
    float scaleY = (float)plaIn->uiHeight / (float)plaOut->uiHeight;

    for (j = 0; j < (int)plaOut->uiHeight; ++j)
    {
        for (i = 0; i < (int)plaOut->uiWidth; ++i)
        {
            int qOut = i + j * (int)plaOut->uiStride;
            float x = (float)i * scaleX;
            float y = (float)j * scaleY;
            int X = (int)x;
            int Y = (int)y;
            int qIn = X + Y * (int)plaIn->uiStride;
            float s = cosf(pi * (x - (float)X)) * 0.5f + 0.5f;
            float t = cosf(pi * (y - (float)Y)) * 0.5f + 0.5f;

            if (X + 1 < (int)plaIn->uiWidth && Y + 1 < (int)plaIn->uiHeight)
            {
                plaOut->ucCorner[qOut] =(unsigned char)(
                    (s) * (t) * plaIn->ucCorner[qIn] + 
                    (1.0f - s) * (t) * plaIn->ucCorner[qIn + 1] + 
                    (s) * (1.0f - t) * plaIn->ucCorner[qIn + plaIn->uiWidth] + 
                    (1.0f - s) * (1.0f - t) * plaIn->ucCorner[qIn + 1 + plaIn->uiWidth]);
            }
            else if (X + 1 < (int)plaIn->uiWidth)
            {
                plaOut->ucCorner[qOut] = (unsigned char)(
                    plaIn->ucCorner[qIn] * (s) + 
                    plaIn->ucCorner[qIn + 1] * (1.0f - s));
            }
            else if (Y + 1 < (int)plaIn->uiHeight)
            {
                plaOut->ucCorner[qOut] = (unsigned char)(
                    plaIn->ucCorner[qIn] * (t) + 
                    plaIn->ucCorner[qIn + plaIn->uiWidth] * (1.0f - t));
            }
            else
                plaOut->ucCorner[qOut] = plaIn->ucCorner[qIn];
        }
    }
}
void ReSample(Frame *frmOut, Frame *frmIn)
{
    Plane plaTemp;
    int filter;

    if (frmOut->plaY.uiWidth > frmIn->plaY.uiWidth)
        filter = 3;
    else
        filter = 0;

    if (frmOut->plaY.uiWidth != frmIn->plaY.uiWidth || frmOut->plaY.uiHeight != frmIn->plaY.uiHeight)
    {
        Plane_Create(&plaTemp, frmOut->plaY.uiWidth, frmOut->plaY.uiHeight, 0);

        if (filter == 0)
            Filter_sinc(&plaTemp, &frmIn->plaY);
        else if (filter == 1)
            Filter_bilinear(&plaTemp, &frmIn->plaY);
        else if (filter == 2)
            Filter_block(&plaTemp, &frmIn->plaY);
        else if (filter == 3)
            Filter_gradient(&plaTemp, &frmIn->plaY);
        else if (filter == 4)
            Filter_cos(&plaTemp, &frmIn->plaY);

        AddBorders(&plaTemp, &frmOut->plaY, frmOut->plaY.uiBorder);
        Plane_Release(&plaTemp);
    }
    else
        AddBorders(&frmIn->plaY, &frmOut->plaY, frmOut->plaY.uiBorder);

    if (frmOut->plaU.uiWidth != frmIn->plaU.uiWidth || frmOut->plaU.uiHeight != frmIn->plaU.uiHeight)
    {
        Plane_Create(&plaTemp, frmOut->plaU.uiWidth, frmOut->plaU.uiHeight, 0);

        if (filter == 0)
            Filter_sinc(&plaTemp, &frmIn->plaU);
        else if (filter == 1)
            Filter_bilinear(&plaTemp, &frmIn->plaU);
        else if (filter == 2)
            Filter_block(&plaTemp, &frmIn->plaU);
        else if (filter == 3)
            Filter_gradient(&plaTemp, &frmIn->plaU);
        else if (filter == 4)
            Filter_cos(&plaTemp, &frmIn->plaU);

        AddBorders(&plaTemp, &frmOut->plaU, frmOut->plaU.uiBorder);
        Plane_Release(&plaTemp);
    }
    else
        AddBorders(&frmIn->plaU, &frmOut->plaU, frmOut->plaU.uiBorder);

    if (frmOut->plaV.uiWidth != frmIn->plaV.uiWidth || frmOut->plaV.uiHeight != frmIn->plaV.uiHeight)
    {
        Plane_Create(&plaTemp, frmOut->plaV.uiWidth, frmOut->plaV.uiHeight, 0);

        if (filter == 0)
            Filter_sinc(&plaTemp, &frmIn->plaV);
        else if (filter == 1)
            Filter_bilinear(&plaTemp, &frmIn->plaV);
        else if (filter == 2)
            Filter_block(&plaTemp, &frmIn->plaV);
        else if (filter == 3)
            Filter_gradient(&plaTemp, &frmIn->plaV);
        else if (filter == 4)
            Filter_cos(&plaTemp, &frmIn->plaV);

        AddBorders(&plaTemp, &frmOut->plaV, frmOut->plaV.uiBorder);
        Plane_Release(&plaTemp);
    }
    else
        AddBorders(&frmIn->plaV, &frmOut->plaV, frmOut->plaV.uiBorder);
    ptir_memcpy(frmOut->plaY.ucStats.ucSAD,frmIn->plaY.ucStats.ucSAD,sizeof(double) * 5);
    frmOut->frmProperties.tindex = frmIn->frmProperties.tindex;
}
