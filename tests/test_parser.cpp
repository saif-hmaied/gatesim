#include <gtest/gtest.h>

#include <string>
#include <string_view>
#include <vector>

#include "gatesim/lexer.hpp"
#include "gatesim/parser.hpp"

using gatesim::Gate;
using gatesim::GateId;
using gatesim::GateType;
using gatesim::Net;
using gatesim::NetId;
using gatesim::Netlist;
using gatesim::NetType;
using gatesim::ParseResult;

namespace {

// Lexes and parses a source text. The lexer must succeed: these tests are about the parser.
ParseResult parseSource(std::string_view source) {
    auto lexed = gatesim::lex(source);
    EXPECT_FALSE(lexed.error) << "lexer error: " << lexed.error->error_message;
    return gatesim::parse(lexed.tokens);
}

// Finds a net by name, using the netlist's own name -> id map.
const Net& netNamed(const Netlist& netlist, const std::string& name) {
    return netlist.wires.at(netlist.m.at(name));
}

NetId netId(const Netlist& netlist, const std::string& name) {
    return netlist.m.at(name);
}

// Finds a gate by name. Gate IDs are positions in netlist.Gates.
GateId gateId(const Netlist& netlist, const std::string& name) {
    for (std::size_t i = 0; i < netlist.Gates.size(); i++) {
        if (netlist.Gates[i].name == name) {
            return static_cast<GateId>(i);
        }
    }
    ADD_FAILURE() << "no gate named " << name;
    return 0;
}

const Gate& gateNamed(const Netlist& netlist, const std::string& name) {
    return netlist.Gates.at(gateId(netlist, name));
}

// Turns a list of net IDs back into names, so port order is easy to compare.
std::vector<std::string> namesOf(const Netlist& netlist, const std::vector<NetId>& ids) {
    std::vector<std::string> names;
    for (NetId id : ids) {
        names.push_back(netlist.wires.at(id).name);
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

// =====================================================================
// Valid files
// =====================================================================

TEST(ParserTest, MinimalModule) {
    auto result = parseSource(
        "module m(a,y);\n"
        "input a;\n"
        "output y;\n"
        "buf g(y,a);\n"
        "endmodule\n");
    ASSERT_FALSE(result.error) << result.error->message;
    EXPECT_EQ(result.G.name, "m");
    EXPECT_EQ(result.G.Gates.size(), 1u);
    EXPECT_EQ(result.G.wires.size(), 2u);
}

TEST(ParserTest, FullAdderCounts) {
    auto result = parseSource(kFullAdder);
    ASSERT_FALSE(result.error) << result.error->message;
    EXPECT_EQ(result.G.name, "full_adder");
    EXPECT_EQ(result.G.Gates.size(), 5u);
    EXPECT_EQ(result.G.wires.size(), 8u);  // a b cin cout sum w1 w2 w3
}

TEST(ParserTest, NameMapHasEveryNet) {
    auto result = parseSource(kFullAdder);
    ASSERT_FALSE(result.error) << result.error->message;
    const auto& g = result.G;
    EXPECT_EQ(g.m.size(), g.wires.size());
    for (const char* name : {"a", "b", "cin", "cout", "sum", "w1", "w2", "w3"}) {
        ASSERT_TRUE(g.m.count(name)) << "missing net " << name;
        EXPECT_EQ(netNamed(g, name).name, name);
    }
}

TEST(ParserTest, PortOrderComesFromModuleHeader) {
    // The header says (a,b,cin,...) but the declaration says "input cin,a,b".
    // The truth table columns must follow the header.
    auto result = parseSource(kFullAdder);
    ASSERT_FALSE(result.error) << result.error->message;
    EXPECT_EQ(namesOf(result.G, result.G.input),
              (std::vector<std::string>{"a", "b", "cin"}));
    EXPECT_EQ(namesOf(result.G, result.G.output),
              (std::vector<std::string>{"cout", "sum"}));
}

TEST(ParserTest, NetTypes) {
    auto result = parseSource(kFullAdder);
    ASSERT_FALSE(result.error) << result.error->message;
    const auto& g = result.G;
    EXPECT_EQ(netNamed(g, "a").type, NetType::Input);
    EXPECT_EQ(netNamed(g, "cin").type, NetType::Input);
    EXPECT_EQ(netNamed(g, "cout").type, NetType::Output);
    EXPECT_EQ(netNamed(g, "sum").type, NetType::Output);
    EXPECT_EQ(netNamed(g, "w1").type, NetType::Wire);
}

TEST(ParserTest, GatePins) {
    // xor g1(w1,a,b): the first pin is the output, the others are inputs, in order.
    auto result = parseSource(kFullAdder);
    ASSERT_FALSE(result.error) << result.error->message;
    const auto& g = result.G;
    const Gate g1 = gateNamed(g, "g1");
    EXPECT_EQ(g1.type, GateType::Xor);
    EXPECT_EQ(g1.out, netId(g, "w1"));
    EXPECT_EQ(g1.in, (std::vector<NetId>{netId(g, "a"), netId(g, "b")}));

    const Gate g5 = gateNamed(g, "g5");
    EXPECT_EQ(g5.type, GateType::Or);
    EXPECT_EQ(g5.out, netId(g, "cout"));
    EXPECT_EQ(g5.in, (std::vector<NetId>{netId(g, "w3"), netId(g, "w2")}));
}

TEST(ParserTest, DriversAndReaders) {
    // Net::in = gates that drive the net, Net::out = gates that read it.
    auto result = parseSource(kFullAdder);
    ASSERT_FALSE(result.error) << result.error->message;
    const auto& g = result.G;

    const Net w1 = netNamed(g, "w1");
    EXPECT_EQ(w1.in, (std::vector<GateId>{gateId(g, "g1")}));
    EXPECT_EQ(w1.out, (std::vector<GateId>{gateId(g, "g2"), gateId(g, "g4")}));

    // An input has no driver inside the circuit.
    const Net a = netNamed(g, "a");
    EXPECT_TRUE(a.in.empty());
    EXPECT_EQ(a.out, (std::vector<GateId>{gateId(g, "g1"), gateId(g, "g3")}));

    // An output is driven, and here nobody inside the circuit reads it.
    const Net cout = netNamed(g, "cout");
    EXPECT_EQ(cout.in, (std::vector<GateId>{gateId(g, "g5")}));
    EXPECT_TRUE(cout.out.empty());
}

TEST(ParserTest, AllGateTypes) {
    auto result = parseSource(
        "module m(a,b,y1,y2,y3,y4,y5,y6,y7,y8);\n"
        "input a,b;\n"
        "output y1,y2,y3,y4,y5,y6,y7,y8;\n"
        "and g1(y1,a,b);\n"
        "or g2(y2,a,b);\n"
        "not g3(y3,a);\n"
        "nand g4(y4,a,b);\n"
        "nor g5(y5,a,b);\n"
        "xor g6(y6,a,b);\n"
        "xnor g7(y7,a,b);\n"
        "buf g8(y8,a);\n"
        "endmodule\n");
    ASSERT_FALSE(result.error) << result.error->message;
    const auto& g = result.G;
    EXPECT_EQ(gateNamed(g, "g1").type, GateType::And);
    EXPECT_EQ(gateNamed(g, "g2").type, GateType::Or);
    EXPECT_EQ(gateNamed(g, "g3").type, GateType::Not);
    EXPECT_EQ(gateNamed(g, "g4").type, GateType::Nand);
    EXPECT_EQ(gateNamed(g, "g5").type, GateType::Nor);
    EXPECT_EQ(gateNamed(g, "g6").type, GateType::Xor);
    EXPECT_EQ(gateNamed(g, "g7").type, GateType::Xnor);
    EXPECT_EQ(gateNamed(g, "g8").type, GateType::Buf);
}

TEST(ParserTest, GateWithManyInputs) {
    auto result = parseSource(
        "module m(a,b,c,d,y);\n"
        "input a,b,c,d;\n"
        "output y;\n"
        "and g(y,a,b,c,d);\n"
        "endmodule\n");
    ASSERT_FALSE(result.error) << result.error->message;
    const auto& g = result.G;
    EXPECT_EQ(gateNamed(g, "g").in,
              (std::vector<NetId>{netId(g, "a"), netId(g, "b"), netId(g, "c"), netId(g, "d")}));
}

TEST(ParserTest, DeclarationAfterGateIsAllowed) {
    // Declare before use: "wire v" comes after g1, but before g2 uses it.
    auto result = parseSource(
        "module m(a,y);\n"
        "input a;\n"
        "output y;\n"
        "wire w;\n"
        "not g1(w,a);\n"
        "wire v;\n"
        "not g2(v,w);\n"
        "buf g3(y,v);\n"
        "endmodule\n");
    ASSERT_FALSE(result.error) << result.error->message;
    EXPECT_EQ(result.G.Gates.size(), 3u);
}

TEST(ParserTest, MultipleDriversAreAccepted) {
    // A short circuit is a circuit problem, not a grammar problem: `check` reports it.
    auto result = parseSource(
        "module m(a,b,y);\n"
        "input a,b;\n"
        "output y;\n"
        "and g1(y,a,b);\n"
        "or g2(y,a,b);\n"
        "endmodule\n");
    ASSERT_FALSE(result.error) << result.error->message;
    const auto& g = result.G;
    EXPECT_EQ(netNamed(g, "y").in,
              (std::vector<GateId>{gateId(g, "g1"), gateId(g, "g2")}));
}

TEST(ParserTest, UnusedNetIsAccepted) {
    // An unused wire is also a `check` problem, not a parse error.
    auto result = parseSource(
        "module m(a,y);\n"
        "input a;\n"
        "output y;\n"
        "wire u;\n"
        "buf g(y,a);\n"
        "endmodule\n");
    ASSERT_FALSE(result.error) << result.error->message;
    const Net u = netNamed(result.G, "u");
    EXPECT_TRUE(u.in.empty());
    EXPECT_TRUE(u.out.empty());
}

// =====================================================================
// Errors: the parser stops at the first one, and points at the token
// where it found the problem.
// =====================================================================

TEST(ParserTest, EmptyInputIsError) {
    auto result = parseSource("");
    ASSERT_TRUE(result.error);
    EXPECT_EQ(result.error->line, 1);
    EXPECT_EQ(result.error->column, 1);
}

TEST(ParserTest, MissingModuleKeyword) {
    auto result = parseSource("xor g1(w1,a,b);");
    ASSERT_TRUE(result.error);
    EXPECT_EQ(result.error->line, 1);
    EXPECT_EQ(result.error->column, 1);
}

TEST(ParserTest, MissingModuleName) {
    auto result = parseSource("module (a);");
    ASSERT_TRUE(result.error);
    EXPECT_EQ(result.error->line, 1);
    EXPECT_EQ(result.error->column, 8);  // the '('
}

TEST(ParserTest, EmptyPortListIsError) {
    auto result = parseSource("module m();\nendmodule\n");
    ASSERT_TRUE(result.error);
    EXPECT_EQ(result.error->line, 1);
    EXPECT_EQ(result.error->column, 10);  // the ')'
}

TEST(ParserTest, MissingCommaBetweenPorts) {
    auto result = parseSource("module m(a b);");
    ASSERT_TRUE(result.error);
    EXPECT_EQ(result.error->line, 1);
    EXPECT_EQ(result.error->column, 12);  // the 'b'
}

TEST(ParserTest, MissingSemicolonAfterHeader) {
    auto result = parseSource("module m(a)\ninput a;\nendmodule\n");
    ASSERT_TRUE(result.error);
    EXPECT_EQ(result.error->line, 2);
    EXPECT_EQ(result.error->column, 1);  // the 'input'
}

TEST(ParserTest, MissingSemicolonAfterDeclaration) {
    auto result = parseSource("module m(a);\ninput a\nendmodule\n");
    ASSERT_TRUE(result.error);
    EXPECT_EQ(result.error->line, 3);
    EXPECT_EQ(result.error->column, 1);  // the 'endmodule'
}

TEST(ParserTest, MissingGateName) {
    auto result = parseSource(
        "module m(a,y);\n"
        "input a;\n"
        "output y;\n"
        "xor (y,a,a);\n"
        "endmodule\n");
    ASSERT_TRUE(result.error);
    EXPECT_EQ(result.error->line, 4);
    EXPECT_EQ(result.error->column, 5);  // the '('
}

TEST(ParserTest, GateWithoutInputsIsError) {
    auto result = parseSource(
        "module m(a,y);\n"
        "input a;\n"
        "output y;\n"
        "xor g(y);\n"
        "endmodule\n");
    ASSERT_TRUE(result.error);
    EXPECT_EQ(result.error->line, 4);
    EXPECT_EQ(result.error->column, 8);  // the ')'
}

TEST(ParserTest, MissingSemicolonAfterGate) {
    auto result = parseSource(
        "module m(a,y);\n"
        "input a;\n"
        "output y;\n"
        "buf g(y,a)\n"
        "endmodule\n");
    ASSERT_TRUE(result.error);
    EXPECT_EQ(result.error->line, 5);
    EXPECT_EQ(result.error->column, 1);  // the 'endmodule'
}

TEST(ParserTest, UnknownStatement) {
    auto result = parseSource(
        "module m(a,y);\n"
        "input a;\n"
        "output y;\n"
        "foo g(y,a);\n"
        "endmodule\n");
    ASSERT_TRUE(result.error);
    EXPECT_EQ(result.error->line, 4);
    EXPECT_EQ(result.error->column, 1);  // the 'foo'
}

TEST(ParserTest, UndeclaredInputNet) {
    auto result = parseSource(
        "module m(a,y);\n"
        "input a;\n"
        "output y;\n"
        "and g(y,a,w4);\n"
        "endmodule\n");
    ASSERT_TRUE(result.error);
    EXPECT_EQ(result.error->line, 4);
    EXPECT_EQ(result.error->column, 11);  // the 'w4'
    EXPECT_NE(result.error->message.find("w4"), std::string::npos);
}

TEST(ParserTest, UndeclaredOutputNet) {
    auto result = parseSource(
        "module m(a,y);\n"
        "input a;\n"
        "output y;\n"
        "buf g(z,a);\n"
        "endmodule\n");
    ASSERT_TRUE(result.error);
    EXPECT_EQ(result.error->line, 4);
    EXPECT_EQ(result.error->column, 7);  // the 'z'
}

TEST(ParserTest, NetUsedBeforeDeclarationIsError) {
    // Declare before use: w is declared, but only after the gate that uses it.
    auto result = parseSource(
        "module m(a,y);\n"
        "input a;\n"
        "output y;\n"
        "not g1(w,a);\n"
        "wire w;\n"
        "buf g2(y,w);\n"
        "endmodule\n");
    ASSERT_TRUE(result.error);
    EXPECT_EQ(result.error->line, 4);
    EXPECT_EQ(result.error->column, 8);  // the 'w'
}

TEST(ParserTest, DuplicateDeclaration) {
    auto result = parseSource(
        "module m(a,y);\n"
        "input a;\n"
        "output y;\n"
        "wire a;\n"
        "buf g(y,a);\n"
        "endmodule\n");
    ASSERT_TRUE(result.error);
    EXPECT_EQ(result.error->line, 4);
    EXPECT_EQ(result.error->column, 6);  // the second 'a'
}

TEST(ParserTest, PortWithoutDirection) {
    // y is in the header, but it is declared as a wire, not as input or output.
    auto result = parseSource(
        "module m(a,y);\n"
        "input a;\n"
        "wire y;\n"
        "buf g(y,a);\n"
        "endmodule\n");
    ASSERT_TRUE(result.error);
    EXPECT_EQ(result.error->line, 1);
    EXPECT_EQ(result.error->column, 12);  // the 'y' in the header
    EXPECT_NE(result.error->message.find("y"), std::string::npos);
}

TEST(ParserTest, InputThatIsNotAPort) {
    // b is declared as an input, but it is not in the header's port list.
    auto result = parseSource(
        "module m(a,y);\n"
        "input a, b;\n"
        "output y;\n"
        "and g(y,a,b);\n"
        "endmodule\n");
    ASSERT_TRUE(result.error);
    EXPECT_EQ(result.error->line, 2);
    EXPECT_EQ(result.error->column, 10);  // the 'b'
}

TEST(ParserTest, MissingEndmodule) {
    auto result = parseSource(
        "module m(a,y);\n"
        "input a;\n"
        "output y;\n"
        "buf g(y,a);\n");
    ASSERT_TRUE(result.error);
    EXPECT_EQ(result.error->line, 5);
    EXPECT_EQ(result.error->column, 1);  // EndOfFile
}

TEST(ParserTest, TokensAfterEndmoduleAreError) {
    auto result = parseSource(
        "module m(a,y);\n"
        "input a;\n"
        "output y;\n"
        "buf g(y,a);\n"
        "endmodule\n"
        "wire x;\n");
    ASSERT_TRUE(result.error);
    EXPECT_EQ(result.error->line, 6);
    EXPECT_EQ(result.error->column, 1);  // the 'wire'
}
