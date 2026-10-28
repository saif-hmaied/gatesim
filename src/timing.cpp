#include "gatesim/timing.hpp"
#include <algorithm>
#include <vector>
using namespace std;

namespace gatesim{
    double gate_delay(GateType type){
        switch(type){
            case GateType::Not:  return 1;
            case GateType::Buf:  return 1;
            case GateType::Nand: return 1.5;
            case GateType::Nor:  return 1.5;
            case GateType::And:  return 2;
            case GateType::Or:   return 2;
            case GateType::Xor:  return 3;
            case GateType::Xnor: return 3;
        }
        return 0;
    }

    TimingResult analyze_timing(const Netlist &netlist,const vector<GateId> &order){
        TimingResult result;
        result.arrival.assign(netlist.wires.size(),0.0);

        for(GateId g:order){
            const Gate &gate=netlist.Gates[g];
            double latest=0;
            for(NetId net:gate.in){
                latest=max(latest,result.arrival[net]);
            }
            result.arrival[gate.out]=latest+gate_delay(gate.type);
        }

        if(netlist.output.empty())return result;
        NetId cur=netlist.output[0];
        for(NetId net:netlist.output){
            if(result.arrival[net]>result.arrival[cur])cur=net;
        }
        result.worst_arrival=result.arrival[cur];

        while(true){
            result.critical_path.push_back(cur);
            const Net &net=netlist.wires[cur];
            if(net.in.empty())break;
            const Gate &tmp=netlist.Gates[net.in[0]];
            NetId slowest=tmp.in[0];
            for(NetId in:tmp.in){
                if(result.arrival[in]>result.arrival[slowest])slowest=in;
            }
            cur=slowest;
        }
        reverse(result.critical_path.begin(),result.critical_path.end());
        return result;
    }
}
