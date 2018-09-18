#!/bin/bash

set -e # Exit immediately if a command exits with a non-zero status.
set -u # Treat unset variables as an error.

DRY_RUN=false

usage() {
    if [ "$*" ]; then
        echo "$*"
        echo
    fi

    echo "usage: $( basename $0 ) [OPTION]... WEBHOOK

Arguments:
  WEBHOOK               URL of the MicroBadger webhook.

Options:
  -n, --dry-run         Don't invoke the MicroBadger webhook.
  -h, --help            Display this help.
"
}

# Parse arguments.
while [[ $# > 0 ]]
do
    key="$1"

    case $key in
        -h|--help)
            usage
            exit 0
            ;;
        -n|--dry-run)
            DRY_RUN=true
            ;;
        -*)
            usage "ERROR: Unknown argument: $key"
            exit 1
            ;;
        *)
            break
            ;;
    esac
    shift
done

MICROBADGER_WEB_HOOK="${1:-UNSET}"
[ "$MICROBADGER_WEB_HOOK" = "UNSET" ] && usage "ERROR: MicroBadger webhook URL must be specified." && exit 1

echo "Invoking MicroBadger webhook..."
$DRY_RUN || curl -X POST --fail ${MICROBADGER_WEB_HOOK}

# vim:ts=4:sw=4:et:sts=4
