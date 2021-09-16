#!/usr/bin/python

from flask import Flask, request, redirect, session, json, url_for, make_response
from flask.json import jsonify
from requests_oauthlib import OAuth2Session
import os
import sys
import logging

log = logging.getLogger('requests_oauthlib')
log.addHandler(logging.StreamHandler(sys.stdout))
log.setLevel(logging.DEBUG)

os.environ['OAUTHLIB_INSECURE_TRANSPORT'] = '1'

PORT = 8080

if 'NUGU_CONFIG_PATH' in os.environ:
    CONFIG_PATH = os.environ['NUGU_CONFIG_PATH']
else:
    CONFIG_PATH = '/var/lib/nugu'

if 'NUGU_OAUTH2_URL' in os.environ:
    OAUTH2_URL = os.environ['NUGU_OAUTH2_URL']
else:
    OAUTH2_URL = 'https://api.sktnugu.com/'

CONFIG_PATH_AUTH = CONFIG_PATH + '/nugu-auth.json'
DEFAULT_JSON_AUTH = """{
    "access_token": "",
    "expires_in": "",
    "refresh_token": "",
    "token_type": ""
}
"""

CONFIG_PATH_OAUTH = CONFIG_PATH + '/nugu-oauth.json'
DEFAULT_JSON_OAUTH = """{
    "clientId": "",
    "clientSecret": "",
    "deviceSerialNumber": ""
}
"""

authorization_base_url = OAUTH2_URL + 'v1/auth/oauth/authorize'
token_url = OAUTH2_URL + 'v1/auth/oauth/token'
revoke_url = OAUTH2_URL + 'v1/auth/oauth/revoke'
redirect_uri = 'http://localhost:8080/callback'

app = Flask(__name__)
app.secret_key = 'test'

def refresh_token():
    print '\n\033[1mRefresh the access_token\033[0m'
    with open(CONFIG_PATH_AUTH, 'r') as reader:
        authinfo = json.load(reader)

    with open(CONFIG_PATH_OAUTH, 'r') as reader:
        oauthinfo = json.load(reader)

    if 'refresh_token' not in authinfo or authinfo['refresh_token'] == '':
        print '\n\033[1;93mrefresh_token is not exist\033[0m'
        return

    extra = {
        'client_id': oauthinfo['clientId'],
        'client_secret': oauthinfo['clientSecret'],
        'refresh_token': authinfo['refresh_token'],
        'data': {
            'deviceSerialNumber': oauthinfo['deviceSerialNumber']
        }
    }

    nugu = OAuth2Session(oauthinfo['clientId'], token=authinfo['access_token'])
    token = nugu.refresh_token(token_url, **extra)

    token_json = json.dumps(token, indent=4)
    print token_json

    with open(CONFIG_PATH_AUTH, 'w') as writer:
        writer.write(token_json)

    session['token'] = token

@app.route('/auth', methods=['GET', 'PUT'])
def auth():
    if request.method == 'GET':
        with open(CONFIG_PATH_AUTH, 'r') as reader:
            resp = make_response(reader.read())
            resp.mimetype = 'application/json'
            return resp

    elif request.method == 'PUT':
        with open(CONFIG_PATH_AUTH, 'w') as writer:
            writer.write(request.data)
        return jsonify(success=True)


@app.route('/oauth', methods=['GET', 'PUT', 'POST'])
def oauth():
    if request.method == 'GET':
        with open(CONFIG_PATH_OAUTH, 'r') as reader:
            resp = make_response(reader.read())
            resp.mimetype = 'application/json'
            return resp

    elif request.method == 'PUT':
        with open(CONFIG_PATH_OAUTH, 'w') as writer:
            writer.write(request.data)
        return jsonify(success=True)

    elif request.method == 'POST':
        clientId = request.form['clientId']
        clientSecret = request.form['clientSecret']
        deviceSerialNumber = request.form['serial']
        buf = """{{
    "clientId": "{clientId}",
    "clientSecret": "{clientSecret}",
    "deviceSerialNumber": "{serial}"
}}""".format(clientId=clientId, clientSecret=clientSecret, serial=deviceSerialNumber)
        with open(CONFIG_PATH_OAUTH, 'w') as writer:
            writer.write(buf)
        return redirect(url_for('index'))

@app.route('/refresh', methods=['GET'])
def refresh():
    refresh_token()
    return redirect(url_for('index'))

@app.route('/revoke', methods=['POST'])
def revoke():
    print '\n\033[1mRevoke the access_token\033[0m'
    data = {
        'token' : request.form['token'],
        'client_id':  request.form['clientId'],
        'client_secret':  request.form['clientSecret'],
    }
    print data

    headers = {
        "Accept": "application/json",
        "Content-Type": ("application/x-www-form-urlencoded;charset=UTF-8"),
    }
    print headers

    nugu = OAuth2Session(request.form['clientId'])
    resp = nugu.post(revoke_url, data=data, headers=headers)
    print resp

    if resp.status_code == 200:
        with open(CONFIG_PATH_AUTH, 'w') as writer:
            writer.write(DEFAULT_JSON_AUTH)

        session.clear()

    return """<!DOCTYPE HTML>
<html>
    <head>
    </head>
    <body>
        <h1>Revoke</h1>
        <table>
        <tr>
            <td>Response code</td>
            <td>{code}</td>
        </tr>
        <tr>
            <td>Response message</td>
            <td>${msg}</td>
        </tr>
        </table>
        <p>
        <a href="/">Back to main</a>
        </p>
    </body>
</html>""".format(code=resp.status_code, msg=resp.text)

@app.route('/logout')
def logout():
    session.clear()
    return redirect(url_for('index'))

@app.route('/')
def index():
    with open(CONFIG_PATH_OAUTH, 'r') as reader:
        oauthinfo = json.load(reader)
    with open(CONFIG_PATH_AUTH, 'r') as reader:
        authinfo = json.load(reader)

    if 'access_token' in authinfo:
        issuedToken = authinfo['access_token']
    else:
        issuedToken = ''

    if 'clientId' in oauthinfo:
        clientId = oauthinfo['clientId']
    else:
        clientId = ''

    if 'clientSecret' in oauthinfo:
        clientSecret = oauthinfo['clientSecret']
    else:
        clientSecret = ''

    if 'deviceSerialNumber' in oauthinfo:
        serial = oauthinfo['deviceSerialNumber']
    else:
        serial = ''

    if 'token' in session:
        token = session['token']
        issuedToken = token['access_token']
        if 'expires_at' in token:
            expires_at = token['expires_at']
        else:
            expires_at = ''
        if 'refresh_token' in token:
            refresh_token = token['refresh_token']
            refresh_token_action = ' or <a href="/refresh">Refresh the token</a>'
        else:
            refresh_token = ''
            refresh_token_action = ''
        if 'scope' in token:
            scope = token['scope']
        else:
            scope = ''

        loginForm = """
<table width=100%>
    <tr>
        <th>access_token</th>
        <td width=100%><input disabled style="width:100%;" type=text name=clientId value="{access_token}"/></td>
    </tr>
    <tr>
        <th>expires_at</th>
        <td width=100%><input disabled style="width:100%;" type=text name=clientId value="{expires_at}"/></td>
    </tr>
    <tr>
        <th>expires_in</th>
        <td width=100%><input disabled style="width:100%;" type=text name=clientId value="{expires_in}"/></td>
    </tr>
    <tr>
        <th>refresh_token</th>
        <td width=100%><input disabled style="width:100%;" type=text name=clientId value="{refresh_token}"/></td>
    </tr>
    <tr>
        <th>token_type</th>
        <td width=100%><input disabled style="width:100%;" type=text name=clientId value="{token_type}"/></td>
    </tr>
    <tr>
        <th>scope</th>
        <td width=100%><input disabled type=text style="width:100%;" name=scope value="{scope}"/></td>
    </tr>

</table>
<p align="center"><a href="/logout">Logout</a>{refresh_token_action}</p>
""".format(access_token=token['access_token'], expires_at=expires_at, expires_in=token['expires_in'], refresh_token=refresh_token, token_type=token['token_type'], scope=scope, refresh_token_action=refresh_token_action)
    else:
        loginForm = """
<p align="left">
    <a href="/loginAuthorizationCode">Get OAuth2 token (Authorization code)</a><br/>
    <a href="/loginClientCredentials">Get OAuth2 token (Client credentials)</a><br/>
    <a href="/refresh">Refresh OAuth2 token</a>
</p>
"""
    return """<!DOCTYPE HTML>
<html>
    <head>
    </head>
    <body>
        <fieldset>
            <legend>User</legend>
            {loginForm}
        </fieldset>
        <br/>
        <form method=post action='/oauth'>
            <fieldset>
                <legend>OAuth2 information</legend>
                <table width=100%>
                    <tr>
                        <th>client_id</th>
                        <td width=100%><input style="width:100%;" type=text name=clientId value="{clientId}"/></td>
                    </tr>
                    <tr>
                        <th>client_secret</th>
                        <td width=100%><input type=text style="width:100%;" name=clientSecret value="{clientSecret}"/></td>
                    </tr>
                    <tr>
                        <th>device serial</th>
                        <td width=100%><input type=text style="width:100%;" name=serial value="{serial}"/></td>
                    </tr>
                    <tr>
                        <td colspan=2 align=center>
                            <button type="button" onclick="location.href='/'">Reload</button> <input type=submit value="Save"/>
                        </td>
                    </tr>
                </table>
            </fieldset>
        </form>
        <br/>
        <form method=post action='/revoke'>
            <fieldset>
                <legend>Revoke</legend>
                <table width=100%>
                    <tr>
                        <th>access_token</th>
                        <td width=100%><input style="width:100%;" type=text name=token value="{issuedToken}"/></td>
                    </tr>
                    <tr>
                        <th>client_id</th>
                        <td width=100%><input style="width:100%;" type=text name=clientId value="{clientId}"/></td>
                    </tr>
                    <tr>
                        <th>client_secret</th>
                        <td width=100%><input style="width:100%;" type=text name=clientSecret value="{clientSecret}"/></td>
                    </tr>
                    <tr>
                        <td colspan=2 align=center>
                            <input type=submit value="Revoke"/>
                        </td>
                    </tr>
                </table>
            </fieldset>
        </form>
    </body>
</html>""".format(loginForm=loginForm, issuedToken=issuedToken, clientId=clientId, clientSecret=clientSecret, serial=serial)

@app.route('/loginAuthorizationCode')
def login_authorization_code():
    with open(CONFIG_PATH_OAUTH, 'r') as reader:
        oauthinfo = json.load(reader)

    clientId = oauthinfo['clientId']
    serial = oauthinfo['deviceSerialNumber']

    nugu = OAuth2Session(clientId, redirect_uri=redirect_uri)
    authorization_url, state = nugu.authorization_url(
        authorization_base_url, data='{{"deviceSerialNumber":"{serial}"}}'.format(serial=serial))

    # State is used to prevent CSRF(Cross Site Request Forgery), keep this for later.
    print 'state=%s' % state

    session['oauth_state'] = state
    return redirect(authorization_url)

@app.route('/callback')
def callback():
    with open(CONFIG_PATH_OAUTH, 'r') as reader:
        oauthinfo = json.load(reader)

    clientId = oauthinfo['clientId']
    clientSecret = oauthinfo['clientSecret']
    serial = oauthinfo['deviceSerialNumber']

    if 'oauth_state' in session:
        oauth_state = session['oauth_state']
        print 'state=%s' % oauth_state
    else:
        oauth_state = ''
        print 'can not found oauth_state'

    nugu = OAuth2Session(
        clientId, state=oauth_state, redirect_uri=redirect_uri)
    token = nugu.fetch_token(token_url, client_secret=clientSecret,
                                authorization_response=request.url,
                                data='{{"deviceSerialNumber":"{serial}"}}'.format(serial=serial))

    token_json = json.dumps(token, indent=4)
    print token_json
    with open(CONFIG_PATH_AUTH, 'w') as writer:
        writer.write(token_json)

    session['token'] = token

    return redirect(url_for('index'))

@app.route('/loginClientCredentials')
def login_client_credentials():
    with open(CONFIG_PATH_OAUTH, 'r') as reader:
        oauthinfo = json.load(reader)

    clientId = oauthinfo['clientId']
    data = {
        'grant_type': 'client_credentials',
        'client_id': clientId,
        'client_secret': oauthinfo['clientSecret'],
        'data': {
            'deviceSerialNumber': oauthinfo['deviceSerialNumber']
        }
    }
    headers = {
        'Accept': 'application/json',
        'Content-Type': ('application/x-www-form-urlencoded;charset=UTF-8'),
    }

    nugu = OAuth2Session(clientId)
    resp = nugu.post(token_url, data=data, headers=headers)
    print resp

    if resp.status_code == 200:
        token_json = json.dumps(resp.json(), indent=4)
        print token_json

        with open(CONFIG_PATH_AUTH, 'w') as writer:
            writer.write(token_json)

        session['token'] = resp.json()

    return redirect(url_for('index'))

if __name__ == '__main__':
    print '\033[1mNUGU Out-Of-Box sample server for authentication\033[0m'

    if not os.path.isdir(CONFIG_PATH):
        print ' - Create Configuration directory %s' % CONFIG_PATH
        os.makedirs(CONFIG_PATH)

    print ' - OAuth2 url = %s' % OAUTH2_URL

    print ' - OAuth2 configuration path = %s' % CONFIG_PATH_OAUTH
    if not os.path.exists(CONFIG_PATH_OAUTH):
        print '   - Create default %s' % CONFIG_PATH_OAUTH
        with open(CONFIG_PATH_OAUTH, 'w') as writer:
            writer.write(DEFAULT_JSON_OAUTH)

    print ' - Authentication path = %s' % CONFIG_PATH_AUTH
    if not os.path.exists(CONFIG_PATH_AUTH):
        print '   - Create default %s' % CONFIG_PATH_AUTH
        with open(CONFIG_PATH_AUTH, 'w') as writer:
            writer.write(DEFAULT_JSON_AUTH)

    for arg in sys.argv:
        if arg == '-r':
            refresh_token()
            sys.exit(0)

    print '\n\033[1mPlease connect to %d port.\033[0m' % PORT

    app.run(host='0.0.0.0', port=PORT)
