#!/usr/bin/perl

# (C) Sergey Kandaurov
# (C) Maxim Dounin
# (C) Nginx, Inc.

# Tests for 51Degrees Hash Trie module.

###############################################################################

use warnings;
use strict;
use File::Temp qw/ tempdir /;
use Test::More;
use File::Copy;

BEGIN { use FindBin; chdir($FindBin::Bin); }

use lib '../nginx-tests/lib';
use Test::Nginx;
use URI::Escape;

###############################################################################

select STDERR; $| = 1;
select STDOUT; $| = 1;

sub read_example($) {
	my ($name) = @_;
	open my $fh, '<', '../../examples/hash/responseHeaders/' . $name or die "Can't open file $name: $!";
	read $fh, my $content, -s $fh;
	close $fh;

	return $content;
}

my $n = 0;
my $t_lite = 1;

# The Lite data file version does not contains properties that can be used
# to test the overrides feature. Please consider to use one that have at least
# ScreenPixelsWidth and ScreenPixelsWidthJavascript properties
if (scalar(@ARGV) > 0 && index($ARGV[0], "51Degrees-Lite") == -1) {
    $n += 0;
    $t_lite = 0;
}
my $t = Test::Nginx->new()->has(qw/http/)->plan($n);

my $t_file = read_example('responseHeader.conf');
# Remove documentation block
$t_file =~ s/\/\*\*.+\*\//''/gmse;
# Replace all variable place holder
$t_file =~ s/%%DAEMON_MODE%%/'off'/gmse;
$t_file =~ s/%%MODULE_PATH%%/$ENV{TEST_MODULE_PATH}/gmse;
$t_file =~ s/%%FILE_PATH%%/$ENV{TEST_FILE_PATH}/gmse;
$t->write_file_expand('nginx.conf', $t_file);

my $t_html_file = read_example('response.html');
$t->write_file('response.html', $t_html_file);

$t->run();

sub get_with_ua {
	my ($uri, $ua) = @_;
	return http(<<EOF);
GET $uri HTTP/1.1
Host: localhost
Connection: close
User-Agent: $ua

EOF
}

sub get_with_ch_ua {
	my ($uri, $chua) = @_;
	return http(<<EOF);
GET $uri HTTP/1.1
Host: localhost
Connection: close
sec-ch-ua: $chua

EOF
}

###############################################################################
# Constants.
###############################################################################

my $chromeCHUA = '" Not A;Brand";v="99", "Chromium";v="90", "Google Chrome";v="90"';
my $chromeUserAgent = 'Mozilla/5.0 (X11; Linux x86_64) AppleWebKit/537.36 (KHTML, like Gecko) Chrome/90.0.4430.85 Safari/537.36';

###############################################################################
# Test responseHeader.conf example.
###############################################################################

# TODO: Enable when UACH is supported
if (!$t_lite && 0) {
	my $r = get_with_ch_ua('/response.html', $chromeCHUA);
	like($r, qr/Accept-CH: SEC-CH-[a-zA-Z\-]+(,SEC-CH-[a-zA-Z\-])*/, 'Response Header');
	like($r, qr/Platform Name:.*NoMatch/, 'Get Platform Name using Client Hint');
	like($r, qr/Browser Vendor:.*Google/, 'Get Browser Vendor using Client Hint');
	like($r, qr/Browser Name:.*Chrome/, 'Get Browser Name using Client Hint');
	like($r, qr/Browser Version:.*90/, 'Get Browser Version using Client Hint');

	$r = get_with_ua('/response.html', $chromeUserAgent);
	like($r, qr/Accept-CH: SEC-CH-[a-zA-Z\-]+(,SEC-CH-[a-zA-Z\-])*/, 'Response Header');
	like($r, qr/Platform Name:.*Linux/, 'Get Platform Name using User Agent');
	like($r, qr/Browser Vendor:.*Google/, 'Get Browser Vendor using User Agent');
	like($r, qr/Browser Name:.*Chrome/, 'Get Browser Name using User Agent');
	like($r, qr/Browser Version:.*90/, 'Get Browser Version using User Agent');
}

###############################################################################

# Print out warnings at the end for user attention
if ($t_lite) {
    print "WARNING: Client Hints response headers feature test will not be run \n";
    print "         as the Lite version of data file does not contains the \n";
    print "         required properties. Please consider to obtain a data \n";
    print "         file that have at least the SetHeaderBrowserAccept-CH,\n";
    print "         SetHeaderPlatformAccept-CH and \n";
    print "         SetHeaderHardwareAccept-CH supported.\n";
}
