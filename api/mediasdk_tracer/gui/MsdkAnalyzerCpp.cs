/* ****************************************************************************** *\

INTEL CORPORATION PROPRIETARY INFORMATION
This software is supplied under the terms of a license agreement or nondisclosure
agreement with Intel Corporation and may not be copied or disclosed except in
accordance with the terms of that agreement
Copyright(c) 2012 Intel Corporation. All Rights Reserved.

File Name: 

\* ****************************************************************************** */

using System;
using System.Collections.Generic;
using System.Text;
using System.Runtime.InteropServices;
using System.Diagnostics.Eventing;

namespace msdk_analyzer
{
    class MsdkAnalyzerCpp
    {
        //const string msdk_analyzer_path = "C:\\Program Files (x86)\\Intel\\Mediasdk Analyzer\\msdk_analyzer_mbcs.dll";
#if DEBUG
        const string msdk_analyzer_path = "tracer_core32_d.dll";
#else
        const string msdk_analyzer_path = "tracer_core32.dll";
#endif

        [DllImport(msdk_analyzer_path, CharSet = CharSet.Auto, CallingConvention = CallingConvention.StdCall)]
        public static extern void convert_etl_to_text
            ([MarshalAs(UnmanagedType.U4)] int hwnd
           , [MarshalAs(UnmanagedType.U4)] int hinst
           , [MarshalAs(UnmanagedType.LPStr)]string lpszCmdLine
           , int nCmdShow);

        [DllImport(msdk_analyzer_path, CharSet = CharSet.Auto, CallingConvention = CallingConvention.StdCall)]
        public static extern UInt32 install([MarshalAs(UnmanagedType.LPStr)] string folder_path,
                                            [MarshalAs(UnmanagedType.LPStr)] string app_data);//default place for log file

        [DllImport(msdk_analyzer_path, CharSet = CharSet.Auto, CallingConvention = CallingConvention.StdCall)]
        public static extern UInt32 uninstall();

    }


}
