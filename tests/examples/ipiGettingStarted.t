#!/usr/bin/perl

# (C) Sergey Kandaurov
# (C) Maxim Dounin
# (C) Nginx, Inc.

# Tests for 51Degrees IP Intelligence module.

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
	open my $fh, '<', '../../examples/ipi/' . $name or die "Can't open file $name: $!";
	read $fh, my $content, -s $fh;
	close $fh;

	return $content;
}

# The IP intelligence data file is optional for the test suite. Skip the
# tests if it is not present.
my $ipiFilePath = $ENV{TEST_FILE_PATH_IPI};
if (!defined $ipiFilePath || !-e $ipiFilePath) {
	plan(skip_all => 'No IP intelligence data file. Set TEST_FILE_PATH_IPI.');
}

my $t = Test::Nginx->new()->has(qw/http/)->plan(3);

my $t_file = read_example('gettingStarted.conf');
# Remove the documentation block
$t_file =~ s/\/\*\*.+\*\//''/gmse;
# Replace all variable place holders.
$t_file =~ s/%%DAEMON_MODE%%/'off'/gmse;
$t_file =~ s/%%MODULE_PATH%%/$ENV{TEST_MODULE_PATH}/gmse;
$t_file =~ s/%%FILE_PATH_IPI%%/$ipiFilePath/gmse;
$t->write_file_expand('nginx.conf', $t_file);

$t->write_file('ipi', '');

$t->run();

sub get_uri {
	my ($uri) = @_;
	return http(<<EOF);
HEAD $uri HTTP/1.1
Host: localhost
Connection: close

EOF
}

###############################################################################
# Constants.
###############################################################################

# An IP address with a stable, well known network registration which is
# expected to be present in all IP intelligence data files. The test runs
# on a local machine where the client IP address reported by Nginx is a
# loopback address, so this address is passed in the client_ip query
# string argument to override the reported client IP address for the
# match.
my $knownIp = '212.58.224.22';

###############################################################################
# Test ipi/gettingStarted.conf example.
###############################################################################

my $r = get_uri('/ipi?client_ip=' . $knownIp);
like($r, qr/x-asn: .+/, 'ASN name set (client IP)');
like($r, qr/x-asn-query: .+/, 'ASN name set (query IP)');
unlike($r, qr/x-asn-query: (NoMatch)?\r/, 'Query IP matched');

###############################################################################
