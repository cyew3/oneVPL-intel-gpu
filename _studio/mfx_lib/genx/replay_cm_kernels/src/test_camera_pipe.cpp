/*//////////////////////////////////////////////////////////////////////////////
////                  INTEL CORPORATION PROPRIETARY INFORMATION
//     This software is supplied under the terms of a license agreement or
//     nondisclosure agreement with Intel Corporation and may not be copied
//     or disclosed except in accordance with the terms of that agreement.
//          Copyright(c) 2012-2017 Intel Corporation. All Rights Reserved.
//
*/

#pragma warning(push)
#pragma warning(disable : 4100)
#pragma warning(disable : 4201)
#include "cm_rt.h"
#pragma warning(pop)

#include <string>
#include <sstream>

#include "../include/test_common.h"
#include "../include/genx_camera_pipe_skl_isa.h"
#include "../../WarpMDF_engine/pvt_mdf_kernel_config.hpp"

struct camera_pipe_params
{
	Ipp32u y_width;
	Ipp32u y_height;

	std::string inp_fname;
	std::string out_fname;
	std::string smp_parms;
};

static int PrepareParams(int argc, char *argv[], camera_pipe_params &params)
{
	if(argc < 6) {
		fprintf(stderr, "Insufficient input params.\n");
		return FAILED;
	}

	params.inp_fname = std::string(argv[1]);
	params.out_fname = std::string(argv[2]);
	params.smp_parms = std::string(argv[3]);

	std::stringstream ss_image_input_width(argv[4]);
	std::stringstream ss_image_input_height(argv[5]);

	ss_image_input_width >> params.y_width;
	ss_image_input_height >> params.y_height;

	return PASSED;
}

struct TAvsMsg
{
	float x0;
	float dx;
	float ddx;

	float y0;
	float dy;
	float ddy;

};

static int RunGpu(Ipp8u *inData, Ipp8u **outData, camera_pipe_params &params)
{
    // Create device
    mfxU32 version = 0;
    CmDevice *device = 0;
    Ipp32s res = ::CreateCmDevice(device, version);
    CHECK_CM_ERR(res);
    res = device->InitPrintBuffer();
    CHECK_CM_ERR(res);

	// Create queue
    CmQueue *queue = 0;
    res = device->CreateQueue(queue);
    CHECK_CM_ERR(res);

	// Create program
	CmProgram *program = 0;
	res = device->LoadProgram((void*)genx_camera_pipe_skl, sizeof(genx_camera_pipe_skl), program);
	CHECK_CM_ERR(res);

	// Create kernel
	CmKernel *kernel = 0;
	res = device->CreateKernel(program, CM_KERNEL_FUNCTION(camera_pipe), kernel);
	CHECK_CM_ERR(res);

	// Create sampler
	CM_SAMPLER_STATE sampler_state = {
		CM_TEXTURE_FILTER_TYPE_LINEAR,
		CM_TEXTURE_FILTER_TYPE_LINEAR,
		CM_TEXTURE_ADDRESS_CLAMP,
		CM_TEXTURE_ADDRESS_CLAMP,
		CM_TEXTURE_ADDRESS_CLAMP
	};

	CmSampler *sampler = 0;
	res = device->CreateSampler(sampler_state, sampler);
	CHECK_CM_ERR(res);

	Ipp32u blocks_x = (params.y_width + 15) >> 4;
    Ipp32u blocks_y = (params.y_height + 15) >> 4;
	Ipp32u a_width  = blocks_x << 4;
	Ipp32u a_heigth = blocks_y << 4;
	Ipp32u smpl_w   = blocks_x * sizeof(TAvsMsg);
	Ipp32u smpl_h   = (params.y_height + 3) >> 2;

	// Load sampler params
	Ipp32u file_size;
	TAvsMsg *sampler_params = (TAvsMsg*)Read(params.smp_parms.c_str(), &file_size);
	if (file_size != smpl_w * smpl_h)
		return FAILED;

	CmSurface2D *smpl_params = 0;
	res = device->CreateSurface2D(smpl_w, smpl_h, CM_SURFACE_FORMAT_A8, smpl_params);
	CHECK_CM_ERR(res);

	res = smpl_params->WriteSurface((const unsigned char*)sampler_params, NULL);
	CHECK_CM_ERR(res);

	// Create input surface & load data into it
    CmSurface2D *inpSurf = 0;
	res = device->CreateSurface2D(a_width, a_heigth, CM_SURFACE_FORMAT_NV12, inpSurf);
    CHECK_CM_ERR(res);

	// Load input picture into aligned buffer
	Ipp8u *algnPic = NULL;
	res = CopyToAlignedNV12(algnPic, inData, a_width, a_heigth, params.y_width, params.y_height);
	CHECK_CM_ERR(res);

	res = inpSurf->WriteSurface(algnPic, NULL);
	CHECK_CM_ERR(res);

	// Create output surface
    CmSurface2D *outSurf = 0;
    res = device->CreateSurface2D(a_width, a_heigth, CM_SURFACE_FORMAT_NV12, outSurf);
    CHECK_CM_ERR(res);

	// Set kernel args
	Ipp32u index = 0;
	SurfaceIndex *idx_smpl_params = 0;
	res = smpl_params->GetIndex(idx_smpl_params);
	CHECK_CM_ERR(res);
	res = kernel->SetKernelArg(index++, sizeof(SurfaceIndex), idx_smpl_params);
	SurfaceIndex *idx_outSurf = 0;
	res = outSurf->GetIndex(idx_outSurf);
	CHECK_CM_ERR(res);
	res = kernel->SetKernelArg(index++, sizeof(SurfaceIndex), idx_outSurf);
	CHECK_CM_ERR(res);
	SurfaceIndex *idx_inpSurf = 0;
	res = device->CreateSamplerSurface2D(inpSurf, idx_inpSurf);
	CHECK_CM_ERR(res);
	res = kernel->SetKernelArg(index++, sizeof(SurfaceIndex), idx_inpSurf);
	CHECK_CM_ERR(res);
	SamplerIndex *idx_sampler = 0;
	res = sampler->GetIndex(idx_sampler);
	CHECK_CM_ERR(res);
	res = kernel->SetKernelArg(index++, sizeof(SamplerIndex), idx_sampler);
	CHECK_CM_ERR(res);
	res = kernel->SetKernelArg(index++, sizeof(Ipp32u), &a_width);
	CHECK_CM_ERR(res);
	res = kernel->SetKernelArg(index++, sizeof(Ipp32u), &a_heigth);
	CHECK_CM_ERR(res);

    // Set kernel threads
    res = kernel->SetThreadCount(blocks_x * blocks_y);
    CHECK_CM_ERR(res);
    CmThreadSpace *threadSpace = 0;
    res = device->CreateThreadSpace(blocks_x, blocks_y, threadSpace);
    CHECK_CM_ERR(res);
    res = threadSpace->SelectThreadDependencyPattern(CM_NONE_DEPENDENCY);
    CHECK_CM_ERR(res); 

	// Create task
	CmTask *task = 0;
	res = device->CreateTask(task);
	CHECK_CM_ERR(res);
	res = task->AddKernel(kernel);
    CHECK_CM_ERR(res);

	// Enqueue kernel & get execution time
	CmEvent *event = 0;
	res = queue->Enqueue(task, event, threadSpace);
	CHECK_CM_ERR(res);

	res = event->WaitForTaskFinished();
	CHECK_CM_ERR(res);

	res = device->FlushPrintBufferIntoFile("printf.txt");
	CHECK_CM_ERR(res);

	//// Enqueue kernel & get execution time
	//Ipp64u exec_time = GetAccurateGpuTime(queue, task, threadSpace);
	//fprintf(stdout, "%s: %12lu us \n", __FUNCTION__, exec_time / 1000);

	// Allocate data for kernel output & read surface to it
	res = outSurf->ReadSurface(algnPic, NULL);
	CHECK_CM_ERR(res);

	// Write output to file
	*outData = NULL;
	res = CopyFromAlignedNV12(*outData, algnPic, params.y_width, params.y_height, a_width, a_heigth);
	CHECK_CM_ERR(res);
	free(algnPic);
	
	res = Dump(*outData, params.y_width * params.y_height * 3 / 2, params.out_fname.c_str());
	CHECK_CM_ERR(res);

	// Destroy resources
    device->DestroyThreadSpace(threadSpace);
    device->DestroyTask(task);
	device->DestroySurface(inpSurf);
    device->DestroySurface(outSurf);
	device->DestroySampler(sampler);
	device->DestroyKernel(kernel);
    device->DestroyProgram(program);
	::DestroyCmDevice(device);

	return PASSED;
}

int main(int argc, char *argv[])
{
	camera_pipe_params params;

	Ipp32s res = PrepareParams(argc, argv, params);
	CHECK_CM_ERR(res);
	
	Ipp32u inp_pic_size;
	Ipp8u *inp_picture = (Ipp8u*)Read(params.inp_fname.c_str(), &inp_pic_size);
	if (inp_pic_size != params.y_width * params.y_height * 3 / 2) {
		return FAILED;
	}

	Ipp8u *out_picture;
	res = RunGpu(inp_picture, &out_picture, params);
	CHECK_CM_ERR(res);

	free(inp_picture);
	free(out_picture);

	return 0;
}