#pragma once
#include <cstdint>
#include <string_view>
namespace gatesim{
    enum class TokenKind : std::uint8_t {
        // Keywords
        Module,
        EndModule,
        Input,
        Output,
        Wire,
        // Gate types
        And,
        Or,
        Not,
        Nand,
        Nor,
        Xor,
        Xnor,
        Buf,
        // Names
        Identifier,
        // Punctuation
        LeftParen,
        RightParen,
        Comma,
        Semicolon,
        // Special
        EndOfFile,
    };
    struct Token{
        TokenKind kind=TokenKind::EndOfFile;
        std::string_view text;
        int line=0;
        int column=0;
    };
}
    