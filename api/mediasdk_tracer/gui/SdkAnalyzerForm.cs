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
using System.ComponentModel;
using System.Data;
using System.Drawing;
using System.Linq;
using System.Text;
using System.Windows.Forms;
using System.Diagnostics;
using System.Threading;
using System.IO;
using Microsoft.Win32;
using System.Reflection;

namespace msdk_analyzer
{
    public partial class SdkAnalyzerForm : Form
    {
        IDataCollector m_collector;
        EtlToTextConverter converter = new EtlToTextConverter();

        private uint progressPercent;
        System.Windows.Forms.Timer progress_update_timer;
        EtlToTextConverter m_converter;
        System.Windows.Forms.Timer t1;

        public SdkAnalyzerForm()
        {
            InitializeComponent();
            m_collector = new DataCollector();
            m_converter = new EtlToTextConverter();
            //m_collector.Create();
            converter.ConversionUpdated +=new ConversionStatusUpdated(converter_ConversionUpdated);
            progress_update_timer = new System.Windows.Forms.Timer();
            progress_update_timer.Interval = 100;
            progress_update_timer.Tick += new EventHandler(timer1_Tick);
            tbxLogOutput.Text = Registry.GetValue(Properties.Resources.target_folder_key
                , Properties.Resources.target_folder_value, "") as string;

            t1 = new System.Windows.Forms.Timer();
            t1.Interval = 500;
            t1.Tick += new EventHandler(OnUpdateEtlFileSize);
            //AssemblyName name = Assembly.GetExecutingAssembly().GetName();
            //Text = Text + " v" + name.Version.Major + "." + name.Version.Minor;
        }

        void OnUpdateEtlFileSize(object sender, EventArgs e)
        {
            CollectorStat stat = new CollectorStat();
            m_collector.Query(stat);
            lblBytesWritten.Text = stat.KBytesWritten.ToString();
        }

        void  converter_ConversionUpdated(ConversionStatus sts, int percentage)
        {
             throw new NotImplementedException();
        }

        private void button_Start_Click(object sender, EventArgs e)
        {
            //button disabled since running a collector might take significant time
            
            if (m_collector.isRunning)
            {
                
                OnStop();
            }
            else
            {
                if (checkBox_Append.Checked != true)
                {
                    try
                    {
                        File.Delete(tbxLogOutput.Text);
                    }
                    catch (System.Exception ex)
                    {
                        MessageBox.Show(ex.ToString());
                    }
                }
                OnStart();
            }
        }

        private void OnStart()
        {
            button_Start.Enabled = false;
            checkBox_PerFrame.Enabled = false;
            checkBox_Append.Enabled = false;

            try
            {
                int nLevel =
                    checkBox_PerFrame.Checked ? 2 : 1;

                m_collector.SetLevel(nLevel);
                this.Cursor = Cursors.WaitCursor;
                m_collector.Start();
                this.Cursor = Cursors.Default;
                button_Start.Text = "Stop";
                t1.Enabled = true;
            }
            catch (System.Exception ex)
            {
                MessageBox.Show(ex.ToString());
                checkBox_PerFrame.Enabled = true;
                checkBox_Append.Enabled = true;
            }
            button_Start.Enabled = true;
        }

        private void OnStop()
        {
            t1.Enabled = false;
            button_Start.Enabled = false;

            try
            {
                this.Cursor = Cursors.WaitCursor;
                m_collector.Stop();
                RunConversionDialog();
                
            }
            catch (System.Exception ex)
            {
                MessageBox.Show(ex.ToString());
                button_Start.Enabled = true;
                button_Start.Text = "Start";
            }
            checkBox_PerFrame.Enabled = true;
            checkBox_Append.Enabled = true;
        }

        private void button_Open_Click(object sender, EventArgs e)
        {
            try
            {
                Process.Start(tbxLogOutput.Text);
            }
            catch (System.Exception ex)
            {
                MessageBox.Show(ex.ToString());
            }
        }

        private void Form4_FormClosing(object sender, FormClosingEventArgs e)
        {
            m_collector.Delete();
        }

        protected void RunConversionDialog()
        {
            progressPercent = 0;
            m_converter.StartConvert(m_collector.TargetFile, "\"" + tbxLogOutput.Text + "\"");
            progress_update_timer.Start();
        }

        private void timer1_Tick(object sender, EventArgs e)
        {
            //TODO: correct progress updating
            //progressPercent++;
            ConversionStatus sts = ConversionStatus.CONVERSION_ABORTED;
            m_converter.GetConversionStatus(ref sts, ref progressPercent);

            //if (pd.HasUserCancelled)
            //{
            //    OnConvertionFinished();
            //}
            //else
            {
                // Update the progress value
                if (progressPercent >= 100)
                {
                    OnConvertionFinished();
                }
            }
        }

        private void OnConvertionFinished()
        {
            progress_update_timer.Stop();
            //pd.CloseDialog();
            Select();
            button_Start.Enabled = true;
            button_Start.Text = "Start";
            this.Cursor = Cursors.Default;
        }

        private void btnDeleteLog_Click(object sender, EventArgs e)
        {
            try
            {
                File.Delete(tbxLogOutput.Text);
            }
            catch (System.Exception ex)
            {
                MessageBox.Show(ex.ToString());
            }
            
        }

        private void tbxLogOutput_Leave(object sender, EventArgs e)
        {
            Registry.SetValue(Properties.Resources.target_folder_key
                , Properties.Resources.target_folder_value, tbxLogOutput.Text);
        }

        private void tbxLogOutput_KeyPress(object sender, KeyPressEventArgs e)
        {
            Registry.SetValue(Properties.Resources.target_folder_key
                , Properties.Resources.target_folder_value, tbxLogOutput.Text);
        }

        private void checkBox_PerFrame_CheckedChanged(object sender, EventArgs e)
        {
            RegistryKey key;

            key = Registry.CurrentUser.CreateSubKey("Software\\Intel\\MediaSDK\\Debug\\Analyzer");
            key.SetValue("level", checkBox_PerFrame.Checked ? 2 : 1);
        }
    }
}
