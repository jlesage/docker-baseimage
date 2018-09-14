#!/bin/env bats

DOCKER_EXTRA_OPTS="-e CLEAN_TMP_DIR=0"

setup() {
    load setup_common
    load setup_container_daemon
}

teardown() {
    load teardown_container_daemon
}

@test "Checking that the /tmp directory is not cleaned when CLEAN_TMP_DIR=0..." {
    exec_container_daemon sh -c "touch /tmp/test_file"

    restart_container_daemon

    exec_container_daemon sh -c "[ -f /tmp/test_file ]"
}
