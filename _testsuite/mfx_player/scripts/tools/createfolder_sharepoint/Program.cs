using System;
using System.Collections.Generic;
using System.Linq;
using System.Text;
using createfolder_sharepoint.dwsservice;
using System.Net;

namespace createfolder_sharepoint
{
    enum DirectoryCreateStatus
    {
        Ok = 0,
        AlreadyExists = 13,
        FolderNoFound = 10
    }
    class Program
    {
        static void Main(string[] args)
        {
            if (args.Length<2)
            {
                Console.Write("Sharepoint folder creator v0.1\n" +
                    "Usage: createfolder_sharepoint.exe <url> <folder>\n" +
                    "<url> : path to target site (ex: https://sharepoint.amr.ith.intel.com/sites/SSG-0249/MSDK/MFXTranscoder\n" +
                    "<folder> : relative folder within site (ex: Document/New%20Folder");
                return;
            }

            CreatePath(args[0], args[1]);
        }

        private static bool CreatePath(string site_url, string folder)
        {
            Dws dwsService = new Dws();
            dwsService.Credentials = CredentialCache.DefaultNetworkCredentials;

            dwsService.Url = site_url + "/_vti_bin/dws.asmx";

            try
            {
                string strResult = dwsService.CreateFolder(folder);


                if (IsDwsErrorResult(strResult))
                {
                    int intErrorID = 0;
                    string strErrorMsg = "";
                    ParseDwsErrorResult(strResult, out intErrorID, out strErrorMsg);
                    Console.WriteLine(
                        "A document workspace error occurred.\r\n" +
                        "Error number: " + intErrorID.ToString() + "\r\n" +
                        "Error description: " + strErrorMsg);

                    if (intErrorID == (int)DirectoryCreateStatus.AlreadyExists)
                    {
                        return true;
                    }

                    return false;
                }
                else
                {
                    Console.WriteLine("The folder :" + folder + " created");
                }
            }
            catch (System.Exception ex)
            {
                Console.WriteLine(ex.ToString());
                return false;
            }

            return true;
        }
        private static bool IsDwsErrorResult(string ResultFragment)
        {
            System.IO.StringReader srResult =
                new System.IO.StringReader(ResultFragment);
            System.Xml.XmlTextReader xtr =
                new System.Xml.XmlTextReader(srResult);
            xtr.Read();
            if (xtr.Name == "Error")
            {
                return true;
            }
            else
            {
                return false;
            }
        }
        private static void ParseDwsErrorResult(string ErrorFragment, out int ErrorID,
    out string ErrorMsg)
        {
            System.IO.StringReader srError =
                new System.IO.StringReader(ErrorFragment);
            System.Xml.XmlTextReader xtr =
                new System.Xml.XmlTextReader(srError);
            xtr.Read();
            xtr.MoveToAttribute("ID");
            xtr.ReadAttributeValue();
            ErrorID = System.Convert.ToInt32(xtr.Value);
            ErrorMsg = xtr.ReadString();
        }

        private static string ParseDwsSingleResult(string ResultFragment)
        {
            System.IO.StringReader srResult =
                new System.IO.StringReader(ResultFragment);
            System.Xml.XmlTextReader xtr =
                new System.Xml.XmlTextReader(srResult);
            xtr.Read();
            return xtr.ReadString();
        }
        
    }
}
