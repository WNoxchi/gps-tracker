## Build & Run

### Host (testing)

```bash
mkdir -p build/host && cd build/host
cmake ../.. -DBUILD_FOR_PICO=OFF -DBUILD_TESTS=ON
cmake --build .
```

### Pico (cross-compile)

```bash
export PICO_SDK_PATH=/path/to/pico-sdk
mkdir -p build/pico && cd build/pico
cmake ../.. -DBUILD_FOR_PICO=ON -DBUILD_TESTS=OFF
cmake --build .
```

## Validation

Run these after implementing to get immediate feedback:

- Tests: `cd build/host && ctest --output-on-failure`
- Lint: `cppcheck --enable=all --error-exitcode=1 src/`
- Build: `cmake --build build/host` (uses `-Wall -Wextra -Werror`)

## Operational Notes

- Language: C11
- All hardware access via `src/hal/hal.h`
- `HOST_BUILD=1` defined on host builds
- Test framework: Unity (ThrowTheSwitch) in `external/Unity/`
- Specs are in `specs/` — read these for requirements

### Codebase Patterns

- One `.h` + `.c` per module, functions prefixed with module name (e.g., `nmea_parser_`, `gps_filter_`)
- No dynamic allocation after startup (only `nmea_parser_create()`)
- No `printf` in production code; use only in test code
- Constants in SCREAMING_SNAKE_CASE in relevant headers
