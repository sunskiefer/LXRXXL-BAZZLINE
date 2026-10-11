# Changelog

## 0.2.0
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
