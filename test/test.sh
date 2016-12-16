#!/usr/bin/sh

#! TRY RUNNING THE SERVER
echo "################ TEST 1 ################"
if src/auth-server; then
    echo "OK"
else
    echo "FAILED"
fi

#! TRY RUNNING THE CLIENT
echo "################ TEST 2 ################"
if src/auth-client 2>&1 | grep -q "USAGE"; then
    echo "OK"
else
    echo "FAILED"
fi

#! TRY RUNNING THE SERVER WITH INVALID ARGUMENTS
echo "################ TEST 3 ################"
if src/auth-server -l idonotexist 2>&1 | grep -q "No such file"; then
    echo "OK"
else
    echo "FAILED"
fi

#! TRY RUNNING THE SERVER WITH INVALID ARGUMENTS 2
echo "################ TEST 4 ################"
if src/auth-server -l database argtoomuch 2>&1 | grep -q "USAGE"; then
    echo "OK"
else
    echo "FAILED"
fi

#! TRY RUNNING THE SERVER WITH INVALID ARGUMENTS 3
echo "################ TEST 5 ################"
if src/auth-server idonotbelonghere 2>&1 | grep -q "USAGE"; then
    echo "OK"
else
    echo "FAILED"
fi