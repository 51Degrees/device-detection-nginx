#!/usr/bin/perl

# Tests that the 51Degrees IP Intelligence module refuses a data file it cannot
# use, with a message naming the file, rather than starting without one.

###############################################################################

use warnings;
use strict;
use File::Temp qw/ tempdir /;
use Test::More;

BEGIN { use FindBin; chdir($FindBin::Bin); }

###############################################################################

select STDERR; $| = 1;
select STDOUT; $| = 1;

# The files that are refused are made from a good data file.
my $goodFile = $ENV{TEST_FILE_PATH_IPI};
if (!defined $goodFile || !-e $goodFile) {
	plan(skip_all => 'No IP intelligence data file. Set TEST_FILE_PATH_IPI.');
}

# Where the header of a data file holds its minor version and profile offset
# shift, as 32 bit integers.
my $minorVersionOffset = 4;
my $shiftOffset = 54;

my $nginx = $ENV{TEST_NGINX_BINARY};
my $dir = tempdir('nginx-ipi-XXXXXXXXXX', TMPDIR => 1, CLEANUP => 1);
my $loadModule = $ENV{TEST_NGINX_STATIC} ? '' :
	"load_module $ENV{TEST_MODULE_PATH}modules/ngx_http_51D_ipi_module.so;";

sub read_binary {
	my ($path) = @_;
	open my $fh, '<:raw', $path or die "Can't open $path: $!";
	local $/;
	return scalar <$fh>;
}

sub write_binary {
	my ($name, $content) = @_;
	my $path = "$dir/$name";
	open my $fh, '>:raw', $path or die "Can't open $path: $!";
	print $fh $content;
	close $fh or die "Can't close $path: $!";
	return $path;
}

sub with_int32 {
	my ($content, $offset, $value) = @_;
	substr($content, $offset, 4) = pack('l<', $value);
	return $content;
}

# Runs NGINX with the arguments given, stopping it if it stays up. Returns its
# exit code, which is 124 if it had to be stopped and over 127 after a signal,
# and everything it wrote.
sub run_nginx {
	my ($file, @args) = @_;
	open my $conf, '>', "$dir/nginx.conf" or die "Can't open nginx.conf: $!";
	print $conf <<EOF;
$loadModule
daemon off;
master_process off;
pid $dir/nginx.pid;
error_log $dir/error.log;
events {
}
http {
	access_log off;
	51D_file_path_ipi $file;
	server {
		listen 127.0.0.1:8097;
	}
}
EOF
	close $conf or die "Can't close nginx.conf: $!";
	unlink "$dir/error.log";

	my $command = qq{timeout 60 "$nginx" -p "$dir/" -c "$dir/nginx.conf" } .
		qq{-e "$dir/error.log" @args 2>&1};
	my $output = `$command`;
	my $code = $? >> 8;
	if (-e "$dir/error.log") {
		$output .= read_binary("$dir/error.log");
	}
	return ($code, $output);
}

sub expect_refused {
	my ($name, $file, $mode, @args) = @_;
	my ($code, $output) = run_nginx($file, @args);
	ok($code != 0 && $code != 124 && $code < 128,
		"$name: $mode fails without a signal")
		or diag("Exit code $code\n$output");
	unlike($output, qr/ERROR: AddressSanitizer/,
		"$name: $mode has no memory error");
	like($output, qr/51Degrees.*\Q$file\E/,
		"$name: $mode logs a message naming the file");
}

my $good = read_binary($goodFile);
my @files = (
	['Missing file', "$dir/missing.ipi"],
	['Empty file', write_binary('empty.ipi', '')],
	['Too short for a header',
		write_binary('short.ipi', substr($good, 0, 16))],
	['Cut short', write_binary('cut.ipi', substr($good, 0, 4096))],
	['Minor version it cannot read',
		write_binary('version.ipi',
			with_int32($good, $minorVersionOffset, 5))],
	['Shift it cannot read',
		write_binary('shift.ipi', with_int32($good, $shiftOffset, 99))],
);

plan(tests => 1 + scalar(@files) * 6);

# A good file passes with the same configuration, so that the failures below
# come from the data file.
my ($goodCode, $goodOutput) = run_nginx($goodFile, '-t');
is($goodCode, 0, 'Good file: nginx -t passes') or diag($goodOutput);

foreach my $entry (@files) {
	my ($name, $file) = @$entry;
	expect_refused($name, $file, 'nginx -t', '-t');
	expect_refused($name, $file, 'starting');
}
