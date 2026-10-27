#include <gtest/gtest.h>

#include <string_view>
#include <vector>

#include "gatesim/check.hpp"
#include "gatesim/lexer.hpp"
#include "gatesim/parser.hpp"
#include "gatesim/simulator.hpp"

using gatesim::evaluate;
using gatesim::GateId;
using gatesim::GateType;
using gatesim::Netlist;

namespace {

// Lexes, parses and checks a source text. These tests are about the simulator,
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

std::vector<bool> run(const Circuit& c, const std::vector<bool>& inputs) {
    return gatesim::simulate(c.netlist, c.order, inputs);
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

// =====================================================================
// evaluate(): one gate
// =====================================================================

// Checks a 2-input gate on 00, 01, 10, 11.
void expectTwoInputTable(GateType type, bool r00, bool r01, bool r10, bool r11) {
    EXPECT_EQ(evaluate(type, {false, false}), r00);
    EXPECT_EQ(evaluate(type, {false, true}), r01);
    EXPECT_EQ(evaluate(type, {true, false}), r10);
    EXPECT_EQ(evaluate(type, {true, true}), r11);
}

TEST(SimulatorTest, AndTable)  { expectTwoInputTable(GateType::And,  0, 0, 0, 1); }
TEST(SimulatorTest, OrTable)   { expectTwoInputTable(GateType::Or,   0, 1, 1, 1); }
TEST(SimulatorTest, XorTable)  { expectTwoInputTable(GateType::Xor,  0, 1, 1, 0); }
TEST(SimulatorTest, NandTable) { expectTwoInputTable(GateType::Nand, 1, 1, 1, 0); }
TEST(SimulatorTest, NorTable)  { expectTwoInputTable(GateType::Nor,  1, 0, 0, 0); }
TEST(SimulatorTest, XnorTable) { expectTwoInputTable(GateType::Xnor, 1, 0, 0, 1); }

TEST(SimulatorTest, NotAndBuf) {
    EXPECT_TRUE(evaluate(GateType::Not, {false}));
    EXPECT_FALSE(evaluate(GateType::Not, {true}));
    EXPECT_FALSE(evaluate(GateType::Buf, {false}));
    EXPECT_TRUE(evaluate(GateType::Buf, {true}));
}

TEST(SimulatorTest, ThreeInputXorIsParity) {
    // 1 when the number of 1s is odd.
    EXPECT_FALSE(evaluate(GateType::Xor, {false, false, false}));
    EXPECT_TRUE(evaluate(GateType::Xor, {true, false, false}));
    EXPECT_FALSE(evaluate(GateType::Xor, {true, true, false}));
    EXPECT_TRUE(evaluate(GateType::Xor, {true, true, true}));
}

TEST(SimulatorTest, WideAndAndOr) {
    EXPECT_TRUE(evaluate(GateType::And, {true, true, true, true}));
    EXPECT_FALSE(evaluate(GateType::And, {true, true, false, true}));
    EXPECT_FALSE(evaluate(GateType::Or, {false, false, false, false}));
    EXPECT_TRUE(evaluate(GateType::Or, {false, false, true, false}));
}

// =====================================================================
// simulate(): a whole circuit
// =====================================================================

TEST(SimulatorTest, FullAdderTruthTable) {
    // cout sum must be a + b + cin in binary, for all 8 combinations.
    auto circuit = build(kFullAdder);
    for (int m = 0; m < 8; m++) {
        bool a = m & 4, b = m & 2, cin = m & 1;
        int total = a + b + cin;
        auto out = run(circuit, {a, b, cin});  // inputs in header order: a, b, cin
        ASSERT_EQ(out.size(), 2u);
        EXPECT_EQ(out[0], total >= 2) << "cout for a=" << a << " b=" << b << " cin=" << cin;
        EXPECT_EQ(out[1], total % 2 == 1) << "sum for a=" << a << " b=" << b << " cin=" << cin;
    }
}

TEST(SimulatorTest, InputsFollowHeaderOrder) {
    // The header says (x,y,...) but the declaration says "input y,x".
    // The first value must go to x, the second to y.
    auto circuit = build(
        "module m(x,y,o,p);\n"
        "input y,x;\n"
        "output o,p;\n"
        "buf g1(o,x);\n"
        "not g2(p,y);\n"
        "endmodule\n");
    auto out = run(circuit, {true, false});  // x=1, y=0
    ASSERT_EQ(out.size(), 2u);
    EXPECT_TRUE(out[0]);   // o = x = 1
    EXPECT_TRUE(out[1]);   // p = not y = 1
}

TEST(SimulatorTest, OutputsFollowHeaderOrder) {
    // The header says (...,p,o) but the declaration says "output o,p".
    auto circuit = build(
        "module m(a,p,o);\n"
        "input a;\n"
        "output o,p;\n"
        "buf g1(o,a);\n"
        "not g2(p,a);\n"
        "endmodule\n");
    auto out = run(circuit, {true});
    ASSERT_EQ(out.size(), 2u);
    EXPECT_FALSE(out[0]);  // p = not a = 0
    EXPECT_TRUE(out[1]);   // o = a = 1
}

TEST(SimulatorTest, GatesWrittenOutOfOrder) {
    // g2 is written before g1, but it needs g1's output: the topological order fixes that.
    auto circuit = build(
        "module m(a,y);\n"
        "input a;\n"
        "output y;\n"
        "wire w;\n"
        "buf g2(y,w);\n"
        "not g1(w,a);\n"
        "endmodule\n");
    EXPECT_TRUE(run(circuit, {false})[0]);   // w = not 0 = 1, y = 1
    EXPECT_FALSE(run(circuit, {true})[0]);   // w = not 1 = 0, y = 0
}

TEST(SimulatorTest, WideGateInCircuit) {
    auto circuit = build(
        "module m(a,b,c,d,y);\n"
        "input a,b,c,d;\n"
        "output y;\n"
        "and g(y,a,b,c,d);\n"
        "endmodule\n");
    EXPECT_TRUE(run(circuit, {true, true, true, true})[0]);
    EXPECT_FALSE(run(circuit, {true, true, false, true})[0]);
}
