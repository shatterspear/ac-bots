#!/usr/bin/env bash
set -euo pipefail

CONF_DIR="${CONF_DIR:-/azerothcore/env/dist/etc}"
LOGS_DIR="${LOGS_DIR:-/azerothcore/env/dist/logs}"
TEMP_DIR="${TEMP_DIR:-/azerothcore/env/dist/temp}"
RUN_USER="${ACORE_RUN_USER:-acore}"

# Dokploy volumes can be created as root even when the image contents are
# owned by the service user. Repair those runtime mount permissions before
# dropping privileges for the actual AzerothCore process.
if [[ "$(id -u)" == "0" ]]; then
    mkdir -p "$CONF_DIR" "$LOGS_DIR" "$TEMP_DIR"
    chown -R "$RUN_USER:$RUN_USER" "$CONF_DIR" "$LOGS_DIR" "$TEMP_DIR"
fi

if ! touch "$CONF_DIR/.write-test" || ! touch "$LOGS_DIR/.write-test"; then
    cat <<EOF
===== WARNING =====
The current user doesn't have write permissions for
the configuration dir ($CONF_DIR) or logs dir ($LOGS_DIR).
It's likely that services will fail due to this.

This is usually caused by cloning the repository as root,
so the files are owned by root (uid 0).

To resolve this, you can set the ownership of the
configuration directory with the command on the host machine.
Note that if the files are owned as root, the ownership must
be changed as root (hence sudo).

$ sudo chown -R $(id -u):$(id -g) /path/to$CONF_DIR /path/to$LOGS_DIR

Alternatively, you can set the DOCKER_USER environment
variable (on the host machine) to "root", though this
isn't recommended.

$ DOCKER_USER=root docker-compose up -d
====================
EOF
fi

[[ -f "$CONF_DIR/.write-test" ]] && rm -f "$CONF_DIR/.write-test"
[[ -f "$LOGS_DIR/.write-test" ]] && rm -f "$LOGS_DIR/.write-test"

# Copy all default config files to env/dist/etc if they don't already exist
# -r == recursive
# --update=none == no clobber (don't overwrite)
# -v == be verbose
cp -rv --update=none /azerothcore/env/ref/etc/* "$CONF_DIR"

CONF="$CONF_DIR/$ACORE_COMPONENT.conf"
CONF_DIST="$CONF_DIR/$ACORE_COMPONENT.conf.dist"

# Copy the "dist" file to the "conf" if the conf doesn't already exist
if [[ -f "$CONF_DIST" ]]; then
    cp -vn "$CONF_DIST" "$CONF"
else
    touch "$CONF"
fi

if [[ "$(id -u)" == "0" ]]; then
    chown -R "$RUN_USER:$RUN_USER" "$CONF_DIR" "$LOGS_DIR" "$TEMP_DIR"
fi

echo "Starting $ACORE_COMPONENT..."

if [[ "$(id -u)" == "0" ]]; then
    exec gosu "$RUN_USER" "$@"
fi

exec "$@"
