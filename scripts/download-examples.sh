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
    # A directory of its own, so that the checkout the caller is standing in
    # is never the one that gets cloned over and removed
    local scratch
    scratch=$(mktemp -d) || return 1
    trap 'rm -rf "${scratch}"' RETURN

    ASSERT git clone -q https://github.com/tenok-rtos/tenok.git -b blob "${scratch}/blob"
    mkdir -p rom/ msg/
    cp -r "${scratch}"/blob/rom/* rom/
    cp -r "${scratch}"/blob/msg/* msg/
}

download_examples && OK
