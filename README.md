# Ananas Server

> _Ananas necessitates another networked audio system_

## Why?

Because existing systems, such as _JackTrip_ and _Audio Over OSC_, either do not
support multicast transmission, or were not designed with time-sensitive (i.e. 
sub-microsecond sync) applications in mind, nor those that entail distributed
signal processing.

Ananas exists with the aim of being a lightweight, multicast, _time-sensitive_
audio system for local area networks.

Ananas is developed on Linux. It also builds and runs on macOS, with a few
differences in setup; see [macOS](#macos) below.

## Dependencies

- JUCE 8.0.6 (included as a submodule)
- clap-juce-extensions (also included as a submodule)
- cmake 3.30
- libcurl (Arch/Manjaro `sudo pacman -Syu curl`, Ubuntu
  `sudo apt-get install libcurl-dev` (or `libcurlpp-dev`))

Additionally, on Linux, in order for Ananas to read timestamps from PTP
follow-up packets (on port 320) it may be necessary to change what `sysctl` deems an 
"unprivileged" port.

```shell
sysctl net.ipv4.ip_unprivileged_port_start
```
If running that command returns a higher number than 320, add the following line
to a file in `/etc/system.d/` or `/etc/systemd/`

```shell
net.ipv4.ip_unprivileged_port_start=0 # or some number lower than 320
```

## Clone

When cloning, be sure to get all submodules recursively:

```shell
git clone --recurse-submodules https://github.com/hatchjaw/ananas 
```

## Configure & build

Most straightforward is to use CLion, or VSCode with the _CMake Tools_ 
extension.

However, from the commandline, configure with:

```shell
cmake -DCMAKE_BUILD_TYPE=(Debug|Release) -DCMAKE_MAKE_PROGRAM=ninja -G Ninja -B cmake-build-(debug|release)
```
Then build. For instance for the ananasRenderer_CLAP target:

```shell
cmake --build cmake-build-(debug|release) --target ananasRenderer_CLAP -j 18
```
In this instance, the build process will automatically install the .clap plugin
to an appropriate directory (typically `~/.clap/`).

## Hardware setup

The machine running a plugin (or standalone) must be connected to an 
appropriately configured ethernet switch. The ethernet interface should be 
configured manually, i.e. _without DHCP_ as follows:

- Address: 192.168.10.10
- Subnet mask: 255.255.0.0
- Gateway: 192.168.10.x

where 'x' is the last octet of the IP address assigned to the switch.

The subnet mask must be `255.255.0.0`: modules give themselves addresses of
the form `192.168.<mac[4]>.<mac[5]>` within `192.168.0.0/16`. With a narrower
mask, multicast audio still works, but speaker positions, which are sent to
each module directly, can't reach modules outside `192.168.10.x`.

### macOS

Setup on macOS differs from the above as follows.

- **Skip the `sysctl` step.** macOS lets unprivileged processes bind to ports
  below 1024 as long as they bind to all interfaces, which is what Ananas does
  for PTP port 320.
- **Leave the Router field empty.** Configure the ethernet adapter manually
  (System Settings → Network → _adapter_ → Details → TCP/IP) with address
  `192.168.10.10` and subnet mask `255.255.0.0`, but leave _Router_ blank.
  With a router set, the ethernet adapter can take over the default route and
  cut off internet access over Wi-Fi. Ananas only talks to devices on the
  `192.168.0.0/16` subnet, so it doesn't need a gateway. (If your Wi-Fi
  network also uses `192.168.x.x` addresses, the two will clash.)
- **Allow the host through the firewall.** If the macOS firewall is on, it can
  silently block incoming PTP and announcement packets. Allow the host
  application (e.g. REAPER) under System Settings → Network → Firewall →
  Options, or:
  ```shell
  sudo /usr/libexec/ApplicationFirewall/socketfilterfw --add /Applications/REAPER.app
  sudo /usr/libexec/ApplicationFirewall/socketfilterfw --unblockapp /Applications/REAPER.app
  ```
- **Select "Teensy Ananas Out" as the audio device, at 48 kHz.** macOS shows
  the time authority as two audio devices, "Teensy Ananas Out" and "Teensy
  Ananas In (unused)" (with older firmware, both are called "Teensy Ananas").
  Only the output device keeps the host's audio clock locked to the
  authority. With the other one, the host runs at the wrong rate and the
  clients receive no usable audio.
- **Use a buffer size of 128 samples or less.** With larger buffers, audio on
  the clients can be distorted.

To check that PTP packets arrive on the ethernet interface (replace `en11`
with your adapter; `ifconfig` lists them):

```shell
sudo tcpdump -i en11 -n -v udp port 320
```

Follow-up messages should arrive about once a second, with
`preciseOriginTimeStamp` increasing by one second each time. If the seconds
jump around randomly, click _Reset PTP_ for the switch in the plugin's
network tab. This can be needed after the time authority has been rebooted.

## Deliverables

### `ananas_console`

A basic commandline application that embeds the `ananas_server` library.

#### Usage

```shell
ananas_console [-f |--file=][filename]
```

Transmits two channeels of `filename` (.wav, .aif) to the network on UDP 
multicast IP `224.4.224.4`, port `41952`.

### ananasServer

A CLAP DAW plugin (and standalone application) that embeds `ananas-server` and, 
as with `ananas_console`, transmits two channels of audio data to 
`224.4.224.4:49152`. Provides functionality for monitoring (and basic management
of) connected switches, the connected time authority, and connected clients.

### ananasWFS

A CLAP DAW plugin that embeds `ananas_server` transmits sixteen channels of 
audio data, representing wave field synthesis (WFS) sound sources, and controls 
a distributed WFS algorithm running on a network of embedded devices. See 
associated repository [ananas-client](https://github.com/hatchjaw/ananas-client).

(Full instructions to follow.)
