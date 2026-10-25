#pragma once
#include<string>
#include "gatesim/token.hpp"
#include<vector>
#include<optional>
#include<string_view>
namespace gatesim{
    struct LexError{
        std::string error_message;
        int column=0;
        int line=0;
    };
    struct LexResult{
        std::vector<Token>tokens;
        std::optional<LexError>error;
    };
    LexResult lex(std::string_view text);
}