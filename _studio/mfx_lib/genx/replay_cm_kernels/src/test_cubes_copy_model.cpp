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
#include "../include/genx_cubes_copy_model_skl_isa.h"
#include "../../WarpMDF_engine/pvt_mdf_3cubes.hpp"
#include "../../WarpMDF_engine/pvt_mdf_controller.hpp"
#include "../../WarpMDF_engine/pvt_mdf_kernel_config.hpp"

struct cubes_copy_model_params
{
	Ipp32u image_input_width;
	Ipp32u image_input_height;
	Ipp32u image_output_width;
	Ipp32u image_output_height;

	std::string inp_fname;
	std::string out_fname;
};

static int PrepareParams(int argc, char *argv[], cubes_copy_model_params &params)
{
	if(argc < 7) {
		fprintf(stderr, "Insufficient input params.\n");
		return FAILED;
	}

	params.inp_fname = std::string(argv[1]);
	params.out_fname = std::string(argv[2]);

	std::stringstream ss_image_input_width(argv[3]);
	std::stringstream ss_image_input_height(argv[4]);
	std::stringstream ss_image_output_width(argv[5]);
	std::stringstream ss_image_output_height(argv[6]);

	ss_image_input_width	>> params.image_input_width;
	ss_image_input_height	>> params.image_input_height;
	ss_image_output_width	>> params.image_output_width;
	ss_image_output_height	>> params.image_output_height;

	return PASSED;
}

static int RunGpu(Ipp8u *inData, Ipp8u *outData, cubes_copy_model_params &params)
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
	res = device->LoadProgram((void*)genx_cubes_copy_model_skl, sizeof(genx_cubes_copy_model_skl), program);
	CHECK_CM_ERR(res);

	// Create kernel
	CmKernel *kernel = 0;
	res = device->CreateKernel(program, CUBES_COPY_MODEL, kernel);
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

	// Create input surface & load data into it
    CmSurface2D *inpSurf = 0;
    res = device->CreateSurface2D(params.image_input_width, params.image_input_height, CM_SURFACE_FORMAT_NV12, inpSurf);
    CHECK_CM_ERR(res);
	res = inpSurf->WriteSurface(inData, NULL);
	CHECK_CM_ERR(res);

	// Create output surface
    CmSurface2D *outSurf = 0;
    res = device->CreateSurface2D(params.image_output_width, params.image_output_height, CM_SURFACE_FORMAT_NV12, outSurf);
    CHECK_CM_ERR(res);

	// Set kernel args
	SurfaceIndex *idx_outSurf = 0;
	res = outSurf->GetIndex(idx_outSurf);
	CHECK_CM_ERR(res);
	res = kernel->SetKernelArg(0, sizeof(SurfaceIndex), idx_outSurf);
	CHECK_CM_ERR(res);
	SurfaceIndex *idx_inpSurf = 0;
	res = device->CreateSamplerSurface2D(inpSurf, idx_inpSurf);
	CHECK_CM_ERR(res);
	res = kernel->SetKernelArg(1, sizeof(SurfaceIndex), idx_inpSurf);
	CHECK_CM_ERR(res);
	SamplerIndex *idx_sampler = 0;
	res = sampler->GetIndex(idx_sampler);
	CHECK_CM_ERR(res);
	res = kernel->SetKernelArg(2, sizeof(SamplerIndex), idx_sampler);
	CHECK_CM_ERR(res);
	res = kernel->SetKernelArg(3, sizeof(Ipp32u), &params.image_input_width);
	CHECK_CM_ERR(res);
	res = kernel->SetKernelArg(4, sizeof(Ipp32u), &params.image_input_height);
	CHECK_CM_ERR(res);
	res = kernel->SetKernelArg(5, sizeof(Ipp32u), &params.image_output_width);
	CHECK_CM_ERR(res);
	res = kernel->SetKernelArg(6, sizeof(Ipp32u), &params.image_output_height);

    // Set kernel threads
	Ipp32u blocks_x = (params.image_output_width + 15) >> 4;
    Ipp32u blocks_y = (params.image_output_height + 15) >> 4;
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
	Ipp64u exec_time = GetAccurateGpuTime(queue, task, threadSpace);
	fprintf(stdout, "%s: %12lu us \n", __FUNCTION__, exec_time / 1000);

	// Allocate data for kernel output & read surface to it
	res = outSurf->ReadSurface(outData, NULL);
	CHECK_CM_ERR(res);

	// Write output to file
	res = Dump(outData, params.image_output_width * params.image_output_height * 3 / 2, params.out_fname.c_str());
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
	cubes_copy_model_params params;

	Ipp32s res = PrepareParams(argc, argv, params);
	CHECK_CM_ERR(res);
	
	Ipp32u inp_pic_size;
	Ipp8u *inp_picture = (Ipp8u*)Read(params.inp_fname.c_str(), &inp_pic_size);
	if (inp_pic_size != params.image_input_width * params.image_input_height * 3 / 2) {
		return FAILED;
	}

	Ipp8u *out_picture = (Ipp8u*)malloc(params.image_output_width * params.image_output_height * 3 / 2);
	if (!out_picture) {
		return FAILED;
	}

	res = RunGpu(inp_picture, out_picture, params);
	CHECK_CM_ERR(res);

	free(inp_picture);
	free(out_picture);

	return 0;
}