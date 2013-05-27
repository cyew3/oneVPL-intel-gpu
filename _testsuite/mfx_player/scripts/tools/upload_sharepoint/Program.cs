using System;
using System.Collections.Generic;
using System.Linq;
using System.Text;
using System.IO;
using System.Web;
using System.Xml;


namespace upload_sharepoint
{
    class Program
    {
        static void Main(string[] args)
        {
            if (args.Length<2)
            {
                Console.Write("2 Parameters required. 1 - where to upload, 2 - file to upload\n"+
                    "Intel old Shrepoint URL: http://moss.amr.ith.intel.com \n" +
                    "Intel new Sharepoint - 2010 url: https://mysharepoint.gar.ith.intel.com");
                return;
            }

            CreateNewDocumentWithCopyService(args[1], args[0]);
        }
        
        public static void CreateNewDocumentWithCopyService(string fileName, string where) 
        {
            copyservice.Copy c = new copyservice.Copy();

            UriBuilder bld = new UriBuilder(where);
            
            c.Url = bld.Scheme + "://" + bld.Host + "/_vti_bin/copy.asmx"; 
            c.UseDefaultCredentials = true;
            

            byte[] myBinary = File.ReadAllBytes(fileName); 
            string destination = where + "/" + Path.GetFileName(fileName); 
            string[] destinationUrl = { destination };

            copyservice.FieldInformation info1 = new copyservice.FieldInformation(); 
            info1.DisplayName = "Title"; 
            info1.InternalName = "Title"; 
            info1.Type = copyservice.FieldType.Text; 
            info1.Value = "new title";

            copyservice.FieldInformation info2 = new copyservice.FieldInformation(); 
            info2.DisplayName = "Modified By"; 
            info2.InternalName = "Editor"; 
            info2.Type = copyservice.FieldType.User; 
            info2.Value = "-1;#servername\\testmoss";

            copyservice.FieldInformation[] info = { info1, info2 }; 
            copyservice.CopyResult resultTest = new copyservice.CopyResult(); 
            copyservice.CopyResult[] result = { resultTest };

            try 
            { 
                //When creating new content use the same URL in the SourceURI as in the Destination URL argument
                c.CopyIntoItems(destination, destinationUrl, info, myBinary, out result); 
            } 
            catch (Exception ex) 
            {
                Console.Write(ex.ToString());
            }finally
            {
                Console.WriteLine("Error Code:" + result[0].ErrorCode.ToString());
                Console.WriteLine("Error Message:" + result[0].ErrorMessage);
            }
        }
    }
}
