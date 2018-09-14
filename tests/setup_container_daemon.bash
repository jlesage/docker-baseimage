
exec_container_daemon() {
    [ -n "$CONTAINER_DAEMON_ID" ]
    docker exec "$CONTAINER_DAEMON_ID" "$@"
}

getlog_container_daemon() {
    [ -n "$CONTAINER_DAEMON_ID" ]
    docker logs "$CONTAINER_DAEMON_ID"
}

wait_for_container_daemon() {
    echo "Waiting for the docker container daemon to be ready..."
    TIMEOUT=60
    while [ "$TIMEOUT" -ne 0 ]; do
        run exec_container_daemon sh -c "[ -f /tmp/appready ]"
        if [ "$status" -eq 0 ]; then
            break
        fi
        echo "waiting $TIMEOUT..."
        sleep 1
        TIMEOUT="$(expr "$TIMEOUT" - 1 || true)"
    done

    if [ "$TIMEOUT" -eq 0 ]; then
        echo "Docker container daemon wait timeout."
        echo "====================================================================="
        echo " DOCKER LOGS"
        echo "====================================================================="
        getlog_container_daemon
        echo "====================================================================="
        echo " END DOCKER LOGS"
        echo "====================================================================="
        false
    else
        echo "Docker container ready."
    fi
}

restart_container_daemon() {
    [ -n "$CONTAINER_DAEMON_ID" ]

    echo "Restarting docker container daemon..."
    exec_container_daemon sh -c "rm /tmp/appready"
    docker restart "$CONTAINER_DAEMON_ID"
    echo "Docker container daemon restarted."

    wait_for_container_daemon
}

# Create workdir to store temporary stuff.
TESTS_WORKDIR="$(mktemp -d)"

# Create a fake startapp.h
cat << EOF > "$TESTS_WORKDIR"/startapp.sh
#!/usr/bin/with-contenv sh
touch /tmp/appready
while true;do sleep 999; done
EOF
chmod +x "$TESTS_WORKDIR"/startapp.sh

# Start the container in daemon mode.
echo "Starting docker container daemon..."
docker_run -d -v "$TESTS_WORKDIR"/startapp.sh:/startapp.sh $DOCKER_EXTRA_OPTS --name dockertest $DOCKER_IMAGE
[ "$status" -eq 0 ]
CONTAINER_DAEMON_ID="$output"

# Wait for the container to be ready.
wait_for_container_daemon
