#! /usr/bin/env bash

copy_out_in() {
    if [ "${#}" -ne 1 ]; then
        echo "Usage: copy_out_in mountpoint" >&2
        exit 0
    fi
    local mountpoint="${1}"
    if [ ! -d "${mountpoint}" ]; then
        echo "mountpoint must be a directory" >&2
        exit 1
    fi
    local test_file="/tmp/16B.txt"
    cat /dev/urandom | base64 | head -c 16 > "${test_file}" 
    cp "${test_file}" "${mountpoint}"
    local destination_file="${mountpoint}/$(basename ${test_file})"
    diff "${test_file}" "${destination_file}"
    local diff_return_code="${?}"
    if [ "${diff_return_code}" -ne 0 ]; then
        return "${diff_return_code}"
    fi
    return 0
}

copy_in_out() {
    if [ "${#}" -ne 1 ]; then
        echo "Usage: copy_out_in mountpoint" >&2
        exit 0
    fi
    local mountpoint="${1}"
    if [ ! -d "${mountpoint}" ]; then
        echo "mountpoint must be a directory" >&2
        exit 1
    fi
    local test_file="/tmp/16B.txt"
    cat /dev/urandom | base64 | head -c 16 > "${test_file}" 
    cp "${test_file}" "${mountpoint}"
    local destination_file="${mountpoint}/$(basename ${test_file})"
    local back_file="/tmp/back.txt"
    cp "${test_file}" "${back_file}"
    diff "${test_file}" "${back_file}"
    local diff_return_code="${?}"
    if [ "${diff_return_code}" -ne 0 ]; then
        return "${diff_return_code}"
    fi
    return 0
}

copy_in_in() {
    if [ "${#}" -ne 1 ]; then
        echo "Usage: copy_out_in mountpoint" >&2
        exit 0
    fi
    local mountpoint="${1}"
    if [ ! -d "${mountpoint}" ]; then
        echo "mountpoint must be a directory" >&2
        exit 1
    fi
    local test_file="/tmp/16B.txt"
    cat /dev/urandom | base64 | head -c 16 > "${test_file}" 
    cp "${test_file}" "${mountpoint}"
    local destination_file="${mountpoint}/$(basename ${test_file})"
    local other_file="${mountpoint}/copy.txt"
    cp "${test_file}" "${other_file}"
    diff "${test_file}" "${other_file}"
    local diff_return_code="${?}"
    if [ "${diff_return_code}" -ne 0 ]; then
        return "${diff_return_code}"
    fi
    return 0
}

main() {
    if [ "${#}" -ne 1 ]; then
        echo "Usage: ${0} mountpoint" >&2
        exit 0
    fi
    local mountpoint="${1}"
    if [ ! -d "${mountpoint}" ]; then
        echo "mountpoint must be a directory" >&2
        exit 1
    fi
    echo -ne "copy out in:\t"
    copy_out_in "${mountpoint}"
    if [ "${?}" -eq 0 ]; then
        echo "OK"
    else
        echo "KO"
    fi

    copy_in_in "${mountpoint}"
    if [ "${?}" -eq 0 ]; then
        echo "OK"
    else
        echo "KO"
    fi

    echo -ne "copy in out:\t"
    copy_in_out "${mountpoint}"
    if [ "${?}" -eq 0 ]; then
        echo "OK"
    else
        echo "KO"
    fi
}

main "$@"