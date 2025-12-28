# Calc

A lightweight command-line calculator written in C.

## Features
- **Variables**: `x=10`, `y=x*2`, chain assign `x=y=z`, `unset x`, `vars`

- **Equation Solving**:
  - Linear: `x+12=15`
  - Quadratic: `4x^2-5x-12=0`
  - Simultaneous: `7x+2y=24; 8x+2y=30`
- **Implicit Multiplication**: `2x`, `(a)(b)`
- **UI**: GNU-style interface with `Ctrl-C` (quit) and `Ctrl-H` (help).
- **Line Editing**: Left/Right arrows, Backspace, Insertion.
- **History**: Up/Down arrows.


## Build & Install

```bash
# Build
make

# Run
./build/calc

# Install (optional)
sudo make install
```

## Building for Windows (Cross-Compilation)

To compile for Windows on Linux (requires `mingw-w64`):

```bash
make -f Makefile.win
```

The executable `calc.exe` will be found in the `build-win` directory.

## Building with CMake (Cross-Platform)

You can also use CMake to build the project on Linux, Windows, or macOS.

```bash
mkdir build
cd build
cmake ..
cmake --build .
```

To run tests (if configured):
```bash
ctest
```

## Usage



## CI/CD Artifacts
This project uses GitHub Actions for continuous integration.
- **Build Artifacts**: on every push to `main` or pull request, the workflow builds the calculator.
- **Download**: You can download the compiled executable (`calc` for Linux/macOS, `calc.exe` for Windows) from the **Actions** tab in GitHub > select the run > **Artifacts**.

## Versioning
The project uses dynamic versioning based on git tags.
- The build system automatically extracts the version using `git describe`.
- The version string (e.g., `v1.0.0-3-g1a2b3c`) is displayed in the startup banner.

```text
calc> 10 + 5
= 15
calc> x = 10
= 10
calc> 2sin(0) + x
= 10
```
