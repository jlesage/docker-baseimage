#!/bin/env bats

setup() {
    load setup_common
    load setup_container_daemon
}

teardown() {
    load teardown_container_daemon
}

@test "Checking that environment variable can be installed..." {
    exec_container_daemon sh -c 'echo -n TEST_VALUE1 > /etc/cont-env.d/TEST_VAR1'
    exec_container_daemon sh -c 'echo TEST_VALUE2 > /etc/cont-env.d/TEST_VAR2'
    exec_container_daemon sh -c 'echo TEST_VALUE3 > /etc/cont-env.d/TEST_VAR3'
    exec_container_daemon sh -c 'echo TEST_VALUE3a >> /etc/cont-env.d/TEST_VAR3'

    restart_container_daemon

    exec_container_daemon sh -c '[ -f /var/run/cont-env/TEST_VAR1 ]'
    exec_container_daemon sh -c '[ -f /var/run/cont-env/TEST_VAR2 ]'
    exec_container_daemon sh -c '[ -f /var/run/cont-env/TEST_VAR3 ]'

    exec_container_daemon /usr/bin/with-contenv sh -c '[ "$TEST_VAR1" = "TEST_VALUE1" ]'
    exec_container_daemon /usr/bin/with-contenv sh -c '[ "$TEST_VAR2" = "TEST_VALUE2" ]'
    exec_container_daemon /usr/bin/with-contenv sh -c '[ "$TEST_VAR3" = "TEST_VALUE3" ]'
}
