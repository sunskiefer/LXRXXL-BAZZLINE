# Changelog

## 0.2.8
- PANEL: the VOICE RESET button sits on its bezel exactly as in Suns Kiefer's artwork (it was 4 px off).

## 0.2.7
- PANEL: VOICE RESET, Suns Kiefer's orange button on its bezel under the TRACK keys: the selected track's sound back
  to the default sound. SHIFT + VOICE RESET: MUTE ALL (every track muted and silenced; again: all back).

## 0.2.6
- No Q-Link numbers on the screen: the pages are the artwork only (the order is in the README's Q-Link table).

## 0.2.5
- Q-Links: Suns Kiefer's own knob order from the Q-Link Map (`qlinks.json`), with empty knobs where he left them:
  PANEL 1-3, MODULES 1-4, FX 1-5 (delay, gate/duck + MASTER, LEVEL 1-8, PAN 1-8, DLY SEND 1-8). The build reads
  `qlinks.json`.

## 0.2.4
- Q-Links: 8 per Q-Link page (one Force knob bank, knobs 1-8; no hidden second bank), every parameter in reading
  order, 13 pages: PANEL 1-3, MODULES 1-4, FX 1-2, PAN, SEND, LEVEL, MASTER. On the screen, each control of the
  selected Q-Link page shows an orange number: the knob that turns it.

## 0.2.3
- PANEL: SELECT/GATE and BACK/NOTE are endless encoders: each knob stays where it was turned and steps from there, so
  walking the steps with SELECT/GATE no longer turns BACK/NOTE to each step's note.
- Releases are pre-releases until their version is in tested.json (tested on a device); then the build promotes them.

## 0.2.2
- PANEL artwork: the small "LXR-02" text (bottom left) is off the panel.
- README, catalog and source wording: LXRXXL is a port of the open-source Sonic Potions LXR engine inspired by the
  Erica Synths LXR-02, and the bass voices are inspired by the Erica Synths Bassline DB-01.

## 0.2.1
- PANEL artwork: the SONIC POTIONS X ERICA SYNTHS legend is off the panel (the credits stay in the documentation).
- Tested on an Akai Force (MPC OS 3.9.1): the same build as 0.2.0, only the artwork changed.

## 0.2.0
- Tested on an Akai Force (MPC OS 3.9.1).
- Three pages, each filling the whole screen: PANEL (the front, stretched to MPC's full width), MODULES (bigger, with a
  readout) and FX MIXER (the delay and the gate/duck, bigger, with a readout and the mixer).
- Knobs turn in place: their filmstrips follow the framework's rule for MPC (128 frames, at most 126 px). In 0.1.0 some
  knobs slid up and down instead.
- The MIXER: PAN, DLY SEND, LEVEL, MUTE and SOLO for each of the eight tracks, and MASTER. The delay is now fed by
  each track's send (100 by default: as before).
- The readout: the control last touched, its track and its value, in big letters (MODULES, FX MIXER).
- TRACK (MODULES) and the duck's MIDI CH (FX MIXER) step both ways with the data wheel, a Q-Link or a drag (in 0.1.0
  the TRACK tap only went forwards).
- Q-Links: every parameter of every page, in reading order (top left first, row by row); 0.1.0 had only 16 per page.
- Projects from 0.1.0 load (the mixer as it sounded then).
- PRESETS (FX MIXER): SOUND / PERF, a SLOT (01-64), SAVE and LOAD. Sounds (one track's voice) and performances (the
  whole instance) are files in `/sdcard/LXRXXL BAZZLINE/` (Sounds, Performances), kept across updates. SAVE and
  LOAD PERF ask for a second tap; the readout says what happens.
- MODULES: the two sliders printed DRIVE POS and CURVE (both already on their selector bars) are now DRIVE and SUB LVL,
  with their labels reprinted.
- SHIFT functions: only BACK/NOTE sets the function's value (both encoders moved before), and both encoders now step
  both ways, one value per wheel click or Q-Link tick (they only went one way). SELECT/GATE steps one step at a time.
- The Q-Link pages have their own names (PANEL 1, PANEL 2, MODULES 1, MODULES 2, FX, PAN SEND, LEVELS) in MPC's page
  strip, so every one of them can be reached.
- The clean circuit art (Suns Kiefer) fills the mixer, the readouts and the strip above the delay (no patched band).

## 0.1.0
- First version: a variation of LXRXXL with eight bass voices built after the Erica Synths Bassline (DB-01) signal
  path on the LXR's oscillator tables (VCO / FM / WAVE, sub, noise, BBD or unison swarm, drive pre/post with three
  curves, Polivoks or diode filter LP/BP, accent with sweep, slide, pitch envelope, modulator).
- The LXR-02 style sequencer of LXRXXL: 64 patterns (banks A-D) of 8 tracks up to 64 steps, on MPC's grid, with the
  DB-01's step functions: ACC, SLIDE, MOD, PITCH ENV, LAST STEP, PLAY MODE, TRANSP, SCALE, ARPEGG, LFO, RND PATT,
  CLEAR, COPY, PASTE, BANK, PATTERN.
- A delay (after the Erica Synths DIY Delay's manual: tape / digital, tap sync, hold, add, reverse) and a MIDI-triggered
  gate/duck.
- Two pages from Suns Kiefer's artwork: PANEL and MODULES; the display in DSEG14 Classic (OFL) in the panel's orange.
- Factory project: eight bass sounds and six demo patterns.
- Tested offline only (x86 under valgrind, the MPC simulation on ARM under qemu); not yet on a device.
