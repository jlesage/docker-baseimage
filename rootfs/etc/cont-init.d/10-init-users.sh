#!/bin/sh
#
# Initialize Linux users and groups, by (re)writing /etc/passwd, /etc/group and
# /etc/shadow.
#

set -e # Exit immediately if a command exits with a non-zero status.
set -u # Treat unset variables as an error.

err() {
    echo "ERROR: $*" >&2
}

die() {
    err "$@"
    exit 1
}

get_content() {
    if [ -x "$1" ]; then
        set +e
        val="$("$1")"
        rc="$?"
        set -e
        if [ "${rc}" -ne 0 ]; then
            err "$1 terminated with error ${rc}."
            exit 1
        fi
        printf "%s" "${val}"
    elif [ -f "$1" ]; then
        if [ "${2:-string}" = "boolean" ] && [ "$(stat -c "%s" "$1")" -eq 0 ]; then
            echo "1"
        else
            printf "%s" "$(cat "$1")"
        fi
    fi
}

user_id_valid() {
    case "$1" in
        '' | *[!0-9]*)
            return 1
            ;;
        *)
            return 0
            ;;
    esac
}

user_name_valid() {
    case "$1" in
        # Must start with `[a-z_]` and be followed by `[a-z0-9_-]`.
        '' | [a-z_][a-z0-9_-]*)
            return 0
            ;;
        *)
            return 1
            ;;
    esac
}

user_id_exists() {
    [ -f /etc/passwd ] && cut -d':' -f3 < /etc/passwd | grep -q "^$1\$"
}

user_name_exists() {
    [ -f /etc/passwd ] && cut -d':' -f1 < /etc/passwd | grep -q "^$1\$"
}

group_id_valid() {
    user_id_valid "$@"
}

group_name_valid() {
    user_name_valid "$@"
}

group_id_exists() {
    [ -f /etc/group ] && cut -d':' -f3 < /etc/group | grep -q "^$1\$"
}

group_name_exists() {
    [ -f /etc/group ] && cut -d':' -f1 < /etc/group | grep -q "^$1\$"
}

get_group_name_from_group_id() {
    if [ -f /etc/group ]; then
        grep ":x:$1:" /etc/group | head -n1 | cut -d':' -f1
    fi
}

add_group() {
    ag_allow_duplicate_id=false
    if [ "$1" = "--allow-duplicate-id" ]; then
        ag_allow_duplicate_id=true
        shift
    fi

    ag_name="$1"
    ag_gid="$2"

    if group_name_exists "${ag_name}"; then
        err "group '${ag_name}' already exists."
        return 1
    elif ! ${ag_allow_duplicate_id} && group_id_exists "${ag_gid}"; then
        err "group ID '${ag_gid}' already exists."
        return 1
    fi

    echo "${ag_name}:x:${ag_gid}:" >> /etc/group

    # Add a corresponding entry to '/etc/gshadow'.
    if [ -L /etc/gshadow ]; then
        echo "${ag_name}:*::" >> /etc/gshadow
    fi
}

add_user() {
    au_allow_duplicate_id=false
    if [ "$1" = "--allow-duplicate-id" ]; then
        au_allow_duplicate_id=true
        shift
    fi

    au_name="$1"
    au_uid="$2"
    au_gid="$3"
    au_homedir="${4:-/dev/null}"
    au_password_hash="${5:-}"

    [ -n "${au_password_hash}" ] || au_password_hash="!"

    if user_name_exists "${au_name}"; then
        err "user '${au_name}' already exists."
        return 1
    elif ! ${au_allow_duplicate_id} && user_id_exists "${au_uid}"; then
        err "user ID '${au_uid}' already exists."
        return 1
    elif ! group_id_exists "${au_gid}"; then
        err "group ID '${au_gid}' doesn't exist."
        return 1
    fi

    # Add the user to '/etc/passwd'.
    echo "${au_name}:x:${au_uid}:${au_gid}::${au_homedir}:/sbin/nologin" >> /etc/passwd

    # Add a corresponding entry to '/etc/shadow'.
    echo "${au_name}:${au_password_hash}::0:::::" >> /etc/shadow
}

add_user_to_group() {
    uname="$1"
    gname="$2"

    if ! user_name_exists "${uname}"; then
        err "user '${uname}' doesn't exists."
        exit 1
    elif ! group_name_exists "${gname}"; then
        err "group '${gname}' doesn't exists."
        exit 1
    fi

    members="$(grep "^${gname}:" /etc/group | head -n1 | cut -d: -f4)"
    if printf '%s\n' "${members}" | tr ',' '\n' | grep -qx "${uname}"; then
        return 0
    fi

    if grep -q "^${gname}:.*:$" /etc/group; then
        sed-patch "/^${gname}:/ s/$/${uname}/" /etc/group
    else
        sed-patch "/^${gname}:/ s/$/,${uname}/" /etc/group
    fi
}

add_user_to_group_id() {
    uname="$1"
    extra_gid="$2"

    if ! group_id_exists "${extra_gid}"; then
        add_group "grp${extra_gid}" "${extra_gid}"
        add_user_to_group "${uname}" "grp${extra_gid}"
    else
        add_user_to_group "${uname}" "$(get_group_name_from_group_id "${extra_gid}")"
    fi
}

# Add defined groups.
process_groups() {
    allow_dup_pass="$1"

    if [ ! -d /etc/cont-groups.d ]; then
        return 0
    fi

    find /etc/cont-groups.d -type d -mindepth 1 -maxdepth 1 | while read -r entry; do
        disabled="$(get_content "${entry}"/disabled boolean)"
        if is-bool-val-true "${disabled:-0}"; then
            continue
        fi

        allow_duplicate_id="$(get_content "${entry}"/allow_duplicate_id boolean)"
        has_dup=false
        if is-bool-val-true "${allow_duplicate_id:-0}"; then
            has_dup=true
        fi
        [ "${has_dup}" = "${allow_dup_pass}" ] || continue

        name="$(get_content "${entry}"/name)"
        id="$(get_content "${entry}"/id)"

        # Fallback to directory name if group name not explicitly set.
        [ -n "${name}" ] || name="$(basename "${entry}")"

        # Validate attributes.
        group_name_valid "${name}" || die "group name defined at ${entry} is not valid."
        group_id_valid "${id}" || die "group id defined at ${entry} is not valid."

        if [ "${has_dup}" = "true" ]; then
            add_group --allow-duplicate-id "${name}" "${id}"
        else
            add_group "${name}" "${id}"
        fi
    done
}

# Add defined users.
process_users() {
    allow_dup_pass="$1"

    if [ ! -d /etc/cont-users.d ]; then
        return 0
    fi

    find /etc/cont-users.d -type d -mindepth 1 -maxdepth 1 | while read -r entry; do
        disabled="$(get_content "${entry}"/disabled boolean)"
        if is-bool-val-true "${disabled:-0}"; then
            continue
        fi

        allow_duplicate_id="$(get_content "${entry}"/allow_duplicate_id boolean)"
        has_dup=false
        if is-bool-val-true "${allow_duplicate_id:-0}"; then
            has_dup=true
        fi
        [ "${has_dup}" = "${allow_dup_pass}" ] || continue

        name="$(get_content "${entry}"/name)"
        uid="$(get_content "${entry}"/id)"
        gid="$(get_content "${entry}"/gid)"
        home="$(get_content "${entry}"/home)"
        grps="$(get_content "${entry}"/grps)"
        gids="$(get_content "${entry}"/gids)"
        password="$(get_content "${entry}"/password)"
        password_hash="$(get_content "${entry}"/password_hash)"

        # Fallback to directory name if user name not explicitly set.
        [ -n "${name}" ] || name="$(basename "${entry}")"

        # Validate attributes.
        user_name_valid "${name}" || die "user name defined at ${entry} is not valid."
        user_id_valid "${uid}" || die "user id defined at ${entry} is not valid."
        group_id_valid "${gid}" || die "group id defined at ${entry} is not valid."

        # Handle password.
        if [ -n "${password}" ]; then
            password_hash="$(echo "${password}" | /opt/base/bin/mkpasswd)"
        fi

        if [ "${has_dup}" = "true" ]; then
            add_user --allow-duplicate-id "${name}" "${uid}" "${gid}" "${home}" "${password_hash}"
        else
            add_user "${name}" "${uid}" "${gid}" "${home}" "${password_hash}"
        fi
        usr_name="${name}"
        usr_gid="${gid}"
        printf "%s\n" "${grps}" | while read -r grp; do
            [ -n "${grp}" ] || continue
            group_name_valid "${grp}" || err "group name '${grp}' defined at ${entry} is not valid."
            add_user_to_group "${usr_name}" "${grp}"
        done
        printf "%s\n" "${gids}" | while read -r extra_gid; do
            [ -n "${extra_gid}" ] || continue
            [ "${extra_gid}" != "0" ] || continue
            [ "${extra_gid}" != "${usr_gid}" ] || continue
            group_id_valid "${extra_gid}" || die "supplementary group ID '${extra_gid}' defined at ${entry} is not valid."
            add_user_to_group_id "${usr_name}" "${extra_gid}"
        done
    done
}

# Initialize files.
cat /dev/null > /etc/passwd
cat /dev/null > /etc/group
cat /dev/null > /etc/shadow
if [ -L /etc/gshadow ]; then
    cat /dev/null > /etc/gshadow
fi

# Groups before users. For each, create entries without allow_duplicate_id
# first so a runtime ID (e.g. app from USER_ID/GROUP_ID) cannot take an ID
# that a defined account such as root or cinit requires.
process_groups false
process_groups true
process_users false
process_users true

# Finally, set correct permissions on files.
chmod 644 /etc/passwd
chmod 644 /etc/group
chown root:shadow /etc/shadow
chmod 640 /etc/shadow
if [ -L /etc/gshadow ]; then
    chown root:shadow /etc/gshadow
    chmod 640 /etc/gshadow
fi

# vim:ft=sh:ts=4:sw=4:et:sts=4
