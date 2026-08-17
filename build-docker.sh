#!/bin/sh
# Containerized firmware build: wraps `docker|podman run --rm` on the
# official espressif/idf:v5.4.4 image - multi-arch (native on Apple
# Silicon) and the same image CI uses (.github/workflows/firmware-build.yml).
# The image sources the IDF environment in its entrypoint and ships
# everything else the build needs (git, patch), so no local toolchain
# install and no custom image.
#
#   ./build-docker.sh                     # idf.py build
#   ./build-docker.sh idf.py menuconfig
#   ./build-docker.sh bash
#
# Before the command runs, the six standing patches (tools/patches/*.patch -
# see each file's preamble) are applied: the IDF-tree three against the
# container's own IDF copy (ephemeral, so re-applied every run - cheap once
# the dry-run check reports already-applied), and the managed three after
# managed_components/ exists (an `idf.py reconfigure` resolves it when
# missing). All idempotent.
set -eu

repo=$(CDPATH= cd -- "$(dirname -- "$0")" && pwd)
image=${QMX_IDF_IMAGE:-espressif/idf:v5.4.4}

engine=${QMX_CONTAINER_ENGINE:-}
if [ -z "$engine" ]; then
    if command -v docker >/dev/null 2>&1; then engine=docker
    elif command -v podman >/dev/null 2>&1; then engine=podman
    else echo "docker/build.sh: need docker or podman on PATH" >&2; exit 1; fi
fi

tty_flags=""
if [ -t 0 ] && [ -t 1 ]; then tty_flags="-it"; fi

[ $# -gt 0 ] || set -- idf.py build

exec "$engine" run --rm $tty_flags -v "$repo":/workspace -w /workspace "$image" \
    bash -c '
        sh tools/patches/apply_patches.sh --idf-only \
            || echo "warning: IDF-tree patches did not all apply - continuing" >&2
        if [ ! -d managed_components ]; then
            echo "== managed_components/ missing - running idf.py reconfigure to fetch it =="
            idf.py reconfigure
        fi
        sh tools/patches/apply_patches.sh --managed-only \
            || echo "warning: managed-component patches did not all apply - continuing" >&2
        exec "$@"
    ' -- "$@"
