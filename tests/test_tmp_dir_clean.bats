#!/bin/env bats

setup() {
    load setup_common
    load setup_container_daemon
}

teardown() {
    load teardown_container_daemon
}

@test "Checking that the /tmp directory is cleaned when CLEAN_TMP_DIR=1..." {
    exec_container_daemon sh -c "touch /tmp/test_file"

    restart_container_daemon

    exec_container_daemon sh -c "[ ! -f /tmp/test_file ]"
}
