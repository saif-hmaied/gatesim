#pragma once
#include<string>
#include<vector>
#include "netlist.hpp"
#include <cstdint>
namespace gatesim{
    enum class Severity:std::uint8_t{
        Error,Warning
    };
    enum class IssueKind:std::uint8_t{
        Undriven,MultiDriven,Unused,Loop,DrivenInput
    };
    struct  Diagnostic{
        Severity severity=Severity::Error;
        IssueKind issue=IssueKind::Unused;
        std::string message;
    };
    struct CheckResult{
        std::vector<Diagnostic>diagnostics;
        std::vector<GateId>topsort;
    };
    CheckResult check(const Netlist& netlist);
}