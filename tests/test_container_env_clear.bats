#!/bin/env bats

setup() {
    load setup_common
    load setup_container_daemon
}

teardown() {
    load teardown_container_daemon
}

@test "Checking that the container's environment is properly cleared after a restart..." {
    exec_container_daemon sh -c "echo 1 > /var/run/cont-env/TEST_VAR"

    restart_container_daemon

    exec_container_daemon sh -c "[ ! -f /var/run/cont-env/TEST_VAR ]"
}
