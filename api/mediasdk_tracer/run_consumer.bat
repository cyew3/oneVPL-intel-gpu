del provider_000001.etl
logman delete MySession1
logman create trace MySession1 -o .\provider.etl -pf .\msdk_analyzer.guid -bs 128 -nb 64 128
  
logman start MySession1

pause

logman stop MySession1

C:\Windows\SysWOW64\rundll32.exe "C:\Program Files (x86)\Intel\Mediasdk Analyzer\msdk_analyzer_mbcs.dll",convert_etl_to_text provider_000001.etl msdk_analyzer.log