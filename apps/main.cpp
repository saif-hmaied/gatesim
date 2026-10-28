#include <algorithm>
#include <charconv>
#include <cmath>
#include <cstddef>
#include <fstream>
#include <iostream>
#include <optional>
#include <sstream>
#include <string>
#include <string_view>
#include <system_error>
#include <vector>

#include "gatesim/check.hpp"
#include "gatesim/lexer.hpp"
#include "gatesim/parser.hpp"
#include "gatesim/simulator.hpp"
#include "gatesim/timing.hpp"
using namespace std;
using namespace gatesim;

namespace{
    void print_usage(){
        cerr<<"usage:\n"
            <<"  gatesim check <file.v>\n"
            <<"  gatesim sim <file.v> --input name=0|1 ...\n"
            <<"  gatesim sim <file.v> --all\n"
            <<"  gatesim timing <file.v> [--clock <ns>]"<<endl;
    }

    optional<Netlist> load(const string &path){
        ifstream file(path);
        if(!file){
            cerr<<"gatesim: cannot open '"<<path<<"'"<<endl;
            return nullopt;
        }
        ostringstream buffer;
        buffer<<file.rdbuf();
        string text=buffer.str();

        LexResult lexed=lex(text);
        if(lexed.error){
            cerr<<path<<":"<<lexed.error->line<<":"<<lexed.error->column<<": error: "<<lexed.error->error_message<<endl;
            return nullopt;
        }
        ParseResult parsed=parse(lexed.tokens);
        if(parsed.error){
            cerr<<path<<":"<<parsed.error->line<<":"<<parsed.error->column<<": error: "<<parsed.error->message<<endl;
            return nullopt;
        }
        return parsed.G;
    }

    int run_check(const string &path){
        optional<Netlist> netlist=load(path);
        if(!netlist)return 1;
        CheckResult result=check(*netlist);
        int errors=0,warnings=0;
        for(const Diagnostic &d:result.diagnostics){
            if(d.severity==Severity::Error){
                cout<<path<<": error: "<<d.message<<endl;
                errors++;
            }else{
                cout<<path<<": warning: "<<d.message<<endl;
                warnings++;
            }
        }
        cout<<"module "<<netlist->name<<": "<<netlist->Gates.size()<<" gates, "<<netlist->wires.size()<<" nets, "
            <<errors<<" errors, "<<warnings<<" warnings"<<endl;
        return errors>0?1:0;
    }

    optional<vector<GateId>> valid_order(const string &path,const Netlist &netlist,string_view action){
        CheckResult result=check(netlist);
        int errors=0;
        for(const Diagnostic &d:result.diagnostics){
            if(d.severity==Severity::Error){
                cerr<<path<<": error: "<<d.message<<endl;
                errors++;
            }
        }
        if(errors>0){
            cerr<<"gatesim: cannot "<<action<<" a circuit with errors"<<endl;
            return nullopt;
        }
        return result.topsort;
    }

    int run_sim_input(const Netlist &netlist,const vector<GateId> &order,const vector<string_view> &args){
        size_t n=netlist.input.size();
        vector<bool> values(n,false);
        vector<bool> given(n,false);
        for(string_view arg:args){
            size_t eq=arg.find('=');
            if(eq==string_view::npos){
                cerr<<"gatesim: expected name=value, got '"<<arg<<"'"<<endl;
                return 1;
            }
            string_view name=arg.substr(0,eq);
            string_view value=arg.substr(eq+1);

            size_t index=n;
            for(size_t i=0;i<n;i++){
                if(netlist.wires[netlist.input[i]].name==name)index=i;
            }
            if(index==n){
                cerr<<"gatesim: '"<<name<<"' is not an input of module "<<netlist.name<<endl;
                return 1;
            }
            if(value!="0"&&value!="1"){
                cerr<<"gatesim: the value of '"<<name<<"' must be 0 or 1, got '"<<value<<"'"<<endl;
                return 1;
            }
            if(given[index]){
                cerr<<"gatesim: input '"<<name<<"' is given twice"<<endl;
                return 1;
            }
            values[index]=(value=="1");
            given[index]=true;
        }
        for(size_t i=0;i<n;i++){
            if(!given[i]){
                cerr<<"gatesim: missing value for input '"<<netlist.wires[netlist.input[i]].name<<"'"<<endl;
                return 1;
            }
        }

        vector<bool> outputs=simulate(netlist,order,values);
        for(size_t i=0;i<outputs.size();i++){
            cout<<netlist.wires[netlist.output[i]].name<<" = "<<(outputs[i]?1:0)<<endl;
        }
        return 0;
    }

    void print_cell(const string &name,bool value){
        cout<<(value?'1':'0')<<string(name.size()-1,' ')<<' ';
    }

    int run_sim_all(const Netlist &netlist,const vector<GateId> &order){
        size_t n=netlist.input.size();
        if(n>16){
            cerr<<"gatesim: --all supports at most 16 inputs, module "<<netlist.name<<" has "<<n<<endl;
            return 1;
        }

        for(NetId net:netlist.input)cout<<netlist.wires[net].name<<' ';
        cout<<"| ";
        for(NetId net:netlist.output)cout<<netlist.wires[net].name<<' ';
        cout<<endl;

        vector<bool> values(n,false);
        for(size_t mask=0;mask<(size_t(1)<<n);mask++){
            for(size_t i=0;i<n;i++){
                values[i]=(mask>>(n-1-i))&1;
            }
            vector<bool> outputs=simulate(netlist,order,values);
            for(size_t i=0;i<n;i++)print_cell(netlist.wires[netlist.input[i]].name,values[i]);
            cout<<"| ";
            for(size_t i=0;i<outputs.size();i++)print_cell(netlist.wires[netlist.output[i]].name,outputs[i]);
            cout<<'\n';
        }
        return 0;
    }

    int run_sim(const string &path,string_view mode,const vector<string_view> &args){
        optional<Netlist> netlist=load(path);
        if(!netlist)return 1;
        optional<vector<GateId>> order=valid_order(path,*netlist,"simulate");
        if(!order)return 1;
        if(mode=="--all")return run_sim_all(*netlist,*order);
        return run_sim_input(*netlist,*order,args);
    }

    optional<double> parse_clock(string_view text){
        double value=0;
        const char *first=text.data();
        const char *last=text.data()+text.size();
        auto [end,error]=from_chars(first,last,value);
        if(error!=errc()||end!=last||!isfinite(value)||value<=0){
            cerr<<"gatesim: --clock must be a positive number of ns, got '"<<text<<"'"<<endl;
            return nullopt;
        }
        return value;
    }

    int run_timing(const string &path,optional<double> clock){
        optional<Netlist> netlist=load(path);
        if(!netlist)return 1;
        optional<vector<GateId>> order=valid_order(path,*netlist,"analyze the timing of");
        if(!order)return 1;
        if(netlist->output.empty()){
            cout<<"module "<<netlist->name<<" has no outputs, nothing to analyze"<<endl;
            return 0;
        }
        TimingResult result=analyze_timing(*netlist,*order);

        size_t width=0;
        for(NetId net:netlist->output)width=max(width,netlist->wires[net].name.size());
        cout<<"output arrival times:"<<endl;
        for(NetId net:netlist->output){
            const string &name=netlist->wires[net].name;
            cout<<"  "<<name<<string(width-name.size(),' ')<<"  "<<result.arrival[net]<<" ns"<<endl;
        }

        cout<<"critical path ("<<result.worst_arrival<<" ns):"<<endl<<"  ";
        for(size_t i=0;i<result.critical_path.size();i++){
            NetId net=result.critical_path[i];
            if(i>0){
                GateId driver=netlist->wires[net].in[0];
                cout<<" -> "<<netlist->Gates[driver].name<<" -> ";
            }
            cout<<netlist->wires[net].name;
        }
        cout<<endl;

        if(!clock)return 0;
        double slack=*clock-result.worst_arrival;
        cout<<"clock "<<*clock<<" ns, slack "<<slack<<" ns: "<<(slack<0?"FAILS":"OK")<<endl;
        return slack<0?1:0;
    }
}

int main(int argc,char* argv[]){
    if(argc<3){
        print_usage();
        return 1;
    }
    string_view command=argv[1];
    string path=argv[2];

    if(command=="check"&&argc==3){
        return run_check(path);
    }
    if(command=="sim"&&argc>=4){
        string_view mode=argv[3];
        vector<string_view> args(argv+4,argv+argc);
        if(mode=="--all"&&args.empty())return run_sim(path,mode,args);
        if(mode=="--input")return run_sim(path,mode,args);
    }
    if(command=="timing"){
        if(argc==3)return run_timing(path,nullopt);
        if(argc==5&&string_view(argv[3])=="--clock"){
            optional<double> clock=parse_clock(argv[4]);
            if(!clock)return 1;
            return run_timing(path,clock);
        }
    }
    print_usage();
    return 1;
}
