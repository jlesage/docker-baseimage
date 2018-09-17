
get_init_script_exit_code() {
    script=$1
    lines=$2
    regex_success="^\[cont-init\] $script: terminated successfully\.$"
    regex_error="^\[cont-init\] $script: terminated with error ([0-9]+)\.$"

    for item in "${lines[@]}"; do
        if [[ "$item" =~ $regex_success ]]; then
            echo 0
            return 0;
        fi
        if [[ "$item" =~ $regex_error ]]; then
            echo ${BASH_REMATCH[1]}
            return 0;
        fi
    done

    echo "ERROR: No exit code found for init script '$script'." >&2
    return 1
}

docker_run() {
    if [ -n "$QEMU_HANDLER" ] && [ "$QEMU_HANDLER" != "UNSET" ]; then
        run docker run -v "$QEMU_HANDLER:/usr/bin/$(basename "$QEMU_HANDLER")" "$@"
    else
        run docker run "$@"
    fi
}

[ -n "$DOCKER_IMAGE" ]

# Make sure the docker image exists.
docker inspect "$DOCKER_IMAGE" > /dev/null
