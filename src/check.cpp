#include "gatesim/check.hpp"
#include <string>
#include <vector>
#include <queue>
#include <algorithm>
using namespace std;


namespace gatesim{
    namespace{
        void dfs(GateId g,const vector<vector<GateId>> &adj,vector<bool> &vis,vector<GateId> &postorder){
            vis[g]=true;
            for(GateId next:adj[g]){
                if(!vis[next])dfs(next,adj,vis,postorder);
            }
            postorder.push_back(g);
        }

        void dfs_reversed(GateId g,const vector<vector<GateId>> &rg,vector<bool> &vis,vector<int> &comp,int c){
            vis[g]=true;
            comp[g]=c;
            for(GateId prev:rg[g]){
                if(!vis[prev])dfs_reversed(prev,rg,vis,comp,c);
            }
        }

        string describe_cycle(const Netlist &netlist,const vector<vector<GateId>> &adj,const vector<int> &comp,GateId start){
            vector<int>index_in_path(adj.size(),-1);
            vector<GateId>path;
            GateId g=start;
            while(index_in_path[g]==-1){
                index_in_path[g]=path.size();
                path.push_back(g);
                for(GateId next:adj[g]){
                    if(comp[next]==comp[g]){
                        g=next;
                        break;
                    }
                }
            }
            string message;
            for(size_t k=index_in_path[g];k<path.size();k++){
                GateId gate=path[k];
                message+=netlist.Gates[gate].name+" -> "+netlist.wires[netlist.Gates[gate].out].name+" -> ";
            }
            message+=netlist.Gates[g].name;
            return message;
        }

        vector<string> find_loops(const Netlist &netlist){
            size_t n=netlist.Gates.size();
            vector<vector<GateId>>adj(n),rg(n);
            for(size_t g=0;g<n;g++){
                for(GateId reader:netlist.wires[netlist.Gates[g].out].out){
                    adj[g].push_back(reader);
                    rg[reader].push_back(static_cast<GateId>(g));
                }
            }

            vector<bool>vis(n,false);
            vector<GateId>postorder;
            for(size_t g=0;g<n;g++){
                if(!vis[g])dfs(static_cast<GateId>(g),adj,vis,postorder);
            }

            vis.assign(n,false);
            vector<int>comp(n,-1);
            int count=0;
            for(size_t k=postorder.size();k-->0;){
                GateId g=postorder[k];
                if(!vis[g]){
                    dfs_reversed(g,rg,vis,comp,count);
                    count++;
                }
            }

            vector<vector<GateId>>members(count);
            for(size_t g=0;g<n;g++){
                members[comp[g]].push_back(static_cast<GateId>(g));
            }
            vector<string>loops;
            for(int c=0;c<count;c++){
                GateId first=members[c][0];
                bool self_loop=find(adj[first].begin(),adj[first].end(),first)!=adj[first].end();
                if(members[c].size()>1||self_loop){
                    loops.push_back(describe_cycle(netlist,adj,comp,first));
                }
            }
            return loops;
        }
    }
    CheckResult check(const Netlist &netlist){
        CheckResult result;
        for(auto &elem:netlist.wires){
            if(elem.in.size()>1&&elem.type!=NetType::Input){
                string message="net '"+elem.name+"' has "+to_string(elem.in.size())+" drivers:";
                for(GateId i:elem.in){
                    message+=" "+netlist.Gates[i].name;
                }
                result.diagnostics.push_back({Severity::Error,IssueKind::MultiDriven,message});
            }
            if(elem.in.empty()&&elem.type!=NetType::Input&&(elem.type==NetType::Output||!elem.out.empty())){
                result.diagnostics.push_back({Severity::Error,IssueKind::Undriven,"net '"+elem.name+"' has no driver"});
            }
            if(elem.out.empty()&&elem.type!=NetType::Output){
                result.diagnostics.push_back({Severity::Warning,IssueKind::Unused,"net '"+elem.name+"' is unused"});
            }
            if(elem.type==NetType::Input&&!elem.in.empty()){
                result.diagnostics.push_back({Severity::Error,IssueKind::DrivenInput,"input '"+elem.name+"' is driven by gate '"+netlist.Gates[elem.in[0]].name+"'"});
            }
        }
        size_t n=netlist.Gates.size();
        vector<size_t>in(n,0);
        for(size_t g=0;g<n;g++){
            for(NetId net:netlist.Gates[g].in){
                in[g]+=netlist.wires[net].in.size();
            }
        }
        queue<GateId>ready;
        for(size_t g=0;g<n;g++){
            if(in[g]==0)ready.push(g);
        }
        vector<GateId>order;
        while(!ready.empty()){
            GateId g=ready.front();
            ready.pop();
            order.push_back(g);
            for(GateId reader:netlist.wires[netlist.Gates[g].out].out){
                in[reader]--;
                if(in[reader]==0)ready.push(reader);
            }
        }
        if(order.size()<n){
            for(const string &loop:find_loops(netlist)){
                result.diagnostics.push_back({Severity::Error,IssueKind::Loop,"combinational loop: "+loop});
            }
        }else{
            result.topsort=order;
        }
        return result;
    }
}
