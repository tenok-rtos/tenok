#!/usr/bin/env bash
#
# The libraries a board of the STM32F7 family is built against.
#
# They are fetched rather than carried: between them they are the better part
# of a hundred megabytes, and none of them is patched here, so what the tree
# needs to remember about them is only where they came from and which commit.
# Each is left as a plain directory -- nothing here is a submodule.

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

# Directory, repository, commit. The licence of each is recorded beside them in
# lib/F7-VENDOR-NOTE.txt; all four sit well with the licence of this tree and
# none of them needs a repository of its own to keep it apart.
LIBS=(
    "lib/cmsis-core|https://github.com/STMicroelectronics/cmsis_core.git|afc5ca6af0a49232fde7eb4548dd0962d119ce14"
    "lib/cmsis-device-f7|https://github.com/STMicroelectronics/cmsis_device_f7.git|2352e888e821aa0f4fe549bd5ea81d29c67a3222"
    "lib/stm32f7xx-ll|https://github.com/STMicroelectronics/stm32f7xx_hal_driver.git|910731a0babb10f4908827de90921e559a825e08"
    "lib/lwip|https://github.com/lwip-tcpip/lwip.git|77dcd25a72509eb83f72b033d219b1d40cd8eb95"
)

function download_one
{
    local dir=$1 repo=$2 commit=$3

    if [ -d "${dir}" ] && [ -n "$(ls -A "${dir}" 2>/dev/null)" ]; then
        printf "%-24s already there" "$(basename "${dir}")"
        OK
        return 0
    fi

    printf "%-24s fetching" "$(basename "${dir}")"
    rm -rf "${dir}"
    ASSERT git clone -q "${repo}" "${dir}"
    ASSERT git -C "${dir}" checkout -q "${commit}"

    # The history is of no use here and is most of the size
    rm -rf "${dir}/.git"
    OK
}

for entry in "${LIBS[@]}"; do
    IFS='|' read -r dir repo commit <<< "${entry}"
    download_one "${dir}" "${repo}" "${commit}"
done
