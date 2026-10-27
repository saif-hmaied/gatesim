#include "gatesim/parser.hpp"
#include <optional>
#include<unordered_map>
#include<unordered_set>
#include<string>
#include<vector>
using namespace std;
namespace gatesim{
    struct Parser{
        std::size_t pos;
        const std::vector<Token>&tokens;
        ParseResult result;
        std::vector<Token>ports;
        std::unordered_set<std::string>gate_names;
        Parser(const std::vector<Token>&tokens):tokens(tokens){
            pos=0;
        }
        const Token &peek(){
            return tokens[pos];
        }
        const Token &advance(){
            const Token& cur=tokens[pos];
            if(cur.kind!=TokenKind::EndOfFile){
                pos++;
            }
            return cur;
        }
        optional<Token> expect(TokenKind token,string text){
            if(peek().kind==token){
                return advance();
            }else{
                result.error={"expected "+text+" found "+string(peek().text),peek().line,peek().column};
                return nullopt;
            }
        }
        bool parse_module(){
            if(!expect(TokenKind::Module,"Module"))return false;
            auto  tmp=expect(TokenKind::Identifier,"module name");
            if(!tmp)return false;
            result.G.name=tmp->text;
            if(!expect(TokenKind::LeftParen,"("))return false;
            int f=0;
            while(true){
                if(!f){
                    auto tmp=expect(TokenKind::Identifier,"port name");
                    if(!tmp)return false;
                    ports.push_back(*tmp);
                    f^=1;
                }else{
                    if(peek().kind==TokenKind::Comma){
                        advance();
                        f^=1;
                    }
                    else if(peek().kind==TokenKind::RightParen){
                        advance();
                        if(expect(TokenKind::Semicolon,";")){
                            break;
                        }else{
                            return false;
                        }
                    }else{
                        result.error={"unexpected token "+string(peek().text),peek().line,peek().column};
                        return false;
                    }
                }
            }
            return true;
        }
        bool is_port(Token t){
            for(auto &elem:ports){
                if(elem.text==t.text)return true;
            }
            return false;
        }
        bool parse_declaration(){
            auto &dec=peek();
            advance();
            int f=0;
            while(true){
                if(!f){
                    if(peek().kind==TokenKind::Identifier){
                        if(dec.kind!=TokenKind::Wire&&!is_port(peek())){
                            result.error={"non declared port "+ string(peek().text),peek().line,peek().column};
                            return false;
                        }else{
                            if(result.G.m.find(string(peek().text))!=result.G.m.end()){
                                result.error={"redeclaration of port "+ string(peek().text),peek().line,peek().column};
                                return false;
                            }
                            result.G.m[string(peek().text)]=result.G.wires.size();
                            result.G.wires.push_back({string(peek().text),{},{},(dec.kind==TokenKind::Wire?NetType::Wire:dec.kind==TokenKind::Input?NetType::Input:NetType::Output)});
                        }
                        advance();
                        f^=1;
                    }else{
                        expect(TokenKind::Identifier,"port name");
                        return false;
                    }
                }
                else{
                    if(peek().kind==TokenKind::Semicolon){
                        advance();
                        break;
                    }else if(peek().kind==TokenKind::Comma){
                        f^=1;
                        advance();
                    }else{
                        result.error={"unexpected Token "+string(peek().text),peek().line,peek().column};
                        return false;
                    }
                }
            }
            return true;
        }
        bool fail(const Token& at,string message){
            result.error=ParseError{message,at.line,at.column};
            return false;
        }
        string describe(const Token& t){
            if(t.kind==TokenKind::EndOfFile)return "end of file";
            return "'"+string(t.text)+"'";
        }
        optional<NetId> find_net(const Token& t){
            auto it=result.G.m.find(string(t.text));
            if(it==result.G.m.end()){
                fail(t,"undeclared net '"+string(t.text)+"'");
                return nullopt;
            }
            return it->second;
        }
        GateType to_gate_type(TokenKind kind){
            switch(kind){
                case TokenKind::And:  return GateType::And;
                case TokenKind::Or:   return GateType::Or;
                case TokenKind::Not:  return GateType::Not;
                case TokenKind::Nand: return GateType::Nand;
                case TokenKind::Nor:  return GateType::Nor;
                case TokenKind::Xor:  return GateType::Xor;
                case TokenKind::Xnor: return GateType::Xnor;
                default:              return GateType::Buf;
            }
        }
        bool parse_gate(){
            const Token& type_token=advance();  
            GateType type=to_gate_type(type_token.kind);
            auto name=expect(TokenKind::Identifier,"gate name");
            if(!name)return false;
            if(gate_names.contains(string(name->text))){
                return fail(*name,"gate '"+string(name->text)+"' is already declared");
            }
            if(!expect(TokenKind::LeftParen,"'('"))return false;

            auto out_token=expect(TokenKind::Identifier,"output net name");
            if(!out_token)return false;
            auto out=find_net(*out_token);
            if(!out)return false;
            if(!expect(TokenKind::Comma,"','"))return false;
            vector<NetId> inputs;
            while(true){
                auto in_token=expect(TokenKind::Identifier,"input net name");
                if(!in_token)return false;
                auto in=find_net(*in_token);
                if(!in)return false;
                inputs.push_back(*in);
                if(peek().kind==TokenKind::Comma){
                    advance();
                }else if(peek().kind==TokenKind::RightParen){
                    advance();
                    break;
                }else{
                    return fail(peek(),"expected ',' or ')' found "+describe(peek()));
                }
            }
            bool one_input=(type==GateType::Not||type==GateType::Buf);
            if(one_input&&inputs.size()!=1){
                return fail(*name,"gate '"+string(name->text)+"': "+string(type_token.text)+" takes exactly 1 input");
            }
            if(!one_input&&inputs.size()<2){
                return fail(*name,"gate '"+string(name->text)+"': "+string(type_token.text)+" needs at least 2 inputs");
            }
            if(!expect(TokenKind::Semicolon,"';'"))return false;

            GateId id=static_cast<GateId>(result.G.Gates.size());
            Gate gate;
            gate.name=string(name->text);
            gate.type=type;
            gate.out=*out;
            gate.in=inputs;
            result.G.Gates.push_back(gate);
            gate_names.insert(gate.name);

            result.G.wires[*out].in.push_back(id);   
            for(NetId net:inputs){
                result.G.wires[net].out.push_back(id);  
            }
            return true;
        }
        bool parse_body(){
            while(true){
                switch(peek().kind){
                    case TokenKind::Input:
                    case TokenKind::Output:
                    case TokenKind::Wire:
                        if(!parse_declaration())return false;
                        break;
                    case TokenKind::And:
                    case TokenKind::Or:
                    case TokenKind::Not:
                    case TokenKind::Nand:
                    case TokenKind::Nor:
                    case TokenKind::Xor:
                    case TokenKind::Xnor:
                    case TokenKind::Buf:
                        if(!parse_gate())return false;
                        break;
                    case TokenKind::EndModule:
                        return true;
                    default:
                        return fail(peek(),"expected a declaration, a gate or 'endmodule' found "+describe(peek()));
                }
            }
        }
        bool finish_ports(){
            for(const Token& port:ports){
                auto it=result.G.m.find(string(port.text));
                if(it==result.G.m.end()||result.G.wires[it->second].type==NetType::Wire){
                    return fail(port,"port '"+string(port.text)+"' is not declared as input or output");
                }
                if(result.G.wires[it->second].type==NetType::Input){
                    result.G.input.push_back(it->second);
                }else{
                    result.G.output.push_back(it->second);
                }
            }
            return true;
        }
        void run(){
            if(!parse_module())return;
            if(!parse_body())return;
            if(!expect(TokenKind::EndModule,"'endmodule'"))return;
            if(!expect(TokenKind::EndOfFile,"end of file"))return;
            finish_ports();
        }
    };
    ParseResult parse(const std::vector<Token>&tokens){
        Parser P(tokens);
        P.run();
        if(P.result.error){
            P.result.G=Netlist{};  
        }
        return P.result;
    }
}