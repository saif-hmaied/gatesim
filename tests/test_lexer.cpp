#include <gtest/gtest.h>

#include <string>
#include <vector>

#include "gatesim/lexer.hpp"

using gatesim::lex;
using gatesim::LexResult;
using gatesim::TokenKind;

namespace {

// Returns only the kinds of all tokens, so a test can compare a whole list at once.
std::vector<TokenKind> kindsOf(const LexResult& result) {
    std::vector<TokenKind> kinds;
    for (const auto& token : result.tokens) {
        kinds.push_back(token.kind);
    }
    return kinds;
}

}  // namespace

// ---------- Basics ----------

TEST(LexerTest, EmptyInputGivesOnlyEndOfFile) {
    auto result = lex("");
    ASSERT_FALSE(result.error);
    ASSERT_EQ(result.tokens.size(), 1u);
    EXPECT_EQ(result.tokens[0].kind, TokenKind::EndOfFile);
    EXPECT_EQ(result.tokens[0].line, 1);
    EXPECT_EQ(result.tokens[0].column, 1);
}

TEST(LexerTest, OnlyWhitespaceGivesOnlyEndOfFile) {
    auto result = lex("  \n\t \n");
    ASSERT_FALSE(result.error);
    EXPECT_EQ(kindsOf(result), std::vector<TokenKind>{TokenKind::EndOfFile});
}

// ---------- Keywords ----------

TEST(LexerTest, AllKeywords) {
    auto result = lex("module endmodule input output wire");
    ASSERT_FALSE(result.error);
    std::vector<TokenKind> expected = {
        TokenKind::Module, TokenKind::EndModule, TokenKind::Input,
        TokenKind::Output, TokenKind::Wire,      TokenKind::EndOfFile,
    };
    EXPECT_EQ(kindsOf(result), expected);
}

TEST(LexerTest, AllGateTypes) {
    auto result = lex("and or not nand nor xor xnor buf");
    ASSERT_FALSE(result.error);
    std::vector<TokenKind> expected = {
        TokenKind::And, TokenKind::Or,  TokenKind::Not,  TokenKind::Nand,
        TokenKind::Nor, TokenKind::Xor, TokenKind::Xnor, TokenKind::Buf,
        TokenKind::EndOfFile,
    };
    EXPECT_EQ(kindsOf(result), expected);
}

TEST(LexerTest, KeywordTextIsKept) {
    auto result = lex("module");
    ASSERT_FALSE(result.error);
    ASSERT_EQ(result.tokens.size(), 2u);
    EXPECT_EQ(result.tokens[0].text, "module");
}

// ---------- Identifiers ----------

TEST(LexerTest, IdentifierKeepsText) {
    auto result = lex("w1");
    ASSERT_FALSE(result.error);
    ASSERT_EQ(result.tokens.size(), 2u);
    EXPECT_EQ(result.tokens[0].kind, TokenKind::Identifier);
    EXPECT_EQ(result.tokens[0].text, "w1");
}

TEST(LexerTest, IdentifierWithUnderscore) {
    auto result = lex("full_adder _tmp");
    ASSERT_FALSE(result.error);
    ASSERT_EQ(result.tokens.size(), 3u);
    EXPECT_EQ(result.tokens[0].kind, TokenKind::Identifier);
    EXPECT_EQ(result.tokens[0].text, "full_adder");
    EXPECT_EQ(result.tokens[1].kind, TokenKind::Identifier);
    EXPECT_EQ(result.tokens[1].text, "_tmp");
}

TEST(LexerTest, KeywordPrefixIsIdentifier) {
    // "modules" and "xor1" start like keywords, but they are names.
    auto result = lex("modules xor1");
    ASSERT_FALSE(result.error);
    ASSERT_EQ(result.tokens.size(), 3u);
    EXPECT_EQ(result.tokens[0].kind, TokenKind::Identifier);
    EXPECT_EQ(result.tokens[0].text, "modules");
    EXPECT_EQ(result.tokens[1].kind, TokenKind::Identifier);
    EXPECT_EQ(result.tokens[1].text, "xor1");
}

TEST(LexerTest, KeywordsAreCaseSensitive) {
    // Verilog is case-sensitive: "MODULE" is a name, not the keyword.
    auto result = lex("MODULE");
    ASSERT_FALSE(result.error);
    ASSERT_EQ(result.tokens.size(), 2u);
    EXPECT_EQ(result.tokens[0].kind, TokenKind::Identifier);
}

// ---------- Punctuation ----------

TEST(LexerTest, Punctuation) {
    auto result = lex("(),;");
    ASSERT_FALSE(result.error);
    std::vector<TokenKind> expected = {
        TokenKind::LeftParen, TokenKind::RightParen, TokenKind::Comma,
        TokenKind::Semicolon, TokenKind::EndOfFile,
    };
    EXPECT_EQ(kindsOf(result), expected);
}

TEST(LexerTest, GateLineWithoutSpaces) {
    auto result = lex("xor g1(w1,a,b);");
    ASSERT_FALSE(result.error);
    std::vector<TokenKind> expected = {
        TokenKind::Xor,        TokenKind::Identifier, TokenKind::LeftParen,
        TokenKind::Identifier, TokenKind::Comma,      TokenKind::Identifier,
        TokenKind::Comma,      TokenKind::Identifier, TokenKind::RightParen,
        TokenKind::Semicolon,  TokenKind::EndOfFile,
    };
    EXPECT_EQ(kindsOf(result), expected);
}

// ---------- Locations ----------

TEST(LexerTest, ColumnsOnOneLine) {
    auto result = lex("xor g1(w1");
    ASSERT_FALSE(result.error);
    ASSERT_EQ(result.tokens.size(), 5u);
    EXPECT_EQ(result.tokens[0].column, 1);  // xor
    EXPECT_EQ(result.tokens[1].column, 5);  // g1
    EXPECT_EQ(result.tokens[2].column, 7);  // (
    EXPECT_EQ(result.tokens[3].column, 8);  // w1
}

TEST(LexerTest, LinesAndColumnsAfterNewline) {
    auto result = lex("a\n  b");
    ASSERT_FALSE(result.error);
    ASSERT_EQ(result.tokens.size(), 3u);
    EXPECT_EQ(result.tokens[0].line, 1);
    EXPECT_EQ(result.tokens[0].column, 1);
    EXPECT_EQ(result.tokens[1].line, 2);
    EXPECT_EQ(result.tokens[1].column, 3);
}

TEST(LexerTest, EndOfFileIsAfterLastCharacter) {
    auto result = lex("wire\n");
    ASSERT_FALSE(result.error);
    ASSERT_EQ(result.tokens.size(), 2u);
    EXPECT_EQ(result.tokens[1].kind, TokenKind::EndOfFile);
    EXPECT_EQ(result.tokens[1].line, 2);
    EXPECT_EQ(result.tokens[1].column, 1);
}

// ---------- Comments ----------

TEST(LexerTest, CommentIsSkipped) {
    auto result = lex("// a comment\nwire");
    ASSERT_FALSE(result.error);
    ASSERT_EQ(result.tokens.size(), 2u);
    EXPECT_EQ(result.tokens[0].kind, TokenKind::Wire);
    EXPECT_EQ(result.tokens[0].line, 2);
}

TEST(LexerTest, CommentAtEndOfLine) {
    auto result = lex("wire w1; // internal\n");
    ASSERT_FALSE(result.error);
    std::vector<TokenKind> expected = {
        TokenKind::Wire,
        TokenKind::Identifier,
        TokenKind::Semicolon,
        TokenKind::EndOfFile,
    };
    EXPECT_EQ(kindsOf(result), expected);
}

TEST(LexerTest, CommentAtEndOfFileWithoutNewline) {
    auto result = lex("wire // last line");
    ASSERT_FALSE(result.error);
    EXPECT_EQ(kindsOf(result),
              (std::vector<TokenKind>{TokenKind::Wire, TokenKind::EndOfFile}));
}

// ---------- Errors ----------

TEST(LexerTest, InvalidCharacterGivesError) {
    auto result = lex("@");
    ASSERT_TRUE(result.error);
    EXPECT_EQ(result.error->line, 1);
    EXPECT_EQ(result.error->column, 1);
}

TEST(LexerTest, InvalidCharacterLocation) {
    auto result = lex("xor g1(w1,a,b) @;");
    ASSERT_TRUE(result.error);
    EXPECT_EQ(result.error->line, 1);
    EXPECT_EQ(result.error->column, 16);
}

TEST(LexerTest, InvalidCharacterOnLaterLine) {
    auto result = lex("wire a;\n  #");
    ASSERT_TRUE(result.error);
    EXPECT_EQ(result.error->line, 2);
    EXPECT_EQ(result.error->column, 3);
}

TEST(LexerTest, ErrorMessageNamesTheCharacter) {
    auto result = lex("@");
    ASSERT_TRUE(result.error);
    EXPECT_NE(result.error->error_message.find('@'), std::string::npos);
}

TEST(LexerTest, NameStartingWithDigitIsError) {
    // "w1" is a name, but "1w" is not: names cannot start with a digit.
    auto result = lex("wire 1w;");
    ASSERT_TRUE(result.error);
    EXPECT_EQ(result.error->line, 1);
    EXPECT_EQ(result.error->column, 6);
}

TEST(LexerTest, SingleSlashIsError) {
    // Only "//" starts a comment; one "/" alone is not allowed.
    auto result = lex("a / b");
    ASSERT_TRUE(result.error);
    EXPECT_EQ(result.error->column, 3);
}

// ---------- Whole file ----------

TEST(LexerTest, FullAdder) {
    const char* source =
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
    auto result = lex(source);
    ASSERT_FALSE(result.error);
    ASSERT_FALSE(result.tokens.empty());
    EXPECT_EQ(result.tokens.front().kind, TokenKind::Module);
    EXPECT_EQ(result.tokens.back().kind, TokenKind::EndOfFile);
    // The token before EndOfFile is "endmodule", on line 10.
    const auto& last = result.tokens[result.tokens.size() - 2];
    EXPECT_EQ(last.kind, TokenKind::EndModule);
    EXPECT_EQ(last.line, 10);
    EXPECT_EQ(last.column, 1);
}
