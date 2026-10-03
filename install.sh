#!/usr/bin/env bash
#
# Put POLY on a card.
#
#   ./install.sh /Volumes/YOUR_CARD [slot]        multi-firmware launcher card
#   ./install.sh --standalone /Volumes/YOUR_CARD  no launcher, like a stock firmware
#
# Launcher: copies build/CHOMPI.bin to /FIRMWARE/NN_POLY.bin (slot defaults
# to 04, so key 4 starts it).
# Standalone: copies it to /CHOMPI.bin, the one file the stock bootloader
# installs. Refuses if another .bin is in the root (it would compete).
# The factory patches are built into the firmware, so nothing else is copied.
# Nothing else on the card is touched.

set -euo pipefail

HERE="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
STANDALONE=0
if [[ "${1:-}" == "--standalone" ]]; then
    STANDALONE=1
    shift
fi
CARD="${1:-}"
SLOT="${2:-04}"

if [[ -z "$CARD" || ! -d "$CARD" ]]; then
    echo "usage: $(basename "$0") /path/to/card [slot]" >&2
    echo "       $(basename "$0") --standalone /path/to/card" >&2
    exit 1
fi

BIN="$HERE/code/src/build/CHOMPI.bin"
if [[ ! -f "$BIN" ]]; then
    echo "error: not built yet. Run 'make' in code/src with ARM GCC 10.3-2021.10." >&2
    exit 1
fi

if (( STANDALONE )); then
    shopt -s nullglob
    for f in "$CARD"/*.bin; do
        if [[ "$(basename "$f")" != "CHOMPI.bin" ]]; then
            echo "error: '$f' would compete with the synth for the bootloader." >&2
            echo "  The card root must hold no .bin other than CHOMPI.bin." >&2
            exit 1
        fi
    done
    shopt -u nullglob
    if [[ -d "$CARD/FIRMWARE" ]]; then
        echo "note: this card has a /FIRMWARE folder, so it was a launcher card." >&2
        echo "  Its CHOMPI.bin (the launcher) is being replaced by the synth." >&2
    fi
    cp "$BIN" "$CARD/CHOMPI.bin"
    sync
    echo "installed: /CHOMPI.bin (standalone; the bootloader installs it at next power-on)"
    exit 0
fi

if [[ ! -d "$CARD/FIRMWARE" ]]; then
    echo "error: no /FIRMWARE folder on $CARD. This needs a card set up with the" >&2
    echo "  multi-firmware launcher (github.com/sfaber02/CHOMPI), or use --standalone." >&2
    exit 1
fi
if ! [[ "$SLOT" =~ ^[0-9]{1,2}$ ]] || (( 10#$SLOT < 1 || 10#$SLOT > 15 )); then
    echo "error: slot must be 1-15" >&2
    exit 1
fi
SLOT=$(printf '%02d' $((10#$SLOT)))

shopt -s nullglob
taken=("$CARD/FIRMWARE/${SLOT}_"*.bin)
shopt -u nullglob
for f in "${taken[@]}"; do
    if [[ "$(basename "$f")" != "${SLOT}_POLY.bin" ]]; then
        echo "error: key $((10#$SLOT)) already has $(basename "$f")." >&2
        echo "  Pick a free slot: $(basename "$0") $CARD <slot>" >&2
        exit 1
    fi
done

cp "$BIN" "$CARD/FIRMWARE/${SLOT}_POLY.bin"
sync
echo "installed: /FIRMWARE/${SLOT}_POLY.bin (key $((10#$SLOT)))"
