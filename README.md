<img src="assets/icon.svg" width="64" height="64" alt="">

# gatesim

![CI](https://github.com/saif-hmaied/gatesim/actions/workflows/ci.yml/badge.svg)

A small C++20 tool that reads **gate-level Verilog netlists**, checks them for
design errors, simulates them, and runs a simple static timing analysis.

> **Status: work in progress.** The lexer and the netlist data model are done.
> The parser is next, then `check`, `sim` and `timing`.

## Planned commands

```bash
gatesim check  examples/full_adder.v                 # undriven, multi-driven and unused nets, combinational loops
gatesim sim    examples/full_adder.v --input a=1 b=0 cin=1
gatesim sim    examples/full_adder.v --all           # full truth table
gatesim timing examples/full_adder.v --lib delays.lib --clock 5
```

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
include/gatesim/   public headers (token, lexer, netlist)
src/               library implementation
apps/              command-line tool
tests/             GoogleTest unit tests
examples/          example netlists
```
