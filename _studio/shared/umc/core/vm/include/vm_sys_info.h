//
// INTEL CORPORATION PROPRIETARY INFORMATION
//
// This software is supplied under the terms of a license agreement or
// nondisclosure agreement with Intel Corporation and may not be copied
// or disclosed except in accordance with the terms of that agreement.
//
// Copyright(C) 2003-2016 Intel Corporation. All Rights Reserved.
//

#ifndef __VM_SYS_INFO_H__
#define __VM_SYS_INFO_H__


#include "vm_types.h"
#include "vm_strings.h"

#ifdef __cplusplus
extern "C"
{
#endif /* __cplusplus */

/* Functions to obtain processor's specific information */
Ipp32u vm_sys_info_get_cpu_num(void);
Ipp32u vm_sys_info_get_numa_nodes_num(void);
Ipp64u vm_sys_info_get_cpu_mask_of_numa_node(Ipp32u node);
void vm_sys_info_get_cpu_name(vm_char *cpu_name);
void vm_sys_info_get_vga_card(vm_char *vga_card);
void vm_sys_info_get_os_name(vm_char *os_name);
void vm_sys_info_get_computer_name(vm_char *computer_name);
void vm_sys_info_get_program_name(vm_char *program_name);
void vm_sys_info_get_program_path(vm_char *program_path);
Ipp32u vm_sys_info_get_cpu_speed(void);
Ipp32u vm_sys_info_get_mem_size(void);

#ifdef __cplusplus
}
#endif /* __cplusplus */

#endif /* __VM_SYS_INFO_H__ */
