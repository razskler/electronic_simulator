# elsim

A small, hackable circuit simulator in the spirit of Qspice/LTspice/PROTEUS:
a Qt schematic editor with drag & drop components on top of a pure-C
simulation core (Modified Nodal Analysis), plus a plugin system so new
components can be added without recompiling anything.

## Features

- **Schematic editor (Qt Widgets)**: component palette with drag & drop,
  pin-to-pin wiring, grid snapping, rotation, parameter dialogs, save/load.
- **Simulation core (C11)**: DC operating point (Newton-Raphson with pn-junction
  limiting and gmin stepping), AC small-signal sweep (complex MNA, log/linear),
  and transient (backward Euler and trapezoidal companion models).
- **Built-in components**: resistor, capacitor, inductor, voltage source,
  current source, diode, ground.
- **Plugin components**: drop a shared library into `plugins/` and it appears
  in the palette. Two examples are included: an LED and a switch.
- **Probing & plotting**: probe mode marks nets; results are drawn in a
  scope-style plot (zoom/pan/crosshair). DC results are annotated directly
  on the wires.
- **CLI**: `elsim file.cir` runs text netlists (see `examples/`).

## Layout

```
core/     simulation engine (pure C) + CLI + unit tests
app/      Qt GUI (C++)
plugins/  example dynamic-load components (led, switch)
examples/ example netlists for the CLI
```

## Building

The core, CLI, tests and plugins need only gcc + make:

    make          # builds build/libelsim-core.so, elsim, elsim_tests, plugins
    make check    # runs the unit tests
    make demo     # runs the example netlists through the CLI

The GUI additionally needs CMake and Qt (5.15 or 6):

    cmake -B build-cmake -DCMAKE_BUILD_TYPE=Release
    cmake --build build-cmake
    ./build-cmake/app/elsim-gui        # plugins are picked up from build-cmake/plugins

Fedora dependencies:

    sudo dnf install cmake qt6-qtbase-devel

## Using the GUI

1. Drag components from the palette onto the canvas.
2. Click a pin, then another pin, to draw a wire. `R` rotates, `Del` deletes,
   double-click edits parameters (values accept `1k`, `2.2u`, `100n`, `1Meg`).
3. Add at least one ground.
4. Toggle **Probe** and click wires/nodes to mark them for plotting.
5. Pick an analysis (DC / AC / Transient), adjust **Settings...**, hit **Run** (F5).

## Netlist format (CLI)

```
; comment
vsource V1 in 0 dc=5 ac=1
resistor R1 in out R=1k
capacitor C1 out 0 C=1u
.op                 ; DC operating point
.ac 1 10k 200 log   ; AC sweep: from to points [log|lin]
.tran 1u 5m trap    ; transient: tstep tstop [be|trap]
```

## Writing a component plugin

A plugin is a shared object exporting `ec_plugin_export` (see
`core/include/ec/plugin.h` and `plugins/switch.c` for a minimal example).
The interface covers metadata (pins, parameters, symbol hint) and behaviour:

- `stamp()` writes conductances/currents into the MNA system via
  `ec_sys_add()` / `ec_sys_rhs()` for DC and transient,
- `stamp_ac()` does the same in the frequency domain (complex numbers),
- `commit()` updates internal state after each accepted transient step,
- `residual()` reports nonlinear convergence error (optional).

Build it against the core and drop it in `plugins/`:

    gcc -shared -fPIC -Icore/include mycomp.c -o plugins/mycomp.so -Lbuild -lelsim-core

## Notes / day-1 limitations

- Dense LU solver: fine up to a few hundred nodes.
- Transient always starts from zero initial conditions (UIC-style).
- Voltage sources are constant in time (no PWL/SIN yet).
