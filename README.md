<img src="assets/icon.svg" width="64" height="64" alt="">

# gatesim

![CI](https://github.com/saif-hmaied/gatesim/actions/workflows/ci.yml/badge.svg)

A small C++20 tool that reads **gate-level Verilog netlists**, checks them for
design errors, simulates them, and runs a simple static timing analysis.

## Commands

### `check`: find design errors

```bash
gatesim check examples/full_adder.v
```
```
module full_adder: 5 gates, 8 nets, 0 errors, 0 warnings
```

It reports every problem in one run:

| problem | severity |
|---|---|
| net with more than one driver | error |
| net that is read (or is an output) but has no driver | error |
| input driven by a gate | error |
| combinational loop (one message per loop) | error |
| net that is never read | warning |

```bash
gatesim check examples/bad_loop.v
```
```
examples/bad_loop.v: error: combinational loop: g1 -> w1 -> g2 -> w2 -> g1
module bad_loop: 3 gates, 4 nets, 1 errors, 0 warnings
```

Syntax errors are reported with `file:line:column`, and the exit code is 1
when there are errors.

### `sim`: simulate the circuit

```bash
gatesim sim examples/full_adder.v --input a=1 b=0 cin=1
```
```
cout = 1
sum = 0
```

```bash
gatesim sim examples/full_adder.v --all      # full truth table, up to 16 inputs
```
```
a b cin | cout sum
0 0 0   | 0    0
0 0 1   | 0    1
...
1 1 1   | 1    1
```

### `timing`: static timing analysis

```bash
gatesim timing examples/full_adder.v --clock 5
```
```
output arrival times:
  cout  7 ns
  sum   6 ns
critical path (7 ns):
  a -> g1 -> w1 -> g4 -> w3 -> g5 -> cout
clock 5 ns, slack -2 ns: FAILS
```

- **Arrival time** of a net: when its value is ready. Inputs arrive at 0, and a
  gate's output arrives at `max(arrival of its inputs) + gate delay`.
- **Critical path**: the slowest path from an input to an output.
- **Slack** = `clock - worst arrival`. Negative slack means the circuit is too
  slow for this clock, and the exit code is 1.
- `--clock` is optional. Without it, only the arrival times and the critical
  path are printed.

Gate delays (ns), with wire delay taken as 0:

| not, buf | nand, nor | and, or | xor, xnor |
|---|---|---|---|
| 1 | 1.5 | 2 | 3 |

## Supported Verilog subset

```verilog
module full_adder(a, b, cin, cout, sum);
    input  cin, a, b;
    output cout, sum;
    wire   w1, w2, w3;
    xor g1(w1, a, b);      // first port is the output
    xor g2(sum, cin, w1);
    and g3(w2, a, b);
    and g4(w3, w1, cin);
    or  g5(cout, w3, w2);
endmodule
```

- `module` / `endmodule`, `input`, `output`, `wire`
- Gate primitives: `and`, `or`, `not`, `nand`, `nor`, `xor`, `xnor`, `buf`
- `//` line comments

## Design

```
file text ──lex()──► tokens ──parse()──► Netlist ──► check / sim / timing
```

- **Lexer**: turns text into tokens, and reports invalid characters with `line:column`.
- **Netlist**: a bipartite graph of gates and nets. Links are integer IDs into
  `std::vector`s instead of pointers, so they survive vector growth and make
  per-net side arrays (simulation values, arrival times) simple.
- **Parser**: recursive descent. It reports bad grammar, undeclared names,
  duplicates and port errors, and stops at the first one.
- **Check**: Kahn's algorithm gives the topological order of the gates. If it
  can't order every gate, Kosaraju's algorithm finds the strongly connected
  components, and each one is reported as a combinational loop.
- **Simulator** and **timing** both walk the gates once in topological order,
  so every gate's inputs are ready before the gate is computed.
- Errors are split by stage: the lexer reports bad characters, the parser reports
  bad grammar and undeclared names, and `check` reports every circuit problem
  in one run.

## Build and test

Requirements: a C++20 compiler (g++ or clang++), CMake 3.20+, Ninja.

```bash
cmake -S . -B build -G Ninja
cmake --build build
ctest --test-dir build --output-on-failure
```

GoogleTest is downloaded automatically by CMake. CI builds and tests with g++
and clang++, with AddressSanitizer and UndefinedBehaviorSanitizer enabled.

## Project layout

```
include/gatesim/   public headers (token, lexer, netlist, parser, check, simulator, timing)
src/               library implementation
apps/              command-line tool
tests/             GoogleTest unit tests
tests/cli/         script that runs the real gatesim program for the command-line tests
examples/          example netlists, including bad_*.v circuits with known errors
```
