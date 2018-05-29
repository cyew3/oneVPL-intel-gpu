function ParseVcxproj($filename, $name)
{
echo "Found $filename"
mkdir -Force ".\evalproj\$name"
cd .\evalproj\$name
C:\MSDK_ROOT\mdp_msdk-val-tools\plugins\mfoundation\msbuild_dumper\MSBuildDumper.exe $filename C:\MSDK_ROOT\mdp_msdk-lib\_studio\mfx_lib\
cd .\..\..\
}

ls -Recurse -Filter "*.vcxproj" -File | % {
    ParseVcxproj $_.FullName $_.Name
}

