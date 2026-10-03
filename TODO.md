# TODO

## Control messages (source positions etc.)

- [ ] **Send all changed values per tick.** `VirtualSourceMessenger::timerCallback`
      sends only the first changed parameter per 30 Hz tick, so moving all 8
      sources (16 values) takes up to about 0.5 s and arrives staggered. Send
      every changed value in one bundle per tick. Server only.
- [ ] **Timestamp control messages with PTP time.** Bundles currently carry the
      OSC time tag "immediately", and modules apply values on arrival, so
      position changes take effect about one presentation offset (~14 ms)
      before the audio they belong to plays. Set the bundle time tag to the
      timestamp of the matching audio packet (PTP time + presentation offset).
      Applies to `/vs`, and could extend to `/listener` and `/ambi/rmax`.
- [ ] **Apply time-tagged values on the module** when its PTP clock reaches the
      time tag (firmware, `teensy-audiosync`), so position changes line up with
      the audio. Needs a handover to the firmware side together with the item
      above.
- [ ] **Accept ADM-OSC as input** (`/adm/obj/<n>/x|y|z`, `/azim|elev|dist`,
      ...) in AnanasRenderer, mapped onto the source parameters, so external
      panners and show control can position sources. The internal protocol to
      the modules stays as it is.

## Ambisonics

- [x] Test with real Ambisonics firmware, once it implements
      `teensy-audiosync/docs/firmware-handover-ambisonics-interface.md`.
- [ ] Output gain over the network (`/levelout`).
- [ ] **Higher order.** Raise `Constants::AmbisonicOrder` (3rd order: 16
      channels) together with the firmware's decoder order; ideally modules
      announce the order they decode (`numSources = (N+1)²`) and the server
      uses the lowest. Measure the decoder's CPU cost per output first. For
      horizontal-only arrays, consider 2-D (circular) Ambisonics: 2N+1
      channels instead of (N+1)².
- [ ] **Arbitrary 2-D/3-D layouts.** Speaker positions entered per speaker
      (or imported) instead of the evenly spaced line from `ArrayLayout`;
      send `/ss/<j>/z` for elevated speakers. Irregular layouts may need a
      different decoder in the firmware (e.g. AllRAD) than the sampling
      decoder.
- [ ] **Source distance.** The encoder only uses each source's direction
      (plane waves), so a source's distance from the listener has no effect.
      First step: distance gain (and optionally delay); then near-field
      (NFC-HOA) encoding with per-order filters, which only works for
      sources outside the reference radius.

## Timing and clocking

- [ ] Adaptive resampling on the server, so audio devices other than the Teensy
      authority can be used without drift (see
      `../docs-and-dev-notes/clocking-and-resampling.md`).
- [ ] Reset PTP on the switch automatically when its relayed time jumps, e.g.
      after the time authority rebooted.
- [ ] Investigate the timestamp corrections (about −12/+11 ms) seen right
      after adding a switch in the UI; the switch inspector starts a `curl`
      process.

## Robustness and cleanup

- [ ] **Move setup values out of the plugin parameters.** The number of
      modules (`numModules`) and the speaker spacing (`speakerSpacing`) are
      host-automatable parameters, although they describe the hardware, and
      automating them would move every speaker during playback. Store them in
      the saved project state instead (like the slot assignments and
      switches), not automatable. Parameters then only hold what changes
      during a piece: source positions and the Ambisonics listener. Note that
      existing projects store them as parameters; read them from there once
      when loading.

- [ ] Authority announcement: check the received packet size, and guard
      `AuthorityInfo` with a lock (written by the listener thread, read by the UI).
- [ ] Ananas Server plugin: save and restore its state (switches).

## Interoperability

- [ ] AES67 compatibility: RTP framing (L16/L24, RTP timestamps from PTP),
      1 ms packets as an option, the AES67 PTP media profile, SDP/SAP
      announcements, DSCP marking. Control (OSC) stays separate.
