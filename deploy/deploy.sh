#!/usr/bin/env bash
set -euo pipefail
SCRIPT_DIR=$(cd -- "$(dirname -- "$0")" && pwd)
# Keep SDK files owned by the invoking user; package commands elevate separately.
if ((EUID == 0)) && [[ -n "${SUDO_USER:-}" && "$SUDO_USER" != root ]]; then
    exec sudo -u "$SUDO_USER" --preserve-env=CC,CXX,AR,RANLIB,CROSS_COMPILE,SYSROOT,CPPFLAGS,CFLAGS,CXXFLAGS,LDFLAGS,LDLIBS,PKG_CONFIG,PKG_CONFIG_PATH,PKG_CONFIG_LIBDIR,PKG_CONFIG_SYSROOT_DIR,PYTHON_CONFIG bash "$SCRIPT_DIR/deploy.sh" "$@"
fi

# A foreground Ctrl+C must not interrupt an in-progress package database transaction.
bootstrap_package() {
    local pending=0 status=0 pid
    trap 'pending=130; printf "\nWaiting for the current package transaction before cancelling...\n" >&2' INT
    trap 'pending=143; printf "\nWaiting for the current package transaction before cancelling...\n" >&2' TERM
    set -m
    "$@" </dev/null &
    pid=$!
    set +m
    while true; do
        if wait "$pid"; then status=0; break; else status=$?; fi
        if ! kill -0 "$pid" 2>/dev/null; then break; fi
    done
    trap - INT TERM
    if ((pending)); then exit "$pending"; fi
    return "$status"
}

# Bootstrap Python only when needed; other dependencies are checked by deploy.py.
if ! command -v python3 >/dev/null 2>&1; then
    for argument in "$@"; do
        if [[ "$argument" == --skip-packages || "$argument" == --help || "$argument" == -h ]]; then
            echo "Python 3 is required. Install python3 first; package installation was not requested." >&2
            exit 1
        fi
    done
    PRIVILEGE=()
    if ((EUID != 0)); then
        sudo -v
        PRIVILEGE=(sudo -n)
    fi
    if [[ ! -r /etc/os-release ]]; then
        echo "Cannot identify Linux distribution; install Python 3 manually." >&2
        exit 1
    fi
    ID=
    ID_LIKE=
    . /etc/os-release
    case " $ID $ID_LIKE " in
        *debian*|*ubuntu*) bootstrap_package "${PRIVILEGE[@]}" apt-get update; bootstrap_package "${PRIVILEGE[@]}" env DEBIAN_FRONTEND=noninteractive apt-get install -y --no-remove --no-install-recommends python3 ;;
        *fedora*|*rhel*|*centos*|*rocky*|*almalinux*) bootstrap_package "${PRIVILEGE[@]}" dnf install -y python3 ;;
        *suse*) bootstrap_package "${PRIVILEGE[@]}" zypper --non-interactive install --no-recommends python3 ;;
        *) echo "Install Python 3 manually on this distribution." >&2; exit 1 ;;
    esac
fi
exec python3 "$SCRIPT_DIR/deploy.py" "$@"
