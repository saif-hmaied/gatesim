#include <cstddef>
#include <fstream>
#include <iostream>
#include <optional>
#include <sstream>
#include <string>
#include <string_view>
#include <vector>

#include "gatesim/check.hpp"
#include "gatesim/lexer.hpp"
#include "gatesim/parser.hpp"
#include "gatesim/simulator.hpp"
using namespace std;
using namespace gatesim;

namespace{
    void print_usage(){
        cerr<<"usage:\n"
            <<"  gatesim check <file.v>\n"
            <<"  gatesim sim <file.v> --input name=0|1 ...\n"
            <<"  gatesim sim <file.v> --all"<<endl;
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

    optional<vector<GateId>> order_for_simulation(const string &path,const Netlist &netlist){
        CheckResult result=check(netlist);
        int errors=0;
        for(const Diagnostic &d:result.diagnostics){
            if(d.severity==Severity::Error){
                cerr<<path<<": error: "<<d.message<<endl;
                errors++;
            }
        }
        if(errors>0){
            cerr<<"gatesim: cannot simulate a circuit with errors"<<endl;
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
        optional<vector<GateId>> order=order_for_simulation(path,*netlist);
        if(!order)return 1;
        if(mode=="--all")return run_sim_all(*netlist,*order);
        return run_sim_input(*netlist,*order,args);
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
    print_usage();
    return 1;
}
