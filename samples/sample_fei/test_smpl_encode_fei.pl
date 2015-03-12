#!/usr/bin/perl

use 5.6.0;
use strict;
use Getopt::Long;
use _testsuite::suites::msdk_inc;
use _testsuite::suites::msdk_command;

msdk_inc::dump_test_header();
#1. run encode application
#2. run reference decode simple_player application
#3. run mectic_calc to calculate PSNR

#
##############################   INITIAL PHASE   ####################################
#
msdk_inc::begin_init_section();


#
# parse command line
#
Getopt::Long::Configure(qw( bundling permute no_getopt_compat pass_through no_ignore_case ));

my %argh = ();

my ( $app_name,
        $input_stream,
        $output_stream,
        $codec_name,
        $frame_rate,
        $usage,
        $height,
        $width,
        $bit_rate,
        $num_frames,
        $verbose,
        $suite,
        $threads,
    	$nviews,
        $test_id,
        $fourcc,
        $d3d,
        $d3d11,
        $vaapi,
        $hw,
        $sw,
        $dstw,
        $dsth,
        $thresPsnr,
        $rotate_angle,
        $opencl,
        $platform
        ) = ();
my ( @run_array ) = ();

my %GetOptionsHash = (
    'msdk_ver=s'        => \$GLOB::msdk_version,
    'a|application=s' => \$app_name,
    'codec=s' => \$codec_name,
    'i|input=s' => \$input_stream,
    'o|output=s' => \$output_stream,
    'f|framerate=i' => \$frame_rate,
    'tu=s' => \$usage,
    'w|width=i' => \$width,
    'h|height=i' => \$height,
    'b|bit_rate=i' => \$bit_rate,
    'n|num_frames=i' => \$num_frames,
    'v|verbose' => \$verbose,
    'platform=s' => \$GLOB::platform,
    'th=s' => \$threads,
    'nviews=i' => \$nviews,
    'suite=s' => \$suite,
    'noremove' => \$GLOB::no_remove,
    'd3d' => \$d3d,
    'd3d11' => \$d3d11,
    'vaapi' => \$vaapi,
    'test_id=s' => \$test_id,
    'fourcc=s' => \$fourcc,
    'hw=s' => \$hw,
    'sw=s' => \$sw,
    'dsth=i' => \$dsth,
    'dstw=i' => \$dstw,
    'thresPsnr=s' => \$thresPsnr,
    'angle=i' => \$rotate_angle,
    'opencl' => \$opencl,
    'x|dry_run' => \$GLOB::dry_run_mode,
    'help' => \&Getopt::Long::HelpMessage,
);

GetOptions( %GetOptionsHash ) or die "test_func_encode FAILED\noptions parsing failed\n";

$input_stream =~ m/[0-9]+\./;
@{$GLOB::temp_streams{'out_stream'}} = ('');
if ( $codec_name ne 'mvc' ) { $num_frames = $&; chop ($num_frames);}

# define OS specifics
msdk_inc::defineOS();

# prepare environment variables
msdk_inc::setup_env();

$input_stream = msdk_command::get_stream_path_by_name($input_stream);

#
# prepare command line for test application
#

my $output_dir = msdk_inc::create_output_dir(join msdk_inc::get_path_separator(), ('build', $GLOB::platform_name{MS_BUILD}{$GLOB::platform}, 'output', $suite));
$output_stream = join msdk_inc::get_path_separator(), ($output_dir, $test_id);
@{$GLOB::temp_streams{'out_stream'}} = ('');
@{$GLOB::temp_streams{'out_stream'}}[0] = $output_stream.'.'.$codec_name;


$GLOB::temp_streams{'nv12_ethalonstream'}->[0] = $output_stream.'.yuv.nv12';
@{$GLOB::temp_streams{'input_stream'}} = ('');

# prepare phase

if ($codec_name eq 'mvc' and $nviews > 0 ) { 
    my @mvcsplit_cmd_args = ();
    push @mvcsplit_cmd_args, 'mvc_tools_split'.$GLOB::exe_suffix;
    push @mvcsplit_cmd_args, '-w '.$width;
    push @mvcsplit_cmd_args, '-h '.$height;
    push @mvcsplit_cmd_args, '-c '.'420';
    push @mvcsplit_cmd_args, '-i '.$input_stream;
    push @mvcsplit_cmd_args, '-o ';
    for (my $i=0; $i<$nviews; $i++) {
        @{$GLOB::temp_streams{'input_stream'}}[$i] = @{$GLOB::temp_streams{'out_stream'}}[0].'.in'.$i.'.yuv';
        push @mvcsplit_cmd_args,@{$GLOB::temp_streams{'input_stream'}}[$i];
    }
    my @mvcsplit_output = ();
    my $error_code = msdk_inc::exec(\@mvcsplit_cmd_args, \@mvcsplit_output, '', "\nTEST APP ".'mvc split');
    if ($error_code != 0){
      print "CMD: @mvcsplit_cmd_args\nOUT: @mvcsplit_output\nEXIT CODE: $?\nMSG: $!\n";
      msdk_inc::exit_failed($mvcsplit_cmd_args[0].' failed with error code '.$error_code);
    }
}

my $input_stream_nv12 = $input_stream;

if( $fourcc eq 'nv12'){
    %argh = ();
    $input_stream = msdk_command::get_stream_path_by_name($input_stream);
    $input_stream =~ s/%([A-Z_]+)%/$ENV{$1}/;
    $input_stream =~ s/%//g;
    $argh{'INPUT STREAM'}  = $input_stream;
    $argh{'OUTPUT STREAM'} = $GLOB::temp_streams{'nv12_ethalonstream'}->[0];
    $argh{'LABEL'}         = 'Convert to NV12';
    $argh{'width'}         = $width;
    $argh{'height'}        = $height;
    $argh{'num_frames'}    = $num_frames;
    $argh{'fourcc'}    = 'yuv420';
    if( msdk_inc::run_ConvertNV12(\%argh) ){ 
        msdk_inc::exit_failed( $GLOB::errorMsg ); 
    }
    $input_stream = $argh{'OUTPUT STREAM'};
}

my @test_cmd_args = ();
push @test_cmd_args, $app_name;
push @test_cmd_args, $codec_name;
push @test_cmd_args, '-o '.@{$GLOB::temp_streams{'out_stream'}}[0];
#push @test_cmd_args, '-nviews '.$nviews if ($nviews);
push @test_cmd_args, '-w '.$width;
push @test_cmd_args, '-h '.$height;
push @test_cmd_args, '-f '.$frame_rate;
push @test_cmd_args, '-b '.$bit_rate;
push @test_cmd_args, '-u '.$usage;
#push @test_cmd_args, '-t '.$threads if ($threads);
push @test_cmd_args, '-dstw '.$dstw if ($dstw);
push @test_cmd_args, '-dsth '.$dsth if ($dsth);
push @test_cmd_args, '-'.$fourcc if ($fourcc);

push @test_cmd_args, '-i '.$input_stream if (!($codec_name eq 'mvc'));
if ($codec_name eq 'mvc' and $nviews > 0 ) { 
    for (my $i=0; $i<$nviews; $i++) {
        push @test_cmd_args, '-i '.@{$GLOB::temp_streams{'input_stream'}}[$i];
    }
}
push @test_cmd_args, '-d3d' if ($d3d);
push @test_cmd_args, '-d3d11' if ($d3d11);
push @test_cmd_args, '-vaapi' if ($vaapi);
push @test_cmd_args, '-hw ' if ($GLOB::platform =~ /(_hw)/);
push @test_cmd_args, '-angle '.$rotate_angle if ($rotate_angle);
push @test_cmd_args, '-opencl' if ($opencl);
#

$GLOB::streams_info{$argh{'input_stream'}}->{'picstruct'} = $argh{'orig_picstruct'} ? $argh{'orig_picstruct'} : 'progressive';
$GLOB::streams_info{$argh{'input_stream'}}->{'fourcc'} = $argh{'orig_fourcc'} ? $argh{'orig_fourcc'} : 'i420';

##############################   TEST PHASE   ####################################
#
msdk_inc::begin_test_section();

# execute test application
my @test_output = ();
my $error_code = msdk_inc::exec(\@test_cmd_args, \@test_output, '', "\nTEST APP");
if ($error_code != 0) {
    print "CMD: @test_cmd_args\nOUT: @test_output\nEXIT CODE: $?\nMSG: $!\n";
    msdk_inc::exit_failed();
}
print "\n@test_output\n" if ($verbose);


#
##############################   VERIFICATION PHASE   ####################################
#
msdk_inc::begin_verified_section();
#
# caclulate bit-rate
#
my @file_attr = stat (@{$GLOB::temp_streams{'out_stream'}}[0]);
if ($file_attr[7] == 0 && !$GLOB::dry_run_mode,){
    print "Zerro size bit-stream generated\nTEST FAILED\n";
    msdk_inc::exit_failed();
}

print "STREAMSIZE       : $file_attr[7]\n";
print "FRAMERATE        : $frame_rate\n";
print "NUMFRAMES        : $num_frames\n";
print "REQUESTED BITRATE: $bit_rate\n";
my $real_bit_rate = int ((8 * $file_attr[7] * $frame_rate / ($num_frames))/1024);
print "REAL BITRATE     : $real_bit_rate\n";

# pass through reference decoder
# command line for reference decoder
my $output_yuv = @{$GLOB::temp_streams{'out_stream'}}[0].'.yuv';
push @{$GLOB::temp_streams{'output_yuv'}}, $output_yuv;
my @ref_decoder_cmd_args = ();

if( $codec_name eq 'mvc'){
    push @ref_decoder_cmd_args, 'mfx_player'.$GLOB::exe_suffix;
    push @ref_decoder_cmd_args, '-i', @{$GLOB::temp_streams{'out_stream'}}[0];
    push @ref_decoder_cmd_args, '-sw';
    push @ref_decoder_cmd_args, '-o', $output_yuv;
}
else {
    push @ref_decoder_cmd_args, 'simple_player'.$GLOB::exe_suffix;
    push @ref_decoder_cmd_args, '-vfwf', $output_yuv;
    push @ref_decoder_cmd_args, '-anul';
    push @ref_decoder_cmd_args, '-t 1';
    push @ref_decoder_cmd_args, '-fyuv420';
    push @ref_decoder_cmd_args, @{$GLOB::temp_streams{'out_stream'}}[0];
}
my @ref_output = ();
my $error_code = msdk_inc::exec(\@ref_decoder_cmd_args, \@ref_output, '', "\nREF DEC");
if( $error_code != 0 ) {
    print "CMD: @ref_decoder_cmd_args\nOUT: @ref_output\nEXIT CODE: $?\nMSG: $!\n";
    msdk_inc::exit_failed();
}
print "\n@ref_decoder_cmd_args\n" if ($verbose);

my $reff_stream = $input_stream;
#
# Resize ethalon
#
if ($rotate_angle) {
    # execute mencoder twice
    # mencoder.exe -vf rotate=1 -o <out>.yuv -demuxer rawvideo -rawvideo i420:w=<width>:h=<heigh> -ovc raw -of rawvideo <input>.yuv
    my @mencoder_out = ();
    %argh = ();
    ${$GLOB::temp_streams{'turn_yuv_90'}}[0] = @{$GLOB::temp_streams{'out_stream'}}[0].'.90.yuv';
    ${$GLOB::temp_streams{'turn_yuv_180'}}[0] = @{$GLOB::temp_streams{'out_stream'}}[0].'.180.yuv';
    $argh{'INPUT STREAM'} = $input_stream_nv12;
    $argh{'OUTPUT STREAM'} = ${$GLOB::temp_streams{'turn_yuv_90'}}[0];
    $argh{'width'} = $width;
    $argh{'height'} = $height;
    $argh{'dcc'} = 'i420';
    $argh{'rotate'} = 1;
    $argh{'LABEL'        } = "\n".'Ref Rotate APP';
    if( msdk_inc::run_mencoder(\%argh) ){ 
        msdk_inc::exit_failed( $GLOB::errorMsg ); 
    }
    print "@mencoder_out\n" if $verbose;
    # second rotation 
    @mencoder_out = ();
    $argh{'INPUT STREAM'} = ${$GLOB::temp_streams{'turn_yuv_90'}}[0];
    $argh{'OUTPUT STREAM'} = ${$GLOB::temp_streams{'turn_yuv_180'}}[0];
    $argh{'width'} = $height;
    $argh{'height'} = $width;
    if( msdk_inc::run_mencoder(\%argh) ){ 
        msdk_inc::exit_failed( $GLOB::errorMsg ); 
    }
    print "@mencoder_out\n" if $verbose;
    $reff_stream = ${$GLOB::temp_streams{'turn_yuv_180'}}[0];
}

if ($dstw and $dsth and ($dstw ne $width or $dsth ne $height)) {
    my @mencoder_out = ();
    %argh = ();
    $argh{'INPUT STREAM'} = $input_stream_nv12;
    ${$GLOB::temp_streams{'ethalon_rsf_yuv'}}[0] = @{$GLOB::temp_streams{'out_stream'}}[0].'.ethalon.rsf.yuv';
    $argh{'OUTPUT STREAM'} = ${$GLOB::temp_streams{'ethalon_rsf_yuv'}}[0];
    $argh{'width'} = $width;
    $argh{'height'} = $height;
    $argh{'dwidth'} = $dstw;
    $argh{'dheight'} = $dsth;
    $argh{'LABEL'        } = "\n".'Ref Resize APP';
    if( msdk_inc::run_mencoder(\%argh) ){ 
        msdk_inc::exit_failed( $GLOB::errorMsg ); 
    }
    print "@mencoder_out\n" if $verbose;
    $reff_stream = ${$GLOB::temp_streams{'ethalon_rsf_yuv'}}[0];
}
    
# findout real frame-rate

#my ($frame_rate) = grep ( /-Frame Rate[ ]*:/, @ref_decoder_output );
#$frame_rate =~ s/(-Frame Rate[ ]*:)([0-9]*)/$2/g;
#print "REAL FRAMERATE   : $frame_rate\n";

# prepare command line for verify stage
my @verify_cmd_args = ();
push @verify_cmd_args, 'metrics_calc_lite'.$GLOB::exe_suffix;
push @verify_cmd_args, '-i1 '.$reff_stream;
push @verify_cmd_args, '-i2 '.$output_yuv;
push @verify_cmd_args, '-w '.$dstw if ($dstw);
push @verify_cmd_args, '-h '.$dsth if ($dsth);
push @verify_cmd_args, '-w '.$width if (!$dstw);
push @verify_cmd_args, '-h '.$height if (!$dsth);
push @verify_cmd_args, 'psnr all';
my @verify_output = ();

## Compare with previous results
# compare original stream with output yuv from sample_vpp

my $error_code = msdk_inc::exec(\@verify_cmd_args, \@verify_output, '', "\nVERIF");
if( $error_code != 0 ) {
    print "CMD: @verify_cmd_args\nOUT: @verify_output\nEXIT CODE: $?\nMSG: $!\n";
    msdk_inc::exit_failed();
}
print "\n@verify_cmd_args\n" if ($verbose);
my $fail_msg = compare_metrics( \@verify_output, $thresPsnr );
if($fail_msg eq '') {
    msdk_inc::exit_passed();
} else {
    print "\n------------- Additional executions -------------\n";
    # second run
    # YUV -> sample_vpp(resize) -> YUV* -> sample_fei -> bitstream -> sample_decode -> YUV**
    ## YUV -> sample_vpp(resize) -> YUV*
    my @sample_vpp_cmd = my @sample_vpp_output = ();
    my $smpl_vpp_yuv = join msdk_inc::get_path_separator(), ($output_dir, $test_id.'.vpp.yuv');
    push @{$GLOB::temp_streams{'smpl_vpp_yuv'}}, $smpl_vpp_yuv;
    push @sample_vpp_cmd, 'sample_vpp'.$GLOB::exe_suffix;
    push @sample_vpp_cmd, '-i '.$input_stream;
    push @sample_vpp_cmd, '-o '.$smpl_vpp_yuv;
    push @sample_vpp_cmd, '-sh '.$height;
    push @sample_vpp_cmd, '-sw '.$width;
    push @sample_vpp_cmd, '-dw '.$dstw if ($dstw);
    push @sample_vpp_cmd, '-dh '.$dsth if ($dsth);
    push @sample_vpp_cmd, '-nr 0'; # disable denoiser
    if( $input_stream =~ m/foster/ ) {
        push @sample_vpp_cmd, '-spic 0';
        push @sample_vpp_cmd, '-dpic 0';
    }
    my $error_code = msdk_inc::exec(\@sample_vpp_cmd, \@sample_vpp_output, '', "\nTEST APP");
    if ($error_code != 0) {
        print "CMD: @sample_vpp_cmd\nOUT: @sample_vpp_output\nEXIT CODE: $?\nMSG: $!\n";
        msdk_inc::exit_failed();
    }
    if ($codec_name eq 'mvc' and $nviews > 0 ) { 
        my @mvcsplit_cmd_args = ();
        push @mvcsplit_cmd_args, 'mvc_tools_split'.$GLOB::exe_suffix;
        push @mvcsplit_cmd_args, '-w '.$width;
        push @mvcsplit_cmd_args, '-h '.$height;
        push @mvcsplit_cmd_args, '-c '.'420';
        push @mvcsplit_cmd_args, '-i '.$smpl_vpp_yuv;
        push @mvcsplit_cmd_args, '-o ';
        for (my $i=0; $i<$nviews; $i++) {
            @{$GLOB::temp_streams{'smpl_vpp_yuv_mvc'}}[$i] = @{$GLOB::temp_streams{'out_stream'}}[0].'.vpp'.$i.'.yuv';
            push @mvcsplit_cmd_args,@{$GLOB::temp_streams{'smpl_vpp_yuv_mvc'}}[$i];
        }
        my @mvcsplit_output = ();
        my $error_code = msdk_inc::exec(\@mvcsplit_cmd_args, \@mvcsplit_output, '', "\nTEST APP ".'mvc split');
        if ($error_code != 0){
          print "CMD: @mvcsplit_cmd_args\nOUT: @mvcsplit_output\nEXIT CODE: $?\nMSG: $!\n";
          msdk_inc::exit_failed($mvcsplit_cmd_args[0].' failed with error code '.$error_code);
        }
    }

    ## ... -> sample_fei - > bitstream
    my @sample_encode_cmd = my @sample_encode_output = ();
    my $smpl_encode_out = join msdk_inc::get_path_separator(), ($output_dir, $test_id.'.enc');
    push @{$GLOB::temp_streams{'smpl_encode_out'}}, $smpl_encode_out;
    push @sample_encode_cmd, $app_name;
    push @sample_encode_cmd, $codec_name;
    push @sample_encode_cmd, '-o '.$smpl_encode_out;
    push @sample_encode_cmd, '-h '.$dsth if ($dsth);
    push @sample_encode_cmd, '-w '.$dstw if ($dstw);
    push @sample_encode_cmd, '-h '.$height if (!$dsth);;
    push @sample_encode_cmd, '-w '.$width if (!$dstw);;
    push @sample_encode_cmd, '-f '.$frame_rate if ($frame_rate);
    push @sample_encode_cmd, '-b '.$bit_rate if ($bit_rate);
    push @sample_encode_cmd, '-u '.$usage if ($usage);
    push @sample_encode_cmd, '-t '.$threads if ($threads);
    #push @sample_encode_cmd, '-fourcc '.$fourcc if ($fourcc);
    push @sample_encode_cmd, '-'.$fourcc if ($fourcc);
    push @sample_encode_cmd, '-i '.$smpl_vpp_yuv if (!($codec_name eq 'mvc'));
    if ($codec_name eq 'mvc' and $nviews > 0 ) { 
        for (my $i=0; $i<$nviews; $i++) {
            push @sample_encode_cmd, '-i '.@{$GLOB::temp_streams{'smpl_vpp_yuv_mvc'}}[$i];
        }
    }
    push @sample_encode_cmd, '-d3d' if ($d3d);
    push @sample_encode_cmd, '-hw ' if ($GLOB::platform =~ /(_hw)/);
    my $error_code = msdk_inc::exec(\@sample_encode_cmd, \@sample_encode_output, '', "\nTEST APP");
    if ($error_code != 0) {
        print "CMD: @sample_encode_cmd\nOUT: @sample_encode_output\nEXIT CODE: $?\nMSG: $!\n";
        msdk_inc::exit_failed();
    }

    ## ... -> sample_decode -> YUV**
    my $smpl_decode_yuv = $smpl_encode_out.'.yuv';



    push @{$GLOB::temp_streams{'smpl_decode_yuv'}}, $smpl_decode_yuv;
    my @ref_decoder_cmd_args = ();
    if( $codec_name eq 'mvc'){
        push @ref_decoder_cmd_args, 'mfx_player'.$GLOB::exe_suffix;
        push @ref_decoder_cmd_args, '-i', $smpl_encode_out;
        push @ref_decoder_cmd_args, '-sw';
        push @ref_decoder_cmd_args, '-o', $smpl_decode_yuv;
    }
    else {
        push @ref_decoder_cmd_args, 'simple_player'.$GLOB::exe_suffix;
        push @ref_decoder_cmd_args, '-vfwf', $smpl_decode_yuv;
        push @ref_decoder_cmd_args, '-anul';
        push @ref_decoder_cmd_args, '-t 1';
        push @ref_decoder_cmd_args, '-fyuv420';
        push @ref_decoder_cmd_args, $smpl_encode_out;
    }
    my @ref_output = ();
    my $error_code = msdk_inc::exec(\@ref_decoder_cmd_args, \@ref_output, '', "\nREF DEC");
    if( $error_code != 0 ) {
        print "CMD: @ref_decoder_cmd_args\nOUT: @ref_output\nEXIT CODE: $?\nMSG: $!\n";
        msdk_inc::exit_failed();
    }
    
    additional_check( \@verify_cmd_args );   
    msdk_inc::exit_failed('low PSNR #['.$fail_msg.']');

}

###################################################################################################
sub additional_check {  # additional_check( \@verify_cmd_args, $thresPsnr );
    my @verify_cmd_args = @{$_[0]};
    my $thresPsnr = $_[1];
    
    my $reff_stream = @{$GLOB::temp_streams{'reff_stream'}}[0];
    my $smpl_vpp_yuv = @{$GLOB::temp_streams{'smpl_vpp_yuv'}}[0];
    my $smpl_decode_yuv = @{$GLOB::temp_streams{'smpl_decode_yuv'}}[0];
    my $output_yuv = @{$GLOB::temp_streams{'output_yuv'}}[0];
        
    
    print "\n------------- 1 compare -------------\n";
    my $tmp = join( ' ', @verify_cmd_args );
    $tmp =~ s/-i2\s[^\s]*\s/-i2 $smpl_vpp_yuv /;
    @verify_cmd_args = split( ' ', $tmp );
    my @verify_output = ();
    my $error_code = msdk_inc::exec(\@verify_cmd_args, \@verify_output, '', "\nVERIF.: ");
    if( $error_code != 0 ) {
        print "CMD: @verify_cmd_args\nOUT: @verify_output\nEXIT CODE: $?\nMSG: $!\n";
        msdk_inc::exit_failed();
    }
    print "\n@verify_cmd_args\n" if ($verbose);
    my $fail_msg = compare_metrics( \@verify_output, $thresPsnr );
    if( $fail_msg eq '' ) {
        print "sample_vpp (resize):  PASS\n";
    } else {
        print "sample_vpp (resize): FAILED\n";
    }
    
    print "\n------------- 2 compare -------------\n";
    my $tmp = join( ' ', @verify_cmd_args );
    $tmp =~ s/-i2\s[^\s]*\s/-i2 $smpl_decode_yuv /;
    @verify_cmd_args = split( ' ', $tmp );
    @verify_output = ();
    my $error_code = msdk_inc::exec(\@verify_cmd_args, \@verify_output, '', "\nVERIF.: ");
    if( $error_code != 0 ) {
        print "CMD: @verify_cmd_args\nOUT: @verify_output\nEXIT CODE: $?\nMSG: $!\n";
        msdk_inc::exit_failed();
    }
    print "\n@verify_cmd_args\n" if ($verbose);
    my $fail_msg = compare_metrics( \@verify_output, $thresPsnr );
    if( $fail_msg eq '' ) {
        print "encode:  PASS\n";
    } else {
        print "encode: FAILED\n";
    }
    
    print "\n------------- 3 compare -------------\n";
    my $tmp = join( ' ', @verify_cmd_args );
    $tmp =~ s/-i1\s[^\s]*\s/-i1 $smpl_vpp_yuv /;
    @verify_cmd_args = split( ' ', $tmp );
    @verify_output = ();
    my $error_code = msdk_inc::exec(\@verify_cmd_args, \@verify_output, '', "\nVERIF.: ");
    if( $error_code != 0 ) {
        print "CMD: @verify_cmd_args\nOUT: @verify_output\nEXIT CODE: $?\nMSG: $!\n";
        msdk_inc::exit_failed();
    }
    print "\n@verify_cmd_args\n" if ($verbose);
    my $fail_msg = compare_metrics( \@verify_output, $thresPsnr );
    if( $fail_msg eq '' ) {
        print "compare sample_vpp (resize) & encode:  PASS\n";
    } else {
        print "compare sample_vpp (resize) & encode: FAILED\n";
    }

    print "\n------------- 4 compare -------------\n";
    my $tmp = join( ' ', @verify_cmd_args );
    $tmp =~ s/-i1\s[^\s]*\s/-i1 $output_yuv /;
    @verify_cmd_args = split( ' ', $tmp );
    @verify_output = ();
    my $error_code = msdk_inc::exec(\@verify_cmd_args, \@verify_output, '', "\nVERIF.: ");
    if( $error_code != 0 ) {
        print "CMD: @verify_cmd_args\nOUT: @verify_output\nEXIT CODE: $?\nMSG: $!\n";
        msdk_inc::exit_failed();
    }
    print "\n@verify_cmd_args\n" if ($verbose);
    my $fail_msg = compare_metrics( \@verify_output, $thresPsnr );
    if( $fail_msg eq '' ) {
        print "compare encode+resize & encode:  PASS\n";
    } else {
        print "compare encode+resize & encode: FAILED\n";
    }
}

sub compare_metrics {  # compare_metrics( \@verify_output, $thresPsnr )

    my %metricValue = ();
    my @verify_output = @{$_[0]};
    msdk_inc::getMetricValue('PSNR', \%metricValue, \@verify_output);  
    
    my $thresPsnr    = $_[1];
    # prepare metrics to calculate
    my ($thres_y, $thres_u, $thres_v) = split (' ', $thresPsnr);
    my $status = 0;
    my $avg_y_psnr = 0;
    my @frame_y_psnr = ();
    my ($tmp_f_psnr) = grep {/<pfr_metric=Y-PSNR>/} @verify_output;
    $tmp_f_psnr =~ s/(<pfr_metric=Y-PSNR>)//;
    $tmp_f_psnr =~ s/(<\/pfr_metric>)//;
    @frame_y_psnr = split (' ', $tmp_f_psnr);
    my $f_psnr = $tmp_f_psnr;
    my ($tmp_f_psnr) = grep {/<avg_metric=Y-PSNR>/} @verify_output;
    $tmp_f_psnr =~ s/(<avg_metric=Y-PSNR>)//;
    $tmp_f_psnr =~ s/(<\/avg_metric>)//;
    $avg_y_psnr = $tmp_f_psnr;
    my $fail_message = '';
    print "\nFRAME Y PSNR: @frame_y_psnr";
    print "\nAVG.  Y PSNR:$avg_y_psnr";
    if ($thresPsnr) {
        my %statusY = msdk_inc::metric_frame_analysis2($thres_y, $metricValue{'frameY'}, 1);
        $fail_message.= 'Y:min='.$statusY{'min'}.',max='.$statusY{'max'}.',avg='.$statusY{'avg'}.',threshold='.$statusY{'threshold'}.'; ' if($statusY{'status'} eq 'failed');
        print '  ', $statusY{'status'}, "\n\n";
    }
    my $avg_u_psnr = 0;
    my @frame_u_psnr = ();
    my ($tmp_f_psnr) = grep {/<pfr_metric=U-PSNR>/} @verify_output;
    $tmp_f_psnr =~ s/(<pfr_metric=U-PSNR>)//;
    $tmp_f_psnr =~ s/(<\/pfr_metric>)//;
    @frame_u_psnr = split (' ', $tmp_f_psnr);
    $f_psnr = $tmp_f_psnr;
    my ($tmp_f_psnr) = grep {/<avg_metric=U-PSNR>/} @verify_output;
    $tmp_f_psnr =~ s/(<avg_metric=U-PSNR>)//;
    $tmp_f_psnr =~ s/(<\/avg_metric>)//;
    $avg_u_psnr = $tmp_f_psnr;
    print "\nFRAME U PSNR: @frame_u_psnr";
    print "\nAVG.  U PSNR:$avg_u_psnr";
    if ($thresPsnr) {
        my %statusU = msdk_inc::metric_frame_analysis2($thres_u, $metricValue{'frameU'}, 1);
        $fail_message.= 'U:min='.$statusU{'min'}.',max='.$statusU{'max'}.',avg='.$statusU{'avg'}.',threshold='.$statusU{'threshold'}.'; ' if($statusU{'status'} eq 'failed');
        print '  ', $statusU{'status'}, "\n\n";
    }
    my $avg_v_psnr = 0;
    my @frame_v_psnr = ();
    my ($tmp_f_psnr) = grep {/<pfr_metric=V-PSNR>/} @verify_output;
    $tmp_f_psnr =~ s/(<pfr_metric=V-PSNR>)//;
    $tmp_f_psnr =~ s/(<\/pfr_metric>)//;
    @frame_v_psnr = split (' ', $tmp_f_psnr);
    $f_psnr = $tmp_f_psnr;
    my ($tmp_f_psnr) = grep {/<avg_metric=V-PSNR>/} @verify_output;
    $tmp_f_psnr =~ s/(<avg_metric=V-PSNR>)//;
    $tmp_f_psnr =~ s/(<\/avg_metric>)//;
    $avg_v_psnr = $tmp_f_psnr;
    print "\nFRAME V PSNR: @frame_v_psnr";
    print "\nAVG.  V PSNR:$avg_v_psnr";
    if ($thresPsnr) {
        my %statusV = msdk_inc::metric_frame_analysis2($thres_v, $metricValue{'frameV'}, 1);
        $fail_message.= 'V:min='.$statusV{'min'}.',max='.$statusV{'max'}.',avg='.$statusV{'avg'}.',threshold='.$statusV{'threshold'} if($statusV{'status'} eq 'failed');
        print '  ', $statusV{'status'}, "\n\n";
    }
    my ($tmp_f_psnr) = grep {/<avg_metric=PSNR>/} @verify_output;
    $tmp_f_psnr =~ s/(<avg_metric=PSNR>)//;
    $tmp_f_psnr =~ s/(<\/avg_metric>)//;
    print "\nAVG.  PSNR: $tmp_f_psnr";
    my $encoded_frames = scalar @frame_v_psnr;
    print "FRAMES ENCODED: $encoded_frames\n";

    my ($tmp_f_psnr) = grep {/<avg_metric=PSNR>/} @verify_output;
    $tmp_f_psnr =~ s/(<avg_metric=PSNR>)//;
    $tmp_f_psnr =~ s/(<\/avg_metric>)//;
    print "\nAVG.  PSNR: $tmp_f_psnr";

    return $fail_message;
}

################ Documentation ################

=head1 NAME

test_func_encode.pl - test application which drives user-friendly mode functional testing

=head1 SYNOPSIS

test_func_encode.pl [options]

 options:
 --verbose              enable verbose mode
 --suite <suite_name>   register and execute 'suite_name' test suite

=head1 DESCRIPTION

=head1 Command Line Options, an Introduction

=head1 Getting Started with Test Driver

=head2 Simple options

=head2 Options with values

=head2 Options with multiple values

=head2 Case and abbreviations

=head2 Documentation and help texts

=head2 Bundling

=head2 Argument callback

=head1 Exportable Methods

=head1 Return values and Errors

=head1 Legacy

=head2 Default destinations

=head2 Alternative option starters

=head2 Configuration variables

=head1 AUTHOR

=head1 COPYRIGHT AND DISCLAIMER

=cut
