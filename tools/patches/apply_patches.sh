#!/bin/sh
# Apply all qmx-panadapter standing patches with standard unix tools.
#
# Three patch managed_components/ (fetched by the IDF component manager,
# wiped by `idf.py fullclean` / a dependency refresh - re-run after either);
# three patch the IDF install tree itself ($IDF_PATH - wiped by an IDF
# reinstall, re-run per machine/container). Each .patch file carries its own
# rationale as preamble text. Idempotent: an already-applied patch is
# detected by a reverse-apply check and skipped.
#
# Usage:
#   tools/patches/apply_patches.sh            # needs $IDF_PATH for the IDF three
#   tools/patches/apply_patches.sh --managed-only
#   tools/patches/apply_patches.sh --idf-only
#
# CI note: run AFTER the component manager has resolved managed_components/
# (any idf.py command does that, e.g. `idf.py reconfigure`).

set -u
here=$(CDPATH= cd -- "$(dirname -- "$0")" && pwd)
repo=$(CDPATH= cd -- "$here/../.." && pwd)

do_managed=1
do_idf=1
case "${1:-}" in
    --managed-only) do_idf=0 ;;
    --idf-only)     do_managed=0 ;;
    "") ;;
    *) echo "usage: $0 [--managed-only|--idf-only]" >&2; exit 2 ;;
esac

fail=0

apply_one() {
    # $1 = patch file, $2 = directory to apply in (-p1 relative to it)
    p="$here/$1"
    d="$2"
    name=$(basename "$1")
    if patch -R -s -f --dry-run -p1 -d "$d" < "$p" >/dev/null 2>&1; then
        echo "  $name: already applied - skipping"
        return 0
    fi
    if ! patch -s -f --dry-run -p1 -d "$d" < "$p" >/dev/null 2>&1; then
        echo "  $name: DOES NOT APPLY - target changed? Re-port the patch." >&2
        fail=1
        return 1
    fi
    patch -s -p1 -d "$d" < "$p"
    echo "  $name: applied"
}

if [ "$do_managed" = 1 ]; then
    if [ -d "$repo/managed_components" ]; then
        echo "managed_components patches:"
        apply_one esp_hosted_psram.patch          "$repo"
        apply_one esp_hosted_sdio_recovery.patch  "$repo"
        apply_one cdc_acm_close_tolerant.patch    "$repo"
    else
        echo "managed_components/ not present - run any idf.py command first; skipping those three." >&2
        fail=1
    fi
fi

if [ "$do_idf" = 1 ]; then
    if [ -n "${IDF_PATH:-}" ] && [ -d "${IDF_PATH:-}" ]; then
        echo "IDF tree patches ($IDF_PATH):"
        apply_one hcd_bulk_error_recovery.patch "$IDF_PATH"
        apply_one hub_recover_tolerant.patch    "$IDF_PATH"
        apply_one fatfs_exfat.patch             "$IDF_PATH"
    else
        echo "IDF_PATH not set - activate the ESP-IDF environment; skipping the IDF three." >&2
        fail=1
    fi
fi

exit $fail
