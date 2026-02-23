// jpc_gerador.hpp
// Geração de código C para cada OpCode
// CORRIGIDO: Uso de variáveis temporárias para evitar comportamento indefinido com PUSH/POP

#ifndef JPC_GERADOR_HPP
#define JPC_GERADOR_HPP

#include <fstream>
#include <string>
#include <vector>
#include <map>
#include <set>
#include "../opcodes.hpp"

namespace JPCGerador {

    // Escapa string para código C
    inline std::string escaparStringC(const std::string& s) {
        std::string result;
        for (char c : s) {
            switch (c) {
                case '"': result += "\\\""; break;
                case '\\': result += "\\\\"; break;
                case '\n': result += "\\n"; break;
                case '\r': result += "\\r"; break;
                case '\t': result += "\\t"; break;
                default: result += c; break;
            }
        }
        return result;
    }

    // Converte Value para código C
    inline std::string valueParaC(const Value& val) {
        if (std::holds_alternative<long>(val)) {
            return "jp_int(" + std::to_string(std::get<long>(val)) + ")";
        }
        if (std::holds_alternative<double>(val)) {
            char buf[64];
            snprintf(buf, sizeof(buf), "%.6f", std::get<double>(val));
            // Garante ponto decimal independente do locale
            for (char* p = buf; *p; p++) { if (*p == ',') *p = '.'; }
            return "jp_double(" + std::string(buf) + ")";
        }
        if (std::holds_alternative<bool>(val)) {
            return std::get<bool>(val) ? "jp_bool(1)" : "jp_bool(0)";
        }
        if (std::holds_alternative<std::string>(val)) {
            return "jp_string(\"" + escaparStringC(std::get<std::string>(val)) + "\")";
        }
        return "jp_nulo()";
    }

    // Gera código C para uma instrução
    inline void gerarInstrucao(std::ofstream& out, size_t idx, const Instruction& instr,
                               const std::map<std::string, int>& variaveis,
                               const std::map<std::string, int>& nativasMap) {
        
        out << "L_" << idx << ": ";
        
        switch (instr.op) {
            case OpCode::LOAD_CONST:
                if (instr.operand.has_value()) {
                    out << "PUSH(" << valueParaC(instr.operand.value()) << ");\n";
                }
                break;
                
            case OpCode::STORE_VAR:
                if (instr.operand.has_value()) {
                    std::string nome = std::get<std::string>(instr.operand.value());
                    auto it = variaveis.find(nome);
                    if (it != variaveis.end()) {
                        out << "vars[" << it->second << "] = POP(); // " << nome << "\n";
                    }
                }
                break;
                
            case OpCode::LOAD_VAR:
                if (instr.operand.has_value()) {
                    std::string nome = std::get<std::string>(instr.operand.value());
                    auto it = variaveis.find(nome);
                    if (it != variaveis.end()) {
                        out << "PUSH(jp_copiar(vars[" << it->second << "])); // " << nome << "\n";
                    }
                }
                break;
                
            case OpCode::HALT:
                out << "goto FIM;\n";
                break;
                
            // === IMPRESSÃO ===
            // Usa variável temporária para garantir ordem de avaliação
            case OpCode::PRINT_NO_NL:
                out << "{ JPValor _t = POP(); imprimir_valor(_t); }\n";
                break;
                
            case OpCode::PRINT_NL:
                out << "{ JPValor _t = POP(); imprimir_valor_ln(_t); }\n";
                break;
                
            case OpCode::PRINT_RED_NL:
                out << "{ JPValor _t = POP(); printf(\"\\033[91m\"); imprimir_valor_ln(_t); printf(\"\\033[0m\"); }\n";
                break;
                
            case OpCode::PRINT_GREEN_NL:
                out << "{ JPValor _t = POP(); printf(\"\\033[92m\"); imprimir_valor_ln(_t); printf(\"\\033[0m\"); }\n";
                break;
                
            case OpCode::PRINT_BLUE_NL:
                out << "{ JPValor _t = POP(); printf(\"\\033[94m\"); imprimir_valor_ln(_t); printf(\"\\033[0m\"); }\n";
                break;
                
            case OpCode::PRINT_YELLOW_NL:
                out << "{ JPValor _t = POP(); printf(\"\\033[93m\"); imprimir_valor_ln(_t); printf(\"\\033[0m\"); }\n";
                break;
                
            case OpCode::PRINT_RED_NO_NL:
                out << "{ JPValor _t = POP(); printf(\"\\033[91m\"); imprimir_valor(_t); printf(\"\\033[0m\"); }\n";
                break;
                
            case OpCode::PRINT_GREEN_NO_NL:
                out << "{ JPValor _t = POP(); printf(\"\\033[92m\"); imprimir_valor(_t); printf(\"\\033[0m\"); }\n";
                break;
                
            case OpCode::PRINT_BLUE_NO_NL:
                out << "{ JPValor _t = POP(); printf(\"\\033[94m\"); imprimir_valor(_t); printf(\"\\033[0m\"); }\n";
                break;
                
            case OpCode::PRINT_YELLOW_NO_NL:
                out << "{ JPValor _t = POP(); printf(\"\\033[93m\"); imprimir_valor(_t); printf(\"\\033[0m\"); }\n";
                break;
            
            // === ENTRADA E CONVERSÕES ===
            // IMPORTANTE: Separar POP e PUSH em statements diferentes para evitar
            // comportamento indefinido quando ambos modificam 'sp'
            case OpCode::INPUT:
                out << "{ JPValor _prompt = POP(); JPValor _result = ler_entrada(_prompt); PUSH(_result); }\n";
                break;
                
            case OpCode::TO_INT:
                out << "{ JPValor _v = POP(); PUSH(converter_int(_v)); }\n";
                break;
                
            case OpCode::TO_FLOAT:
                out << "{ JPValor _v = POP(); PUSH(converter_double(_v)); }\n";
                break;
                
            case OpCode::TO_STRING:
                out << "{ JPValor _v = POP(); PUSH(converter_string(_v)); }\n";
                break;
                
            case OpCode::TO_BOOL:
                out << "{ JPValor _v = POP(); PUSH(converter_bool(_v)); }\n";
                break;
                
            case OpCode::TYPE_OF:
                out << "{ JPValor _v = POP(); PUSH(tipo_de(_v)); }\n";
                break;
                
            case OpCode::ADD:
                out << "{ JPValor b = POP(); JPValor a = POP(); PUSH(soma(a, b)); }\n";
                break;
                
            case OpCode::SUB:
                out << "{ JPValor b = POP(); JPValor a = POP(); PUSH(subtracao(a, b)); }\n";
                break;
                
            case OpCode::MUL:
                out << "{ JPValor b = POP(); JPValor a = POP(); PUSH(multiplicacao(a, b)); }\n";
                break;
                
            case OpCode::DIV:
                out << "{ JPValor b = POP(); JPValor a = POP(); PUSH(divisao(a, b)); }\n";
                break;
                
            case OpCode::MOD:
                out << "{ JPValor b = POP(); JPValor a = POP(); PUSH(modulo(a, b)); }\n";
                break;
                
            case OpCode::EQ:
                out << "{ JPValor b = POP(); JPValor a = POP(); PUSH(igual(a, b)); }\n";
                break;
                
            case OpCode::NEQ:
                out << "{ JPValor b = POP(); JPValor a = POP(); PUSH(diferente(a, b)); }\n";
                break;
                
            case OpCode::GT:
                out << "{ JPValor b = POP(); JPValor a = POP(); PUSH(maior(a, b)); }\n";
                break;
                
            case OpCode::LT:
                out << "{ JPValor b = POP(); JPValor a = POP(); PUSH(menor(a, b)); }\n";
                break;
                
            case OpCode::GTE:
                out << "{ JPValor b = POP(); JPValor a = POP(); PUSH(maior_igual(a, b)); }\n";
                break;
                
            case OpCode::LTE:
                out << "{ JPValor b = POP(); JPValor a = POP(); PUSH(menor_igual(a, b)); }\n";
                break;
                
            case OpCode::AND:
                out << "{ JPValor b = POP(); JPValor a = POP(); PUSH(logico_e(a, b)); }\n";
                break;
                
            case OpCode::OR:
                out << "{ JPValor b = POP(); JPValor a = POP(); PUSH(logico_ou(a, b)); }\n";
                break;
                
            case OpCode::JUMP:
                if (instr.operand.has_value()) {
                    long addr = std::get<long>(instr.operand.value());
                    out << "goto L_" << addr << ";\n";
                }
                break;
                
            case OpCode::JUMP_IF_FALSE:
                if (instr.operand.has_value()) {
                    long addr = std::get<long>(instr.operand.value());
                    out << "{ JPValor _cond = POP(); if (!is_true(_cond)) goto L_" << addr << "; }\n";
                }
                break;
                
            case OpCode::CALL:
                if (instr.operand.has_value()) {
                    std::string funcInfo = std::get<std::string>(instr.operand.value());
                    size_t pos = funcInfo.find(':');
                    std::string funcName = (pos != std::string::npos) ? funcInfo.substr(0, pos) : funcInfo;
                    out << "call_stack[csp++] = " << (idx + 1) << "; goto L_FUNC_" << funcName << ";\n";
                }
                break;
                
            case OpCode::RETURN:
                out << "goto TRAMPOLINE_RET;\n";
                break;
                
            case OpCode::POP:
                out << "(void)POP();\n";
                break;
                
            // === CHAMADAS NATIVAS ===
            case OpCode::NATIVE_CALL:
                if (instr.operand.has_value()) {
                    std::string callInfo = std::get<std::string>(instr.operand.value());
                    size_t colonPos = callInfo.find(':');
                    std::string nome = callInfo.substr(0, colonPos);
                    int aridade = std::stoi(callInfo.substr(colonPos + 1));
                    
                    out << "{ JPValor args[" << std::max(aridade, 1) << "]; ";
                    if (aridade > 0) {
                        out << "for (int i = " << (aridade - 1) << "; i >= 0; i--) args[i] = POP(); ";
                    }
                    out << "JPValor _r = fn_" << nome << " ? fn_" << nome << "(args, " << aridade << ") : jp_nulo(); ";
                    out << "PUSH(_r); }\n";
                }
                break;
                
            case OpCode::METHOD_CALL:
                if (instr.operand.has_value()) {
                    std::string callInfo = std::get<std::string>(instr.operand.value());
                    size_t colonPos = callInfo.find(':');
                    std::string fullName = callInfo.substr(0, colonPos);
                    int aridade = std::stoi(callInfo.substr(colonPos + 1));
                    
                    size_t dotPos = fullName.find('.');
                    if (dotPos != std::string::npos) {
                        std::string modulo = fullName.substr(0, dotPos);
                        std::string metodo = fullName.substr(dotPos + 1);
                        
                        if (modulo.empty()) {
                            // Chamada de método de instância: obj.metodo()
                            // Precisa fazer dispatch dinâmico baseado na classe do objeto
                            out << "{ JPValor _obj = POP(); ";
                            out << "JPObjeto* _o = obter_objeto((int)jp_get_int(_obj)); ";
                            out << "if (_o && _o->classe[0] != '\\0') { ";
                            out << "PUSH(_obj); ";  // Coloca objeto de volta para o método acessar
                            out << "vars[2] = _obj; ";  // __auto__ é sempre vars[2]
                            
                            // Gera código condicional para cada classe possível
                            // Nota: Isso deve ser gerado dinamicamente pelo compilador
                            // Por enquanto, vamos usar um approach simples
                            out << "char _funcname[512]; ";
                            out << "snprintf(_funcname, 512, \"%s_%s\", _o->classe, \"" << metodo << "\"); ";
                            
                            // Infelizmente, C não permite goto dinâmico
                            // Solução: usar if-else para classes conhecidas
                            out << "if (strcmp(_o->classe, \"Pessoa\") == 0) { ";
                            out << "call_stack[csp++] = " << (idx + 1) << "; ";
                            out << "goto L_FUNC_Pessoa_" << metodo << "; ";
                            out << "} ";
                            out << "} }\n";
                        } else {
                            // Chamada estática: Classe.metodo()
                            std::string funcName = modulo + "_" + metodo;
                            out << "{ ";
                            out << "call_stack[csp++] = " << (idx + 1) << "; ";
                            out << "goto L_FUNC_" << funcName << "; ";
                            out << "}\n";
                        }
                    }
                }
                break;
            
            // === LISTAS ===
            case OpCode::LIST_CREATE:
                out << "{ int _id = criar_lista(); PUSH(jp_int(_id)); }\n";
                break;
                
            case OpCode::LIST_ADD:
                out << "{ JPValor elem = POP(); JPValor lst = POP(); ";
                out << "JPLista* l = obter_lista((int)jp_get_int(lst)); ";
                out << "if (l) lista_adicionar(l, elem); ";
                out << "PUSH(lst); }\n";
                break;
                
            case OpCode::LIST_GET:
                out << "{ JPValor _idx = POP(); JPValor lst = POP(); ";
                out << "int idx = (int)jp_get_int(_idx); ";
                out << "JPLista* l = obter_lista((int)jp_get_int(lst)); ";
                out << "JPValor _r = l ? lista_obter(l, idx) : jp_nulo(); ";
                out << "PUSH(_r); }\n";
                break;
                
            case OpCode::LIST_SET:
                out << "{ JPValor val = POP(); JPValor _idx = POP(); JPValor lst = POP(); ";
                out << "int idx = (int)jp_get_int(_idx); ";
                out << "JPLista* l = obter_lista((int)jp_get_int(lst)); ";
                out << "if (l) lista_definir(l, idx, val); }\n";
                break;
                
            case OpCode::LIST_SIZE:
                out << "{ JPValor lst = POP(); ";
                out << "JPLista* l = obter_lista((int)jp_get_int(lst)); ";
                out << "int _sz = l ? lista_tamanho(l) : 0; ";
                out << "PUSH(jp_int(_sz)); }\n";
                break;
                
            case OpCode::LIST_REMOVE:
                out << "{ JPValor _idx = POP(); JPValor lst = POP(); ";
                out << "int idx = (int)jp_get_int(_idx); ";
                out << "JPLista* l = obter_lista((int)jp_get_int(lst)); ";
                out << "if (l) lista_remover(l, idx); }\n";
                break;
                
            case OpCode::LIST_DISPLAY:
                out << "{ JPValor lst = POP(); ";
                out << "JPLista* l = obter_lista((int)jp_get_int(lst)); ";
                out << "lista_exibir(l); }\n";
                break;
            
            // === OBJETOS ===
            case OpCode::NEW_OBJECT:
                out << "{ int _id = criar_objeto(); PUSH(jp_int(_id)); }\n";
                break;
                
            case OpCode::SET_ATTR:
                if (instr.operand.has_value()) {
                    std::string attrName = std::get<std::string>(instr.operand.value());
                    
                    if (attrName == "__classe__") {
                        // Atributo especial: define a classe do objeto
                        out << "{ JPValor obj = POP(); JPValor val = POP(); ";
                        out << "JPObjeto* o = obter_objeto((int)jp_get_int(obj)); ";
                        out << "if (o) objeto_set_classe(o, jp_get_string(val)); }\n";
                    } else {
                        // Atributo normal
                        out << "{ JPValor obj = POP(); JPValor val = POP(); ";
                        out << "JPObjeto* o = obter_objeto((int)jp_get_int(obj)); ";
                        out << "if (o) objeto_set_attr(o, \"" << escaparStringC(attrName) << "\", val); }\n";
                    }
                }
                break;
                
            case OpCode::GET_ATTR:
                if (instr.operand.has_value()) {
                    std::string attrName = std::get<std::string>(instr.operand.value());
                    out << "{ JPValor obj = POP(); ";
                    out << "JPObjeto* o = obter_objeto((int)jp_get_int(obj)); ";
                    out << "JPValor _r = o ? objeto_get_attr(o, \"" << escaparStringC(attrName) << "\") : jp_nulo(); ";
                    out << "PUSH(_r); }\n";
                }
                break;
                
            default:
                out << "/* OP " << opToString(instr.op) << " nao implementado */;\n";
                break;
        }
    }

} // namespace JPCGerador

#endif // JPC_GERADOR_HPP