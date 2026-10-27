#include "gatesim/simulator.hpp"
#include <cstddef>
#include <vector>
using namespace std;

namespace gatesim{
    bool evaluate(GateType type,const vector<bool> &inputs){
        size_t ones=0;
        for(bool v:inputs){
            if(v)ones++;
        }
        switch(type){
            case GateType::And:  return ones==inputs.size();
            case GateType::Or:   return ones>0;
            case GateType::Xor:  return ones%2==1;
            case GateType::Nand: return ones!=inputs.size();
            case GateType::Nor:  return ones==0;
            case GateType::Xnor: return ones%2==0;
            case GateType::Not:  return !inputs[0];
            case GateType::Buf:  return inputs[0];
        }
        return false;
    }

    vector<bool> simulate(const Netlist &netlist,const vector<GateId> &order,const vector<bool> &input_values){
        vector<bool> values(netlist.wires.size(),false);
        for(size_t i=0;i<netlist.input.size();i++){
            values[netlist.input[i]]=input_values[i];
        }

        vector<bool> gate_inputs;
        for(GateId g:order){
            const Gate &gate=netlist.Gates[g];
            gate_inputs.clear();
            for(NetId net:gate.in){
                gate_inputs.push_back(values[net]);
            }
            values[gate.out]=evaluate(gate.type,gate_inputs);
        }

        vector<bool> outputs;
        for(NetId net:netlist.output){
            outputs.push_back(values[net]);
        }
        return outputs;
    }
}
