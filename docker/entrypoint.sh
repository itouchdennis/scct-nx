#!/usr/bin/env bash
# The builder container's job (docker/Dockerfile), started by build-nro-docker.sh. Mounted:
#   /game    your copy of the game, read only: an Xbox disc image (.iso, .xiso) or the extracted disc's folder
#   /out     where the SD card folder is made: /out/switch/scct/scct.nro and /out/switch/scct/game/
#   /cache   kept between runs: the recompiled code and the build (a Docker volume)
#   /icon.jpg  optional, your own 256x256 JPEG for the Homebrew Menu
# SCCT_ANY_RELEASE=1 builds from an sc3.xbe other than the release the port is made from; SCCT_CODE_OPT=-O2 builds
# the game code as before -O3, for comparing.
set -euo pipefail

TESTED_XBE_SHA1=591ceeb0ee38e013ea3969ceb0196fe5cfafd891  # sc3.xbe of Chaos Theory (Europe), the release it is made from
NVK=/opt/nvk-sdk/opt/devkitpro/portlibs/switch
DEST=/out/switch/scct
GAME="$DEST/game"

step() { printf '\n== %s\n' "$*"; }
fail() { printf '\nerror: %s\n' "$*" >&2; exit 1; }

[ -e /game ] || fail "no game mounted at /game (build-nro-docker.sh does that for you)"
[ -d /out ] && [ -w /out ] || fail "no writable folder mounted at /out"
[ -d /cache ] && [ -w /cache ] || fail "no writable volume mounted at /cache"

step "Game files -> $GAME"
mkdir -p "$GAME"
if [ -d /game ]; then
    [ -f /game/sc3.xbe ] || fail "the folder you chose has no sc3.xbe: choose the extracted disc's folder"
    # sizes only, no owners or permissions: /out can be a Windows folder (Docker Desktop), which takes neither
    rsync -r --info=progress2 --size-only --exclude '*.iso' /game/ "$GAME/"
elif [ -f "$GAME/sc3.xbe" ] && [ "$GAME/sc3.xbe" -nt /game ]; then
    echo "already extracted"
else
    extract-xiso -s -d "$GAME" -x /game || fail "the file you chose could not be read as an Xbox disc image"
fi
[ -f "$GAME/sc3.xbe" ] || fail "no sc3.xbe in what you chose: is it the Splinter Cell: Chaos Theory disc?"

XBE_SHA1="$(sha1sum "$GAME/sc3.xbe" | cut -d' ' -f1)"
if [ "$XBE_SHA1" = "$TESTED_XBE_SHA1" ]; then
    echo "sc3.xbe: the release the port is made from (Europe)"
elif [ "${SCCT_ANY_RELEASE:-0}" = 1 ]; then
    echo "sc3.xbe: sha1 $XBE_SHA1, not the release the port is made from; building it anyway (SCCT_ANY_RELEASE=1)"
else
    fail "sc3.xbe has sha1 $XBE_SHA1, not the release this port is made from ($TESTED_XBE_SHA1).
Another release may not work. To build it anyway, run again with SCCT_ANY_RELEASE=1."
fi

GEN="/cache/generated/$XBE_SHA1"
step "Recompiling sc3.xbe"
if [ -f "$GEN/image_symbols.h" ] && [ "$GEN/codegen.stamp" -nt /opt/scct/config/sc3.toml ]; then
    echo "already recompiled"
else
    mkdir -p "$GEN"
    (cd /opt/scct && xbe_recomp config/sc3.toml "$GAME/sc3.xbe" "$GEN")
    touch "$GEN/codegen.stamp"
fi

step "Building the NRO"
icon=()
[ -f /icon.jpg ] && icon=(SCDA_ICON=/icon.jpg)
env "${icon[@]}" SCDA_GEN_DIR="$GEN" SCDA_NVK_SDK="$NVK" SD_ROOT=/out BUILD_DIR=/cache/build-switch-vk \
    SCDA_CODE_OPT="${SCCT_CODE_OPT:--O3}" /opt/scct/switch/build.sh
[ -f "$DEST/scct.nro" ] || fail "no NRO was made"

step "Done"
echo "Copy the folder switch/ from your output folder onto the root of your SD card, so that it holds"
echo "  sdmc:/switch/scct/scct.nro and sdmc:/switch/scct/game/sc3.xbe"
echo "and start it from the Homebrew Menu with title takeover: hold R while starting any game."
echo
echo "The NRO holds code made from your copy of the game: it is for you only, please do not share it."
