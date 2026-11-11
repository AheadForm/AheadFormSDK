#!/usr/bin/env bash
set -euo pipefail
SCRIPT_DIR=$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")" && pwd)
SDK_DIR=$(cd -- "$SCRIPT_DIR/../CodeProject" && pwd)
ROBOT_IP=192.168.110.51
DDS_DOMAIN=50
NETWORK_INTERFACE=
SKIP_PACKAGES=0
while (($#)); do
    case "$1" in
        --robot) ROBOT_IP=${2:?Missing robot IPv4}; shift 2 ;;

        --domain) DDS_DOMAIN=${2:?Missing external DDS domain}; shift 2 ;;

        --interface) NETWORK_INTERFACE=${2:?Missing local Ethernet interface}; shift 2 ;;

        --skip-packages) SKIP_PACKAGES=1; shift ;;

        --help) echo "bash scripts/deploy_ubuntu.sh [--robot IPv4] [--domain wire_domain] [--interface Ethernet] [--skip-packages]"; exit 0 ;;

        *) echo "Unknown argument: $1" >&2; exit 1 ;;
    esac
done
if ((EUID == 0)); then SUDO=(); else SUDO=(sudo); fi
if ((SKIP_PACKAGES == 0)); then
    "${SUDO[@]}" apt-get update
    "${SUDO[@]}" env DEBIAN_FRONTEND=noninteractive apt-get install -y build-essential pkg-config cmake curl ca-certificates python3 iproute2 iputils-ping \
        libssl-dev libgstreamer1.0-dev libgstreamer-plugins-base1.0-dev gstreamer1.0-tools gstreamer1.0-plugins-base \
        gstreamer1.0-plugins-good gstreamer1.0-plugins-bad gstreamer1.0-libav
fi
python3 - "$ROBOT_IP" "$DDS_DOMAIN" <<'PY'
import ipaddress,sys
ip=ipaddress.IPv4Address(sys.argv[1])
if ip.is_multicast or ip.is_unspecified or ip.is_loopback:
    raise SystemExit('Robot IPv4 must be a reachable unicast address')
if not sys.argv[2].isdigit() or not 0 <= int(sys.argv[2]) <= 232:
    raise SystemExit('External DDS domain must be between 0 and 232')
PY
ROUTE=$(ip -4 -j route get "$ROBOT_IP")
read -r ROUTE_INTERFACE LOCAL_IP < <(python3 -c 'import json,sys;r=json.load(sys.stdin)[0];print(r["dev"],r["prefsrc"])' <<<"$ROUTE")
if [[ -z "$NETWORK_INTERFACE" ]]; then NETWORK_INTERFACE=$ROUTE_INTERFACE; fi
if [[ "$NETWORK_INTERFACE" != "$ROUTE_INTERFACE" || "$NETWORK_INTERFACE" == lo || -d "/sys/class/net/$NETWORK_INTERFACE/wireless" ]]; then
    echo "Use bridged Ethernet. Route to $ROBOT_IP currently uses $ROUTE_INTERFACE ($LOCAL_IP); requested $NETWORK_INTERFACE." >&2
    exit 1
fi
if ! ping -I "$NETWORK_INTERFACE" -c 1 -W 3 "$ROBOT_IP" >/dev/null; then
    echo "Cannot reach robot $ROBOT_IP from $NETWORK_INTERFACE ($LOCAL_IP). Check bridge and cable." >&2
    exit 1
fi
# Generated descriptors require CycloneDDS 11. Build the pinned release if it is not installed.
if ! pkg-config --exact-version=11.0.1 CycloneDDS; then
    BUILD_DIR=$(mktemp -d /tmp/aheadform-cyclonedds.XXXXXX)
    trap 'rm -rf -- "$BUILD_DIR"' EXIT
    curl -fL --retry 2 --connect-timeout 20 https://codeload.github.com/eclipse-cyclonedds/cyclonedds/tar.gz/refs/tags/11.0.1 -o "$BUILD_DIR/source.tar.gz"
    echo "c25d46075ad6b5cee564bda9e5f49509e9a6dbb1a8b858e708eb360d335cf973  $BUILD_DIR/source.tar.gz" | sha256sum -c -
    tar -xzf "$BUILD_DIR/source.tar.gz" -C "$BUILD_DIR"
    cmake -S "$BUILD_DIR/cyclonedds-11.0.1" -B "$BUILD_DIR/build" -DCMAKE_BUILD_TYPE=Release -DCMAKE_INSTALL_PREFIX=/usr/local -DBUILD_TESTING=OFF -DBUILD_EXAMPLES=OFF -DBUILD_DDSPERF=OFF
    cmake --build "$BUILD_DIR/build" --parallel 2
    "${SUDO[@]}" cmake --install "$BUILD_DIR/build"
fi
"${SUDO[@]}" ldconfig
for plugin in appsrc appsink udpsrc udpsink rtpjitterbuffer rtph264depay h264parse rtpL16depay rtpL16pay mpg123audiodec mpegaudioparse audioconvert audioresample; do
    gst-inspect-1.0 "$plugin" >/dev/null
done
CONFIG="$SDK_DIR/AheadFormSDK_Config.ini"
if [[ -f "$CONFIG" ]]; then cp -p -- "$CONFIG" "$CONFIG.bak.$(date +%Y%m%d%H%M%S)"; fi
cat > "$CONFIG" <<EOF
[dds]
strNetworkInterface = $NETWORK_INTERFACE
domain_id = $DDS_DOMAIN

[head]
sourceName = aheadform_sdk

[status]
faultRecordCntMax = 128

[media]
strMulticastAddress = 239.255.42.1
strRobotIPv4 = $ROBOT_IP
videoPort = 5600
micPort = 5602
spkPort = 5604
videoPayloadType = 96
micPayloadType = 96
spkPayloadType = 96
jitterLatency_ms = 60
rtpMtu = 1400
EOF
# Force rebuild so ARM64 objects from a previous SDK environment cannot enter the native executable.
make -C "$SDK_DIR" -B -j2 CC=gcc
if ldd "$SDK_DIR/AheadFormSDK" | grep -q 'not found'; then
    ldd "$SDK_DIR/AheadFormSDK" >&2
    exit 1
fi
printf '\nDeployment complete: interface=%s local IPv4=%s robot=%s external DDS domain=%s\n' "$NETWORK_INTERFACE" "$LOCAL_IP" "$ROBOT_IP" "$DDS_DOMAIN"
printf 'Run:\n  cd %q\n  ./AheadFormSDK\n' "$SDK_DIR"
printf 'Menu:\n  ReadStatus hardware latest\n  PlayScript /path/to/your/show.csv\n  ScriptStatus\n  StopScript\n'
echo 'Provide your own prescribed CSV and same-basename MP3 in the same directory; customer scripts are not included in this repository.'
echo 'Wait for the latest hardware state to arrive; require an empty motion source and permitted face/neck motion before playback.'
echo 'If status has no fresh sample, wait briefly and query again. DDS domain is the external wire domain, not the robot internal domain.'
