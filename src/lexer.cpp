#include "gatesim/lexer.hpp"
#include <string_view>
#include <string>
#include <vector>
#include <algorithm>
using namespace std;
namespace gatesim{
    struct Keyword {
        std::string_view text;
        TokenKind kind;
    };
    constexpr Keyword keywords[] = {
        {"module", TokenKind::Module},
        {"endmodule", TokenKind::EndModule},
        {"input", TokenKind::Input},
        {"output", TokenKind::Output},
        {"wire", TokenKind::Wire},
        {"and", TokenKind::And},
        {"or", TokenKind::Or},
        {"not", TokenKind::Not},
        {"nand", TokenKind::Nand},
        {"nor", TokenKind::Nor},
        {"xor", TokenKind::Xor},
        {"xnor", TokenKind::Xnor},
        {"buf", TokenKind::Buf},
        {"(", TokenKind::LeftParen},
        {")", TokenKind::RightParen},
        {",", TokenKind::Comma},
        {";", TokenKind::Semicolon},
    };
    TokenKind look_up_keyword(string_view text){
        for(const auto &k:keywords){
            if(k.text==text){
                return k.kind;
            }
        }
        return TokenKind::Identifier;
    }
    LexResult lex(std::string_view text){
        int n=text.size();
        int cur_line=1,cur_column=1;
        LexResult result;
        int i=0;
        vector<char>punctuation={',','(',')',';'};
        while(i<n){
            int j=i;
            if(find(punctuation.begin(),punctuation.end(),text[j])!=punctuation.end()){
                result.tokens.push_back({look_up_keyword(text.substr(i,j-i+1)),text.substr(i,j-i+1),cur_line,cur_column});
                cur_column++;
                i=j+1;
                continue;
            }else if(text[j]==' '||text[j]=='\r'||text[j]=='\t'){
                i=j+1;
                cur_column++;
                continue;
            }else if(text[j]=='\n'){
                i=j+1;
                cur_column=1;
                cur_line++;
                continue;
            }else if(text[j]=='/'&&j+1<n&&text[j+1]=='/'){
                j+=2;
                cur_column+=2;
                while(j<n&&text[j]!='\n'){
                    j++;
                    cur_column++;
                }
                if(j<n&&text[j]=='\n'){
                    cur_column=1;
                    cur_line++;
                }
                i=j+1;
                continue;
            }else{
                while(j<n){
                    if(text[j]==' '||text[j]=='\r'||text[j]=='\t'||text[j]=='\n'||find(punctuation.begin(),punctuation.end(),text[j])!=punctuation.end()){
                        result.tokens.push_back({look_up_keyword(text.substr(i,j-i)),text.substr(i,j-i),cur_line,cur_column-(j-i)});
                        i=j;
                        break;
                    }
                    if(!isalnum(text[j])&&text[j]!='_'){
                        result.error={string("unexpected character : "+string(1,text[j])),cur_column,cur_line};
                        return result;
                    }
                    if(j==n-1){
                        result.tokens.push_back({look_up_keyword(text.substr(i,j-i+1)),text.substr(i,j-i+1),cur_line,cur_column-(j-i)});
                        i=j+1;
                        cur_column++;
                        break;
                    }
                    cur_column++;
                    j++;
                }
            }
        }
        result.tokens.push_back({TokenKind::EndOfFile,"",cur_line,cur_column});
        return result;
    }
}