# Calc v3.0.0

A native, zero-dependency (libm only) command-line calculator in C with a split-pane TUI, Unicode Braille graph plotter, equation solvers, and expanded math library.

## Features
- **Split-Pane TUI** (TTY mode): status bar, REPL log + vars/funcs panel, Braille graph canvas, input bar.
- **Braille Graph Plotter**: high-resolution Unicode Braille (U+2800) with axes, range labels, interactive zoom/pan (Ctrl-G, +/-, arrows, r, Esc).
- **Variables**: `x=10`, `y=x*2`, `unset x`, `vars`
- **User Functions**: `f(x)=x^2`, `funcs`, recursion guard
- **Math**: sin, cos, tan, asin, acos, atan, sqrt, log/ln, log10, abs, floor, ceil, round, exp, pow (^), %
- **Constants**: pi, e, tau, phi
- **Operators**: `+ - * / ^ %` (percent: `N%`=N/100, `A%B`=A*B/100)
- **Implicit Multiplication**: `2x`, `(a)(b)`, `500%25`
- **Chained Assignment**: `x = y = 10`
- **Equation Solvers**: `2x+5=15`, `4x^2-5x-12=0`, `7x+2y=24; 8x+2y=30`
- **Plotting**: `plot(expr)` or `plot(expr, xmin, xmax, ymin, ymax)`
- **Non-interactive mode**: automatic line-by-line REPL when stdin is not a TTY (for pipes/CTest).


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
- **Releases**: When a new tag (e.g. `v1.0.0`) is pushed, a GitHub Release is automatically created, and the Linux/Windows binaries are attached to it.

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
