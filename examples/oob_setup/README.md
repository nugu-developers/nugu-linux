# OOB Setup example

This **OOB**(Out-of-Box) program is an example for exchanging authentication information using NUGU OAuth2. It provides only basic functions such as token exchange, but does not support automatic token refresh.

This program is just an example and is not implemented for security or other exceptions. So **DO NOT USE** this sample in production environment.

## Install

The nugu_oob program is included in the `libnugu-examples` debian package.

    apt install libnugu-examples

## Get OAuth2 token

First, please open the [http://localhost:8080](http://localhost:8080) using web browser.

Next, fill the information like `client_id`, `client_secret`, etc. in the `OAuth2 information` form and press the `Save` button to save it.

Now, click the [Get OAuth2 token](http://localhost:8080/login) link to proceed with authentication.

## How to get the OAuth2 token in your program

If the OAuth2 authentication process is successful, the token information is displayed on the web browser screen and the information is also stored in the /var/lib/nugu/nugu-auth.json file as shown below.

    {
        "access_token": "",
        "expires_in": "",
        "refresh_token": "",
        "token_type": ""
    }

Therefore, you can parse and use token information in the file as shown below.

    cat /var/lib/nugu/nugu-auth.json | jq '.access_token' -r
