/*
//
//                  INTEL CORPORATION PROPRIETARY INFORMATION
//     This software is supplied under the terms of a license agreement or
//     nondisclosure agreement with Intel Corporation and may not be copied
//     or disclosed except in accordance with the terms of that agreement.
//       Copyright(c) 2003-2009 Intel Corporation. All Rights Reserved.
//
*/

#include "umc_mutex.h"

namespace UMC
{

Mutex::Mutex(void)
{
    vm_mutex_set_invalid(&m_handle);

    vm_mutex_init(&m_handle);

} // Mutex::Mutex(void)

Mutex::~Mutex(void)
{
    vm_mutex_destroy(&m_handle);

} // Mutex::~Mutex(void)

} // namespace UMC
