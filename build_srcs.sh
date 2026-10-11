# The source lists, shared by build.sh and test/run_tests.sh.
L=third_party/lxr02open/Core
# the LXR's band-limited oscillator tables (const data only)
LXR_SRCS="$L/DSPAudio/wavetable.c"
# the plugin
PLUGIN_SRCS="src/voice.c src/fx.c src/seq.c src/ui.c src/bl.c src/factory.c src/store.c src/engine.c"
INCS="-Isrc -Isrc/shim -Ithird_party/lxr02open -I$L -I$L/DSPAudio"
