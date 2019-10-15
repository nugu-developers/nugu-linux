#!/bin/bash
#
# Shell script to run sample application with token.
#
# Environment variables
#   - NUGU_TOKEN
#   - NUGU_REFRESH_TOKEN
#
# You can overwrite the settings with the above environment variables.
#

if [[ -z "${NUGU_CONFIG_PATH}" ]]; then
  CONFIG_PATH='/var/lib/nugu'
else
  CONFIG_PATH=${NUGU_CONFIG_PATH}
fi

echo "Configuration path = ${CONFIG_PATH}"

if [ ! -d "${CONFIG_PATH}" ]; then
  echo "Invalid configuration path ${CONFIG_PATH}"
  exit 1
fi

CONFIG_PATH_AUTH=${CONFIG_PATH}/nugu-auth.json

if [ ! -e "${CONFIG_PATH_AUTH}" ]; then
  echo "Can't find configuration file ${CONFIG_PATH_AUTH}"
  exit 1
fi

if [ "$#" -eq 0 ]; then
  echo "Usage: $0 {application_path}"
  exit 1
fi

if [[ -z "${NUGU_REFRESH_TOKEN}" ]]; then
  # OAuth refresh token
  refresh_token=`cat ${CONFIG_PATH_AUTH} | jq -rM '.refresh_token'`
  if [[ "${refresh_token}" != "null" ]]; then
    export NUGU_REFRESH_TOKEN=$refresh_token
  fi
fi

if [[ -z "${NUGU_TOKEN}" ]]; then
  # OAuth token
  token=`cat ${CONFIG_PATH_AUTH} | jq -rM '.access_token'`
  if [[ "${token}" != "null" ]]; then
    export NUGU_TOKEN=$token
  fi
fi

if [[ -z "${NUGU_TOKEN}" ]]; then
  echo "Token is empty. Please setup using companion and oob application."
  exit 1
fi

echo "NUGU_TOKEN = $NUGU_TOKEN"
echo "NUGU_REFRESH_TOKEN = $NUGU_REFRESH_TOKEN"

echo "Execute: $@"
"$@"
