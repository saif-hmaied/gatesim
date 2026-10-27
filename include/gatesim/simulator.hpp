#pragma once
#include <vector>
#include "gatesim/netlist.hpp"

namespace gatesim{
    bool evaluate(GateType type,const std::vector<bool> &inputs);

    std::vector<bool> simulate(const Netlist &netlist,
                               const std::vector<GateId> &order,
                               const std::vector<bool> &input_values);
}
