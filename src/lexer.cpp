#include "gatesim/lexer.hpp"
#include <string_view>
namespace gatesim{
    LexResult lex([[maybe_unused]]std::string_view text){
        LexResult result;
        result.tokens.push_back({TokenKind::EndOfFile,"",1,1});
        return result;
    }
}