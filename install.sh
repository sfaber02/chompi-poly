#!/usr/bin/env bash
#
# Put CHOMPI SYNTH on a multi-firmware launcher card.
#
#   ./install.sh /Volumes/YOUR_CARD [slot]
#
# Copies build/CHOMPI.bin to /FIRMWARE/NN_SYNTH.bin (slot defaults to 04, so
# key 4 starts it) and the factory patches to /SYNTH. Patches you have already
# saved in /SYNTH are never overwritten. Nothing else on the card is touched.

set -euo pipefail

HERE="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
CARD="${1:-}"
SLOT="${2:-04}"

if [[ -z "$CARD" || ! -d "$CARD" ]]; then
    echo "usage: $(basename "$0") /path/to/card [slot]" >&2
    exit 1
fi
if [[ ! -d "$CARD/FIRMWARE" ]]; then
    echo "error: no /FIRMWARE folder on $CARD. This needs a card set up with the" >&2
    echo "  multi-firmware launcher (github.com/sfaber02/CHOMPI)." >&2
    exit 1
fi
if ! [[ "$SLOT" =~ ^[0-9]{1,2}$ ]] || (( 10#$SLOT < 1 || 10#$SLOT > 15 )); then
    echo "error: slot must be 1-15" >&2
    exit 1
fi
SLOT=$(printf '%02d' $((10#$SLOT)))

BIN="$HERE/code/src/build/CHOMPI.bin"
if [[ ! -f "$BIN" ]]; then
    echo "error: not built yet. Run 'make' in code/src with ARM GCC 10.3-2021.10." >&2
    exit 1
fi

shopt -s nullglob
taken=("$CARD/FIRMWARE/${SLOT}_"*.bin)
shopt -u nullglob
for f in "${taken[@]}"; do
    if [[ "$(basename "$f")" != "${SLOT}_SYNTH.bin" ]]; then
        echo "error: key $((10#$SLOT)) already has $(basename "$f")." >&2
        echo "  Pick a free slot: $(basename "$0") $CARD <slot>" >&2
        exit 1
    fi
done

cp "$BIN" "$CARD/FIRMWARE/${SLOT}_SYNTH.bin"
mkdir -p "$CARD/SYNTH"
rsync -a --ignore-existing --exclude='._*' --exclude='.DS_Store' "$HERE/card/SYNTH/" "$CARD/SYNTH/"
sync

echo "installed: /FIRMWARE/${SLOT}_SYNTH.bin (key $((10#$SLOT)))"
echo "patches:   /SYNTH ($(ls "$CARD/SYNTH" | grep -c '^P[0-9]*\.txt$') slots)"
