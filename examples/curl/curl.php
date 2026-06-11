<?php

/**
 * Example requests against the mixed example configuration
 * (examples/mixed/gettingStarted.conf) which loads both the device
 * detection and IP intelligence modules and serves the /mixed location.
 *
 * The device detection match uses the User-Agent and Sec-CH-UA-* headers.
 * The IP intelligence match uses the client IP address, and the IP address
 * supplied in the client_ip query argument. The query argument is needed
 * when running on a local machine, where the client IP address reported
 * by Nginx is a loopback address with no useful IP intelligence
 * associated with it.
 *
 * To use these requests with the single engine examples, change the path
 * to /hash (examples/hash/gettingStarted.conf) or /ipi
 * (examples/ipi/gettingStarted.conf).
 */

const HOST = '127.0.0.1:8080';

/**
 * Make a HEAD request to the URL with the headers provided and print the
 * response headers.
 *
 * @param string $title printed before the response.
 * @param string $url the URL to request.
 * @param array $headers the request headers to set.
 */
function request(string $title, string $url, array $headers = [])
{
    echo "=== $title ===\n";

    $c = curl_init();
    curl_setopt($c, CURLOPT_URL, $url);
    curl_setopt($c, CURLOPT_HEADER, true);
    curl_setopt($c, CURLOPT_NOBODY, true);
    curl_setopt($c, CURLOPT_RETURNTRANSFER, true);
    if (count($headers) > 0) {
        curl_setopt($c, CURLOPT_HTTPHEADER, $headers);
    }

    $response = curl_exec($c);
    if ($response === false) {
        echo 'Request failed: ' . curl_error($c) . "\n";
    } else {
        echo $response;
    }

    curl_close($c);
}

request(
    'Device detection: desktop browser with client hints',
    'http://' . HOST . '/mixed',
    [
        'User-Agent: Mozilla/5.0 (Windows NT 10.0; Win64; x64) AppleWebKit/537.36 (KHTML, like Gecko) Chrome/114.0.0.0 Safari/537.36 Edg/114.0.1823.41',
        'Sec-CH-UA: "Not.A/Brand";v="8","Chromium";v="114","Microsoft Edge";v="114"',
        'Sec-CH-UA-Arch: x86',
        'Sec-CH-UA-Bitness: 64',
        'Sec-CH-UA-Full-Version: 114.0.5735.110',
        'Sec-CH-UA-Full-Version-List: "Not.A/Brand";v="8.0.0.0","Chromium";v="114.0.5735.110","Microsoft Edge";v="114.0.1823.41"',
        'Sec-CH-UA-Mobile: ?0',
        'Sec-CH-UA-Model: ',
        'Sec-CH-UA-Platform: Windows',
        'Sec-CH-UA-Platform-Version: 15.0.0',
    ]
);

request(
    'IP intelligence: IP address in the client_ip query argument',
    'http://' . HOST . '/mixed?client_ip=212.58.224.22'
);

request(
    'Both engines: mobile browser and an IP address together',
    'http://' . HOST . '/mixed?client_ip=212.58.224.22',
    [
        'User-Agent: Mozilla/5.0 (iPhone; CPU iPhone OS 7_1 like Mac OS X) AppleWebKit/537.51.2 (KHTML, like Gecko) Version/7.0 Mobile/11D167 Safari/9537.53',
    ]
);
