using System;
using System.Collections.Generic;
using System.ComponentModel;
using System.Data;
using System.Drawing;
using System.Text;
using System.Windows.Forms;

namespace SdkAnalyzerDebug
{
    public partial class Form1 : msdk_analyzer.SdkAnalyzerForm
    {
        public Form1()
        {
            InitializeComponent();
        }

        private void btnConvert_Click(object sender, EventArgs e)
        {
            RunConversionDialog();
        }
    }
}