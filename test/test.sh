#!/usr/bin/sh

cd src

#! TRY RUNNING THE SERVER
echo "################ TEST 1 ################"
if ./auth-server; then
    echo "OK"
else
    echo "FAILED"
fi

#! TRY RUNNING THE CLIENT
echo "################ TEST 2 ################"
if ./auth-client 2>&1 | grep -q "USAGE"; then
    echo "OK"
else
    echo "FAILED"
fi