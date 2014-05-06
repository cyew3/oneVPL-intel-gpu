/*
#***************************************************************************
# INTEL CONFIDENTIAL
# Copyright (C) 2009-2010 Intel Corporation All Rights Reserved. 
# 
# The source code contained or described herein and all documents 
# related to the source code ("Material") are owned by Intel Corporation 
# or its suppliers or licensors. Title to the Material remains with 
# Intel Corporation or its suppliers and licensors. The Material contains 
# trade secrets and proprietary and confidential information of Intel or 
# its suppliers and licensors. The Material is protected by worldwide 
# copyright and trade secret laws and treaty provisions. No part of the 
# Material may be used, copied, reproduced, modified, published, uploaded, 
# posted, transmitted, distributed, or disclosed in any way without 
# Intel's prior express written permission.
# 
# No license under any patent, copyright, trade secret or other intellectual 
# property right is granted to or conferred upon you by disclosure or 
# delivery of the Materials, either expressly, by implication, inducement, 
# estoppel or otherwise. Any license under such intellectual property rights 
# must be express and approved by Intel in writing.
#***************************************************************************
*/

/** 
 * @file
 *
 * @brief This file provides a simple capability for displaying error
 * code values
 */


#include "epid_errors.h"

const char * g_ErrorStrings[] = {
    "EPID_FAILURE",
    "EPID_NOT_IMPLEMENTED",
    "EPID_INVALID_SIGNATURE",
    "EPID_MEMBER_KEY_REVOKED",
    "EPID_INSUFFICIENT_MEMORY_ALLOCATED",
    "EPID_BAD_ARGS",
    "EPID_FILE_ERROR",
    "EPID_NULL_PTR",
    "EPID_OUTOFMEMORY",
    "EPID_OUTOFPRNGDATA",
    "EPID_UNLUCKY",
    "EPID_INVALID_MEMBER_KEY",
    "EPID_INTERNAL_MAX_NEGATIVE"
};
