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

## Speaker layouts

The server can only infer speaker positions from what modules announce for
the evenly spaced linear array. For arbitrary layouts it needs an explicit
description of where each speaker is and which module output drives it.
Positions stay on the server; the firmware never stores or hardcodes them.

- [ ] **Layout as data.** One entry per physical speaker: position
      (x, y, z, metres, array frame), orientation (needed by WFS to pick the
      speakers facing a source), and the module output it's connected to
      (module serial number + output index; the serial rather than the IP).
      Optional per speaker: delay, gain, subwoofer flag, "imaginary" (no
      output, for AllRAD).
- [ ] **Two types: `SpeakerLayout` and `RenderLayout`.**
      - `SpeakerLayout` (class): the editable room description; one entry per
        speaker (position, orientation, module serial, output). Private data,
        changed only through methods that keep it consistent and report
        plausibility problems; notifies listeners (UI, processor) on change.
        This is what the UI edits and the JSON file stores. No network state
        (IPs, connection status) in it.
      - `RenderLayout` (read-only struct, today's `ArrayLayout`): computed by
        the processor from `SpeakerLayout` + module list (serial → IP,
        firmware type, output count) + listener position, whenever any of
        them changes. Holds the positions per connected module for `/ss`,
        `outerSpeakerX` for WFS source scaling, rmax, the speakers per
        technique and the WFS source limit. Passed to all renderers via
        `layoutChanged()`, so they all see one consistent state; they only
        copy finished values (e.g. into atomics for the audio thread).
      - The linear-array generator becomes a function that creates a
        `SpeakerLayout`.
- [ ] **Edit the speaker positions in the UI first**, so they can be checked
      for plausibility before anything is saved: a table (position, z,
      orientation, module, output) next to the top-down view, which shows
      each speaker with its orientation and the module it belongs to.
      Point out what looks wrong: speakers at the same position, a module
      output used twice or not at all, speakers facing away from the
      listening area, and a listener or sources placed where the layout
      can't render them. Saving to a file comes afterwards (below).
- [ ] **The current linear array becomes a generator** ("N modules, spacing
      d") that produces a `SpeakerLayout`, so the WFS workflow stays as it
      is. `/ss` keeps sending
      each module the positions of its outputs; outputs that aren't in the
      layout get none, or are muted.
- [ ] **Save and load the edited layout as an Ananas JSON file:** file-wide
      settings (format version, units, coordinate convention, reference
      point) plus the speaker list,
      and room for generators and new fields. Read and written with
      `juce::JSON`. Stored separately from projects, since it describes the
      room, not the piece; projects refer to it.
- [ ] **CSV import and export of the speaker table only** (position,
      orientation, module serial, output), for editing in spreadsheets and
      importing from design tools. Fixed header row, decimal point, explicit
      separator (German Excel writes `;` and decimal commas).
- [ ] **Import/export of common formats:** IEM JSON (`LoudspeakerLayout`;
      also used by SPARTA), EBU ADM Renderer YAML (`az`/`el`/`r`, BS.2051
      names), SSR ASDF XML (`<reproduction_setup>`, cartesian with
      orientation, closest to Ananas). Their azimuth convention (0° front,
      counter-clockwise) matches the encoder's; converting the listener-centred
      spherical formats needs the listener position as reference. None of
      them can express "module serial, output"; that mapping stays
      Ananas-specific.
- [ ] **Speaker identification:** play noise on one module output at a time,
      to find which physical speaker is wired where. Needs a small firmware
      command for Ambisonics modules, whose stream doesn't address single
      outputs.
- [ ] **Protocol extensions:** `/ss/<j>/z` for elevated speakers, and
      `/ss/<j>/nx|ny` (orientation) for WFS on non-linear layouts.

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
- [ ] **Decoder for irregular layouts.** With arbitrary speaker layouts (see
      "Speaker layouts"), the firmware's sampling decoder may need to be
      replaced or complemented, e.g. by AllRAD.
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

- [ ] **ValueTree usage.** Data such as the module slots (`Modules`) and
      switches is stored as one `juce::var` object per property instead of
      as child nodes. Listeners then only learn that the whole property
      changed, not which entry, and since the shared object is modified in
      place, change notifications have to be sent by hand
      (`sendPropertyChangeMessage`). Use child nodes (e.g. one per module,
      switch and, later, speaker) so listeners see what changed, and undo
      becomes possible. A natural fit for the planned `SpeakerLayout`.
- [ ] **`persistentTree` isn't what gets saved.** Despite its name, the
      project state is built separately in `getStateInformation` from the
      parameters plus `ModuleList` / `SwitchList::toValueTree()`;
      `persistentTree` is only the UI's copy. Either save the tree itself
      (one source of truth for user data), or rename it to reflect that it's
      a UI mirror.

- [ ] Authority announcement: check the received packet size, and guard
      `AuthorityInfo` with a lock (written by the listener thread, read by the UI).
- [ ] Ananas Server plugin: save and restore its state (switches).

## Interoperability

- [ ] AES67 compatibility: RTP framing (L16/L24, RTP timestamps from PTP),
      1 ms packets as an option, the AES67 PTP media profile, SDP/SAP
      announcements, DSCP marking. Control (OSC) stays separate.
