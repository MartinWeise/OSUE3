#!/usr/bin/sh

echo "################ TEST 1 ################"
if src/auth-server | grep -q 'file'; then
    echo "OK"
else
    echo "FAILED"
fi