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

BEGIN { use FindBin; chdir($FindBin::Bin); }

use lib 'nginx-tests/lib';
use Test::Nginx;
use URI::Escape;

###############################################################################

select STDERR; $| = 1;
select STDOUT; $| = 1;

my $n = 28;
my $t_lite = 1;

# The Lite data file version does not contains properties that can be used
# to test the overrides feature. Please consider to use one that have at least
# ScreenPixelsWidth and ScreenPixelsWidthJavascript properties
if (scalar(@ARGV) > 0 && index($ARGV[0], "51Degrees-Lite") == -1) {
	$n += 8;
	$t_lite = 0;
}

my $t = Test::Nginx->new()->has(qw/http/)->plan($n)
	->write_file_expand('nginx.conf', <<'EOF');

daemon off;

%%TEST_GLOBALS%%
events {
}

http {
	%%TEST_GLOBALS_HTTP%%

	51D_value_separator ^sep^;
	51D_performance_profile IN_MEMORY;
	51D_drift 1;
	51D_difference 1;
	51D_allow_unmatched on;
	51D_use_performance_graph on;
	51D_use_predictive_graph on;

	51D_match_ua x-main-ismobile-single IsMobile;
	51D_match_all x-main-ismobile-all IsMobile;

	51D_set_resp_headers on;

	server {
		listen       127.0.0.1:8081;
		server_name  localhost1;

		location /clienthints {
			# Do nothings
		}

		location /addchheaders {
			51D_set_resp_headers off;
			add_header Accept-CH Test1,Test2,Test3;
		}
	}

    server {
        listen       127.0.0.1:8080;
        server_name  localhost;
		
		51D_match_ua x-server-ismobile-single IsMobile;
		51D_match_all x-server-ismobile-all IsMobile;
		51D_set_resp_headers off;

        location /single {
			51D_match_single x-ismobile IsMobile;
            add_header x-ismobile $http_x_ismobile;
        }

        location /ua {
			51D_match_ua x-ismobile IsMobile;
            add_header x-ismobile $http_x_ismobile;
        }

        location /ua_uach {
			51D_match_client_hints x-ismobile IsMobile;
            add_header x-ismobile $http_x_ismobile;
        }

		location /all {
			51D_match_all x-ismobile IsMobile;
			51D_match_all x-browsername BrowserName;
			add_header x-ismobile $http_x_ismobile;
			add_header x-browsername $http_x_browsername;
		}

		location /metrics {
			51D_match_ua x-iterations Iterations;
			51D_match_ua x-drift Drift;
			51D_match_ua x-difference Difference;
			51D_match_ua x-method Method;
			51D_match_ua x-useragents UserAgents;
			51D_match_ua x-matchednodes MatchedNodes;
			51D_match_ua x-deviceid DeviceId;
			add_header x-metrics $http_x_Iterations,$http_x_drift,$http_x_difference,$http_x_method,$http_x_matchedNodes,$http_x_deviceid;
			add_header x-metrics-ua $http_x_useragents;
		}

		location /more-properties {
			51D_match_ua x-more-properties Ismobile,BrowserName;
			add_header x-more-properties $http_x_more_properties;
		}

		location /non-property {
			51D_match_ua x-non-property thisisnotarealpropertyname;
			add_header x-non-property $http_x_non_property;
		}

        location /redirect {
             51D_match_ua x-ismobile IsMobile;
             if ($http_x_ismobile ~ "True") {
              return 301 https://mobilesite;
             }
        }

		location /locations {
			51D_match_ua x-ismobile-single IsMobile;
			51D_match_all x-ismobile-all IsMobile;
			add_header x-location-ismobile $http_x_ismobile_single$http_x_ismobile_all;
			add_header x-server-ismobile $http_x_server_ismobile_single$http_x_server_ismobile_all;
			add_header x-main-ismobile $http_x_main_ismobile_single$http_x_main_ismobile_all;
		}

		location /variable {
			51D_match_ua x-ismobile-from-var IsMobile $arg_ua;
			51D_match_ua x-ismobile-from-wrong-var IsMobile $ua;
			add_header x-ismobile-from-var $http_x_ismobile_from_var;
			add_header x-ismobile-from-wrong-var $http_x_ismobile_from_wrong_var;
		}

		location /overrides {
			51D_match_all x-javascript ScreenPixelsWidthJavascript;
			51D_match_all x-screenpixelswidth ScreenPixelsWidth;
			add_header x-screenpixelswidth $http_x_screenpixelswidth;
		}

		location /clienthints {
			51D_set_resp_headers on;
		}

		location /clienthintsoff {
			51D_set_resp_headers off;
		}

		location /clienthintsnone {
			# Do nothing
		}

		location /51D-single.js {
			51D_get_javascript_single JavascriptHardwareProfile;
		}

		location /51D-single-query.js {
			51D_get_javascript_single JavascriptHardwareProfile $arg_ua;
		}

		location /51D-all.js {
			51D_set_resp_headers on;
			51D_get_javascript_all JavascriptHardwareProfile;
		}

		location /51D-non-property.js {
			51D_set_resp_headers on;
			51D_get_javascript_single thisisnotarealpropertyname;
		}

		location /51D-chua.js {
			51D_set_resp_headers on;
			51D_get_javascript_all ScreenPixelsWidthJavascript;
		}

		location /addchheaders {
			51D_set_resp_headers on;
			proxy_pass http://127.0.0.1:8081/addchheaders;
		}
    }
}

EOF

$t->write_file('single', '');
$t->write_file('ua', '');
$t->write_file('ua_uach', '');
$t->write_file('all', '');
$t->write_file('metrics', '');
$t->write_file('more-properties', '');
$t->write_file('non-property', '');
$t->write_file('redirect', '');
$t->write_file('locations', '');
$t->write_file('variable', '');
$t->write_file('overrides', '');
$t->write_file('clienthints', '');
$t->write_file('clienthintsoff', '');
$t->write_file('clienthintsnone', '');
$t->write_file('addchheaders', '');
$t->write_file('51D-chua.js', '');

$t->run();

sub get_content_with_ua {
	my ($uri, $ua) = @_;
	return http(<<EOF);
GET $uri HTTP/1.1
Host: localhost
Connection: close
User-Agent: $ua

EOF
}

sub get_with_ua_common {
	my ($host, $uri, $ua) = @_;
	return http(<<EOF);
HEAD $uri HTTP/1.1
Host: $host
Connection: close
User-Agent: $ua

EOF
}

sub get_with_ua {
	return get_with_ua_common('localhost', @_);
}

sub get_with_ua_cookie {
	my ($uri, $ua, $cookie) = @_;
	return http(<<EOF);
HEAD $uri HTTP/1.1
Host: localhost
Connection: close
Cookie: $cookie
User-Agent: $ua

EOF
}

sub get_with_opera_header {
	my ($uri, $ua) = @_;
	return http(<<EOF);
HEAD $uri HTTP/1.1
Host: localhost
Connection: close
X-OperaMini-Phone-UA: $ua

EOF
}

###############################################################################
# Constants.
###############################################################################

my $mobileUserAgent = 'Mozilla/5.0 (iPhone; CPU iPhone OS 7_1 like Mac OS X) AppleWebKit/537.51.2 (KHTML, like Gecko) Version/7.0 Mobile/11D167 Safari/9537.53';
my $desktopUserAgent = 'Mozilla/5.0 (Windows NT 6.3; WOW64; rv:41.0) Gecko/20100101 Firefox/41.0';
my $chrome89UserAgent = 'Mozilla/5.0 (Windows NT 10.0; Win64; x64) AppleWebKit/537.36 (KHTML, like Gecko) Chrome/89.0.4389.114 Safari/537.36';

###############################################################################
# Test matching scenarios.
###############################################################################

my $r;

# Single User-Agent (old)
$r = get_with_ua('/single', $desktopUserAgent);
like($r, qr/x-ismobile: False/, 'Desktop match (single User-Agent)');
$r = get_with_ua('/single', $mobileUserAgent);
like($r, qr/x-ismobile: True/, 'Mobile match (single User-Agent)');

# Single User-Agent (new)
$r = get_with_ua('/ua', $desktopUserAgent);
like($r, qr/x-ismobile: False/, 'Desktop match (ua User-Agent)');
$r = get_with_ua('/ua', $mobileUserAgent);
like($r, qr/x-ismobile: True/, 'Mobile match (ua User-Agent)');

# User-Agent and Client-Hints
$r = get_with_ua('/ua_uach', $desktopUserAgent);
like($r, qr/x-ismobile: False/, 'Desktop match (ua_uach User-Agent)');
$r = get_with_ua('/ua_uach', $mobileUserAgent);
like($r, qr/x-ismobile: True/, 'Mobile match (ua_uach User-Agent)');

# Multiple HTTP headers.
$r = get_with_ua('/all', $desktopUserAgent);
like($r, qr/x-ismobile: False/, 'Desktop match (all HTTP headers)');
$r = get_with_ua('/all', $mobileUserAgent);
like($r, qr/x-ismobile: True/, 'Mobile match (all HTTP headers)');

# No User-Agent.
$r = http_get('/single');
like($r, qr/x-ismobile: (True|False)/, 'Handle missing User-Agent');

###############################################################################
# Test properties.
###############################################################################

# Metrics
$r = get_with_ua('/metrics', $mobileUserAgent);
like($r, qr/x-metrics: \d+,\d+,\d+,(PERFORMANCE|COMBINED|PREDICTIVE|NONE),\d+,\d+-\d+-\d+-\d+/, 'Return metrics');
like($r, qr/x-metrics-ua: (?!NoMatch$).*/, 'Return matched user agents');

# Multiple properties.
$r = http_get('/more-properties');
like($r, qr/x-more-properties: \w+\^sep\^\w+/, 'Concatinate multiple property values');

# Non-existent property.
$r = http_get('/non-property');
like($r, qr/x-non-property: NoMatch/, 'Handles non-existent property');

###############################################################################
# test the reload functionality.
###############################################################################

# Reload Nginx.
$t->reload();

###############################################################################
# Test config blocks.
###############################################################################

# Match desktop in server and location blocks.
$r = get_with_ua('/locations', $desktopUserAgent);
like($r, qr/x-server-ismobile: FalseFalse/, 'Desktop match in server block');
like($r, qr/x-location-ismobile: FalseFalse/, 'Desktop match in location block');
like($r, qr/x-main-ismobile: FalseFalse/, 'Desktop match in main block');

# Match mobile in server and location blocks.
$r = get_with_ua('/locations', $mobileUserAgent);
like($r, qr/x-server-ismobile: TrueTrue/, 'Mobile match in server block');
like($r, qr/x-location-ismobile: TrueTrue/, 'Mobile match in location block');
like($r, qr/x-main-ismobile: TrueTrue/, 'Mobile match in main block');

###############################################################################
# Test multiple HTTP header functionality.
###############################################################################

# Match mobile User-Agent in Opera header.
$r = get_with_opera_header('/all', $mobileUserAgent);
like($r, qr/x-ismobile: True/, 'Opera mobile header');

# Match desktop User-Agent in Opera header.
$r = get_with_opera_header('/all', $desktopUserAgent);
like($r, qr/x-ismobile: False/, 'Opera desktop header');

###############################################################################
# Test extended matching from query String.
###############################################################################

# Match with a parameter passed from the config.
$r = get_with_ua('/variable?ua='.uri_escape($mobileUserAgent), $desktopUserAgent);
like($r, qr/x-ismobile-from-var: True/, 'Match single from parameter');
like($r, qr/x-ismobile-from-wrong-var: False/, 'Match single from parameter with wrong name');

# Match all with a parameter passed from the config.
$r = get_with_ua('/all?User-Agent='.uri_escape($mobileUserAgent), $desktopUserAgent);
like($r, qr/x-ismobile: True/, 'Match all from parameter');

###############################################################################
# Test overrides
###############################################################################

# In order to test the overrides feature, a data file that contains at least
# ScreenPixelsWidth and ScreenPixelsWidthJavascript properties are required
if (!$t_lite) {
	# Match with override value from cookie.
	$r = get_with_ua_cookie('/overrides', $desktopUserAgent, '51D_ScreenPixelsWidth=20');
	like($r, qr/x-screenpixelswidth: 20/, 'Screen Pixels Width value was not overriden by value in cookie.');

	# Match with override value from query string.
	$r = get_with_ua('/overrides?51D_ScreenPixelsWidth=20', $desktopUserAgent);
	like($r, qr/x-screenpixelswidth: 20/, 'Screen Pixels Width value was not overriden by value in query string.');
}

###############################################################################
# Test response headers
###############################################################################

# TODO: Enable and UACH is supported
if (!$t_lite && 0) {
	# Set Client Hints response headers locally
	$r = get_with_ua('/clienthints', $chrome89UserAgent);
	like($r, qr/Accept-CH: SEC-CH-[a-zA-Z-]+(,SEC-CH-[a-zA-Z-]+)*/, 'Set Client Hints response header locally.');

	# Set Client Hints response headers locally
	$r = get_with_ua_common('localhost1', '/clienthints', $chrome89UserAgent);
	like($r, qr/Accept-CH: SEC-CH-[a-zA-Z-]+(,SEC-CH-[a-zA-Z-]+)*/, 'Set Client Hints response header locally.');

	# Set No Client Hints response headers
	$r = get_with_ua('/clienthintsoff', $chrome89UserAgent);
	unlike($r, qr/Accept-CH: SEC-CH-[a-zA-Z-]+(,SEC-CH-[a-zA-Z-]+)*/, 'Set No Client Hints response header locally.');

	# No Client Hints response headers set. Relies on Main and Server settings.
	$r = get_with_ua('/clienthintsnone', $chrome89UserAgent);
	unlike($r, qr/Accept-CH: SEC-CH-[a-zA-Z-]+(,SEC-CH-[a-zA-Z-]+)*/, 'Set Client Hints response header locally.');

	# Append Client Hints to already create headers.
	$r = get_with_ua_common('localhost', '/addchheaders', $chrome89UserAgent);
	like($r, qr/Accept-CH: Test1,Test2,Test3,SEC-CH-[a-zA-Z-]+(,SEC-CH-[a-zA-Z-]+)*/, 'Append Client Hints to existing headers.');
}

###############################################################################
# Test CDN javascript
###############################################################################

# In order to test the Javascript feature, JavascriptHardwareProfile is required.
if (!$t_lite) {
	# Javascript using single User-Agent header.
	$r = get_content_with_ua('/51D-single.js', $mobileUserAgent);
	like($r, qr/getProfileId/, 'Javascript response content using single User-Agent header.');

	# Javascript using single User-Agent from query string.
	$r = get_content_with_ua('/51D-single-query.js?ua='.uri_escape($mobileUserAgent), $desktopUserAgent);
	like($r, qr/getProfileId/, 'Javascript response content using variable.');

	# Javascript using all evidence.
	$r = get_content_with_ua('/51D-all.js', $mobileUserAgent);
	like($r, qr/getProfileId/, 'Javascript response content using all evidence.');

	# Javascript using query.
	$r = get_content_with_ua('/51D-all.js?User-Agent='.uri_escape($mobileUserAgent), $desktopUserAgent);
	like($r, qr/getProfileId/, 'Javascript response content using all evidence including query string.');

	# Javascript using property that is not supported.
	$r = get_content_with_ua('/51D-single.js', $desktopUserAgent);
	like($r, qr/51Degrees Javascript not available/, 'Javascript response content body from non supported user agent');

	# Javascript using property with Client Hints response headers on.
	$r = get_content_with_ua('/51D-chua.js', $chrome89UserAgent);
	like($r, qr/document\.cookie.*51D_ScreenPixelsWidth.*/, 'Javascript response content to get ScreenPixelsWidth.');
	# TODO: Enable once UACH is supported
	if (0) {
		like($r, qr/Accept-CH: SEC-CH-[a-zA-Z-]+(,SEC-CH-[a-zA-Z-]+)*/, 'Set Client Hints response header locally.');
	}
}

# Javascript using non property.
$r = get_content_with_ua('/51D-non-property.js', $mobileUserAgent);
like($r, qr/51Degrees Javascript not available/, 'Javascript response content body from non supported property');

# Javascript missing User-Agent.
$r = http_get('/51D-single.js');
like($r, qr/51Degrees Javascript not available/, 'Javascript response content body with missing User-Agent');

###############################################################################
# Test use cases.
###############################################################################

# Redirect mobile using the IsMobile property.
$r = get_with_ua('/redirect', $mobileUserAgent);
like($r, qr/301 Moved Permanently/, 'Redirected to mobile');

# Redirect desktop using the IsMobile property.
$r = get_with_ua('/redirect', $desktopUserAgent);
unlike($r, qr/301 Moved Permanently/, 'Didn\'t redirect for desktop');

###############################################################################

# Print out warnings at the end for user attention
if ($t_lite) {
	print "WARNING: Overrides and Javascript feature test will not be run \n";
	print "         as the Lite version of data file does not contains the \n";
	print "         required properties. Please consider to obtain a data \n";
	print "         file that have at least the ScreenPixelsWidth, \n";
	print "         ScreenPixelsWidthJavascript and \n";
	print "         JavascriptHardwareProfile supported.\n";
}
