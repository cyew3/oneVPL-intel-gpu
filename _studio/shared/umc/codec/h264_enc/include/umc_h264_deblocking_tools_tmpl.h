// Copyright (c) 2003-2019 Intel Corporation
//
// Permission is hereby granted, free of charge, to any person obtaining a copy
// of this software and associated documentation files (the "Software"), to deal
// in the Software without restriction, including without limitation the rights
// to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
// copies of the Software, and to permit persons to whom the Software is
// furnished to do so, subject to the following conditions:
//
// The above copyright notice and this permission notice shall be included in all
// copies or substantial portions of the Software.
//
// THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
// IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
// FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
// AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
// LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
// OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
// SOFTWARE.

#if PIXBITS == 8

#define PIXTYPE Ipp8u
#define COEFFSTYPE Ipp16s
#define H264ENC_MAKE_NAME(NAME) NAME##_8u16s

#elif PIXBITS == 16

#define PIXTYPE Ipp16u
#define COEFFSTYPE Ipp32s
#define H264ENC_MAKE_NAME(NAME) NAME##_16u32s

#else

void H264EncoderFakeFunction() { UNSUPPORTED_PIXBITS; }

#endif //PIXBITS

//public:

// Default constructor
Status H264ENC_MAKE_NAME(H264EncoderThreadedDeblockingTools_Create)(
    void* state);

// Destructor
void H264ENC_MAKE_NAME(H264EncoderThreadedDeblockingTools_Destroy)(
    void* state);

// Initialize object
Status H264ENC_MAKE_NAME(H264EncoderThreadedDeblockingTools_Initialize)(
    void* state,
    H264ENC_MAKE_NAME(H264CoreEncoder) *pEncoder);

// Deblock slice by two thread
void H264ENC_MAKE_NAME(H264EncoderThreadedDeblockingTools_DeblockSliceTwoThreaded)(
    Ipp32u uFirstMB,
    Ipp32u unumMBs,
    H264CoreEncoder_DeblockingFunction pDeblocking);

// Deblock slice asynchronous
void H264ENC_MAKE_NAME(H264EncoderThreadedDeblockingTools_WaitEndOfSlice)(
    void* state);

void H264ENC_MAKE_NAME(H264EncoderThreadedDeblockingTools_DeblockSliceAsync)(
    void* state,
    Ipp32u uFirstMB,
    Ipp32u unumMBs,
    H264CoreEncoder_DeblockingFunction pDeblocking);

// Release object
void H264ENC_MAKE_NAME(H264EncoderThreadedDeblockingTools_Release)(
    void* state);

// Additional deblocking thread
Ipp32u VM_CALLCONVENTION
H264ENC_MAKE_NAME(H264EncoderThreadedDeblockingTools_DeblockSliceSecondThread)(
    void *p);

Ipp32u VM_CALLCONVENTION
H264ENC_MAKE_NAME(H264EncoderThreadedDeblockingTools_DeblockSliceAsyncSecondThread)(
    void *p);


typedef struct H264ENC_MAKE_NAME(sH264EncoderThreadedDeblockingTools)
{
//public:
    Ipp32u m_nMBAFF;                                            // (Ipp32u) MBAFF flag

    H264ENC_MAKE_NAME(H264CoreEncoder)* m_pDecoder;             // (H264CoreEncoder *) pointer to decoder-owner

    std::thread m_hDeblockingSliceSecondThread;                 // (std::thread) handle to deblocking slice second thread
    std::thread m_hDeblockingSliceAsyncSecondThread;            // (std::thread) handle to deblocking slice second thread

    vm_event m_hBeginFrame;                                     // (vm_event) event to begin of deblocking frame
    vm_event m_hBeginSlice;                                     // (vm_event) event to begin of deblocking slice
    vm_event m_hBeginRow;                                       // (vm_event) event to begin of deblocking row
    vm_event m_hDoneBorder;                                     // (vm_event) event of border macroblock is complete
    vm_event m_hDoneRow;                                        // (vm_event) event of row is complete
    vm_event m_hDoneSlice;                                      // (vm_event) event of slice is complete

    bool m_bQuit;                                               // (bool) quit flag
    Ipp32u m_nFirstMB;                                          // (Ipp32u) first macroblock to deblock
    Ipp32u m_nNumMB;                                            // (Ipp32u) number of macroblock to deblock
    H264CoreEncoder_DeblockingFunction m_pDeblocking;           // (H264CoreEncoder::DeblockingFunction) pointer to current deblocking function
} H264ENC_MAKE_NAME(H264EncoderThreadedDeblockingTools);

#undef H264ENC_MAKE_NAME
#undef COEFFSTYPE
#undef PIXTYPE
