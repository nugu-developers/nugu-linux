#!/usr/bin/python

from flask import Flask, request, redirect, session, json, url_for, make_response
from flask.json import jsonify
from requests_oauthlib import OAuth2Session
import os

os.environ['OAUTHLIB_INSECURE_TRANSPORT'] = '1'

PORT = 8080

# Receive from companion application
if 'NUGU_CONFIG_PATH' in os.environ:
    CONFIG_PATH = os.environ['NUGU_CONFIG_PATH']
else:
    CONFIG_PATH = '/var/lib/nugu'

print 'Configuration path = %s' % CONFIG_PATH

if os.path.isdir(CONFIG_PATH):
    print 'path exist'
else:
    print 'create directory'
    os.makedirs(CONFIG_PATH)

CONFIG_PATH_AUTH = CONFIG_PATH + '/nugu-auth.json'
DEFAULT_JSON_AUTH = """{
    "access_token": "",
    "expires_in": "",
    "refresh_token": "",
    "token_type": ""
}
"""

# OAuth
CONFIG_PATH_OAUTH = CONFIG_PATH + '/nugu-oauth.json'
DEFAULT_JSON_OAUTH = """{
    "pocId": "",
    "clientId": "",
    "clientSecret": "",
    "deviceSerialNumber": ""
}
"""

authorization_base_url = 'https://api.sktnugu.com/v1/auth/oauth/authorize'
token_url = 'https://api.sktnugu.com/v1/auth/oauth/token'
redirect_uri = 'http://lvh.me:8080/callback'

app = Flask(__name__)
app.secret_key = "test"


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
        pocId = request.form['pocId']
        clientId = request.form['clientId']
        clientSecret = request.form['clientSecret']
        deviceSerialNumber = request.form['serial']
        buf = """{{
    "pocId": "{pocId}",
    "clientId": "{clientId}",
    "clientSecret": "{clientSecret}",
    "deviceSerialNumber": "{serial}"
}}""".format(pocId=pocId, clientId=clientId, clientSecret=clientSecret, serial=deviceSerialNumber)
        with open(CONFIG_PATH_OAUTH, 'w') as writer:
            writer.write(buf)
        return redirect(url_for('index'))


@app.route('/logout')
def logout():
    session.clear()
    return redirect(url_for('index'))


@app.route('/')
def index():
    with open(CONFIG_PATH_OAUTH, 'r') as reader:
        oauthinfo = json.load(reader)

    if 'pocId' in oauthinfo:
        pocId = oauthinfo['pocId']
    else:
        pocId = ''

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
        if 'refresh_token' in token:
            refresh_token=token['refresh_token']
        else:
            refresh_token=''

        loginForm = """
<table width=100%>
    <tr>
        <th>access_token</th>
        <td width=100%><input style="width:100%;" type=text name=clientId value="{access_token}"/></td>
    </tr>
    <tr>
        <th>expires_at</th>
        <td width=100%><input style="width:100%;" type=text name=clientId value="{expires_at}"/></td>
    </tr>
    <tr>
        <th>expires_in</th>
        <td width=100%><input style="width:100%;" type=text name=clientId value="{expires_in}"/></td>
    </tr>
    <tr>
        <th>refresh_token</th>
        <td width=100%><input style="width:100%;" type=text name=clientId value="{refresh_token}"/></td>
    </tr>
    <tr>
        <th>token_type</th>
        <td width=100%><input style="width:100%;" type=text name=clientId value="{token_type}"/></td>
    </tr>
</table>
<p align="center"><a href="/logout">Logout</a></p>
""".format(access_token=token['access_token'], expires_at=token['expires_at'], expires_in=token['expires_in'], refresh_token=refresh_token, token_type=token['token_type'])
    else:
        loginForm = """
<p align="center"><a href="/login">Get OAuth2 token</a></p>
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
        <form method=post action='/oauth'>
            <fieldset>
                <legend>OAuth2 information</legend>
                <table width=100%>
                    <tr>
                        <th>poc_id</th>
                        <td width=100%><input style="width:100%;" type=text name=pocId value="{pocId}"/></td>
                    </tr>
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
                            <a href="/">Reload</a> <input type=submit value="Save"/>
                        </td>
                    </tr>
                </table>
            </fieldset>
        </form>
    </body>
</html>""".format(loginForm=loginForm, pocId=pocId, clientId=clientId, clientSecret=clientSecret, serial=serial)


@app.route('/login')
def login():
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

    if 'oauth_state' in session:
        oauth_state = session['oauth_state']
        print 'state=%s' % oauth_state
    else:
        oauth_state = ''
        print 'can not found oauth_state'

    nugu = OAuth2Session(
        clientId, state=oauth_state, redirect_uri=redirect_uri)
    token = nugu.fetch_token(token_url, client_secret=clientSecret,
                                authorization_response=request.url)

    print token
    token_json = json.dumps(token)
    print token_json
    with open(CONFIG_PATH_AUTH, 'w') as writer:
        writer.write(token_json)

    session['token'] = token

    return redirect(url_for('index'))


if __name__ == '__main__':
    if not os.path.exists(CONFIG_PATH_AUTH):
        print "Create default %s" % CONFIG_PATH_AUTH
        with open(CONFIG_PATH_AUTH, 'w') as writer:
            writer.write(DEFAULT_JSON_AUTH)

    if not os.path.exists(CONFIG_PATH_OAUTH):
        print "Create default %s" % CONFIG_PATH_OAUTH
        with open(CONFIG_PATH_OAUTH, 'w') as writer:
            writer.write(DEFAULT_JSON_OAUTH)

    print "Please connect to %d port." % PORT

    app.run(host='0.0.0.0', port=PORT)
