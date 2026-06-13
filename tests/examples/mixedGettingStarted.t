#!/usr/bin/perl

# (C) Sergey Kandaurov
# (C) Maxim Dounin
# (C) Nginx, Inc.

# Tests for the 51Degrees device detection and IP intelligence modules
# loaded together in the same Nginx configuration.

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
use POSIX qw/ WNOHANG /;

###############################################################################

select STDERR; $| = 1;
select STDOUT; $| = 1;

# A full IP intelligence data file is loaded into shared memory before the
# master process writes its pid file, which can take tens of seconds. The
# five second wait in Test::Nginx::waitforfile is too short for that, so
# extend it to two minutes. The loop still returns as soon as the pid file
# appears, so small data files are unaffected.
{
	no warnings 'redefine';
	*Test::Nginx::waitforfile = sub {
		my ($self, $file, $pid) = @_;
		my $exited;

		for (1 .. 1200) {
			return 1 if -e $file;
			return 0 if $exited;
			$exited = waitpid($pid, WNOHANG) != 0 if $pid;
			select undef, undef, undef, 0.1;
		}

		return undef;
	};
}

sub read_example($) {
	my ($name) = @_;
	open my $fh, '<', '../../examples/mixed/' . $name or die "Can't open file $name: $!";
	read $fh, my $content, -s $fh;
	close $fh;

	return $content;
}

# Both data files are required for this test. Skip when either is not
# present.
my $hashFilePath = $ENV{TEST_FILE_PATH};
my $ipiFilePath = $ENV{TEST_FILE_PATH_IPI};
if (!defined $hashFilePath || !-e $hashFilePath) {
	plan(skip_all => 'No device detection data file. Set TEST_FILE_PATH.');
}
if (!defined $ipiFilePath || !-e $ipiFilePath) {
	plan(skip_all => 'No IP intelligence data file. Set TEST_FILE_PATH_IPI.');
}

my $t = Test::Nginx->new()->has(qw/http/)->plan(5);

my $t_file = read_example('gettingStarted.conf');
# Remove the documentation block
$t_file =~ s/\/\*\*.+\*\//''/gmse;
# Replace all variable place holders.
$t_file =~ s/%%DAEMON_MODE%%/'off'/gmse;
$t_file =~ s/%%MODULE_PATH%%/$ENV{TEST_MODULE_PATH}/gmse;
# A static build links the module into the Nginx binary, so omit the
# load_module directive, which would fail to open a non existent shared
# object.
$t_file =~ s/^.*load_module.*
//mg if $ENV{TEST_NGINX_STATIC};
$t_file =~ s/%%FILE_PATH%%/$hashFilePath/gmse;
$t_file =~ s/%%FILE_PATH_IPI%%/$ipiFilePath/gmse;
$t->write_file_expand('nginx.conf', $t_file);

$t->write_file('mixed', '');

$t->run();

sub get_with_ua {
	my ($uri, $ua) = @_;
	return http(<<EOF);
HEAD $uri HTTP/1.1
Host: localhost
Connection: close
User-Agent: $ua

EOF
}

###############################################################################
# Constants.
###############################################################################

my $mobileUserAgent = 'Mozilla/5.0 (iPhone; CPU iPhone OS 7_1 like Mac OS X) AppleWebKit/537.51.2 (KHTML, like Gecko) Version/7.0 Mobile/11D167 Safari/9537.53';

# An IP address with a stable, well known network registration which is
# expected to be present in all IP intelligence data files. The test runs
# on a local machine where the client IP address reported by Nginx is a
# loopback address, so this address is passed in the client_ip query
# string argument to override the reported client IP address for the
# match.
my $knownIp = '212.58.224.22';

###############################################################################
# Test mixed/gettingStarted.conf example.
###############################################################################

my $r = get_with_ua('/mixed?client_ip=' . $knownIp, $mobileUserAgent);
like($r, qr/x-mobile: True/, 'Mobile match (device detection)');
like($r, qr/x-asn: .+/, 'ASN name set (client IP)');
like($r, qr/x-asn-query: .+/, 'ASN name set (query IP)');
unlike($r, qr/x-asn-query: (NoMatch)?\r/, 'Query IP matched');

# The client IP address text held by nginx is not null terminated, so a
# parse failure here indicates the module has passed it to the engine
# without taking a null terminated copy.
unlike($t->read_file('error.log'), qr/IP address format is incorrect/,
	'Client IP address parsed');

###############################################################################
