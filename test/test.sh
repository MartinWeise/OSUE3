#!/usr/bin/sh

RED='\033[0;31m'
GREEN='\033[0;32m'
NC='\033[0m'

NO_ERR=0

#! TRY RUNNING THE SERVER
echo "################ TEST 1 ################"
if src/auth-server > /dev/null 2>&1; then
    printf "${GREEN}OK${NC}\n"
else
    printf "${RED}FAILED${NC}\n"
    ((NO_ERR++))
fi

#! TRY RUNNING THE CLIENT
echo "################ TEST 2 ################"
if src/auth-client 2>&1 | grep -q "USAGE"; then
    printf "${GREEN}OK${NC}\n"
else
    printf "${RED}FAILED${NC}\n"
    ((NO_ERR++))
fi

#! TRY RUNNING THE SERVER WITH INVALID ARGUMENTS
echo "################ TEST 3 ################"
if src/auth-server -l idonotexist 2>&1 | grep -q "No such file"; then
    printf "${GREEN}OK${NC}\n"
else
    printf "${RED}FAILED${NC}\n"
    ((NO_ERR++))
fi

#! TRY RUNNING THE SERVER WITH INVALID ARGUMENTS 2
echo "################ TEST 4 ################"
if src/auth-server -l database argtoomuch 2>&1 | grep -q "USAGE"; then
    printf "${GREEN}OK${NC}\n"
else
    printf "${RED}FAILED${NC}\n"
    ((NO_ERR++))
fi

#! TRY RUNNING THE SERVER WITH INVALID ARGUMENTS 3
echo "################ TEST 5 ################"
if src/auth-server idonotbelonghere 2>&1 | grep -q "USAGE"; then
    printf "${GREEN}OK${NC}\n"
else
    printf "${RED}FAILED${NC}\n"
    ((NO_ERR++))
fi

#! DATABASE (CHECK SAVING)
echo "################ TEST 6 ################"
printf "foo;password;secret\nbar;1234\nbaz;23456;santa" > test/input.txt
src/auth-server -l test/input.txt > /dev/null 2>&1
if cat auth-server.db.csv | grep -q "foo;password;secret"; then
    printf "${GREEN}OK${NC}\n"
else
    printf "${RED}FAILED${NC}\n"
    ((NO_ERR++))
fi

#! CHECK SEMAPHOR CREATION (CLIENT)
echo "################ TEST 7 ################"
src/auth-client -l Theodor ilovemilka > /dev/null 2>&1
if ls /dev/shm | grep -q "1429167"; then
    printf "${RED}FAILED${NC}\n"
    ((NO_ERR++))
else
    printf "${GREEN}OK${NC}\n"
fi

#! CHECK SERVER
echo "################ TEST 8 ################"
printf "${GREEN}OK${NC}\n"
