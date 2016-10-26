//
// INTEL CORPORATION PROPRIETARY INFORMATION
//
// This software is supplied under the terms of a license agreement or
// nondisclosure agreement with Intel Corporation and may not be copied
// or disclosed except in accordance with the terms of that agreement.
//
// Copyright(C) 2009-2011 Intel Corporation. All Rights Reserved.
//

#include <mfx_scheduler_core.h>

#include <vm_interlocked.h>

void *mfxSchedulerCore::QueryInterface(const MFX_GUID &guid)
{
    // Specific interface is required
    if (MFXIScheduler_GUID == guid)
    {
        // increment reference counter
        vm_interlocked_inc32(&m_refCounter);

        return (MFXIScheduler *) this;
    }

    if (MFXIScheduler2_GUID == guid)
    {
        // increment reference counter
        vm_interlocked_inc32(&m_refCounter);

        return (MFXIScheduler2 *) this;
    }

    // it is unsupported interface
    return NULL;

} // void *mfxSchedulerCore::QueryInterface(const MFX_GUID &guid)

void mfxSchedulerCore::AddRef(void)
{
    // increment reference counter
    vm_interlocked_inc32(&m_refCounter);

} // void mfxSchedulerCore::AddRef(void)

void mfxSchedulerCore::Release(void)
{
    // decrement reference counter
    vm_interlocked_dec32(&m_refCounter);

    if (0 == m_refCounter)
    {
        delete this;
    }

} // void mfxSchedulerCore::Release(void)

mfxU32 mfxSchedulerCore::GetNumRef(void) const
{
    return m_refCounter;

} // mfxU32 mfxSchedulerCore::GetNumRef(void) const


//explicit specification of interface creation
template<> MFXIScheduler*  CreateInterfaceInstance<MFXIScheduler>(const MFX_GUID &guid)
{
    if (MFXIScheduler_GUID == guid)
        return (MFXIScheduler*) (new mfxSchedulerCore);

    return NULL;

} //template<> MFXIScheduler*  CreateInterfaceInstance<MFXIScheduler>()

template<> MFXIScheduler2*  CreateInterfaceInstance<MFXIScheduler2>(const MFX_GUID &guid)
{
    if (MFXIScheduler2_GUID == guid)
        return (MFXIScheduler2*)(new mfxSchedulerCore);

    return NULL;

} //template<> MFXIScheduler*  CreateInterfaceInstance<MFXIScheduler>()