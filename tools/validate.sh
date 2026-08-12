#!/usr/bin/env bash
set -euo pipefail

ROOT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
cd "${ROOT_DIR}"

before_status="$(mktemp "${TMPDIR:-/tmp}/tracebox-status.XXXXXX")"
after_status="$(mktemp "${TMPDIR:-/tmp}/tracebox-status.XXXXXX")"
cleanup() {
    rm -f "${before_status}" "${after_status}"
}
trap cleanup EXIT

git status --porcelain=v1 --untracked-files=all >"${before_status}"
export DEV_UID="$(id -u)"
export DEV_GID="$(id -g)"

docker compose build dev
docker compose run --rm dev bash -lc '
    set -euo pipefail
    rm -rf /tmp/tracebox-build
    mkdir -p /tmp/tracebox-build

    for compiler in gcc clang; do
        case "${compiler}" in
            gcc) export CC=gcc CXX=g++ ;;
            clang) export CC=clang CXX=clang++ ;;
        esac

        runtime_build="/tmp/tracebox-build/${compiler}"
        cmake -S /workspace -B "${runtime_build}" -G Ninja \
            -DCMAKE_BUILD_TYPE=Debug \
            -DCMAKE_RUNTIME_OUTPUT_DIRECTORY="${runtime_build}/bin"
        cmake --build "${runtime_build}"
        ctest --test-dir "${runtime_build}" --output-on-failure
        ctest --test-dir "${runtime_build}" -R PublicApiTest --output-on-failure

        storage_build="/tmp/tracebox-build/${compiler}-storage"
        cmake -S /workspace -B "${storage_build}" -G Ninja \
            -DCMAKE_BUILD_TYPE=Debug \
            -DTRACEBOX_BUILD_RUNTIME=OFF \
            -DCMAKE_RUNTIME_OUTPUT_DIRECTORY="${storage_build}/bin"
        cmake --build "${storage_build}"
        ctest --test-dir "${storage_build}" --output-on-failure

    done
'

git status --porcelain=v1 --untracked-files=all >"${after_status}"
if ! cmp -s "${before_status}" "${after_status}"; then
    echo "Tracebox validation changed the working tree:" >&2
    diff -u "${before_status}" "${after_status}" >&2 || true
    exit 1
fi

echo "Tracebox Docker validation passed and left the working tree unchanged."
