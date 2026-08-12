# Contributing to Tracebox

## Development environment

Docker is the supported development environment and the authoritative path for
Tracebox validation. The host only needs Docker and Docker Compose.

Run the complete validation workflow from the repository root:

```sh
./tools/validate.sh
```

The script builds the pinned development image and runs GCC and Clang builds,
runtime tests, storage-only tests, and the public API tests in containers. Build
directories are placed under `/tmp/tracebox-build`, and the container uses the
invoking user's UID and GID so generated files remain owned correctly.

Validation also checks that the Git working tree is unchanged. Do not add build
outputs, IDE settings, macOS metadata, or other generated files to the commit.

Native CMake builds remain available for downstream integration, packaging, and
cross-compilation. They are not the authoritative development or validation
workflow.

## Compatibility

Preserve the public `tracebox` API, storage and wire formats, existing library
target names, and compatibility-sensitive CMake options. In particular,
`TRACEBOX_BUILD_RUNTIME` is the runtime-build switch used for storage-only
builds.
