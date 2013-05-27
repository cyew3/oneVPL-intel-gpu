my $build_path = "..\\..\\build\\win_Win32\\bin";
my $conf_path  = "$ENV{MEDIASDK_STREAMS}\\conformance";
my $logfile    = "mfx_player_verify.html";

my $in_params = [
#h264
	["test_h264_decode", "-i $conf_path\\h264\\PV\\HDDVD\\ShuttleStart_1280x720_60Hz_main_IBBP_f200_29M.264"],
	["test_h264_decode", "-i $conf_path\\h264\\baseline_ext\\aud_mw_e.264"],
	["test_h264_decode", "-i $conf_path\\h264\\baseline_ext\\cama3_sand_e.264"],
	["test_h264_decode", "-i $conf_path\\h264\\high\\high_profile\\Freh12_B.264"],
	["test_h264_decode", "-i $conf_path\\h264\\baseline_ext\\camanl2_toshiba_b.264"],

#mpeg2
	["test_mpeg2_decode", "-i $conf_path\\mpeg2\\streams\\sony-ct1.bits"],
	["test_mpeg2_decode", "-i $conf_path\\mpeg2\\streams\\nokia6_dual_60.bit"],
	["test_mpeg2_decode", "-i $conf_path\\mpeg2\\streams\\tcela-19.bits"],
#vc1
	["test_vc1_decode", "-i $conf_path\\vc1\\streams\\SA00010.vc1"],
	["test_vc1_decode", "-i $conf_path\\vc1\\streams\\SA00039.vc1"],
	["test_vc1_decode", "-i $conf_path\\vc1\\streams_151205\\SA00059.vc1"]
];

my @add_params = (
	"-sw -async 1 -sys",
	"-sw -async 1 -sys -extalloc",
	"-sw -async 1 -sys -d3d",
	"-sw -async 3 -sys",
	"-sw -async 3 -sys -extalloc",
	"-sw -async 3 -sys -d3d"
);

system("echo ^<HTML^>^<BODY^>^<TABLE border=2 ^> > $logfile");

my $post = "";
if (@ARGV[0] eq "-d")
{
    $post = "_d";
}

foreach my $var1( @$in_params )
{
   unlink "out1.yuv" if -e "out1.yuv";
   unlink "out2.yuv" if -e "out2.yuv";

#  printf " $build_path\\$var1->[0]$post.exe $var1->[1] -o out1.yuv";
   my $v1 = system(" $build_path\\$var1->[0]$post.exe $var1->[1] -o out1.yuv");

   foreach( @add_params )
   {
	my $v2 = system(" $build_path\\mfx_player$post.exe  $var1->[1] -o out2.yuv -showframes -cmp out1.yuv -delta 0 $_");
	system("echo ^<tr^> ^<td^> $var1->[1] ^</td^>^<td^> mfx_player$post $_^</td^>^<td^> $v2 ^</td^> ^<td^> $var1->[0]$post ^</td^> ^<td^> $v1 ^</td^> ^</tr^> >> $logfile");
   }
}

system("echo ^</TABLE^>^</BODY^>^</HTML^> >> $logfile");   

