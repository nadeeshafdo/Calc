# Calc

A lightweight command-line calculator written in C.

## Features
- **Variables**: `x=10`, `y=x*2`, chain assign `x=y=z`, `unset x`
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

## Usage

```text
calc> 10 + 5
= 15
calc> x = 10
= 10
calc> 2sin(0) + x
= 10
```
