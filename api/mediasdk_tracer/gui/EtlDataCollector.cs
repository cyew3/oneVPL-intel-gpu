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
using System.Linq;
using System.Text;
using System.Diagnostics;
using System.IO;

namespace msdk_analyzer
{
    class CollectorStat 
    {
        UInt64 m_bytesWritten;
        public UInt64 KBytesWritten
        {
            get { return m_bytesWritten; }
            set { m_bytesWritten = value; }
        }
    }

    interface IDataCollector
    {
        void SetLevel(int level);
        void Start();
        void Stop();
        void Create();
        void Delete();
        void Query(CollectorStat stat);
        bool isRunning{ get; }
        string TargetFile { get; }
    }

    class DataCollector
        : IDisposable
        , IDataCollector
    {
        //TODO: questionalble variable might  be used to reduce collector update calls
        private string m_session_params;
        private bool m_bCollecting;
        private int m_level;
        //temporary filename
        private const string etl_file_name = "provider.etl";
        private const string etl_file_name_actual = "provider_000001.etl";

        public DataCollector()
        {
            //stop because if existed collector is running we cant delete it
            Stop();
        }

        public void Dispose()
        {
            Stop();
            Delete();
        }
        private string GetSdkAnalyzerDataFolder()
        {
            return Environment.GetFolderPath(Environment.SpecialFolder.ApplicationData)
                + "\\" + Properties.Resources.app_data_folder;
        }

        public string TargetFile
        {
            get
            {
                return "\""+GetSdkAnalyzerDataFolder() + etl_file_name_actual+"\"";
            }
        }
        public void Create()
        {
            //session might exist from abnormal termination
            Delete();
            
            //create target folder if not exist
            string path = Path.GetDirectoryName(GetSdkAnalyzerDataFolder());
            Directory.CreateDirectory(path);

            m_session_params = MakeParams();
            InvokeLogMan("create " + "trace " + m_session_params);
        }

        public void SetLevel(int level)
        {
            m_level = level;
        }

        public void Start()
        {
            //have to call to create to not spawn number of etl files
            Create();

            InvokeLogMan("start " + msdk_analyzer.Properties.Resources.msdk_analyzer_session_name);
            m_bCollecting = true;
        }

        public void Stop()
        {
            InvokeLogMan("stop " + msdk_analyzer.Properties.Resources.msdk_analyzer_session_name);
            m_bCollecting = false;
        }

        public void Delete()
        {
            Stop();
            InvokeLogMan("delete " + msdk_analyzer.Properties.Resources.msdk_analyzer_session_name);
        }

        public void Query(CollectorStat statistics)
        {
            EVENT_TRACE_PROPERTIES prop = new EVENT_TRACE_PROPERTIES();
            UInt64 ret;
            prop.Wnode.BufferSize = 2048;
            
            ret = AdvApi.QueryTrace(0, Properties.Resources.msdk_analyzer_session_name, prop);
            
            statistics.KBytesWritten = prop.BufferSize * prop.BuffersWritten;
        }

        public bool isRunning
        {
            get
            {
                return m_bCollecting;
            }
        }

        private void InvokeLogMan(string arguments)
        {
            Process logman = new Process();
            
            logman.StartInfo.WindowStyle = ProcessWindowStyle.Hidden;
            logman.StartInfo.Arguments = arguments;
            logman.StartInfo.FileName = "logman.exe";
            //DEBUG
            //logman.StartInfo.UseShellExecute = false;
            //logman.StartInfo.RedirectStandardOutput = true;
            
            logman.Start();
            logman.WaitForExit();

            //DEBUG
            //TODO: do we need to analyze output for error reporting?
            //string log = logman.StandardOutput.ReadToEnd();
            //Debug.Write(log);

            logman.Close();
        }

        //create string to initialize or modify session
        private string MakeParams()
        {
            //create session
            return  msdk_analyzer.Properties.Resources.msdk_analyzer_session_name +
                    " -o \"" + GetSdkAnalyzerDataFolder() + etl_file_name +"\""+
                    " -ow" + //owerwrite existed
                    " -p " + msdk_analyzer.Properties.Resources.msdk_analyzer_guid + " 0x0 0x" + m_level.ToString() +
                    " -bs 128 -nb 64 128";//buffering settings
        }
    }
}
