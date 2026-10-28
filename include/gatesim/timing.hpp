#pragma once
#include <vector>
#include "gatesim/netlist.hpp"

namespace gatesim{
    struct TimingResult{
        std::vector<double> arrival;
        std::vector<NetId> critical_path;
        double worst_arrival=0;
    };

    double gate_delay(GateType type);

    TimingResult analyze_timing(const Netlist &netlist,
                                const std::vector<GateId> &order);
}
