#!/usr/bin/perl

use 5.6.0;
use strict;
use Cwd;
use Getopt::Long;
#use _testsuite::suites::msdk_inc;
#use _testsuite::suites::msdk_command;
#msdk_inc::dump_test_header();

# This file takes information from _studio\mfxapi.ver file and edit resources files accroding to

#
# parse command line
#
Getopt::Long::Configure(qw( bundling permute no_getopt_compat pass_through no_ignore_case ));

my ( $rc_file, $release_number, $driver_version_digit, $vc_number, $revision, $plugin, $major_ver, $minor_ver) = ();

my %GetOptionsHash = (
    'rc=s' => \$rc_file,
    'release=s' => \$release_number,
    'driver:s' => \$driver_version_digit,
    'vc=s' => \$vc_number,
    'revision=s' => \$revision,
    'plugin=s' => \$plugin,
    'help' => \&Getopt::Long::HelpMessage,
	'major_ver=n' => \$major_ver,
	'minor_ver=n' => \$minor_ver
);
GetOptions( %GetOptionsHash ) or die "update_version_info FAILED\noptions parsing failed\n";

##############################   INITIAL PHASE   ####################################
#msdk_inc::begin_init_section();

($major_ver, $minor_ver) = (0, 0) if( -1 == $major_ver );
$major_ver = 1 if $plugin;

# setting up library's file version
my @time_stamp = localtime(time);
my $file_version = $major_ver.'.'.($time_stamp[5]-100).'.'.($time_stamp[4]+1).'.'.$time_stamp[3];

# getting VC number and revision number
if (!defined($vc_number)) {
    $vc_number = get_validation_cycle();
}
if (!defined($revision)) {
     $revision = get_repo_revision();
}

# setting up library's product version
#my $release_number = 1; # 1 - Alpha, 2- Beta, ...
if ( (!defined($release_number)) && (open my $release_file, 'release.txt')) {
    $release_number = <$release_file>;
}

my $driver_version_str = '0';
if ($rc_file =~ /mf_version/) {
    if (defined($driver_version_digit)) {
        $driver_version_str = '15'.$driver_version_digit;
    }
    elsif ( open my $driver_file, 'driver.txt') {
        $driver_version_digit = <$driver_file>;
        $driver_version_str = '15'.$driver_version_digit;
    }
}

if (!defined($plugin)) {
    if (int($vc_number) < 10) {
        $vc_number = '0'.$vc_number;
    }
    if (int($vc_number) > 99) {
        $vc_number =~ s/^\d(.*)/$1/; # 100 -> 00
    }
}

my $product_version_str = $major_ver.'.'.$minor_ver.'.'.$driver_version_str.'.'.$release_number.$vc_number;
my $product_version_digit = $product_version_str;

if (defined($plugin)) {
    my @api_ver = (1, 17);
    my $plugin_version = 5;
    if ($rc_file =~ 'libmfxhw_plugin_hevce' or $rc_file =~ 'ptir') {
        $plugin_version = 3;
    }
    if ($rc_file =~ 'libmfxhw_plugin_h265fei') {
        $plugin_version = 2;
    }
    $product_version_digit = $api_ver[0].'.'.$api_ver[1].'.'.$plugin_version.'.'.$vc_number;
    $product_version_str = $api_ver[0].'.'.$api_ver[1].'.'.$plugin_version.'.'.$vc_number;
}


##############################   TEST PHASE   ####################################
#msdk_inc::begin_test_section();

print "File version: $file_version\n";
print "Product Ver.: $product_version_digit\n";
if (update_rc_file($rc_file, $file_version, $product_version_digit, $product_version_str)){
    print 'error during update file', "\n";
    return 1;
} else {
    print '  version has been updated', "\n";
}

##############################   VERIFY PHASE   ####################################
#msdk_inc::exit_failed('Cannot get product version') if( -1 == $major_ver );
#msdk_inc::exit_failed('Cannot get VC number')       if( 0  == $vc_number );
#msdk_inc::exit_failed('Cannot get revision')        if( 0  == $revision );
#msdk_inc::exit_passed();

####################################################################################
## FUNCTIONS
# extract fourth line from file .svn/entries
sub get_repo_revision(){
    my $revision = 0;
    my @info = `svn info`;
    for (@info) {
        if (/^Revision: (\d+)$/) {
            $revision = $1;
            last;
        }
    }
    return $revision;
}

sub get_validation_cycle(){
    my $vc = getcwd();
    $vc =~ s/^.*(vc|VC)([0-9]+).*/$2/g;
    if ((!$vc)||($vc eq getcwd())){
        $vc = '00';
    }

    $vc = '0'.$vc if( length($vc) < 2 );

    return $vc;
}

sub update_rc_file($$$$){
    my ($rc_file, $file_version, $product_version_digit, $product_version_str) = @_;
    print "Update: $rc_file\n";
    open RC_FILE, '<'.$rc_file;
    my @rc_file_content = <RC_FILE>;
    close (RC_FILE);
    if ($#rc_file_content == -1) {
        print "ERROR: FILE NOT FOUND $rc_file\n";
        return 1;
    }
    my $file_version_comma = $file_version;
    $file_version_comma =~ s/\./,/g;
    my $product_version_comma = $product_version_digit;
    $product_version_comma =~ s/\./,/g;
    my @l_time = localtime(time);
    my $year = $l_time[5]+1900;
    map {
        $_=~ s/^([ ]+)(VALUE "FileVersion",).*/$1$2 "$file_version"/;
        if ($plugin eq 'evaluation') {
            $_=~ s/^([ ]+)(VALUE "ProductVersion",).*/$1$2 "$product_version_str Evaluation version"/;
        } else {
            $_=~ s/^([ ]+)(VALUE "ProductVersion",).*/$1$2 "$product_version_str"/;
        }
        $_=~ s/MFX_MF_FILE_VERSION .*/ MFX_MF_FILE_VERSION $file_version_comma/;
        $_=~ s/MFX_MF_PRODUCT_VERSION .*/ MFX_MF_PRODUCT_VERSION $product_version_comma/;

        $_=~ s/MFX_MF_FILE_VERSION_STR .*/ MFX_MF_FILE_VERSION_STR "$file_version"/;
        $_=~ s/MFX_MF_PRODUCT_VERSION_STR .*/ MFX_MF_PRODUCT_VERSION_STR "$product_version_str"/;

        $_=~ s/(Copyright. [0-9]{4}-)([0-9]{4})/$1$year/;
    } @rc_file_content;
    open RC_FILE, '>'.$rc_file;
    print RC_FILE @rc_file_content;
    close (RC_FILE);
    return 0;
}
