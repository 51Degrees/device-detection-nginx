#!/usr/bin/perl

# Tests that the 51Degrees Hash module answers each request from a match of
# that request's own evidence.

###############################################################################

use warnings;
use strict;
use Test::More;

BEGIN { use FindBin; chdir($FindBin::Bin); }

use lib 'nginx-tests/lib';
use Test::Nginx;
use URI::Escape;

###############################################################################

select STDERR; $| = 1;
select STDOUT; $| = 1;

# The Lite data file has no SetHeader properties, so it sets no response
# headers, and it has no Javascript properties.
my $t_lite = !(scalar(@ARGV) > 0 && index($ARGV[0], "51Degrees-Lite") == -1);

# No block above the locations has a match directive, so a location without
# one makes no match of its own.
my $t = Test::Nginx->new()->has(qw/http proxy/)->plan($t_lite ? 7 : 23)
	->write_file_expand('nginx.conf', <<'EOF');

daemon off;

%%TEST_GLOBALS%%
events {
}

http {
	%%TEST_GLOBALS_HTTP%%

	server {
		listen       127.0.0.1:8080;
		server_name  localhost;

		location /variable {
			51D_match_ua x-ismobile IsMobile $arg_ua;
			add_header x-ismobile $http_x_ismobile;
			add_header x-worker $pid;
		}

		location /variable-all {
			51D_match_all x-all-ismobile IsMobile;
			51D_match_ua x-ismobile IsMobile $arg_ua;
			add_header x-all-ismobile $http_x_all_ismobile;
			add_header x-ismobile $http_x_ismobile;
		}

		location /match {
			51D_set_resp_headers on;
			51D_match_all x-browser BrowserName;
			add_header x-browser $http_x_browser;
			add_header x-worker $pid;
		}

		location /nomatch {
			51D_set_resp_headers on;
			add_header x-worker $pid;
		}

		location /proxy {
			51D_set_resp_headers on;
			51D_match_all x-browser BrowserName;
			add_header x-browser $http_x_browser;
			proxy_set_header User-Agent $http_x_upstream_user_agent;
			proxy_pass http://127.0.0.1:8080/upstream;
		}

		location /upstream {
			51D_match_ua x-upstream-browser BrowserName;
			add_header x-upstream-browser $http_x_upstream_browser;
		}

		location /append {
			51D_set_resp_headers on;
			51D_match_all x-browser BrowserName;
			proxy_pass http://127.0.0.1:8080/accept-ch;
		}

		location /accept-ch {
			add_header Accept-CH Test1,Test2,Test3;
		}

		location /js {
			51D_set_resp_headers on;
			51D_get_javascript_all ScreenPixelsWidthJavascript;
		}

		location /js-single {
			51D_get_javascript_single JavascriptHardwareProfile $arg_ua;
		}
	}
}

EOF

$t->write_file('variable', '');
$t->write_file('variable-all', '');
$t->write_file('match', '');
$t->write_file('nomatch', '');
$t->write_file('upstream', '');
$t->write_file('accept-ch', '');
$t->write_file('js', '');
$t->write_file('js-single', '');

$t->run();

sub get {
	my ($uri, %headers) = @_;
	my $lines = join('', map { "$_: $headers{$_}\n" } sort keys %headers);
	return http("GET $uri HTTP/1.1\nHost: localhost\nConnection: close\n"
		. $lines . "\n");
}

# The value of the Accept-CH header, or undef if there is none.
sub accept_ch {
	my ($r) = @_;
	my ($value) = $r =~ /^Accept-CH: ?(.*?)\r$/mi;
	return $value;
}

sub worker {
	my ($r) = @_;
	my ($pid) = $r =~ /^x-worker: (\d+)\r$/m;
	return $pid;
}

###############################################################################
# Constants.
###############################################################################

my $firefox = 'Mozilla/5.0 (Windows NT 6.3; WOW64; rv:41.0) Gecko/20100101 Firefox/41.0';
my $chrome = 'Mozilla/5.0 (Windows NT 10.0; Win64; x64) AppleWebKit/537.36 (KHTML, like Gecko) Chrome/89.0.4389.114 Safari/537.36';
my $iphone = 'Mozilla/5.0 (iPhone; CPU iPhone OS 7_1 like Mac OS X) AppleWebKit/537.51.2 (KHTML, like Gecko) Version/7.0 Mobile/11D167 Safari/9537.53';

###############################################################################
# Test the User-Agent a variable gives.
###############################################################################

# A missing argument and a blank one give the variable the same empty value,
# which supplies no User-Agent, so the request's own is used. Each request
# without the argument follows a match on the worker which had the other
# answer.
my $r = get('/variable?ua='.uri_escape($iphone), 'User-Agent' => $firefox);
like($r, qr/^x-ismobile: True\r$/m, 'A User-Agent in a variable is matched');
my $worker = worker($r);
$r = get('/variable', 'User-Agent' => $firefox);
is(worker($r), $worker, 'The next request reaches the same worker');
like($r, qr/^x-ismobile: False\r$/m,
	'A missing variable gives the User-Agent header');

get('/variable?ua='.uri_escape($firefox), 'User-Agent' => $iphone);
$r = get('/variable?ua=', 'User-Agent' => $iphone);
like($r, qr/^x-ismobile: True\r$/m,
	'A blank variable gives the User-Agent header');

get('/variable?ua='.uri_escape($iphone), 'User-Agent' => $firefox);
$r = get('/variable');
like($r, qr/^x-ismobile: NoMatch\r$/m,
	'A missing variable and User-Agent header give no match');

# The match of all the evidence reads the User-Agent argument, which is not
# the variable's argument.
$r = get('/variable-all?User-Agent='.uri_escape($iphone),
	'User-Agent' => $firefox);
like($r, qr/^x-all-ismobile: True\r$/m,
	'The match of all evidence reads the User-Agent argument');
like($r, qr/^x-ismobile: False\r$/m,
	'A missing variable is not given the match of all evidence');

###############################################################################
# Test the Accept-CH header from a request's own match.
###############################################################################

if (!$t_lite) {
	# Firefox sends no client hints, so every SetHeader property of its match
	# is Unknown.
	$r = get('/match', 'User-Agent' => $firefox);
	is(accept_ch($r), undef, 'Firefox is sent no Accept-CH');

	# The hints are split from the SetHeader values, and some are in more
	# than one of them.
	$r = get('/match', 'User-Agent' => $chrome);
	my $chromeHints = accept_ch($r);
	like($chromeHints, qr/^[A-Za-z][A-Za-z0-9-]*(,[A-Za-z][A-Za-z0-9-]*)*$/,
		'Chrome is sent Accept-CH as a list of hints');
	my @hints = split(/,/, $chromeHints // '');
	my %seen;
	is(scalar(grep { !$seen{$_}++ } @hints), scalar(@hints),
		'Chrome is sent each hint once');
	is(scalar(grep { $_ eq 'Unknown' } @hints), 0,
		'Chrome is sent no Unknown');

	# Nothing is matched without a User-Agent, so no SetHeader property has a
	# value.
	$r = get('/match');
	is(accept_ch($r), undef, 'A request with no User-Agent is sent no Accept-CH');

###############################################################################
# Test that response headers come from the request's own match.
###############################################################################

	# A location with no match directive, straight after a match on the same
	# worker.
	$r = get('/match', 'User-Agent' => $chrome);
	$worker = worker($r);
	$r = get('/nomatch', 'User-Agent' => $firefox);
	is(worker($r), $worker, 'Firefox reaches the worker that matched Chrome');
	is(accept_ch($r), undef,
		'Firefox is sent no Accept-CH after a match of Chrome');

	$r = get('/match', 'User-Agent' => $firefox);
	$worker = worker($r);
	$r = get('/nomatch', 'User-Agent' => $chrome);
	is(worker($r), $worker, 'Chrome reaches the worker that matched Firefox');
	is(accept_ch($r), $chromeHints,
		'Chrome is sent its own Accept-CH after a match of Firefox');

	# The upstream request is matched after this request's match, and before
	# its response headers are set.
	$r = get('/proxy', 'User-Agent' => $chrome,
		'X-Upstream-User-Agent' => $firefox);
	like($r, qr/^x-upstream-browser: Firefox\r$/m,
		'The upstream request is matched as Firefox');
	is(accept_ch($r), $chromeHints,
		'Chrome is sent its own Accept-CH after an upstream match of Firefox');

###############################################################################
# Test adding to an Accept-CH header from the upstream.
###############################################################################

	$r = get('/append', 'User-Agent' => $firefox);
	is(accept_ch($r), 'Test1,Test2,Test3',
		'Nothing is added to the upstream Accept-CH for Firefox');

	$r = get('/append', 'User-Agent' => $chrome);
	is(accept_ch($r), "Test1,Test2,Test3,$chromeHints",
		'The hints for Chrome are added to the upstream Accept-CH');

###############################################################################
# Test that a script reads its own request.
###############################################################################

	# The override in the cookie shows that the script has already run. Each
	# script follows a match on the worker which had the other answer.
	my $cookie = '51D_ScreenPixelsWidth=12345';
	get('/match', 'User-Agent' => $chrome);
	$r = get('/js', 'User-Agent' => $chrome, 'Cookie' => $cookie);
	like($r, qr/51Degrees Javascript not available/,
		'A script is not served when its request has the override');

	get('/match', 'User-Agent' => $chrome, 'Cookie' => $cookie);
	$r = get('/js', 'User-Agent' => $chrome);
	like($r, qr/document\.cookie.*51D_ScreenPixelsWidth/,
		'A script is served when its request has no override');

	# A script for a User-Agent from a variable with no value.
	$r = get('/js-single', 'User-Agent' => $iphone);
	like($r, qr/getProfileId/,
		'A script with a missing variable is for the User-Agent header');
}

###############################################################################

if ($t_lite) {
	print "WARNING: Response header and script tests will not be run as the \n";
	print "         Lite data file has no SetHeader or Javascript \n";
	print "         properties. Please consider to obtain a data file that \n";
	print "         has the SetHeaderBrowserAccept-CH, \n";
	print "         SetHeaderHardwareAccept-CH, SetHeaderPlatformAccept-CH, \n";
	print "         ScreenPixelsWidthJavascript and JavascriptHardwareProfile \n";
	print "         properties.\n";
}
