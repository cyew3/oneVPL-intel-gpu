using System;
using System.Collections.Generic;
using System.Linq;
using System.Text;
using System.IO;

namespace msdk_analyzer
{
    class ConfigManager
    {
        public ConfigManager() { }

        public bool CreateConfig (string config_path, Dictionary<string, Dictionary<string, string>> config)
        {
            FileInfo _file = new FileInfo(config_path);
            
            StreamWriter writer = new StreamWriter(config_path);
            if(_file.Exists){
                foreach (KeyValuePair<string, Dictionary <string, string>> pair in config)
                {
                    writer.WriteLine("[" + pair.Key + "]");

                     Dictionary <string, string> section_params = pair.Value;
                     foreach (KeyValuePair<string, string> ppair in section_params) 
                     {
                        writer.WriteLine("  " + ppair.Key + " = " + ppair.Value);
                     }

                     writer.Write("\n");
                }

                writer.Close();
                writer.Dispose();
                return true;
            }
            return false;
        }
    }
}
