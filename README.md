# AheadFormSDK V1.0

**A C SDK for developers on AheadForm robots.**

> **Release status:** AheadFormSDK V1.0 provides programmatic access to robot motion control, scripted performances, audio/video streaming, and device status. It uses **Eclipse CycloneDDS** for control and state messages, and **GStreamer 1.0** for audio/video transport.

## Contents

- [Requirements](#requirements)
- [Installation](#installation)
- [Build and run](#build-and-run)
- [Quick start](#quick-start)
- [Configuration](#configuration)
- [API overview](#api-overview)
- [Examples](#examples)
- [Data formats and constraints](#data-formats-and-constraints)
- [Error handling and troubleshooting](#error-handling-and-troubleshooting)
- [Privacy](#safety-and-privacy)
- [License](#license)


## Requirements

| Dependency | Requirement |
| --- | --- |
| OS | Ubuntu / Debian Linux |
| Toolchain | GCC with `-std=gnu11`, GNU Make, CMake, Git |
| Build metadata | `pkg-config` |
| DDS | Eclipse CycloneDDS **11.0.1**, static `libddsc.a` |
| Media | GStreamer 1.0 development libraries and runtime plugins |

Default CycloneDDS install prefix: `/opt/cyclonedds-11.0.1-static`.

> Even control-only examples include GStreamer headers (`<gst/gst.h>`) and therefore require its development packages.

## Installation

### 1. Install system dependencies

```bash
sudo apt update
sudo apt install -y build-essential cmake git pkg-config \
  libgstreamer1.0-dev libgstreamer-plugins-base1.0-dev \
  gstreamer1.0-tools gstreamer1.0-plugins-base \
  gstreamer1.0-plugins-good gstreamer1.0-plugins-bad \
  gstreamer1.0-plugins-ugly gstreamer1.0-libav
```

> Some media plugins and codecs may have additional redistribution or patent considerations. Check the final distribution's exact dependencies before shipping a commercial package.

### 2. Build CycloneDDS 11.0.1 (static)

```bash
git clone --branch 11.0.1 --depth 1 https://github.com/eclipse-cyclonedds/cyclonedds.git
cmake -S cyclonedds -B cyclonedds/build -DCMAKE_BUILD_TYPE=Release \
  -DCMAKE_INSTALL_PREFIX=/opt/cyclonedds-11.0.1-static \
  -DCMAKE_INSTALL_LIBDIR=lib -DBUILD_SHARED_LIBS=OFF \
  -DENABLE_SECURITY=OFF -DENABLE_SSL=OFF \
  -DBUILD_EXAMPLES=OFF -DBUILD_TESTING=OFF
cmake --build cyclonedds/build -j"$(nproc)"
sudo cmake --install cyclonedds/build
```

If you installed CycloneDDS elsewhere, pass `CYCLONEDDS=<path>` to `make`.

## Build and run

From the SDK repository root:

```bash
make                         # Build all examples into build/
make build/Volume_demo       # Build one example
make clean                   # Remove build/
```

Before running an example, set `NETWORK_INTERFACE`, `DOMAIN_ID`, and `AHEADFORM_CONFIG` in the relevant `user/src/*_demo.c` file to match your computer and robot, then rebuild.

```bash
./build/Volume_demo 1             # Query microphone/speaker volumes
./build/ReadHardwareState_demo 1  # Read hardware state
```

Run an example without arguments to display its usage instructions.

## Quick start

```c
#include "AheadForm.h"
#include <stdio.h>

int main(void)
{
    AHEADFORM_CONFIG cfg = {
        .NetworkInterface = "ens38",        /* Your local network interface */
        .DomainId = 50,                      /* Must match the robot */
        .MulticastAddr = "239.255.42.1",    /* Media multicast address */
        .VideoPort = 5600,
        .MicPort = 5602,
        .PlaybackPort = 5604,
        .DeviceIp = "192.168.110.51"
    };

    AHEADFORM_HANDLE *h = AheadForm_Init(&cfg);
    if (h == NULL) {
        PrintAllErrLog();
        return 1;
    }

    VOLUME volume = {0};
    if (AheadForm_QueryVolume(h, &volume) == RET_SUCCESS)
        printf("mic=%.1f%% spk=%.1f%%\n",
               volume.MicrophonePercent, volume.SpeakerPercent);
    else
        PrintAllErrLog();

    AheadForm_Free(h);
    return 0;
}
```

Save as `user/src/Customer_demo.c`, then run:

```bash
make build/Customer_demo
./build/Customer_demo
```

> Configuration values above are examples only, not production defaults. All `AHEADFORM_CONFIG` fields must be supplied. String values must be null-terminated.

## Configuration

| Field | Example | Meaning |
| --- | --- | --- |
| `NetworkInterface` | `ens38` | Local interface used for DDS and media multicast |
| `DomainId` | `50` | DDS domain ID (`0–232`); must match robot |
| `MulticastAddr` | `239.255.42.1` | Media multicast address (not DDS discovery multicast) |
| `VideoPort` | `5600` | Video RTP/UDP receiver port |
| `MicPort` | `5602` | Microphone RTP/UDP receiver port; must differ from video port |
| `PlaybackPort` | `5604` | Speaker playback RTP/UDP port |
| `DeviceIp` | `192.168.110.51` | Robot unicast address for speaker audio |

## API overview

Public APIs are declared in `API/inc/AheadForm.h`; error definitions are in `API/inc/AheadFormError.h`.

### Lifecycle

| Function | Description |
| --- | --- |
| `AheadForm_Init` | Initialize SDK and return a device handle |
| `AheadForm_Free` | Release handle; stop background work and media receive first |

### Motion and status

| Function | Description |
| --- | --- |
| `AheadForm_SendNeckPose` | Control neck pitch/yaw/roll |
| `AheadForm_SendHeadMotion` | Send neck, eyes, and 52 facial blendshape values |
| `AheadForm_SendFaceExpression` | Send eye pose and facial blendshape values |
| `AheadForm_SendFaceServoAngles` | Send 20 low-level face-servo logic angles (privileged/internal capability) |
| `AheadForm_ReadHardwareState` | Read online/ready/fault/E-stop/control-permission status |
| `AheadForm_ReadNeckState` | Read neck connection, pose, and feedback state |
| `AheadForm_ReadSensorState` | Read sensor/IMU state |
| `AheadForm_ReadFaceState` | Read face connection and feedback state |

### Audio/video and scripts

| Function | Description |
| --- | --- |
| `AheadForm_StartVideoReceive` / `StopVideoReceive` | Receive H.264 Annex-B access units via callback |
| `AheadForm_StartAudioReceive` / `StopAudioReceive` | Receive microphone S16LE PCM via callback |
| `AheadForm_SaveVideo` / `SaveAudio` | Record H.264 / WAV for the specified duration in milliseconds |
| `AheadForm_PlayAudioFile` | Play supported MP3 or WAV audio file |
| `AheadForm_WritePlaybackAudio` | Send speaker PCM in fixed 20 ms frames |
| `AheadForm_PlayScript` | Start a scripted performance |
| `AheadForm_PauseScript` / `ResumeScript` / `StopScript` | Control active script |
| `AheadForm_TakeScriptEvent` | Read script-related events |

Other available APIs include volume/audio-device management, operating-mode switching, request/reply operations, and generic DDS topic publishing/subscription.

**Return values:** A successful business API call returns `RET_SUCCESS` (`0`). Use `ret != RET_SUCCESS` to detect failures and `AheadFormSdk_ErrName(ret)` to obtain an error name.

**Important:** A successful DDS send means the command was accepted by the local DDS writer, **not** that the robot physically completed the action. Check device status and control permissions.

## Examples

The repository includes 13 independently compiled examples in `user/src/`:

| Example | Demonstrates |
| --- | --- |
| `Volume_demo` | Query/set volume and generic volume APIs |
| `Audio_demo` | Audio device management |
| `OperateMode_demo` | Switch `DEV` / `INTERACTIVE` mode |
| `SendNeckPose_demo` | Neck pose control |
| `SendHeadMotion_demo` | Neck/eye/blendshape head motion |
| `SendFaceExpression_demo` | Eye and face expression control |
| `SendFaceServoAngles_demo` | Low-level face-servo control |
| `ReadHardwareState_demo` | Hardware state |
| `ReadNeckState_demo` | Neck state |
| `ReadSensorState_demo` | Sensor/IMU state |
| `ReadFaceState_demo` | Face state |
| `Script_demo` | Script playback, state, and events |
| `Media_demo` | Video/mic receive, capture, audio playback, PCM streaming |

## Data formats and constraints

| Data | Current SDK convention |
| --- | --- |
| Motion command angles | **Degrees** |
| Facial blendshapes | 52 channels, each in `[0, 1]` |
| State neck/IMU pose | **Radians** in the documented pose fields |
| State motor/servo angles | Degrees in `*Deg` and `*LogicDeg` fields |
| Microphone callback | PCM S16LE, 16 kHz, mono |
| Speaker live PCM | Exactly **640 bytes** per call (20 ms, S16LE, 16 kHz mono) |
| WAV playback | PCM format code 1, 16 kHz, mono, S16LE |
| Received video | H.264 Annex-B access units |

Use `HasLatestSample` before consuming `LatestSample` in structures that expose this flag. A successful state API call does not guarantee that a fresh sample is available.

## Error handling and troubleshooting

```c
if (ret != RET_SUCCESS) {
    printf("SDK error: %s\n", AheadFormSdk_ErrName(ret));
    PrintAllErrLog();
}
```

Additional logging helpers include `Print1stErrLog`, `PrintLatest1ErrLog`, `StoreAllErrLogToFile`, and `ClearErrStack`.

| Symptom | Check |
| --- | --- |
| `AheadForm_Init` returns `NULL` | Error stack, network interface, DDS domain, ports, GStreamer plugins |
| `dds/dds.h` or `libddsc.a` not found | CycloneDDS prefix and `CYCLONEDDS` build override |
| DDS endpoint or reply timeout | Robot service, firewall, multicast, interface, QoS, domain |
| Status has no current sample | `HasLatestSample`, connectivity, and state publisher |
| Motion send succeeds but robot does not move | Robot mode, control permission, E-stop, fault state |
| PCM playback rejects buffer | Frame must be exactly 640 bytes |
| WAV/MP3 playback fails | Supported WAV PCM format or required decoder plugins |

## Privacy

- Media recordings may capture personal information. Obtain appropriate consent, provide visible recording indicators where required, and define retention/deletion practices.

## License

---

© 2026 AheadForm. All rights reserved.
