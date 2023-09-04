#!/usr/bin/env bash

function ASSERT
{
    $*
    RES=$?
    if [ $RES -ne 0 ]; then
        echo 'Assert failed: "' $* '"'
        exit $RES
    fi
}

PASS_COLOR='\e[32;01m'
NO_COLOR='\e[0m'
function OK
{
    printf " [ ${PASS_COLOR} OK ${NO_COLOR} ]\n"
}

function download_examples
{
    rm -rf /tmp/tenok
    pushd /tmp/
    ASSERT git clone https://github.com/shengwen-tw/tenok.git -b blob
    popd
    mkdir -p rom/
    cp -r /tmp/tenok/rom/* rom/
    cp -r /tmp/tenok/msg/* msg/
}

download_examples && OK
