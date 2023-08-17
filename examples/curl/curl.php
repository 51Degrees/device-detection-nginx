<?php

$c = curl_init();

curl_setopt($c, CURLOPT_URL, 'http://127.0.0.1:8080/hash');
curl_setopt($c, CURLOPT_HEADER, TRUE);
curl_setopt($c, CURLOPT_NOBODY, TRUE);
curl_setopt($c, CURLOPT_HTTPHEADER, array(
		'User-Agent: Mozilla/5.0 (Windows NT 10.0; Win64; x64) AppleWebKit/537.36 (KHTML, like Gecko) Chrome/114.0.0.0 Safari/537.36 Edg/114.0.1823.41' ,
         'Sec-CH-UA: "Not.A/Brand";v="8","Chromium";v="114","Microsoft Edge";v="114"' ,
         'Sec-CH-UA-Arch: x86' ,
         'Sec-CH-UA-Bitness: 64' ,
         'Sec-CH-UA-Full-Version: 114.0.5735.110' ,
         'Sec-CH-UA-Full-Version-List: "Not.A/Brand";v="8.0.0.0","Chromium";v="114.0.5735.110","Microsoft Edge";v="114.0.1823.41"',
        'Sec-CH-UA-Mobile: ?0',
        'Sec-CH-UA-Model: ',
        'Sec-CH-UA-Platform: Windows' ,
        'Sec-CH-UA-Platform-Version: 15.0.0'
));

$response = curl_exec($c);

curl_close($c);
