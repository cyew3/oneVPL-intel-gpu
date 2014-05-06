my $build_path = "..\\..\\build\\win_Win32\\bin";
my $YUV_path  = "$ENV{MEDIASDK_STREAMS}\\YUV";
my $logfile    = "mfx_transcoder_verify.html";

my $in_params = [
#h264
	["test_h264_encode", "h264 -i $YUV_path\\foreman_352x288_300.yuv -w 352 -h 288"],
	["test_h264_encode", "h264 -i $YUV_path\\bbc_704x576_374.yuv -w 704 -h 576"],

#mpeg2
	["test_mpeg2_encode", "m2 -i $YUV_path\\foreman_352x288_300.yuv -w 352 -h 288"],
	["test_mpeg2_encode", "m2 -i $YUV_path\\bbc_704x576_374.yuv -w 704 -h 576"],
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

	  
   my $v1 = system(" $build_path\\$var1->[0]$post.exe $var1->[1] -o out1.bit");
   foreach( @add_params )
   {
   	my $v2 = system(" $build_path\\mfx_transcoder$post.exe  $var1->[1] -o out2.bit -showframes $_");
	my $v3 = system("echo n | comp.exe out1.bit out2.bit >> nul");
   
        system("echo ^<tr^> ^<td^> $var1->[1] ^</td^>^<td^> mfx_transcoder $_^</td^>^<td^> $v2 ^</td^> ^<td^> $var1->[0] ^</td^> ^<td^> $v1 ^</td^> ^<td^> COMP=$v3 ^</td^> ^</tr^> >> $logfile");
   }
}

system("echo ^</TABLE^>^</BODY^>^</HTML^> >> $logfile");   

