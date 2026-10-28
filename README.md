<img src="assets/icon.svg" width="64" height="64" alt="">

# gatesim

![CI](https://github.com/saif-hmaied/gatesim/actions/workflows/ci.yml/badge.svg)

A C++20 command-line tool that **checks, simulates and times** gate-level
Verilog circuits.

```
$ gatesim timing examples/full_adder.v --clock 5
output arrival times:
  cout  7 ns
  sum   6 ns
critical path (7 ns):
  a -> g1 -> w1 -> g4 -> w3 -> g5 -> cout
clock 5 ns, slack -2 ns: FAILS
```

## Features

- **Design checks**: multi-driven nets, undriven nets, driven inputs, unused
  nets and combinational loops, all reported in one run.
- **Logic simulation**: one input combination, or the full truth table.
- **Static timing analysis**: arrival time of every net, the critical path,
  and the slack against a clock period.
- Clear errors with `file:line:column` for syntax problems.

## Supported Verilog

gatesim reads **gate-level netlists**: one module made of basic gates and the
wires between them.

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
- Gates: `and`, `or`, `not`, `nand`, `nor`, `xor`, `xnor`, `buf`
- `//` comments

## Usage

### `check`: find design errors

```
$ gatesim check examples/full_adder.v
module full_adder: 5 gates, 8 nets, 0 errors, 0 warnings

$ gatesim check examples/bad_loop.v
examples/bad_loop.v: error: combinational loop: g1 -> w1 -> g2 -> w2 -> g1
module bad_loop: 3 gates, 4 nets, 1 errors, 0 warnings
```

| problem | severity |
|---|---|
| net driven by more than one gate | error |
| net that is read (or is an output) but has no driver | error |
| input driven by a gate | error |
| combinational loop (one message per loop) | error |
| net that is never read | warning |

The exit code is 1 when there are errors.

### `sim`: simulate the circuit

```
$ gatesim sim examples/full_adder.v --input a=1 b=0 cin=1
cout = 1
sum = 0

$ gatesim sim examples/full_adder.v --all
a b cin | cout sum
0 0 0   | 0    0
0 0 1   | 0    1
0 1 0   | 0    1
0 1 1   | 1    0
1 0 0   | 0    1
1 0 1   | 1    0
1 1 0   | 1    0
1 1 1   | 1    1
```

### `timing`: static timing analysis

```
$ gatesim timing examples/full_adder.v --clock 8
output arrival times:
  cout  7 ns
  sum   6 ns
critical path (7 ns):
  a -> g1 -> w1 -> g4 -> w3 -> g5 -> cout
clock 8 ns, slack 1 ns: OK
```

- **Arrival time** of a net: when its value is ready. Inputs arrive at 0, and
  a gate's output arrives at `max(arrival of its inputs) + gate delay`.
- **Critical path**: the slowest path from an input to an output. It limits
  how fast the clock can be.
- **Slack** = `clock period - worst arrival`. Negative slack means the circuit
  is too slow for this clock, and the exit code is 1.
- `--clock` is optional. Without it, only the arrival times and the critical
  path are printed.

Gate delays (ns):

| not, buf | nand, nor | and, or | xor, xnor |
|---|---|---|---|
| 1 | 1.5 | 2 | 3 |

## How it works

```
text ──lex()──► tokens ──parse()──► Netlist ──check()──► errors + topological order
                                                              │
                                                              ├──► simulate()
                                                              └──► analyze_timing()
```

- **Lexer**: turns the text into tokens (keywords, names, punctuation) and
  rejects invalid characters.
- **Parser**: recursive descent. It reports bad grammar, undeclared names,
  duplicates and port errors, and builds the netlist.
- **Netlist**: a graph of gates and nets. Each gate knows its input and output
  nets, and each net knows its drivers and readers. Links are integer IDs into
  `std::vector`s instead of pointers: they stay valid when a vector grows, and
  values or arrival times can live in simple side arrays indexed by net ID.
- **Check**: rules on every net, then Kahn's algorithm for the topological
  order of the gates. If some gates can't be ordered, there is a loop:
  Kosaraju's algorithm finds the strongly connected components, and each one
  is reported as a separate loop.
- **Simulation** and **timing** walk the gates once in topological order, so
  every gate's inputs are ready before the gate itself is computed.

Every step is linear in the size of the circuit: O(gates + connections).

## Limitations and future work

- One flat module: no hierarchy, no buses (`wire [3:0]`), no `assign`.
- Combinational circuits only: no flip-flops.
- Timing uses a fixed delay per gate type, and wire delay is 0. Real tools use
  delays that depend on the load and on wire length.
- Loop detection is recursive, so a very long chain of gates could overflow
  the stack.

Next ideas: bit-parallel simulation (64 input patterns at once with
`uint64_t`), reading gate delays from a file, and exporting the circuit to
Graphviz with the critical path highlighted.

## Project layout

```
include/gatesim/   headers (token, lexer, netlist, parser, check, simulator, timing)
src/               library implementation
apps/              command-line tool
examples/          example netlists, including bad_*.v circuits with known errors
```
