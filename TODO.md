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
      (module IP + output index). The IP identifies a module as reliably as
      its serial number: the firmware derives both from the Teensy's
      factory MAC address, so both stay the same across reboots and change
      together only if the board is replaced. Switch to the serial if module
      addressing ever changes (e.g. DHCP).
      Optional per speaker: delay, gain, subwoofer flag, "imaginary" (no
      output, for AllRAD).
- [ ] **Two types: `SpeakerLayout` and `RenderLayout`.**
      - `SpeakerLayout` (class): the editable room description; one entry per
        speaker (position, orientation, module IP, output). Private data,
        changed only through methods that keep it consistent and report
        plausibility problems; notifies listeners (UI, processor) on change.
        This is what the UI edits and the JSON file stores. No network state
        (connection status, firmware type) in it.
      - `RenderLayout` (read-only struct, formerly `ArrayLayout`): computed by
        the processor from `SpeakerLayout` + module list (connection status,
        firmware type, output count) + listener position, whenever any of
        them changes. Holds the positions per connected module for `/ss`,
        `outerSpeakerX` for WFS source scaling, rmax, the speakers per
        technique and the WFS source limit. Passed to all renderers via
        `layoutChanged()`, so they all see one consistent state; they only
        copy finished values (e.g. into atomics for the audio thread).
      - Done so far: `RenderLayout::compute` works from a `SpeakerLayout`
        (minimal for now: position, module IP, output) and the module list;
        `generateLinearArray` creates one. Still to do: the editable class
        (methods per speaker, notification, save/load), z and orientation.
- [ ] **Edit the speaker positions in the UI first**, so they can be checked
      for plausibility before anything is saved: a table (position, z,
      orientation, module, output) next to the top-down view, which shows
      each speaker with its orientation and the module it belongs to.
      Point out what looks wrong: speakers at the same position, a module
      output used twice or not at all, speakers facing away from the
      listening area, and a listener or sources placed where the layout
      can't render them. Outputs without a speaker must be muted (needs the
      protocol extension below). Saving to a file comes afterwards (below).
      - Done: the Speakers tab shows the speakers in use with the problems
        `checkLayout` finds (no module, unknown or disconnected module,
        missing output, output used twice or unused, same position, WFS
        speaker off y = 0). With the custom layout, position, module and
        output are edited in the table, and speakers added and removed;
        "Copy linear array to custom" starts it from the linear array.
      - Still open: z and orientation; the top-down view; the checks for
        speakers facing away and for the listener or sources.
      - **Bulk assignment:** select several rows, choose a module, and the
        selected speakers get its outputs 0, 1, 2… in row order (what a
        slot does in the linear array). Assigning a 16-output module one
        speaker at a time is tedious.
- [x] **The current linear array becomes a generator** ("N modules, spacing
      d") that produces a `SpeakerLayout`, so the WFS workflow stays as it
      is. `/ss` keeps sending each module the positions of its outputs.
      `generateLinearArray` creates the `SpeakerLayout` from the
      settings in `LinearArraySetup` (number of slots, spacing, module per
      slot), which the processor owns and saves with the project. The
      module list only holds what modules report.
- [ ] **A layout made of parts**, e.g. a linear WFS array at the front plus
      an Ambisonics ring or dome around the listening area. Each part is
      either a generator with parameters (linear array; later ring, arc,
      dome) or an explicit list of speakers (edited or imported), and has
      a placement (offset, later rotation). The `SpeakerLayout` is the
      speakers of all parts together; `RenderLayout::compute` needs no
      change. "Linear array only" and "custom only" are layouts with one
      part.
      - Generated parts stay parametric, so a linear array keeps adapting
        to the modules' output counts. They're edited through their
        parameters; to move single speakers, convert the part to an
        explicit one (otherwise regenerating would overwrite the edits).
      - Done: `outerSpeakerX`, which both renderers scale source positions
        to, covers the WFS modules' speakers and speakers without a module
        (all speakers if there are only Ambisonics modules), so e.g. an
        Ambisonics ring doesn't widen it.
        Open: in a *linear array* whose two outer slots both hold
        Ambisonics modules, this is narrower than the array the XY view
        draws (slot selectors and speaker icons span the whole array), so
        a source at the view's edge is drawn above the outer speaker but
        rendered at the end of the WFS part. Audio is consistent between
        the renderers; only the view is off. Option: scale to the whole
        array in linear-array mode (a `compute()` parameter). Decide
        together with how the XY view changes for 3-D coordinates.
      - The WFS firmware and `VirtualSourceMessenger` assume the array on
        y = 0, facing the listener. Keep WFS parts at their default
        position until speaker orientation (`/ss/<j>/nx|ny`) exists.
      - A dome needs z in `Speaker` and `/ss/<j>/z` (see protocol
        extensions below).
- [x] **Save and load the edited layout as an Ananas JSON file:** file-wide
      settings (format version, units, coordinate convention, reference
      point) plus the speaker list,
      and room for generators and new fields. Read and written with
      `juce::JSON`. Stored separately from projects, since it describes the
      room, not the piece; projects refer to it.
      Done: `LayoutFile` (format `ananas-speaker-layout`, version 1);
      "Save layout..." / "Load layout..." in the Speakers tab. Unknown
      fields are ignored, a non-zero z loads as 0 with a warning, and a file
      with errors isn't loaded at all.
      Still open: projects contain the custom layout itself; referring to a
      file instead (and reloading it when it changes) would be a separate
      step, if wanted.
- [ ] **CSV import and export of the speaker table only** (position,
      orientation, module IP, output), for editing in spreadsheets and
      importing from design tools. Fixed header row, decimal point, explicit
      separator (German Excel writes `;` and decimal commas).
- [ ] **Import/export of common formats:** IEM JSON (`LoudspeakerLayout`;
      also used by SPARTA), EBU ADM Renderer YAML (`az`/`el`/`r`, BS.2051
      names), SSR ASDF XML (`<reproduction_setup>`, cartesian with
      orientation, closest to Ananas). Their azimuth convention (0° front,
      counter-clockwise) matches the encoder's; converting the listener-centred
      spherical formats needs the listener position as reference. None of
      them can express "module IP, output"; that mapping stays
      Ananas-specific.
- [ ] **Speaker identification:** play noise on one module output at a time,
      to find which physical speaker is wired where. Needs a small firmware
      command for Ambisonics modules, whose stream doesn't address single
      outputs.
- [ ] **Protocol extensions: height, orientation and gain per output.**
      Proposed to the firmware side in
      `teensy-audiosync/docs/firmware-handover-speaker-geometry.md`, with
      open questions at its end (gain default at boot, WFS range, WFS with
      orientation, speaker identification in the same round). In short:
      one message per output, `/ss/<j>` with `x y z nx ny nz gain`
      (the facing direction is a 3-D unit vector; WFS uses its horizontal
      part, Ambisonics ignores it for now, and `nz` is reserved for tilted
      speakers, so they don't need another protocol change); a set id
      `/ss/id`, echoed in the announcement, so the server resends after a
      reboot (comparing echoed positions misses outputs at (0, 0)); new
      announcement fields `speakerProtocol` and `speakerSetId`; modules
      without them keep getting `/ss/<j>/x|y` (legacy stereo modules
      unchanged).
      - **Why mute:** outputs no speaker uses get the position (0, 0) and
        play as a speaker at the origin; not sending a position isn't safe
        either, since modules keep whatever position they have. Only
        matters with custom layouts; in the linear array every output has a
        speaker.
      - **WFS range:** the WFS firmware clamps speaker x and y to ±2.25 m
        (`MAX_X`), so custom layouts with WFS speakers further out are
        silently clipped today. The handover asks for a larger range.
      - **Server side, once the firmware has it:**
        - `Packet.h`: the two announcement fields; `ModuleInfo` stores them.
        - **First, merge the per-output data in `RenderLayout`.** Today
          `modulePositions` (2-D points) and `moduleHeights` are two
          parallel maps that have to be kept in sync by hand, and
          `getSpeakerPositions` / `getSpeakerHeights` only match because
          they iterate in the same order. Replace both with one entry per
          module output, e.g.
          `std::map<juce::String, std::vector<OutputGeometry>> moduleOutputs`
          with `OutputGeometry { position; z; facing; isUsed; }`: exactly
          what `/ss/<j>` sends (x, y, z, facing, gain = isUsed). Touches
          `compute`, `sendSpeakerPositions` (its "last sent" comparison then
          covers z, facing and mute too), `isEchoedByModule`,
          `getSpeakerPositions` and the Ambisonics renderer, and their
          tests; doing it here means those places change once.
        - `RenderLayout::compute` records, per module output, whether a
          speaker uses it, its z and orientation (in `OutputGeometry`).
        - `sendSpeakerPositions`: for modules on protocol 1, one bundle with
          `/ss/id` and `/ss/<j>` for every output (gain 0 for unused ones),
          resent when the echoed id differs; protocol 0 unchanged.
      - **Server side, independent of the firmware: done.**
        - `Speaker` has a height `z` and a facing direction (`Direction`, a
          3-D unit vector, default `(0, -1, 0)` towards the listener), in
          `CustomLayout`, the JSON format (`"z"`, `"facing": [x, y, z]`,
          both optional) and the mirror for the UI. The speaker table shows
          and, with the custom layout, edits z (m), facing (degrees, 0
          towards the listener, counter-clockwise seen from above) and tilt
          (degrees above the horizontal).
        - `checkLayout` warns about WFS speakers beyond ±2.25 m (clipped by
          the current firmware) and WFS speakers pointing (almost) straight
          up or down, and notes that WFS ignores heights.
        - The Ambisonics renderer's rmax uses 3-D distances
          (`RenderLayout::moduleHeights`).
        - Still open: a warning for speakers facing away from the listening
          area.

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

- [x] **Move setup values out of the plugin parameters.** The number of
      slots and the speaker spacing are no longer host-automatable
      parameters; together with the module per slot they live in
      `LinearArraySetup`, saved with the project. Parameters now only hold
      what changes during a piece: source positions and the Ambisonics
      listener (plus the UI settings below).
- [ ] **Move UI settings out of the plugin parameters as well.**
      `showModuleSelectors` (whether the slot selectors are visible) is a
      host-automatable parameter, but it's a view setting, not something
      to automate during a piece. Store it as plain saved state instead
      (or per editor). Check the Ambisonics input mode the same way: it
      selects where the Ambisonics stream comes from, which is more setup
      than performance.

- [x] **ValueTree usage: modules and switches as child nodes.** The
      processors mirror `ModuleList` and `SwitchList` into `persistentTree`
      as one `Module` / `Switch` child each (`toValueTree(true)`, with
      connection and PTP status), and the UI reads those. UI actions call
      `SwitchList::setSwitch` / `removeSwitch` / `requestPtpReset` directly
      instead of writing command flags into the tree (`handleEdit` is gone).
      Use the same pattern for the planned `SpeakerLayout`.
- [ ] **Remaining `var` data in the trees.** `dynamicTree` still holds the
      connected clients and the time authority as `var` objects
      (`ConnectedClients`, `TimeAuthority`).
- [ ] **`persistentTree` isn't what gets saved.** Despite its name, the
      project state is built separately in `getStateInformation` from the
      parameters plus `ModuleList` / `SwitchList::toValueTree()`;
      `persistentTree` is only the UI's copy. Either save the tree itself
      (one source of truth for user data), or rename it to reflect that it's
      a UI mirror.
- [x] **Remove `ModuleList::toVar()`.** The renderer plugin uses
      `toValueTree(true)`, and the Ananas Server plugin no longer listens to
      the module list (its UI has no module selectors).
- [x] **Update the `Modules` and `Switches` nodes in place.** The processors
      use `Utils::syncChildren` instead of `copyPropertiesAndChildrenFrom`:
      children are matched by a key property (`ip`, `identifier`) and only
      what differs is changed, so an open module drop-down no longer closes
      when a module announces itself.

- [ ] Authority announcement: check the received packet size, and guard
      `AuthorityInfo` with a lock (written by the listener thread, read by the UI).
- [ ] Ananas Server plugin: save and restore its state (switches).

## Interoperability

- [ ] AES67 compatibility: RTP framing (L16/L24, RTP timestamps from PTP),
      1 ms packets as an option, the AES67 PTP media profile, SDP/SAP
      announcements, DSCP marking. Control (OSC) stays separate.
