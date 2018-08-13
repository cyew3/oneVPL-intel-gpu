How to build samples_UWP. You don't need any MSDK package to be installed. 
1. Install DFP appx (Intel.MediaSDK.x64.appx) from DFP folder (by double clicking on it).
Package may request you to add certificate it is signed with, into Trusted Root Certificates. 
To do it, open properties of .appx file, choose Digital Signatures tabe and double click on certificate (it is only one in the list). 
Then choose View Certificate, General/Install Certificate, choose "Local Machine" option, select "Place all certificates in the following store" and click "Browse".
Then choose "Trusted Root Certification Authorities" and click OK.

2. Run setPath.bat - it will set proper value for INTELMEDIASDKROOT
Note that if you had MSDK/MSS installed previously, this variable (INTELMEDIASDKROOT) already has value pointing to MSS/MSDK installation folder - backup it if necessary, it will be overwritten

3. Compile samples using solution file from ExtendedUWPSamples. There're 2 types of configurations - Debug/Release and DebugWithExtras/ReleaseWithExtras.
"WithExtras" configurations require FFMPEG package to be compiled inside ffmpeg folder or unzip precompiled files from provided zip file.
"WithExtras" configurations add capability to save stream into MP4 container file. If you don't need this capability - use Debug/Release configuration.

Note: headers for this package are taken from API 1.26

Additional notes.
If you will make your own sample, it should comply to these requirements:
1. Make sure that Package.appxmanifest includes dependency "<PackageDependency Name="Intel.MediaSDK" MinVersion="1.0.0.0" Publisher="CN=gkrivor"/>"
2. Make sure that application links libmfx_uwp_dfp.lib (instead of standard dispatcher).

Note that you may reuse DLL from MSDKInterop project in your applications and use MSDK via DecoderInterop and EncoderInterop classes.
In this case you don't need to follow steps described above - everything is already included into MSDKInterop

Current limitations:
- 32 bit package is not included, only x64 DFP package is included, so samples won't work in 32-bit mode.
- MP4 files cannot be played out (only saving to MP4 files is supported, via FFMPEG usage).