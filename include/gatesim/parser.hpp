#pragma once
#include "netlist.hpp"
#include <optional>
#include <vector>
#include "token.hpp"
#include <string>
namespace gatesim{
    struct ParseError{
        std::string message;
        int line=0;
        int column=0;
    };
    struct ParseResult{
        Netlist G;
        std::optional<ParseError> error;
    };
    ParseResult parse(const std::vector<Token> &tokens);
}