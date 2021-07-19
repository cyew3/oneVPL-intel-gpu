Introduction
Align.py is used to automatically trim codes in close-source repo and copy corresponding folders/files to open-source repo.

Library Dependency.
1. python tools
   pip3 install regex
   pip3 install portion

Usage: 
1. Copy <sources>/mdp_msdk-lib/config/scripts/sdm/align.py and <sources>/mdp_msdk-lib/config/scripts/sdm/config.txt to <sources>
2. Download opensource codes to <sources>
3. cd to <sources>
4. python3 align.py
5. You can find macro violations in <sources>/violations

Parameters in config.txt:
ScanFolder: path of folders to doing scan&trim.  These folder will going to streamline dev model.
Folder/FileList: close source folders/files which need to be trimmed.
DeleteFolder/FileName: Same as above, but only file/folder name without full path. Wildcard character like *dxva*.cpp can be used as well.
UndefineList: The feature macro is always not defined and it's macro name cannot opensource as well. So macro itself & # sentences & macro logic must be striped.
DefineList:  The feature macro which is always defined in open, so macro itself & # sentences & !macro logic must be striped.
CloseSourceRoot:  Root path of close source workspace. The close source workspace can have a close source patch developed by dev. This is used for script to trim files/folders/macro pieces.
OpenSourceRoot:  Root path of open source workspace. This is used for the trimmed corresponding folder to copy into open source workspace and using "git diff" to generate corresponding open source patch.
ExceptionList:  The macro which won't join the macro rule violation check. (Still working in progress)
ExceptionFileList:  The files' full path which are excluded from doing trim and macro scan. Although the files are inside ScanFolder. (Still working in progress)

Details:
Refer to https://wiki.ith.intel.com/display/MediaWiki/Streamlined+Dev+Model#StreamlinedDevModel-CodingGuidelineforMacrobasedRTFeatureMgmt

Contributors:
Wang, Yuchen
Zhang, Zhijie
Xu, Xing
Zhou, Aijun