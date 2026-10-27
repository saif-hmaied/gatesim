#include <gtest/gtest.h>

#include <string>
#include <string_view>
#include <vector>

#include "gatesim/check.hpp"
#include "gatesim/lexer.hpp"
#include "gatesim/parser.hpp"

using gatesim::CheckResult;
using gatesim::GateId;
using gatesim::IssueKind;
using gatesim::Netlist;
using gatesim::Severity;

namespace {

// Lexes and parses a source text. These tests are about check(), so the file must be valid.
Netlist build(std::string_view source) {
    auto lexed = gatesim::lex(source);
    EXPECT_FALSE(lexed.error) << "lexer error: " << lexed.error->error_message;
    auto parsed = gatesim::parse(lexed.tokens);
    EXPECT_FALSE(parsed.error) << "parse error: " << parsed.error->message;
    return parsed.G;
}

// How many diagnostics of this kind were reported.
int countOf(const CheckResult& result, IssueKind kind) {
    int count = 0;
    for (const auto& d : result.diagnostics) {
        if (d.issue == kind) count++;
    }
    return count;
}

// The first diagnostic of this kind (the test must check countOf first).
const gatesim::Diagnostic& firstOf(const CheckResult& result, IssueKind kind) {
    for (const auto& d : result.diagnostics) {
        if (d.issue == kind) return d;
    }
    return result.diagnostics.at(result.diagnostics.size());  // throws: no such diagnostic
}

bool contains(const std::string& text, const std::string& part) {
    return text.find(part) != std::string::npos;
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
// A correct circuit
// =====================================================================

TEST(CheckTest, FullAdderHasNoDiagnostics) {
    // Inputs have no driver and outputs have no reader: both are normal.
    auto result = gatesim::check(build(kFullAdder));
    EXPECT_TRUE(result.diagnostics.empty());
}

TEST(CheckTest, TopsortContainsEveryGateOnce) {
    auto netlist = build(kFullAdder);
    auto result = gatesim::check(netlist);
    ASSERT_EQ(result.topsort.size(), netlist.Gates.size());
    std::vector<int> seen(netlist.Gates.size(), 0);
    for (GateId g : result.topsort) {
        ASSERT_LT(g, netlist.Gates.size());
        seen[g]++;
    }
    for (int s : seen) EXPECT_EQ(s, 1);
}

TEST(CheckTest, TopsortPutsDriversFirst) {
    // Every gate must come after the gates that drive its inputs.
    auto netlist = build(kFullAdder);
    auto result = gatesim::check(netlist);
    ASSERT_EQ(result.topsort.size(), netlist.Gates.size());
    std::vector<std::size_t> position(netlist.Gates.size());
    for (std::size_t i = 0; i < result.topsort.size(); i++) {
        position[result.topsort[i]] = i;
    }
    for (std::size_t g = 0; g < netlist.Gates.size(); g++) {
        for (auto net : netlist.Gates[g].in) {
            for (GateId driver : netlist.wires[net].in) {
                EXPECT_LT(position[driver], position[g])
                    << netlist.Gates[driver].name << " must come before " << netlist.Gates[g].name;
            }
        }
    }
}

// =====================================================================
// Net rules
// =====================================================================

TEST(CheckTest, MultiDrivenNet) {
    auto result = gatesim::check(build(
        "module m(a,b,y);\n"
        "input a,b;\n"
        "output y;\n"
        "and g1(y,a,b);\n"
        "or g2(y,a,b);\n"
        "endmodule\n"));
    ASSERT_EQ(countOf(result, IssueKind::MultiDriven), 1);
    const auto& d = firstOf(result, IssueKind::MultiDriven);
    EXPECT_EQ(d.severity, Severity::Error);
    EXPECT_TRUE(contains(d.message, "y"));
    EXPECT_TRUE(contains(d.message, "g1"));
    EXPECT_TRUE(contains(d.message, "g2"));
}

TEST(CheckTest, UndrivenWireThatIsRead) {
    auto result = gatesim::check(build(
        "module m(a,y);\n"
        "input a;\n"
        "output y;\n"
        "wire w;\n"
        "and g1(y,a,w);\n"
        "endmodule\n"));
    ASSERT_EQ(countOf(result, IssueKind::Undriven), 1);
    const auto& d = firstOf(result, IssueKind::Undriven);
    EXPECT_EQ(d.severity, Severity::Error);
    EXPECT_TRUE(contains(d.message, "w"));
}

TEST(CheckTest, UndrivenOutput) {
    // Nobody inside reads an output, but the outside world needs its value.
    auto result = gatesim::check(build(
        "module m(a,y,z);\n"
        "input a;\n"
        "output y,z;\n"
        "buf g1(y,a);\n"
        "endmodule\n"));
    ASSERT_EQ(countOf(result, IssueKind::Undriven), 1);
    EXPECT_TRUE(contains(firstOf(result, IssueKind::Undriven).message, "z"));
}

TEST(CheckTest, UnconnectedWireIsOnlyUnused) {
    // No driver and no reader: it cannot affect anything, so it is only a warning.
    auto result = gatesim::check(build(
        "module m(a,y);\n"
        "input a;\n"
        "output y;\n"
        "wire u;\n"
        "buf g1(y,a);\n"
        "endmodule\n"));
    EXPECT_EQ(countOf(result, IssueKind::Undriven), 0);
    ASSERT_EQ(countOf(result, IssueKind::Unused), 1);
    const auto& d = firstOf(result, IssueKind::Unused);
    EXPECT_EQ(d.severity, Severity::Warning);
    EXPECT_TRUE(contains(d.message, "u"));
}

TEST(CheckTest, UnusedInputIsWarning) {
    auto result = gatesim::check(build(
        "module m(a,b,y);\n"
        "input a,b;\n"
        "output y;\n"
        "buf g1(y,a);\n"
        "endmodule\n"));
    ASSERT_EQ(countOf(result, IssueKind::Unused), 1);
    const auto& d = firstOf(result, IssueKind::Unused);
    EXPECT_EQ(d.severity, Severity::Warning);
    EXPECT_TRUE(contains(d.message, "b"));
    EXPECT_FALSE(result.topsort.empty());  // a warning does not stop the order
}

TEST(CheckTest, DrivenInput) {
    // The pins of g1 are in the wrong order: it drives the input 'a'.
    auto result = gatesim::check(build(
        "module m(a,b,y);\n"
        "input a,b;\n"
        "output y;\n"
        "and g1(a,b,b);\n"
        "buf g2(y,b);\n"
        "endmodule\n"));
    ASSERT_EQ(countOf(result, IssueKind::DrivenInput), 1);
    const auto& d = firstOf(result, IssueKind::DrivenInput);
    EXPECT_EQ(d.severity, Severity::Error);
    EXPECT_TRUE(contains(d.message, "a"));
    EXPECT_TRUE(contains(d.message, "g1"));
    EXPECT_EQ(countOf(result, IssueKind::MultiDriven), 0);
}

TEST(CheckTest, AllProblemsAreReportedAtOnce) {
    auto result = gatesim::check(build(
        "module m(a,b,c,y,z);\n"
        "input a,b,c;\n"
        "output y,z;\n"
        "wire w2,w5,u;\n"
        "and g3(w2,a,b);\n"
        "or g6(w2,a,b);\n"
        "and g7(y,w2,w5);\n"
        "endmodule\n"));
    EXPECT_EQ(countOf(result, IssueKind::MultiDriven), 1);  // w2
    EXPECT_EQ(countOf(result, IssueKind::Undriven), 2);     // w5 and z
    EXPECT_EQ(countOf(result, IssueKind::Unused), 2);       // c and u
}

// =====================================================================
// Loops
// =====================================================================

TEST(CheckTest, SimpleLoop) {
    auto result = gatesim::check(build(
        "module m(a,y);\n"
        "input a;\n"
        "output y;\n"
        "wire w1,w2;\n"
        "and g1(w1,a,w2);\n"
        "not g2(w2,w1);\n"
        "buf g3(y,w1);\n"
        "endmodule\n"));
    ASSERT_EQ(countOf(result, IssueKind::Loop), 1);
    const auto& d = firstOf(result, IssueKind::Loop);
    EXPECT_EQ(d.severity, Severity::Error);
    EXPECT_TRUE(contains(d.message, "g1"));
    EXPECT_TRUE(contains(d.message, "g2"));
    EXPECT_FALSE(contains(d.message, "g3"));  // g3 is after the loop, not in it
    EXPECT_TRUE(result.topsort.empty());      // no valid order with a loop
}

TEST(CheckTest, SelfLoop) {
    // The gate reads its own output.
    auto result = gatesim::check(build(
        "module m(a,y);\n"
        "input a;\n"
        "output y;\n"
        "and g(y,y,a);\n"
        "endmodule\n"));
    ASSERT_EQ(countOf(result, IssueKind::Loop), 1);
    EXPECT_TRUE(contains(firstOf(result, IssueKind::Loop).message, "g"));
    EXPECT_TRUE(result.topsort.empty());
}

TEST(CheckTest, TwoSeparateLoopsGiveTwoErrors) {
    auto result = gatesim::check(build(
        "module m(a,b,y,z);\n"
        "input a,b;\n"
        "output y,z;\n"
        "wire w1,w2,w3,w4;\n"
        "and g1(w1,a,w2);\n"
        "not g2(w2,w1);\n"
        "and g3(w3,b,w4);\n"
        "not g4(w4,w3);\n"
        "buf g5(y,w1);\n"
        "buf g6(z,w3);\n"
        "endmodule\n"));
    EXPECT_EQ(countOf(result, IssueKind::Loop), 2);
}

TEST(CheckTest, OverlappingLoopsGiveOneError) {
    // g1<->g2 and g1<->g3 share g1: one group of connected gates, so one error.
    auto result = gatesim::check(build(
        "module m(a,y);\n"
        "input a;\n"
        "output y;\n"
        "wire w1,w2,w3;\n"
        "and g1(w1,a,w2,w3);\n"
        "not g2(w2,w1);\n"
        "not g3(w3,w1);\n"
        "buf g4(y,w1);\n"
        "endmodule\n"));
    EXPECT_EQ(countOf(result, IssueKind::Loop), 1);
}

TEST(CheckTest, LoopAndSelfLoopGiveTwoErrors) {
    auto result = gatesim::check(build(
        "module m(a,y,z);\n"
        "input a;\n"
        "output y,z;\n"
        "wire w1,w2;\n"
        "and g1(w1,a,w2);\n"
        "not g2(w2,w1);\n"
        "buf g3(y,w1);\n"
        "and g4(z,z,a);\n"
        "endmodule\n"));
    EXPECT_EQ(countOf(result, IssueKind::Loop), 2);
}
