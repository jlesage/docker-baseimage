#!/usr/bin/with-contenv sh

#
# To make sure the app is started last, make it depends on all user-defined
# services.
#

set -e # Exit immediately if a command exits with a non-zero status.
set -u # Treat unset variables as an error.

find /etc/services.d -mindepth 1 -maxdepth 1 -type d -not -name ".*" -not -name "s6-*" -not -name app -exec basename {} ';' | while read SVC
do
    # Ignore disabled services.
    [ ! -f /etc/services.d/$SVC/disabled ] || continue

    # Avoid making circular dependency.
    [ ! -f /etc/services.d/$SVC/app.dep ] || continue

    # Add dependency.
    touch /etc/services.d/app/$SVC.dep
done

# vim:ft=sh:ts=4:sw=4:et:sts=4
