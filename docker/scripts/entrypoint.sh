#!/bin/bash
set -e

# StrongSwan with QKD plugin entrypoint

echo "Starting strongSwan with QKD plugin..."

# Load swanctl configuration if available
if [ -d "/etc/swanctl/conf.d" ] && [ "$(ls -A /etc/swanctl/conf.d 2>/dev/null)" ]; then
    echo "Configuration files found in /etc/swanctl/conf.d"
fi

# Start charon daemon
case "$1" in
    charon)
        echo "Starting ipsec starter (includes charon)..."
        # Using ipsec starter which handles charon and logging properly
        exec /usr/libexec/ipsec/starter --nofork
        ;;

    starter)
        echo "Starting ipsec starter..."
        exec /usr/sbin/ipsec start --nofork
        ;;

    swanctl)
        shift
        exec swanctl "$@"
        ;;

    *)
        exec "$@"
        ;;
esac
