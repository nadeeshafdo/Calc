# CalcPro

A lightweight, professional command-line calculator written in C.

## Features
- **Arithmetic**: `+`, `-`, `*`, `/`, `^`, `%`
- **Math Functions**: `sin`, `cos`, `tan`, `sqrt`, `log`
- **Variables**: `x=10`, `y=x*2`, `unset x`

- **Equation Solving**: `x+12=15`, `2x=10` (solves for linear unknown)
- **Implicit Multiplication**: `2x`, `(a)(b)`

- **Line Editing**: Left/Right arrows, Backspace, Insertion
- **History**: Up/Down arrows to access previous commands

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
