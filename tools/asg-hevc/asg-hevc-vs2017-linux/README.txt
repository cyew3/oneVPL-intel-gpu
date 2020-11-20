
1. Install Visual Studio 2017 with remote Linux debugging support

2. Use asg-hevc-vs2017-linux.vcxproj.template as a template for your local VCXPROJ. Rename it to asg-hevc-vs2017-linux.vcxproj and open in Visual Studio

Project contains asg-hevc sources along with Linux build scripts and original CMakeLists.txt

3. Make sure that remote build machine credentials are set correctly

4. Make sure that "Remote Build Project Directory" should have local copies of:
- mdp_msdk-api
- mdp_msdk-contrib
- mdp_msdk-tools
- mdp_msdk-lib

5. After build running, all sources files are copied to the remote machine appropriatelly, build scripts are copied to "Remote Build Project Directory"
