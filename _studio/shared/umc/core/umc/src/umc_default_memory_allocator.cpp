// Copyright (c) 2006-2018 Intel Corporation
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

#include "umc_default_memory_allocator.h"

namespace UMC
{

  // structure to describe one memory block
  struct MemoryInfo
  {
    void *pMemory;          // allocated memory block
    size_t Size;            // allocated size
    MemID  MID;             // MID_INVALID if unused
    int32_t Alignment;       // requested alignment
    int32_t LocksCount;      // lock counter
    int32_t InvalidatedFlag; // set after Free()

    void Init()
    {
      pMemory = 0;
      Size = 0;
      MID = MID_INVALID;
    }
    // Released descriptor with not released memory of suitable size
    int32_t CanReuse(size_t s_Size, int32_t s_Alignment)
    {
      if(MID == MID_INVALID &&
        pMemory != 0 &&
        s_Size+(align_pointer<uint8_t*>(pMemory, s_Alignment) - (uint8_t*)pMemory) <= Size)
        return 1;
      return 0;

    }
    // assign memory to descriptor
    Status Alloc(size_t s_Size, int32_t s_Alignment)
    {
      if(!CanReuse(s_Size, s_Alignment)) { // can't reuse
        pMemory = malloc((int32_t)s_Size+s_Alignment);
        if(pMemory == 0) {
          vm_debug_trace1(VM_DEBUG_ERROR, VM_STRING("failed to allocate %d bytes"), (int32_t)Size);
          return UMC_ERR_ALLOC;
        }
        Size = s_Size+s_Alignment; // align to be done on Lock() call
      }
      Alignment = s_Alignment;
      LocksCount = 0;
      InvalidatedFlag = 0;
      return UMC_OK;
    }
    // mark as no more used checking the state
    void Clear()
    {
      if(pMemory != 0) {
        if(InvalidatedFlag == 0) {
          vm_debug_trace2(VM_DEBUG_MEMORY, VM_STRING("Mem block ID:%d size:%d wasn't released"),
            (int32_t)MID, (int32_t)Size);
          InvalidatedFlag = 1;
        }
        if(LocksCount != 0) {
          vm_debug_trace2(VM_DEBUG_MEMORY, VM_STRING("Mem block ID:%d size:%d has bad lock counter"),
            (int32_t)MID, (int32_t)Size);
          LocksCount = 0;
        }
        MID = MID_INVALID;
      }
    }
    // release memory allocated to descriptor
    void Release()
    {
      if(pMemory != 0) {
        Clear();
        free(pMemory);
        pMemory = 0;
      }

    }
  };


DefaultMemoryAllocator::DefaultMemoryAllocator(void)
{
    memInfo = 0;
    memCount = 0; // allocate only on call
    memUsed = 0;
    lastMID = MID_INVALID;
    Init(NULL);
}

DefaultMemoryAllocator::~DefaultMemoryAllocator(void)
{
    DefaultMemoryAllocator::Close();
}

Status DefaultMemoryAllocator::Init(MemoryAllocatorParams* /*pParams*/)
{
    AutomaticUMCMutex guard(m_guard);

    DefaultMemoryAllocator::Close();

    return UMC_OK;
}

Status DefaultMemoryAllocator::Close()
{
  AutomaticUMCMutex guard(m_guard);

  int32_t i;
  for(i=0; i<memUsed; i++) {
    memInfo[i].Release();
  }
  if(memInfo != 0) {
    delete[] memInfo;
    memInfo = 0;
  }
  memUsed = 0;
  memCount = 0;

  return UMC_OK;
}

Status DefaultMemoryAllocator::Alloc(MemID *pNewMemID, size_t Size, uint32_t /*Flags*/, uint32_t Align/*=16*/)
{
  int32_t i;
  MemoryInfo* pmem = 0;
  MemoryInfo* pmemtofree = 0;

  AutomaticUMCMutex guard(m_guard);

  if (pNewMemID == NULL)
    return UMC_ERR_NULL_PTR;

  if (Size == 0 || Align == 0)
    return UMC_ERR_INVALID_PARAMS;

  *pNewMemID = MID_INVALID;


  for (i = 1; i <= (1 << 20); i <<= 1) {
    if (i & Align) {
      break; // stop at nonzero bit
    }
  }

  if (i != (int32_t)Align) // no 1 in 20 ls bits or more than 1 nonzero bit
    return UMC_ERR_INVALID_PARAMS;

  for (i=0; i<memUsed; i++) { // search unused or free
    if (memInfo[i].pMemory == 0 || memInfo[i].CanReuse(Size, Align)) {
      pmem = &memInfo[i];
      break;
    } else if(memInfo[i].MID == MID_INVALID)
      pmemtofree = &memInfo[i];
  }
  if (pmem == 0 && memUsed < memCount) { // take from never used
    pmem = &memInfo[memUsed];
    memUsed ++;
  }
  if(pmem == 0 && pmemtofree != 0) { // release last unsuitable
    pmemtofree->Release();
    pmem = pmemtofree;
  }
  if (pmem == 0) { // relocate all descriptors
    int32_t newcount = MFX_MAX(8, memCount*2);
    MemoryInfo* newmem = new MemoryInfo[newcount];
    if (newmem == 0) {
      vm_debug_trace1(VM_DEBUG_ERROR, VM_STRING("failed to allocate %d bytes"), newcount*sizeof(MemoryInfo));
      return UMC_ERR_ALLOC;
    }

    for (i=0; i<memCount; i++) // copy existing
      newmem[i] = memInfo[i];

    // free old descriptors
    if (memInfo)
        delete[] memInfo;

    memInfo = newmem;
    memCount = newcount;
    for (; i<memCount; i++)
      memInfo[i].Init(); // init new

    pmem = &memInfo[memUsed]; // take first in new
    memUsed ++;
  }

  if(UMC_OK != pmem->Alloc(Size, Align))
    return UMC_ERR_ALLOC;
  lastMID++;
  pmem->MID = lastMID;
  *pNewMemID = lastMID;

  return UMC_OK;
}

void* DefaultMemoryAllocator::Lock(MemID MID)
{
  int32_t i;

  if( MID == MID_INVALID )
    return NULL;

  AutomaticUMCMutex guard(m_guard);

  for (i=0; i<memUsed; i++) {
    if (memInfo[i].MID == MID) {
      if(memInfo[i].pMemory == 0 || memInfo[i].InvalidatedFlag)
        return NULL; // no memory or invalidated
      memInfo[i].LocksCount ++;
      // return with aligning
      return align_pointer<uint8_t*>(memInfo[i].pMemory, memInfo[i].Alignment);
    }
  }

  return NULL;
}

Status DefaultMemoryAllocator::Unlock(MemID MID)
{
  int32_t i;

  if( MID == MID_INVALID )
    return UMC_ERR_FAILED;

  AutomaticUMCMutex guard(m_guard);

  for (i=0; i<memUsed; i++) {
    if (memInfo[i].MID == MID) {
      if(memInfo[i].pMemory == 0 || memInfo[i].LocksCount <= 0)
        return UMC_ERR_FAILED; // no mem or lock /unlock mismatch
      memInfo[i].LocksCount --;
      if(memInfo[i].LocksCount == 0 && memInfo[i].InvalidatedFlag)
        memInfo[i].Clear(); //  no more use
      return UMC_OK;
    }
  }

  return UMC_ERR_FAILED;
}

Status DefaultMemoryAllocator::Free(MemID MID)
{
  int32_t i;

  if( MID == MID_INVALID )
    return UMC_ERR_FAILED;

  AutomaticUMCMutex guard(m_guard);

  for (i=0; i<memUsed; i++) {
    if (memInfo[i].MID == MID) {
      if(memInfo[i].pMemory == 0 || memInfo[i].InvalidatedFlag != 0)
        return UMC_ERR_FAILED; // no mem or re-free
      memInfo[i].InvalidatedFlag = 1;
      if(memInfo[i].LocksCount == 0)
        memInfo[i].Clear(); // not in use
      return UMC_OK;
    }
  }

  return UMC_ERR_FAILED;
}

Status DefaultMemoryAllocator::DeallocateMem(MemID MID)
{
  int32_t i;

  if( MID == MID_INVALID )
    return UMC_ERR_FAILED;

  AutomaticUMCMutex guard(m_guard);

  for (i=0; i<memUsed; i++) {
    if (memInfo[i].MID == MID) {
      if(memInfo[i].pMemory == 0)
        return UMC_ERR_FAILED; // no memory
      memInfo[i].InvalidatedFlag = 1;
      memInfo[i].Clear();
      return UMC_OK;
    }
  }

  return UMC_OK;
}

} // namespace UMC
