#pragma once
#include<string>
#include<vector>
#include<unordered_map>
#include<cstdint>
namespace gatesim{
    using NetId=std::uint32_t;
    using GateId=std::uint32_t;
    enum class GateType:std::uint8_t {
        And, Or, Not, Nand, Nor, Xor, Xnor, Buf,
    };
    enum class NetType:std::uint8_t {
        Input, Output, Wire,
    };
    struct Net{
        std::string name;
        std::vector<GateId>in;
        std::vector<GateId>out;
        NetType type=NetType::Wire;
    };
    struct Gate{
        std::string name;
        std::vector<NetId>in;
        NetId out=0;
        GateType type=GateType::And;
    };
    struct Netlist{
        std::string name;
        std::unordered_map<std::string,NetId>m;
        std::vector<Gate>Gates;
        std::vector<Net>wires;
        std::vector<NetId>input;
        std::vector<NetId>output;
    };
}