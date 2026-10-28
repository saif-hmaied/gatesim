#include <gtest/gtest.h>

#include <string>
#include <string_view>
#include <vector>

#include "gatesim/check.hpp"
#include "gatesim/lexer.hpp"
#include "gatesim/parser.hpp"
#include "gatesim/timing.hpp"

using gatesim::analyze_timing;
using gatesim::gate_delay;
using gatesim::GateId;
using gatesim::GateType;
using gatesim::NetId;
using gatesim::Netlist;
using gatesim::TimingResult;

namespace {

// Lexes, parses and checks a source text. These tests are about timing,
// so the circuit must be valid and have a topological order.
struct Circuit {
    Netlist netlist;
    std::vector<GateId> order;
};

Circuit build(std::string_view source) {
    auto lexed = gatesim::lex(source);
    EXPECT_FALSE(lexed.error) << "lexer error: " << lexed.error->error_message;
    auto parsed = gatesim::parse(lexed.tokens);
    EXPECT_FALSE(parsed.error) << "parse error: " << parsed.error->message;
    auto checked = gatesim::check(parsed.G);
    EXPECT_TRUE(checked.diagnostics.empty()) << "check found problems";
    return {parsed.G, checked.topsort};
}

TimingResult run(const Circuit& c) {
    return analyze_timing(c.netlist, c.order);
}

double arrival_of(const Circuit& c, const TimingResult& t, const std::string& net) {
    return t.arrival[c.netlist.m.at(net)];
}

std::vector<std::string> path_names(const Circuit& c, const TimingResult& t) {
    std::vector<std::string> names;
    for (NetId net : t.critical_path) {
        names.push_back(c.netlist.wires[net].name);
    }
    return names;
}

const char* kFullAdder =
    "module full_adder(a,b,cin,cout,sum);\n"
    "    input cin,a,b;\n"
    "    output cout,sum;\n"
    "    wire w1,w2,w3;\n"
    "    xor g1(w1,a,b);\n"
    "    xor g2(sum,cin,w1);\n"
    "    and g3(w2,a,b);\n"
    "    and g4(w3,w1,cin);\n"
    "    or g5(cout,w3,w2);\n"
    "endmodule\n";

}  // namespace

TEST(TimingTest, GateDelayPerType) {
    EXPECT_DOUBLE_EQ(gate_delay(GateType::Not), 1);
    EXPECT_DOUBLE_EQ(gate_delay(GateType::Buf), 1);
    EXPECT_DOUBLE_EQ(gate_delay(GateType::Nand), 1.5);
    EXPECT_DOUBLE_EQ(gate_delay(GateType::Nor), 1.5);
    EXPECT_DOUBLE_EQ(gate_delay(GateType::And), 2);
    EXPECT_DOUBLE_EQ(gate_delay(GateType::Or), 2);
    EXPECT_DOUBLE_EQ(gate_delay(GateType::Xor), 3);
    EXPECT_DOUBLE_EQ(gate_delay(GateType::Xnor), 3);
}

TEST(TimingTest, InputsArriveAtZero) {
    Circuit c = build(kFullAdder);
    TimingResult t = run(c);
    EXPECT_DOUBLE_EQ(arrival_of(c, t, "a"), 0);
    EXPECT_DOUBLE_EQ(arrival_of(c, t, "b"), 0);
    EXPECT_DOUBLE_EQ(arrival_of(c, t, "cin"), 0);
}

TEST(TimingTest, OneArrivalPerNet) {
    Circuit c = build(kFullAdder);
    TimingResult t = run(c);
    EXPECT_EQ(t.arrival.size(), c.netlist.wires.size());
}

TEST(TimingTest, SingleGate) {
    Circuit c = build(
        "module m(a,b,y);\n"
        "    input a,b;\n"
        "    output y;\n"
        "    and g1(y,a,b);\n"
        "endmodule\n");
    TimingResult t = run(c);
    EXPECT_DOUBLE_EQ(arrival_of(c, t, "y"), 2);
    EXPECT_DOUBLE_EQ(t.worst_arrival, 2);
    EXPECT_EQ(path_names(c, t), (std::vector<std::string>{"a", "y"}));
}

TEST(TimingTest, BufPath) {
    Circuit c = build(
        "module m(a,y);\n"
        "    input a;\n"
        "    output y;\n"
        "    buf g1(y,a);\n"
        "endmodule\n");
    TimingResult t = run(c);
    EXPECT_DOUBLE_EQ(t.worst_arrival, 1);
    EXPECT_EQ(path_names(c, t), (std::vector<std::string>{"a", "y"}));
}

TEST(TimingTest, ChainAddsDelays) {
    Circuit c = build(
        "module m(a,y);\n"
        "    input a;\n"
        "    output y;\n"
        "    wire w1,w2;\n"
        "    not g1(w1,a);\n"
        "    not g2(w2,w1);\n"
        "    not g3(y,w2);\n"
        "endmodule\n");
    TimingResult t = run(c);
    EXPECT_DOUBLE_EQ(arrival_of(c, t, "w1"), 1);
    EXPECT_DOUBLE_EQ(arrival_of(c, t, "w2"), 2);
    EXPECT_DOUBLE_EQ(arrival_of(c, t, "y"), 3);
    EXPECT_EQ(path_names(c, t), (std::vector<std::string>{"a", "w1", "w2", "y"}));
}

TEST(TimingTest, FractionalDelays) {
    Circuit c = build(
        "module m(a,b,y);\n"
        "    input a,b;\n"
        "    output y;\n"
        "    wire w;\n"
        "    nand g1(w,a,b);\n"
        "    nor g2(y,w,a);\n"
        "endmodule\n");
    TimingResult t = run(c);
    EXPECT_DOUBLE_EQ(arrival_of(c, t, "w"), 1.5);
    EXPECT_DOUBLE_EQ(arrival_of(c, t, "y"), 3);
}

// The gate waits for its slowest input: max(1, 3) + 2 = 5, not 1 + 3 + 2 = 6.
TEST(TimingTest, TakesMaxNotSum) {
    Circuit c = build(
        "module m(a,b,c,y);\n"
        "    input a,b,c;\n"
        "    output y;\n"
        "    wire w1,w2;\n"
        "    not g1(w1,a);\n"
        "    xor g2(w2,b,c);\n"
        "    and g3(y,w1,w2);\n"
        "endmodule\n");
    TimingResult t = run(c);
    EXPECT_DOUBLE_EQ(arrival_of(c, t, "w1"), 1);
    EXPECT_DOUBLE_EQ(arrival_of(c, t, "w2"), 3);
    EXPECT_DOUBLE_EQ(arrival_of(c, t, "y"), 5);
    EXPECT_EQ(path_names(c, t), (std::vector<std::string>{"b", "w2", "y"}));
}

// The slowest input of a wide gate is its last one, so the path must go through it.
TEST(TimingTest, WideGateFollowsSlowestInput) {
    Circuit c = build(
        "module m(a,b,c,y);\n"
        "    input a,b,c;\n"
        "    output y;\n"
        "    wire w1,w2;\n"
        "    not g1(w1,c);\n"
        "    not g2(w2,w1);\n"
        "    and g3(y,a,b,w2);\n"
        "endmodule\n");
    TimingResult t = run(c);
    EXPECT_DOUBLE_EQ(arrival_of(c, t, "y"), 4);
    EXPECT_EQ(path_names(c, t), (std::vector<std::string>{"c", "w1", "w2", "y"}));
}

TEST(TimingTest, FullAdderArrivals) {
    Circuit c = build(kFullAdder);
    TimingResult t = run(c);
    EXPECT_DOUBLE_EQ(arrival_of(c, t, "w1"), 3);
    EXPECT_DOUBLE_EQ(arrival_of(c, t, "w2"), 2);
    EXPECT_DOUBLE_EQ(arrival_of(c, t, "w3"), 5);
    EXPECT_DOUBLE_EQ(arrival_of(c, t, "sum"), 6);
    EXPECT_DOUBLE_EQ(arrival_of(c, t, "cout"), 7);
}

TEST(TimingTest, FullAdderCriticalPath) {
    Circuit c = build(kFullAdder);
    TimingResult t = run(c);
    EXPECT_DOUBLE_EQ(t.worst_arrival, 7);
    EXPECT_EQ(path_names(c, t), (std::vector<std::string>{"a", "w1", "w3", "cout"}));
}

// The slow output is listed second, so the code must switch away from the first one.
TEST(TimingTest, PicksSlowestOutput) {
    Circuit c = build(
        "module m(a,b,fast,slow);\n"
        "    input a,b;\n"
        "    output fast,slow;\n"
        "    buf g1(fast,a);\n"
        "    xor g2(slow,a,b);\n"
        "endmodule\n");
    TimingResult t = run(c);
    EXPECT_DOUBLE_EQ(t.worst_arrival, 3);
    EXPECT_EQ(path_names(c, t), (std::vector<std::string>{"a", "slow"}));
}

// Two outputs arrive at the same time: the first one in the header wins.
TEST(TimingTest, TieKeepsFirstOutput) {
    Circuit c = build(
        "module m(a,b,y1,y2);\n"
        "    input a,b;\n"
        "    output y1,y2;\n"
        "    and g1(y1,a,b);\n"
        "    or g2(y2,a,b);\n"
        "endmodule\n");
    TimingResult t = run(c);
    EXPECT_DOUBLE_EQ(t.worst_arrival, 2);
    EXPECT_EQ(path_names(c, t), (std::vector<std::string>{"a", "y1"}));
}

TEST(TimingTest, PathGoesFromInputToOutput) {
    Circuit c = build(kFullAdder);
    TimingResult t = run(c);
    ASSERT_FALSE(t.critical_path.empty());
    EXPECT_EQ(c.netlist.wires[t.critical_path.front()].type, gatesim::NetType::Input);
    EXPECT_EQ(c.netlist.wires[t.critical_path.back()].type, gatesim::NetType::Output);
}

TEST(TimingTest, EmptyNetlist) {
    Netlist empty;
    TimingResult t = analyze_timing(empty, {});
    EXPECT_TRUE(t.arrival.empty());
    EXPECT_TRUE(t.critical_path.empty());
    EXPECT_DOUBLE_EQ(t.worst_arrival, 0);
}
