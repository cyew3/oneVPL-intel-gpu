/*//////////////////////////////////////////////////////////////////////////////
//
//                  INTEL CORPORATION PROPRIETARY INFORMATION
//     This software is supplied under the terms of a license agreement or
//     nondisclosure agreement with Intel Corporation and may not be copied
//     or disclosed except in accordance with the terms of that agreement.
//          Copyright(c) 2015 Intel Corporation. All Rights Reserved.
//
*/

#include <stdio.h>
#include <stdlib.h> 
#include <math.h>
#pragma warning(push)
#pragma warning(disable : 4100)
#pragma warning(disable : 4201)
#include "cm_rt.h"
#pragma warning(pop)
#include "../include/test_common.h"
#include "../include/genx_h265_cmcode_isa.h"

#ifdef CMRT_EMU
extern "C"
	void SaoProcess(SurfaceIndex SURF_SRC, SurfaceIndex SURF_DST);
#endif //CMRT_EMU

typedef mfxI8  Ipp8s;
typedef mfxU8  Ipp8u;
typedef mfxU16 Ipp16u;
typedef mfxI16 Ipp16s;
typedef mfxU32 Ipp32u;
typedef mfxI32 Ipp32s;
typedef mfxI64 Ipp64s;
typedef mfxF64 Ipp64f;


struct VideoParam
{
	Ipp32s Width;
	Ipp32s Height;		
		
	Ipp32s PicWidthInCtbs;	
	Ipp32s PicHeightInCtbs;		
	Ipp8u  chromaFormatIdc;
	Ipp32s MaxCUSize;		

	Ipp32s bitDepthLuma;
	Ipp32s saoOpt;
	Ipp64f m_rdLambda;
	Ipp32s SAOChromaFlag;
};

struct AddrNode
{
	Ipp32s ctbAddr;
	Ipp32s sameTile;
	Ipp32s sameSlice;
};

struct SaoCtuParam;

struct AddrInfo
{
	AddrNode left;
	AddrNode right;
	AddrNode above;
	AddrNode below;
	AddrNode belowRight;

	Ipp32u region_border_right, region_border_bottom;
	Ipp32u region_border_left, region_border_top;
};

#define MAX_NUM_SAO_CLASSES  32
enum SaoComponentIdx
{
  SAO_Y =0,
  SAO_Cb,
  SAO_Cr,
  NUM_SAO_COMPONENTS
};


struct SaoOffsetParam
{
	int mode_idx;     //ON, OFF, MERGE
	int type_idx;     //EO_0, EO_90, EO_135, EO_45, BO. MERGE: left, above
	int typeAuxInfo; //BO: starting band index
	int offset[MAX_NUM_SAO_CLASSES];
	int saoMaxOffsetQVal;
};


struct SaoCtuParam
{
	SaoOffsetParam& operator[](int compIdx){ return m_offsetParam[compIdx];}

private:
	//SaoCtuParam& operator= (const SaoCtuParam& src);
	SaoOffsetParam m_offsetParam[NUM_SAO_COMPONENTS];
};

namespace {
	void SetVideoParam(VideoParam & videoParam, Ipp32s width, Ipp32s height);
	int RunGpu(const Ipp8u* frameOrigin, int pitchOrigin, Ipp8u* frameRecon, int pitchRecon, const VideoParam* m_par, const AddrInfo* frame_addr_info, SaoCtuParam* frame_sao_param);
	int RunCpu(const Ipp8u* frameOrigin, int pitchOrigin, Ipp8u* frameRecon, int pitchRecon, const VideoParam* m_par, const AddrInfo* frame_addr_info, SaoCtuParam* frame_sao_param);	
	int Compare(const Ipp8u *data1, Ipp32s pitch1, const Ipp8u *data2, Ipp32s pitch2, Ipp32s width, Ipp32s height);	
	int Dump(Ipp8u* data, size_t frameSize, const char* fileName);
};

void SimulateRecon(Ipp8u* input, Ipp32s inPitch, Ipp8u* recon, Ipp32s reconPitch, Ipp32s width, Ipp32s height, Ipp32s maxAbsPixDiff);
int CompareParam(SaoCtuParam* param_ref, SaoCtuParam* param_tst, Ipp32s numCtbs);
void EstimateSao(const Ipp8u* frameOrigin, int pitchOrigin, Ipp8u* frameRecon, int pitchRecon, const VideoParam* m_par, const AddrInfo* frame_addr_info, SaoCtuParam* frame_sao_param);

int TestSao()
{
	FILE *f = fopen(YUV_NAME, "rb");
	if (!f)
		return FAILED;

	// const per frame params
	Ipp32s width = WIDTH;
	Ipp32s height = HEIGHT;

	Ipp32s frameSize = 3 * (width * height) >> 1;
	Ipp8u *input = new Ipp8u[frameSize];	
	Ipp32s inPitch = width;
	size_t bytesRead = fread(input, 1, frameSize, f);
	if (bytesRead != frameSize)
		return FAILED;
	fclose(f);

	Ipp8u *recon = new Ipp8u[frameSize];
	Ipp32s reconPitch = width;

	const int maxAbsPixDiff = 10;
	SimulateRecon(input, inPitch, recon, reconPitch, width, height, maxAbsPixDiff);
	// recon = input + random[-diff, +diff]

	Ipp8u *outputGpu = new Ipp8u[frameSize];
	Ipp32s outPitchGpu = width;
	Ipp8u *outputCpu = new Ipp8u[frameSize];
	memset(outputCpu, 0, frameSize);
	Ipp32s outPitchCpu = width;	

	VideoParam videoParam;
	SetVideoParam(videoParam, width, height);

	Ipp32s numCtbs = videoParam.PicHeightInCtbs * videoParam.PicWidthInCtbs;
    SaoCtuParam* frame_sao_param_cpu = new SaoCtuParam [numCtbs];
	SaoCtuParam* frame_sao_param_gpu = new SaoCtuParam [numCtbs];

	AddrInfo* frame_addr_info = new AddrInfo[numCtbs];

	Dump(input, frameSize, "input.yuv");
	Dump(recon, frameSize, "recon.yuv");

	Ipp32s res;	
	res = RunCpu(input, inPitch, recon, reconPitch, &videoParam, frame_addr_info, frame_sao_param_cpu); 
	CHECK_ERR(res);

	res = RunGpu(input, inPitch, recon, reconPitch, &videoParam, frame_addr_info, frame_sao_param_gpu);
	CHECK_ERR(res);

	res = CompareParam(frame_sao_param_cpu, frame_sao_param_gpu, numCtbs);
	CHECK_ERR(res);    

	delete [] input;
	delete [] recon;
	delete [] outputGpu;
	delete [] outputCpu;
	delete [] frame_sao_param_cpu;
	delete [] frame_sao_param_gpu;
	delete [] frame_addr_info;

	return PASSED;
}

namespace {	
	int RunGpu(const Ipp8u* frameOrigin, int pitchOrigin, Ipp8u* frameRecon, int pitchRecon, const VideoParam* m_par, const AddrInfo* frame_addr_info, SaoCtuParam* frame_sao_param)
	{
		// fake now
		{
			return RunCpu(frameOrigin, pitchOrigin, frameRecon, pitchRecon, m_par, frame_addr_info, frame_sao_param);
		}

		mfxU32 version = 0;
		CmDevice *device = 0;
		Ipp32s res = ::CreateCmDevice(device, version);
		CHECK_CM_ERR(res);

		CmProgram *program = 0;
		res = device->LoadProgram((void *)genx_h265_cmcode, sizeof(genx_h265_cmcode), program);
		CHECK_CM_ERR(res);

		CmKernel *kernel = 0;
		res = device->CreateKernel(program, CM_KERNEL_FUNCTION(SaoProcess), kernel);
		CHECK_CM_ERR(res);

		CmSurface2D *input = 0;
		res = device->CreateSurface2D(WIDTH, HEIGHT, CM_SURFACE_FORMAT_NV12, input);
		CHECK_CM_ERR(res);

		res = input->WriteSurfaceStride(frameOrigin, NULL, pitchOrigin);
		CHECK_CM_ERR(res);

		CmSurface2D *output = 0;
		res = device->CreateSurface2D(WIDTH, HEIGHT, CM_SURFACE_FORMAT_NV12, output);
		CHECK_CM_ERR(res);    

		SurfaceIndex *idxInput = 0;    
		res = input->GetIndex(idxInput);
		CHECK_CM_ERR(res);

		SurfaceIndex *idxOutput = 0;    
		res = output->GetIndex(idxOutput);
		CHECK_CM_ERR(res);    

		res = kernel->SetKernelArg(0, sizeof(*idxInput), idxInput);
		CHECK_CM_ERR(res);
		res = kernel->SetKernelArg(1, sizeof(*idxOutput), idxOutput);
		CHECK_CM_ERR(res);
		/*res = kernel->SetKernelArg(2, sizeof(*idxOutput8x8Cm), idxOutput8x8Cm);
		CHECK_CM_ERR(res);
		res = kernel->SetKernelArg(3, sizeof(WIDTH), &WIDTH);
		CHECK_CM_ERR(res);*/

		//mfxU32 maxCuSize = 16;		
		mfxU32 tsWidth  = 0;//dblkPar.PicWidthInCtbs;
		mfxU32 tsHeight = 0;//dblkPar.PicHeightInCtbs;
		res = kernel->SetThreadCount(tsWidth * tsHeight);
		CHECK_CM_ERR(res);

		CmThreadSpace * threadSpace = 0;
		res = device->CreateThreadSpace(tsWidth, tsHeight, threadSpace);
		CHECK_CM_ERR(res);

		res = threadSpace->SelectThreadDependencyPattern(CM_NONE_DEPENDENCY);
		CHECK_CM_ERR(res);

		CmTask * task = 0;
		res = device->CreateTask(task);
		CHECK_CM_ERR(res);

		res = task->AddKernel(kernel);
		CHECK_CM_ERR(res);

		CmQueue *queue = 0;
		res = device->CreateQueue(queue);
		CHECK_CM_ERR(res);

		CmEvent * e = 0;
		res = queue->Enqueue(task, e, threadSpace);
		CHECK_CM_ERR(res);

		device->DestroyThreadSpace(threadSpace);
		device->DestroyTask(task);

		res = e->WaitForTaskFinished();
		CHECK_CM_ERR(res);

		mfxU64 time;
		e->GetExecutionTime(time);
		printf("TIME=%.3f ms ", time / 1000000.0);

		//output->ReadSurfaceStride(outData, NULL, outPitch);

		queue->DestroyEvent(e);

		device->DestroySurface(input);
		device->DestroySurface(output);
		device->DestroyKernel(kernel);
		device->DestroyProgram(program);
		::DestroyCmDevice(device);

		return PASSED;
	}

	int RunCpu(const Ipp8u* frameOrigin, int pitchOrigin, Ipp8u* frameRecon, int pitchRecon, const VideoParam* m_par, const AddrInfo* frame_addr_info, SaoCtuParam* frame_sao_param)
	{
		EstimateSao(frameOrigin, pitchOrigin, frameRecon, pitchRecon, m_par, frame_addr_info, frame_sao_param);

		return PASSED;
	}

	int Compare(const Ipp8u *data1, Ipp32s pitch1, const Ipp8u *data2, Ipp32s pitch2, Ipp32s width, Ipp32s height)
	{
		// Luma
		for (Ipp32s y = 0; y < height; y++) {
			for (Ipp32s x = 0; x < width; x++) {            
				Ipp32s diff = abs(data1[y * pitch1 + x] - data2[y * pitch2 + x]);
				if (diff) {
					printf("Luma bad pixels (x,y)=(%i,%i)\n", x, y);
					return FAILED;
				}
			}
		}

		// Chroma
		const Ipp8u* refChroma =  data1 + height*pitch1;
		const Ipp8u* tstChroma =  data2 + height*pitch2;

		for (Ipp32s y = 0; y < height/2; y++) {
			for (Ipp32s x = 0; x < width; x++) {            
				Ipp32s diff = abs(refChroma[y * pitch1 + x] - tstChroma[y * pitch2 + x]);
				if (diff) {
					printf("Chroma %s bad pixels (x,y)=(%i,%i)\n", x >> 1, y, (x & 1) ? "U" : "V");
					return FAILED;
				}
			}
		}

		return PASSED;
	}

	int Dump(Ipp8u* data, size_t frameSize, const char* fileName)
	{
		FILE *fout = fopen(fileName, "wb");
		if (!fout)
			return FAILED;

		fwrite(data, 1, frameSize, fout);	
		fclose(fout);

		return PASSED;
	}
};

int CompareParam(SaoCtuParam* param_ref, SaoCtuParam* param_tst, Ipp32s numCtbs)
{
	for (Ipp32s ctb = 0; ctb < numCtbs; ctb++) {
		for (Ipp32s idx = 0; idx < 3; idx++) {
			SaoOffsetParam & ref = param_ref[ctb][idx];
			SaoOffsetParam & tst = param_tst[ctb][idx];

			Ipp32s sts = memcmp(&ref, &tst, sizeof(SaoOffsetParam));
			if (sts) {
				printf("param[ctb = %i][comp=%i] not equal\n", ctb, idx);
				return FAILED;
			}
		}
	}

	return PASSED;
}


//---------------------------------------------------------
// Test Utils/Sim
//---------------------------------------------------------
static
int random(int a,int b)
{    
	if (a > 0) return a + rand() % (b - a);
	else return a + rand() % (abs(a) + b);
}

#define Saturate(min_val, max_val, val) MAX((min_val), MIN((max_val), (val)))

// 8b / 420 only!!!
void SimulateRecon(Ipp8u* input, Ipp32s inPitch, Ipp8u* recon, Ipp32s reconPitch, Ipp32s width, Ipp32s height, Ipp32s maxAbsPixDiff)
{	
	int pix;
	for (int row = 0; row < height; row++) {		
		for (int x = 0; x < width; x++) {
			pix = input[row * inPitch + x] + random(-maxAbsPixDiff, maxAbsPixDiff);
			recon[row * reconPitch + x] = Saturate(0, 255, pix);
		}
	}

	
	Ipp8u* inputUV = input + height * inPitch;
	Ipp8u* reconUV = recon + height * reconPitch;
	for (int row = 0; row < height/2; row++) {
		for (int x = 0; x < width; x++) {
			pix = inputUV[row * inPitch + x] + random(-maxAbsPixDiff, maxAbsPixDiff);
			reconUV[row * reconPitch + x] = Saturate(0, 255, pix);
		}
	}
}

namespace {	
	void SetVideoParam(VideoParam & par, Ipp32s width, Ipp32s height)
	{
		par.Width = width;
		par.Height= height;		

		par.MaxCUSize = 64;// !!!		

		par.PicWidthInCtbs  = (par.Width + par.MaxCUSize - 1) / par.MaxCUSize;	
		par.PicHeightInCtbs = (par.Height + par.MaxCUSize - 1) / par.MaxCUSize;		

		par.chromaFormatIdc = 1;// !!!	

		par.bitDepthLuma = 8;// !!!

		par.saoOpt = 1;
		par.SAOChromaFlag = 1;	

		Ipp32s qp = 27; // !!!
		par.m_rdLambda = pow(2.0, (qp - 12) * (1.0 / 3.0)) * (1.0 / 256.0);	
	}
};

//---------------------------------------------------------
// SAO CPU CODE
//---------------------------------------------------------
#define NUM_SAO_BO_CLASSES_LOG2  5
#define NUM_SAO_BO_CLASSES  (1<<NUM_SAO_BO_CLASSES_LOG2)

#define NUM_SAO_EO_TYPES_LOG2 2
//#define MAX_NUM_SAO_CLASSES  32

enum
{
    SAO_OPT_ALL_MODES       = 1,
    SAO_OPT_FAST_MODES_ONLY = 2
};


enum SaoEOClasses
{
  SAO_CLASS_EO_FULL_VALLEY = 0,
  SAO_CLASS_EO_HALF_VALLEY = 1,
  SAO_CLASS_EO_PLAIN       = 2,
  SAO_CLASS_EO_HALF_PEAK   = 3,
  SAO_CLASS_EO_FULL_PEAK   = 4,
  NUM_SAO_EO_CLASSES,
};


enum SaoModes
{
  SAO_MODE_OFF = 0,
  SAO_MODE_ON,
  SAO_MODE_MERGE,
  NUM_SAO_MODES
};


enum SaoMergeTypes
{
  SAO_MERGE_LEFT =0,
  SAO_MERGE_ABOVE,
  NUM_SAO_MERGE_TYPES
};


enum SaoBaseTypes
{
  SAO_TYPE_EO_0 = 0,
  SAO_TYPE_EO_90,
  SAO_TYPE_EO_135,
  SAO_TYPE_EO_45,

  SAO_TYPE_BO,

  NUM_SAO_BASE_TYPES
};



struct SaoCtuStatistics //data structure for SAO statistics
{
	//static const int MAX_NUM_SAO_CLASSES = 32;

	Ipp64s diff[MAX_NUM_SAO_CLASSES];
	Ipp64s count[MAX_NUM_SAO_CLASSES];

	void Reset()
	{
		memset(diff, 0, sizeof(Ipp64s)*MAX_NUM_SAO_CLASSES);
		memset(count, 0, sizeof(Ipp64s)*MAX_NUM_SAO_CLASSES);
	}
};

struct SaoEstimator
{
    Ipp32s   m_numSaoModes;
    Ipp32s   m_bitDepth;
    Ipp32s   m_saoMaxOffsetQVal;
    SaoCtuStatistics m_statData[NUM_SAO_COMPONENTS][NUM_SAO_BASE_TYPES];    
    Ipp64f   m_labmda[NUM_SAO_COMPONENTS];
};

union CTBBorders
{
	struct
	{
		Ipp8u m_left         : 1;
		Ipp8u m_top          : 1;
		Ipp8u m_right        : 1;
		Ipp8u m_bottom       : 1;
		Ipp8u m_top_left     : 1;
		Ipp8u m_top_right    : 1;
		Ipp8u m_bottom_left  : 1;
		Ipp8u m_bottom_right : 1;
	};
	Ipp8u data;
};


typedef Ipp8u PixType;

#if (defined(__INTEL_COMPILER) || defined(_MSC_VER)) && !defined(_WIN32_WCE)
#define __ALIGN32 __declspec (align(32))
//#define __ALIGN16 __declspec (align(16))
//#define __ALIGN8 __declspec (align(8))
#elif defined(__GNUC__)
#define __ALIGN32 __attribute__ ((aligned (32)))
//#define __ALIGN16 __attribute__ ((aligned (16)))
//#define __ALIGN8 __attribute__ ((aligned (8)))
#else
#define __ALIGN32
//#define __ALIGN16
//#define __ALIGN8
#endif

/* ColorFormat */
enum {
	MFX_CHROMAFORMAT_MONOCHROME =0,
	MFX_CHROMAFORMAT_YUV420     =1,
	MFX_CHROMAFORMAT_YUV422     =2,
	MFX_CHROMAFORMAT_YUV444     =3,
	MFX_CHROMAFORMAT_YUV400     = MFX_CHROMAFORMAT_MONOCHROME,
	MFX_CHROMAFORMAT_YUV411     = 4,
	MFX_CHROMAFORMAT_YUV422H    = MFX_CHROMAFORMAT_YUV422,
	MFX_CHROMAFORMAT_YUV422V    = 5
};


#define MAX_DOUBLE                  1.7e+308    ///< max. value of double-type value
#define Saturate(min_val, max_val, val) MAX((min_val), MIN((max_val), (val)))

// see IEEE "Sample Adaptive Offset in the HEVC standard", p1760 Fast Distortion Estimation
inline Ipp64s EstimateDeltaDist(Ipp64s count, Ipp64s offset, Ipp64s diffSum, int shift)
{
    return (( count*offset*offset - diffSum*offset*2 ) >> shift);
}

Ipp64s GetDistortion( int typeIdx, int typeAuxInfo, int* quantOffset,  SaoCtuStatistics& statData, int shift)
{
    Ipp64s dist=0;
    int startFrom = 0;
    int end = NUM_SAO_EO_CLASSES;

    if (SAO_TYPE_BO == typeIdx) {
        startFrom = typeAuxInfo;
        end = startFrom + 4;
    }
    for (int offsetIdx=startFrom; offsetIdx < end; offsetIdx++) {
        dist += EstimateDeltaDist( statData.count[offsetIdx], quantOffset[offsetIdx], statData.diff[offsetIdx], shift);
    }
    return dist;
}


// try on each offset from [0, inputOffset]
inline int GetBestOffset( int typeIdx, int classIdx, Ipp64f lambda, int inputOffset, 
                          Ipp64s count, Ipp64s diffSum, int shift, int offsetTh,
                          Ipp64f* bestCost = NULL, Ipp64s* bestDist = NULL)
{
    int iterOffset = inputOffset;
    int outputOffset = 0;
    int testOffset;
    Ipp64s tempDist, tempRate;
    Ipp64f cost, minCost;
    const int deltaRate = (typeIdx == SAO_TYPE_BO) ? 2 : 1;
    
    minCost = lambda;
    while (iterOffset != 0) {
        tempRate = abs(iterOffset) + deltaRate;
        if (abs(iterOffset) == offsetTh) {
            tempRate --;
        }
     
        testOffset  = iterOffset;
        tempDist    = EstimateDeltaDist( count, testOffset, diffSum, shift);
        cost    = (Ipp64f)tempDist + lambda * (Ipp64f) tempRate;

        if (cost < minCost) {
            minCost = cost;
            outputOffset = iterOffset;
            if (bestDist) *bestDist = tempDist;
            if (bestCost) *bestCost = cost;
        }
        
        iterOffset = (iterOffset > 0) ? (iterOffset-1) : (iterOffset+1);
    }
    return outputOffset;
}

static int tab_numSaoClass[] = {NUM_SAO_EO_CLASSES, NUM_SAO_EO_CLASSES, NUM_SAO_EO_CLASSES, NUM_SAO_EO_CLASSES, NUM_SAO_BO_CLASSES};


double floor1(double x)
{
	if (x > 0)
		return (int)x;
	else
		return (int)(x-0.9999999999999999);
}

void GetQuantOffsets(int typeIdx,  SaoCtuStatistics& statData, int* quantOffsets, int& typeAuxInfo, Ipp64f lambda, int bitDepth, int saoMaxOffsetQval, int shift)
{
    memset(quantOffsets, 0, sizeof(int)*MAX_NUM_SAO_CLASSES);
    
    int numClasses = tab_numSaoClass[typeIdx];
    int classIdx;

    // calculate 'native' offset
    for (classIdx = 0; classIdx < numClasses; classIdx++) {
        if ( 0 == statData.count[classIdx] || (SAO_CLASS_EO_PLAIN == classIdx && SAO_TYPE_BO != typeIdx) ) {
            continue; //offset will be zero
        }

        Ipp64f meanDiff = (Ipp64f)(statData.diff[classIdx] << (bitDepth - 8)) / (Ipp64f)(statData.count[classIdx]);

        int offset;
        if (bitDepth > 8) {
            offset = (int)meanDiff;
            offset += offset >= 0 ? (1<<(bitDepth-8-1)) : -(1<<(bitDepth-8-1));
            offset >>= (bitDepth-8);
        } else {
            offset = (int)floor1(meanDiff + 0.5);
        }

        quantOffsets[classIdx] = Saturate(-saoMaxOffsetQval, saoMaxOffsetQval, offset);
    }
    
    // try on to improve a 'native' offset
    if (SAO_TYPE_BO == typeIdx) {
        Ipp64f cost[NUM_SAO_BO_CLASSES];
        for (int classIdx = 0; classIdx < NUM_SAO_BO_CLASSES; classIdx++) {
            cost[classIdx] = lambda;
            if (quantOffsets[classIdx] != 0 ) {
                quantOffsets[classIdx] = GetBestOffset( typeIdx, classIdx, lambda, quantOffsets[classIdx], 
                                                        statData.count[classIdx], statData.diff[classIdx],
                                                        shift, saoMaxOffsetQval, cost + classIdx);
            }
        }
        
        Ipp64f minCost = MAX_DOUBLE, bandCost;
        int band, startBand = 0;
        for (band = 0; band < (NUM_SAO_BO_CLASSES - 4 + 1); band++) {
            bandCost  = cost[band  ] + cost[band+1] + cost[band+2] + cost[band+3];

            if (bandCost < minCost) {
                minCost = bandCost;
                startBand = band;
            }
        }

        // clear unused bands
        for (band = 0; band < startBand; band++)
            quantOffsets[band] = 0;

        for (band = startBand + 4; band < NUM_SAO_BO_CLASSES; band++)
            quantOffsets[band] = 0;

        typeAuxInfo = startBand;

    } else { // SAO_TYPE_E[0/45/90/135]
        for (classIdx = 0; classIdx < NUM_SAO_EO_CLASSES; classIdx++) {
            if (SAO_CLASS_EO_FULL_PEAK == classIdx && quantOffsets[classIdx] > 0   ||
                SAO_CLASS_EO_HALF_PEAK == classIdx && quantOffsets[classIdx] > 0   ||
                SAO_CLASS_EO_FULL_VALLEY == classIdx && quantOffsets[classIdx] < 0 ||
                SAO_CLASS_EO_HALF_VALLEY == classIdx && quantOffsets[classIdx] < 0) {
                quantOffsets[classIdx] = 0;
            }

            if ( quantOffsets[classIdx] != 0 ) {
                quantOffsets[classIdx] = GetBestOffset( typeIdx, classIdx, lambda, quantOffsets[classIdx],
                                                        statData.count[classIdx], statData.diff[classIdx], shift, saoMaxOffsetQval);
            }
        }
        typeAuxInfo = 0;
    }
}

Ipp32s CodeSaoCtbOffsetParam_BitCost(int compIdx, SaoOffsetParam& ctbParam, bool sliceEnabled)
{
    Ipp32u code;
    Ipp32s bitsCount = 0;
    if (!sliceEnabled) {
        return 0;
    }
    
    if (compIdx == SAO_Y || compIdx == SAO_Cb) {
        if (ctbParam.mode_idx == SAO_MODE_OFF) {
            code =0;
            bitsCount++;
        } else if(ctbParam.type_idx == SAO_TYPE_BO) {
            code = 1;
            bitsCount++;
        } else {
            //VM_ASSERT(ctbParam.type_idx < SAO_TYPE_BO); //EO
            code = 2;
            bitsCount += 2;
        }
    }

    if (ctbParam.mode_idx == SAO_MODE_ON) {
        int numClasses = (ctbParam.type_idx == SAO_TYPE_BO)?4:NUM_SAO_EO_CLASSES;
        int offset[5]; // only 4 are used, 5th is added for KW
        int k=0;
        for (int i=0; i< numClasses; i++) {
            if (ctbParam.type_idx != SAO_TYPE_BO && i == SAO_CLASS_EO_PLAIN) {
                continue;
            }
            int classIdx = (ctbParam.type_idx == SAO_TYPE_BO)?(  (ctbParam.typeAuxInfo+i)% NUM_SAO_BO_CLASSES   ):i;
            offset[k] = ctbParam.offset[classIdx];
            k++;
        }

        for (int i=0; i< 4; i++) {

            code = (Ipp32u)( offset[i] < 0) ? (-offset[i]) : (offset[i]);
            if (ctbParam.saoMaxOffsetQVal != 0) {
                Ipp32u i;
                Ipp8u code_last = ((Ipp32u)ctbParam.saoMaxOffsetQVal > code);
                if (code == 0) {
                    bitsCount++;
                } else {
                    bitsCount++;
                    for (i=0; i < code-1; i++) {
                        bitsCount++;
                    }
                    if (code_last) {
                        bitsCount++;
                    }
                }
            }
        }


        if (ctbParam.type_idx == SAO_TYPE_BO) {
            for (int i=0; i< 4; i++) {
                if (offset[i] != 0) {
                    bitsCount++;
                }
            }
            bitsCount += NUM_SAO_BO_CLASSES_LOG2;
        } else {
            if(compIdx == SAO_Y || compIdx == SAO_Cb) {
                //VM_ASSERT(ctbParam.type_idx - SAO_TYPE_EO_0 >=0);
                bitsCount +=  NUM_SAO_EO_TYPES_LOG2;
            }
        }
    }

    return bitsCount << 8;
} 


Ipp32s CodeSaoCtbParam_BitCost(SaoCtuParam& saoBlkParam, bool* sliceEnabled, bool leftMergeAvail, bool aboveMergeAvail, bool onlyEstMergeInfo)
{
    bool isLeftMerge = false;
    bool isAboveMerge= false;
    Ipp32u code = 0;
    Ipp32s bitsCount = 0;

    if(leftMergeAvail) {
        isLeftMerge = ((saoBlkParam[SAO_Y].mode_idx == SAO_MODE_MERGE) && (saoBlkParam[SAO_Y].type_idx == SAO_MERGE_LEFT));
        code = isLeftMerge ? 1 : 0;
        bitsCount++;
    }

    if( aboveMergeAvail && !isLeftMerge) {
        isAboveMerge = ((saoBlkParam[SAO_Y].mode_idx == SAO_MODE_MERGE) && (saoBlkParam[SAO_Y].type_idx == SAO_MERGE_ABOVE));
        code = isAboveMerge ? 1 : 0;
        bitsCount++;
    }

    bitsCount <<= 8;
    if(onlyEstMergeInfo) {
        return bitsCount;
    }
    
    if(!isLeftMerge && !isAboveMerge) {
        
        for (int compIdx=0; compIdx < NUM_SAO_COMPONENTS; compIdx++) {
            bitsCount += CodeSaoCtbOffsetParam_BitCost(compIdx, saoBlkParam[compIdx], sliceEnabled[compIdx]);
        }
    }

    return bitsCount;
}


void GetBestSao_BitCost(bool* sliceEnabled, SaoCtuParam* mergeList[2], SaoCtuParam* codedParam, SaoEstimator & state)
{
	Ipp32s   m_numSaoModes = state.m_numSaoModes;
	Ipp32s   m_bitDepth = state.m_bitDepth;
	Ipp32s   m_saoMaxOffsetQVal = state.m_saoMaxOffsetQVal;
	Ipp64f   m_labmda[NUM_SAO_COMPONENTS] = {state.m_labmda[0], state.m_labmda[1], state.m_labmda[2]};		

    SaoCtuParam &bestParam = *codedParam;

    const Ipp32s shift = 2 * (m_bitDepth - 8);
    Ipp64f cost = 0.0;
    Ipp64s dist[NUM_SAO_COMPONENTS] = {0};
    Ipp64s modeDist[NUM_SAO_COMPONENTS] = {0};
    SaoOffsetParam testOffset[NUM_SAO_COMPONENTS];

    bestParam[SAO_Y].mode_idx = SAO_MODE_OFF;
    bestParam[SAO_Y].saoMaxOffsetQVal = m_saoMaxOffsetQVal;
    
    Ipp32s bitCost = CodeSaoCtbParam_BitCost(bestParam, sliceEnabled, mergeList[SAO_MERGE_LEFT] != NULL, mergeList[SAO_MERGE_ABOVE] != NULL, true);
    
    bitCost = CodeSaoCtbOffsetParam_BitCost(SAO_Y, bestParam[SAO_Y], sliceEnabled[SAO_Y]);
    
    Ipp64f minCost = m_labmda[SAO_Y] * bitCost;

    if (sliceEnabled[SAO_Y]) {
        for (Ipp32s type_idx = 0; type_idx < m_numSaoModes; type_idx++) {
            testOffset[SAO_Y].mode_idx = SAO_MODE_ON;
            testOffset[SAO_Y].type_idx = type_idx;
            testOffset[SAO_Y].saoMaxOffsetQVal = m_saoMaxOffsetQVal;

            GetQuantOffsets(type_idx, state.m_statData[SAO_Y][type_idx], testOffset[SAO_Y].offset,
                testOffset[SAO_Y].typeAuxInfo, m_labmda[SAO_Y], m_bitDepth, m_saoMaxOffsetQVal, shift);

            dist[SAO_Y] = GetDistortion(testOffset[SAO_Y].type_idx, testOffset[SAO_Y].typeAuxInfo,
                testOffset[SAO_Y].offset, state.m_statData[SAO_Y][type_idx], shift);
            
            bitCost = CodeSaoCtbOffsetParam_BitCost(SAO_Y, testOffset[SAO_Y], sliceEnabled[SAO_Y]);

            cost = dist[SAO_Y] + m_labmda[SAO_Y]*bitCost;
            if (cost < minCost) {
                minCost = cost;
                modeDist[SAO_Y] = dist[SAO_Y];
                memcpy(&bestParam[SAO_Y], testOffset + SAO_Y, sizeof(SaoOffsetParam));
            }
        }
    }

    Ipp64f chromaLambda = m_labmda[SAO_Cb];
    if ( sliceEnabled[SAO_Cb] ) {
        //VM_ASSERT(m_labmda[SAO_Cb] == m_labmda[SAO_Cr]);
        //VM_ASSERT(sliceEnabled[SAO_Cb] == sliceEnabled[SAO_Cr]);

        bestParam[SAO_Cb].mode_idx = SAO_MODE_OFF;
        bestParam[SAO_Cb].saoMaxOffsetQVal = m_saoMaxOffsetQVal;
        bitCost = CodeSaoCtbOffsetParam_BitCost(SAO_Cb, bestParam[SAO_Cb], sliceEnabled[SAO_Cb]);

        bestParam[SAO_Cr].mode_idx = SAO_MODE_OFF;
        bestParam[SAO_Cr].saoMaxOffsetQVal = m_saoMaxOffsetQVal;
        bitCost += CodeSaoCtbOffsetParam_BitCost(SAO_Cr, bestParam[SAO_Cr], sliceEnabled[SAO_Cr]);

        minCost= chromaLambda * bitCost;
        for (Ipp32s type_idx = 0; type_idx < m_numSaoModes; type_idx++) {
            for (Ipp32s compIdx = SAO_Cb; compIdx < NUM_SAO_COMPONENTS; compIdx++) {
                testOffset[compIdx].mode_idx = SAO_MODE_ON;
                testOffset[compIdx].type_idx = type_idx;
                testOffset[compIdx].saoMaxOffsetQVal = m_saoMaxOffsetQVal;

                GetQuantOffsets(type_idx, state.m_statData[compIdx][type_idx], testOffset[compIdx].offset,
                    testOffset[compIdx].typeAuxInfo, m_labmda[compIdx], m_bitDepth, m_saoMaxOffsetQVal, shift);

                dist[compIdx]= GetDistortion(type_idx, testOffset[compIdx].typeAuxInfo,
                    testOffset[compIdx].offset, state.m_statData[compIdx][type_idx], shift);
            }

            bitCost = CodeSaoCtbOffsetParam_BitCost(SAO_Cb, testOffset[SAO_Cb], sliceEnabled[SAO_Cb]);
            bitCost += CodeSaoCtbOffsetParam_BitCost(SAO_Cr, testOffset[SAO_Cr], sliceEnabled[SAO_Cr]);

            cost = (dist[SAO_Cb]+ dist[SAO_Cr]) + chromaLambda*bitCost;
            if (cost < minCost) {
                minCost = cost;
                modeDist[SAO_Cb] = dist[SAO_Cb];
                modeDist[SAO_Cr] = dist[SAO_Cr];
                memcpy(&bestParam[SAO_Cb], testOffset + SAO_Cb, 2*sizeof(SaoOffsetParam));
            }
        }
    }
    
    bitCost = CodeSaoCtbParam_BitCost(bestParam, sliceEnabled, (mergeList[SAO_MERGE_LEFT]!= NULL), (mergeList[SAO_MERGE_ABOVE]!= NULL), false);

    Ipp64f bestCost = modeDist[SAO_Y] / m_labmda[SAO_Y] + (modeDist[SAO_Cb]+ modeDist[SAO_Cr])/chromaLambda + bitCost;

#if defined(SAO_MODE_MERGE_ENABLED)
    SaoCtuParam testParam;
    for (Ipp32s mode = 0; mode < 2; mode++) {
        if (NULL == mergeList[mode])
            continue;
        
        small_memcpy(&testParam, mergeList[mode], sizeof(SaoCtuParam));
        testParam[0].mode_idx = SAO_MODE_MERGE;
        testParam[0].type_idx = mode;

        SaoOffsetParam& mergedOffset = (*(mergeList[mode]))[0];

        Ipp64s distortion=0;
        if ( SAO_MODE_OFF != mergedOffset.mode_idx ) {
            distortion = GetDistortion( mergedOffset.type_idx, mergedOffset.typeAuxInfo,
                mergedOffset.offset, state.m_statData[0][mergedOffset.type_idx], shift);
        }

        bitCost = CodeSaoCtbParam_BitCost(testParam, sliceEnabled, (NULL != mergeList[SAO_MERGE_LEFT]), (NULL != mergeList[SAO_MERGE_ABOVE]), false);

        cost = distortion / m_labmda[0] + bitCost;
        if (cost < bestCost) {
            bestCost = cost;
            small_memcpy(&bestParam, &testParam, sizeof(SaoCtuParam));
        }
    }
#endif

}

template <typename PixType>
void h265_SplitChromaCtb( PixType* in, Ipp32s inPitch, PixType* out1, Ipp32s outPitch1, PixType* out2, Ipp32s outPitch2, Ipp32s width, Ipp32s height)
{
    PixType* src = in;
    PixType* dst1 = out1;
    PixType* dst2 = out2;

    for (Ipp32s h = 0; h < height; h++) {
        for (Ipp32s w = 0; w < width; w++) {
            dst1[w] = src[2*w];
            dst2[w] = src[2*w+1];
        }
        src += inPitch;
        dst1 += outPitch1;
        dst2 += outPitch2;
    }
}

    enum SAOType
    {
        SAO_EO_0 = 0,
        SAO_EO_1,
        SAO_EO_2,
        SAO_EO_3,
        SAO_BO,
        MAX_NUM_SAO_TYPE
    };

    /** get the sign of input variable
    * \param   x
    */
    inline Ipp32s getSign(Ipp32s x)
    {
        return ((x >> 31) | ((Ipp32s)( (((Ipp32u) -x)) >> 31)));
    }

    const int   g_skipLinesR[3] = {1, 1, 1};//YCbCr
    const int   g_skipLinesB[3] = {1, 1, 1};//YCbCr

void h265_GetCtuStatistics_8u(int compIdx, const Ipp8u* recBlk, int recStride, const Ipp8u* orgBlk, int orgStride, int width, 
        int height, int shift,  CTBBorders& borders, int numSaoModes, SaoCtuStatistics* statsDataTypes)       
    {
        Ipp16s signLineBuf1[64+1];
        Ipp16s signLineBuf2[64+1];

        int x, y, startX, startY, endX, endY;
        int firstLineStartX, firstLineEndX;
        int edgeType;
        Ipp16s signLeft, signRight, signDown;
        Ipp64s *diff, *count;

        //const int compIdx = SAO_Y;
        int skipLinesR = g_skipLinesR[compIdx];
        int skipLinesB = g_skipLinesB[compIdx];

        const PixType *recLine, *orgLine;
        const PixType* recLineAbove;
        const PixType* recLineBelow;


        for(int typeIdx=0; typeIdx< numSaoModes; typeIdx++)
        {
            SaoCtuStatistics& statsData= statsDataTypes[typeIdx];
            statsData.Reset();

            recLine = recBlk;
            orgLine = orgBlk;
            diff    = statsData.diff;
            count   = statsData.count;
            switch(typeIdx)
            {
            case SAO_EO_0://SAO_TYPE_EO_0:
                {
                    diff +=2;
                    count+=2;
                    endY   = height - skipLinesB;

                    startX = borders.m_left ? 0 : 1;
                    endX   = width - skipLinesR;

                    for (y=0; y<endY; y++)
                    {
                        signLeft = getSign(recLine[startX] - recLine[startX-1]);
                        for (x=startX; x<endX; x++)
                        {
                            signRight =  getSign(recLine[x] - recLine[x+1]);
                            edgeType  =  signRight + signLeft;
                            signLeft  = -signRight;

                            diff [edgeType] += (orgLine[x << shift] - recLine[x]);
                            count[edgeType] ++;
                        }
                        recLine  += recStride;
                        orgLine  += orgStride;
                    }
                }
                break;

            case SAO_EO_1: //SAO_TYPE_EO_90:
                {
                    diff +=2;
                    count+=2;
                    endX   = width - skipLinesR;

                    startY = borders.m_top ? 0 : 1;
                    endY   = height - skipLinesB;
                    if ( 0 == borders.m_top )
                    {
                        recLine += recStride;
                        orgLine += orgStride;
                    }

                    recLineAbove = recLine - recStride;
                    Ipp16s *signUpLine = signLineBuf1;
                    for (x=0; x< endX; x++) 
                    {
                        signUpLine[x] = getSign(recLine[x] - recLineAbove[x]);
                    }

                    for (y=startY; y<endY; y++)
                    {
                        recLineBelow = recLine + recStride;

                        for (x=0; x<endX; x++)
                        {
                            signDown  = getSign(recLine[x] - recLineBelow[x]); 
                            edgeType  = signDown + signUpLine[x];
                            signUpLine[x]= -signDown;

                            diff [edgeType] += (orgLine[x<<shift] - recLine[x]);
                            count[edgeType] ++;
                        }
                        recLine += recStride;
                        orgLine += orgStride;
                    }
                }
                break;

            case SAO_EO_2: //SAO_TYPE_EO_135:
                {
                    diff +=2;
                    count+=2;
                    startX = borders.m_left ? 0 : 1 ;
                    endX   = width - skipLinesR;
                    endY   = height - skipLinesB;

                    //prepare 2nd line's upper sign
                    Ipp16s *signUpLine, *signDownLine, *signTmpLine;
                    signUpLine  = signLineBuf1;
                    signDownLine= signLineBuf2;
                    recLineBelow = recLine + recStride;
                    for (x=startX; x<endX+1; x++)
                    {
                        signUpLine[x] = getSign(recLineBelow[x] - recLine[x-1]);
                    }

                    //1st line
                    recLineAbove = recLine - recStride;
                    firstLineStartX = borders.m_top_left ? 0 : 1;
                    firstLineEndX   = borders.m_top      ? endX : 1;

                    for(x=firstLineStartX; x<firstLineEndX; x++)
                    {
                        edgeType = getSign(recLine[x] - recLineAbove[x-1]) - signUpLine[x+1];
                        diff [edgeType] += (orgLine[x<<shift] - recLine[x]);
                        count[edgeType] ++;
                    }
                    recLine  += recStride;
                    orgLine  += orgStride;


                    //middle lines
                    for (y=1; y<endY; y++)
                    {
                        recLineBelow = recLine + recStride;

                        for (x=startX; x<endX; x++)
                        {
                            signDown = getSign(recLine[x] - recLineBelow[x+1]);
                            edgeType = signDown + signUpLine[x];
                            diff [edgeType] += (orgLine[x<<shift] - recLine[x]);
                            count[edgeType] ++;

                            signDownLine[x+1] = -signDown; 
                        }
                        signDownLine[startX] = getSign(recLineBelow[startX] - recLine[startX-1]);

                        signTmpLine  = signUpLine;
                        signUpLine   = signDownLine;
                        signDownLine = signTmpLine;

                        recLine += recStride;
                        orgLine += orgStride;
                    }
                }
                break;

            case SAO_EO_3: //SAO_TYPE_EO_45:
                {
                    diff +=2;
                    count+=2;
                    endY   = height - skipLinesB;

                    startX = borders.m_left ? 0 : 1;
                    endX   = width - skipLinesR;

                    //prepare 2nd line upper sign
                    recLineBelow = recLine + recStride;
                    Ipp16s *signUpLine = signLineBuf1+1;
                    for (x=startX-1; x<endX; x++)
                    {
                        signUpLine[x] = getSign(recLineBelow[x] - recLine[x+1]);
                    }

                    //first line
                    recLineAbove = recLine - recStride;

                    firstLineStartX = borders.m_top ? startX : endX;
                    firstLineEndX   = endX;

                    for(x=firstLineStartX; x<firstLineEndX; x++)
                    {
                        edgeType = getSign(recLine[x] - recLineAbove[x+1]) - signUpLine[x-1];
                        diff [edgeType] += (orgLine[x<<shift] - recLine[x]);
                        count[edgeType] ++;
                    }

                    recLine += recStride;
                    orgLine += orgStride;

                    //middle lines
                    for (y=1; y<endY; y++)
                    {
                        recLineBelow = recLine + recStride;

                        for(x=startX; x<endX; x++)
                        {
                            signDown = getSign(recLine[x] - recLineBelow[x-1]) ;
                            edgeType = signDown + signUpLine[x];

                            diff [edgeType] += (orgLine[x<<shift] - recLine[x]);
                            count[edgeType] ++;

                            signUpLine[x-1] = -signDown; 
                        }
                        signUpLine[endX-1] = getSign(recLineBelow[endX-1] - recLine[endX]);
                        recLine  += recStride;
                        orgLine  += orgStride;
                    }
                }
                break;

            case SAO_BO: //SAO_TYPE_BO:
                {
                    endX = width- skipLinesR;
                    endY = height- skipLinesB;
                    const int shiftBits = 3;
                    for (y=0; y<endY; y++)
                    {
                        for (x=0; x<endX; x++)
                        {
                            int bandIdx= recLine[x] >> shiftBits; 
                            diff [bandIdx] += (orgLine[x<<shift] - recLine[x]);
                            count[bandIdx]++;
                        }
                        recLine += recStride;
                        orgLine += orgStride;
                    }
                }
                break;

            default:
                {
                    //VM_ASSERT(!"Not a supported SAO types\n");
                }
            }
        }

    } // void GetCtuStatistics_Internal(...)


void EstimateSao(const Ipp8u* frameOrigin, int pitchOrigin, Ipp8u* frameRecon, int pitchRecon, const VideoParam* m_par, const AddrInfo* frame_addr_info, SaoCtuParam* frame_sao_param)
{
	SaoEstimator m_saoEst;

    m_saoEst.m_bitDepth = m_par->bitDepthLuma;// need to fix in case of chroma != luma bitDepth
    m_saoEst.m_saoMaxOffsetQVal = (1<<(MIN(m_saoEst.m_bitDepth,10)-5))-1;
#ifndef AMT_SAO_MIN
    m_saoEst.m_numSaoModes = (m_par->saoOpt == SAO_OPT_ALL_MODES) ? NUM_SAO_BASE_TYPES : NUM_SAO_BASE_TYPES - 1;
#else
    m_saoEst.m_numSaoModes = (m_par->saoOpt == SAO_OPT_ALL_MODES) ? NUM_SAO_BASE_TYPES : ((m_par->saoOpt == SAO_OPT_FAST_MODES_ONLY)? NUM_SAO_BASE_TYPES - 1: 1);
#endif

	int numCtbs = m_par->PicWidthInCtbs * m_par->PicHeightInCtbs;

	// CPU kernel
	for (int ctbAddr = 0; ctbAddr < numCtbs; ctbAddr++) 
	{
		Ipp32s chromaShiftW = (m_par->chromaFormatIdc != MFX_CHROMAFORMAT_YUV444);
		Ipp32s chromaShiftH = (m_par->chromaFormatIdc == MFX_CHROMAFORMAT_YUV420);
		Ipp32s chromaShiftWInv = 1 - chromaShiftW;

		Ipp32s ctbPelX = (ctbAddr % m_par->PicWidthInCtbs) * m_par->MaxCUSize;
		Ipp32s ctbPelY = (ctbAddr / m_par->PicWidthInCtbs) * m_par->MaxCUSize;

		Ipp32s pitchRecLuma = pitchRecon;
		Ipp32s pitchRecChroma = pitchRecon;
		Ipp32s pitchSrcLuma = pitchOrigin;
		Ipp32s pitchSrcChroma = pitchOrigin;

		PixType* reconY  = frameRecon;
		PixType* reconUV = frameRecon + m_par->Height * pitchRecon;
		const PixType* originY  = frameOrigin;
		const PixType* originUV = frameOrigin + m_par->Height * pitchOrigin;

		PixType* m_yRec = (PixType*)reconY + ctbPelX + ctbPelY * pitchRecLuma;
		PixType* m_uvRec = (PixType*)reconUV + (ctbPelX << chromaShiftWInv) + (ctbPelY * pitchRecChroma >> chromaShiftH);
		PixType* m_ySrc = (PixType*)originY + ctbPelX + ctbPelY * pitchSrcLuma;
		PixType* m_uvSrc = (PixType*)originUV + (ctbPelX << chromaShiftWInv) + (ctbPelY * pitchSrcChroma >> chromaShiftH);

		const AddrInfo& info = frame_addr_info[ctbAddr];
		int m_leftAddr  = info.left.ctbAddr;
		int m_aboveAddr = info.above.ctbAddr;
		int m_leftSameTile = info.left.sameTile;
		int m_aboveSameTile = info.above.sameTile;
		int m_leftSameSlice = info.left.sameSlice;
		int m_aboveSameSlice = info.above.sameSlice;

		CTBBorders borders = {0};
		borders.m_left = (-1 == m_leftAddr)  ? 0 : (m_leftSameSlice  && m_leftSameTile) ? 1 : 0;
		borders.m_top  = (-1 == m_aboveAddr) ? 0 : (m_aboveSameSlice && m_aboveSameTile) ? 1 : 0;

		SaoCtuParam tmpMergeParam;
		SaoCtuParam* mergeList[2] = {NULL, NULL};
		if (borders.m_top) { // hack
			mergeList[SAO_MERGE_ABOVE] = &tmpMergeParam;//&(saoParam[m_aboveAddr]);
		}
		if (borders.m_left) {
			mergeList[SAO_MERGE_LEFT] = &tmpMergeParam;//&(saoParam[m_leftAddr]);
		}

		//   workaround (better to rewrite optimized SAO estimate functions to use tmpU and tmpL)
		borders.m_left = 0;
		borders.m_top  = 0;

		Ipp32s height = (ctbPelY + m_par->MaxCUSize > m_par->Height) ? (m_par->Height- ctbPelY) : m_par->MaxCUSize;
		Ipp32s width  = (ctbPelX + m_par->MaxCUSize  > m_par->Width) ? (m_par->Width - ctbPelX) : m_par->MaxCUSize;

		Ipp32s offset = 0;// YV12/NV12 Chroma dispatcher
		h265_GetCtuStatistics_8u(SAO_Y, (PixType*)m_yRec, pitchSrcLuma, (PixType*)m_ySrc, pitchSrcLuma, width, height, offset, borders, m_saoEst.m_numSaoModes, m_saoEst.m_statData[SAO_Y]);

		if (m_par->SAOChromaFlag) {
			const Ipp32s  planePitch = 64;
			__ALIGN32 PixType reconU[64*64];
			__ALIGN32 PixType reconV[64*64];
			__ALIGN32 PixType originU[64*64];
			__ALIGN32 PixType originV[64*64];

			// Chroma - interleaved format
			Ipp32s shiftW = 1;
			Ipp32s shiftH = 1;
			if (m_par->chromaFormatIdc == MFX_CHROMAFORMAT_YUV422) {
				shiftH = 0;
			} else if (m_par->chromaFormatIdc == MFX_CHROMAFORMAT_YUV444) {
				shiftH = 0;
				shiftW = 0;
			}    
			width  >>= shiftW;
			height >>= shiftH;

			h265_SplitChromaCtb( (PixType*)m_uvRec, pitchRecChroma, reconU,  planePitch, reconV,  planePitch, m_par->MaxCUSize >> shiftW, height);
			h265_SplitChromaCtb( (PixType*)m_uvSrc, pitchSrcChroma, originU, planePitch, originV, planePitch, m_par->MaxCUSize >> shiftW, height);

			Ipp32s compIdx = 1;
			h265_GetCtuStatistics_8u(compIdx, reconU, planePitch, originU, planePitch, width, height, offset, borders, m_saoEst.m_numSaoModes, m_saoEst.m_statData[compIdx]);
			compIdx = 2;
			h265_GetCtuStatistics_8u(compIdx, reconV, planePitch, originV, planePitch, width, height, offset, borders, m_saoEst.m_numSaoModes, m_saoEst.m_statData[compIdx]);
		}


		m_saoEst.m_labmda[0] = m_par->m_rdLambda;
		m_saoEst.m_labmda[1] = m_saoEst.m_labmda[2] = m_saoEst.m_labmda[0];

		bool sliceEnabled[NUM_SAO_COMPONENTS] = {true, false, false};
		if (m_par->SAOChromaFlag)
			sliceEnabled[1] = sliceEnabled[2] = true;

		GetBestSao_BitCost(sliceEnabled, mergeList, &frame_sao_param[ctbAddr], m_saoEst);		

		//EncodeSao(bs, 0, 0, 0, saoParam[m_ctbAddr], mergeList[SAO_MERGE_LEFT] != NULL, mergeList[SAO_MERGE_ABOVE] != NULL);

		//ReconstructSao(mergeList, frame_sao_param[ctbAddr], m_par->SAOChromaFlag);
	}	
}

/* EOF */
