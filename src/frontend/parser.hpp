// parser.hpp
// Parser para JPLang v1.0 - Converte tokens em AST

#ifndef JPLANG_PARSER_HPP
#define JPLANG_PARSER_HPP

#include "lexer.hpp"
#include "ast.hpp"
#include <iostream>
#include <string>
#include <vector>
#include <optional>
#include <memory>
#include <fstream>
#include <sstream>

namespace jplang {

// ============================================================================
// MAPA DE COR POR NOME
// ============================================================================

inline Cor cor_from_name(const std::string& name) {
    if (name == "vermelho") return Cor::Vermelho;
    if (name == "verde")    return Cor::Verde;
    if (name == "azul")     return Cor::Azul;
    if (name == "amarelo")  return Cor::Amarelo;
    return Cor::Nenhuma;
}

// ============================================================================
// PARSER
// ============================================================================

class Parser {
public:
    explicit Parser(Lexer& lexer, const std::string& base_dir = "")
        : lex_(lexer), had_error_(false), base_dir_(base_dir),
          lang_config_(lexer.lang_config())
    {
        current_ = lex_.next();
        next_token_ = std::nullopt;
    }

    bool had_error() const { return had_error_; }

    // Acesso à configuração de idioma
    const LangConfig& lang_config() const { return lang_config_; }

    // Método público para parsear uma única expressão (usado em interpolação)
    ExprPtr parse_expr_public() {
        return parse_expr();
    }

    // ========================================================================
    // PONTO DE ENTRADA
    // ========================================================================

    std::optional<Program> parse() {
        Program prog;

        while (!check(TK::TK_EOF) && !had_error_) {
            skip_newlines();
            if (check(TK::TK_EOF)) break;

            auto stmt = parse_statement();
            if (stmt) {
                prog.statements.push_back(std::move(stmt));
            }
            // Insere statements vindos de importar "arquivo.jp"
            for (auto& s : pending_imports_) {
                prog.statements.push_back(std::move(s));
            }
            pending_imports_.clear();

            skip_newlines();
        }

        if (had_error_) return std::nullopt;
        return prog;
    }

private:
    Lexer& lex_;
    Token current_;
    std::optional<Token> next_token_;
    bool had_error_;
    StmtList pending_imports_;  // statements de arquivos importados
    std::string base_dir_;     // diretório base para resolver imports
    LangConfig lang_config_;   // configuração do idioma ativo

    // ========================================================================
    // AUXILIARES
    // ========================================================================

    void do_advance() {
        if (next_token_.has_value()) {
            current_ = std::move(next_token_.value());
            next_token_ = std::nullopt;
        } else {
            current_ = lex_.next();
        }
    }

    bool peek_is(TK tk_type) {
        if (!next_token_.has_value()) {
            next_token_ = lex_.next();
        }
        return next_token_->type == tk_type;
    }

    bool check(TK tk_type) const {
        return current_.type == tk_type;
    }

    bool match(TK tk_type) {
        if (check(tk_type)) {
            do_advance();
            return true;
        }
        return false;
    }

    void expect(TK tk_type, const std::string& msg) {
        if (!match(tk_type)) {
            error(msg);
        }
    }

    void error(const std::string& msg) {
        std::cerr << "Erro linha " << current_.line << ": " << msg
                  << " (token: " << static_cast<int>(current_.type)
                  << " '" << current_.value << "')" << std::endl;
        had_error_ = true;
    }

    void skip_newlines() {
        while (match(TK::NEWLINE)) {}
    }

    int cur_line() const { return current_.line; }

    // ========================================================================
    // BLOCO (indentado)
    // ========================================================================

    StmtList parse_block() {
        skip_newlines();
        if (!match(TK::INDENT)) {
            error("Esperado bloco indentado");
            return {};
        }

        StmtList stmts;
        while (!check(TK::DEDENT) && !check(TK::TK_EOF) && !had_error_) {
            skip_newlines();
            if (check(TK::DEDENT) || check(TK::TK_EOF)) break;

            auto stmt = parse_statement();
            if (stmt) {
                stmts.push_back(std::move(stmt));
            }
            skip_newlines();
        }
        match(TK::DEDENT);
        return stmts;
    }

    // ========================================================================
    // EXPRESSÕES
    // ========================================================================

    ExprPtr parse_expr() {
        return parse_or();
    }

    ExprPtr parse_or() {
        auto left = parse_and();
        while (check(TK::OR)) {
            int line = cur_line();
            do_advance();
            auto right = parse_and();
            auto node = std::make_unique<Expr>(LogicOpExpr{
                LogicOp::Or, std::move(left), std::move(right), line
            });
            left = std::move(node);
        }
        return left;
    }

    ExprPtr parse_and() {
        auto left = parse_comparison();
        while (check(TK::AND)) {
            int line = cur_line();
            do_advance();
            auto right = parse_comparison();
            auto node = std::make_unique<Expr>(LogicOpExpr{
                LogicOp::And, std::move(left), std::move(right), line
            });
            left = std::move(node);
        }
        return left;
    }

    ExprPtr parse_comparison() {
        auto left = parse_add();

        while (check(TK::EQ) || check(TK::NE) || check(TK::GT) ||
               check(TK::LT) || check(TK::GE) || check(TK::LE)) {
            TK op_tk = current_.type;
            int line = cur_line();
            do_advance();
            auto right = parse_add();

            CmpOp op;
            switch (op_tk) {
                case TK::EQ: op = CmpOp::Eq; break;
                case TK::NE: op = CmpOp::Ne; break;
                case TK::GT: op = CmpOp::Gt; break;
                case TK::LT: op = CmpOp::Lt; break;
                case TK::GE: op = CmpOp::Ge; break;
                case TK::LE: op = CmpOp::Le; break;
                default: op = CmpOp::Eq; break;
            }

            auto node = std::make_unique<Expr>(CmpOpExpr{
                op, std::move(left), std::move(right), line
            });
            left = std::move(node);
        }
        return left;
    }

    ExprPtr parse_add() {
        auto left = parse_mul();

        while (check(TK::PLUS) || check(TK::MINUS)) {
            BinOp op = (current_.type == TK::PLUS) ? BinOp::Add : BinOp::Sub;
            int line = cur_line();
            do_advance();
            auto right = parse_mul();

            auto node = std::make_unique<Expr>(BinOpExpr{
                op, std::move(left), std::move(right), line
            });
            left = std::move(node);
        }
        return left;
    }

    ExprPtr parse_mul() {
        auto left = parse_unary();

        while (check(TK::STAR) || check(TK::SLASH) || check(TK::PERCENT)) {
            BinOp op;
            switch (current_.type) {
                case TK::STAR:    op = BinOp::Mul; break;
                case TK::SLASH:   op = BinOp::Div; break;
                case TK::PERCENT: op = BinOp::Mod; break;
                default: op = BinOp::Mul; break;
            }
            int line = cur_line();
            do_advance();
            auto right = parse_unary();

            auto node = std::make_unique<Expr>(BinOpExpr{
                op, std::move(left), std::move(right), line
            });
            left = std::move(node);
        }
        return left;
    }

    ExprPtr parse_unary() {
        if (check(TK::MINUS)) {
            int line = cur_line();
            do_advance();
            auto operand = parse_unary();
            auto zero = std::make_unique<Expr>(NumberLit{0, line});
            return std::make_unique<Expr>(BinOpExpr{
                BinOp::Sub, std::move(zero), std::move(operand), line
            });
        }
        return parse_postfix();
    }

    ExprPtr parse_postfix() {
        auto node = parse_primary();

        while (true) {
            // Acesso a atributo: obj.attr
            if (check(TK::DOT)) {
                do_advance();
                if (!check(TK::IDENT)) {
                    error("Esperado nome após '.'");
                    break;
                }
                std::string attr = current_.value;
                int line = cur_line();
                do_advance();

                // Chamada de método: obj.metodo(args)
                if (check(TK::LPAREN)) {
                    do_advance();
                    auto args = parse_args();
                    expect(TK::RPAREN, "Esperado ')'");
                    node = std::make_unique<Expr>(MetodoChamadaExpr{
                        std::move(node), attr, std::move(args), line
                    });
                } else {
                    node = std::make_unique<Expr>(AttrGetExpr{
                        std::move(node), attr, line
                    });
                }
            }
            // Chamada de função: func(args)
            else if (check(TK::LPAREN) && is_var_expr(node)) {
                std::string name = get_var_name(node);
                int line = cur_line();
                do_advance();
                auto args = parse_args();
                expect(TK::RPAREN, "Esperado ')'");
                node = std::make_unique<Expr>(ChamadaExpr{
                    name, std::move(args), line
                });
            }
            // Acesso por índice: expr[index]
            else if (check(TK::LBRACKET)) {
                int line = cur_line();
                do_advance();  // consome '['
                auto index = parse_expr();
                expect(TK::RBRACKET, "Esperado ']'");
                node = std::make_unique<Expr>(IndexGetExpr{
                    std::move(node), std::move(index), line
                });
            } else {
                break;
            }
        }
        return node;
    }

    // Encadeia operações postfix (.attr, .metodo(), [index]) sobre uma expressão já construída.
    // Usado por parse_ident_stmt quando o nó base já foi montado manualmente.
    ExprPtr parse_postfix_chain(ExprPtr node) {
        while (true) {
            if (check(TK::DOT)) {
                do_advance();
                if (!check(TK::IDENT)) {
                    error("Esperado nome após '.'");
                    break;
                }
                std::string attr = current_.value;
                int line = cur_line();
                do_advance();

                if (check(TK::LPAREN)) {
                    do_advance();
                    auto args = parse_args();
                    expect(TK::RPAREN, "Esperado ')'");
                    node = std::make_unique<Expr>(MetodoChamadaExpr{
                        std::move(node), attr, std::move(args), line
                    });
                } else {
                    node = std::make_unique<Expr>(AttrGetExpr{
                        std::move(node), attr, line
                    });
                }
            }
            else if (check(TK::LBRACKET)) {
                int line = cur_line();
                do_advance();
                auto index = parse_expr();
                expect(TK::RBRACKET, "Esperado ']'");
                node = std::make_unique<Expr>(IndexGetExpr{
                    std::move(node), std::move(index), line
                });
            }
            else {
                break;
            }
        }
        return node;
    }

    std::vector<ExprPtr> parse_args() {
        std::vector<ExprPtr> args;
        if (!check(TK::RPAREN)) {
            args.push_back(parse_expr());
            while (match(TK::COMMA)) {
                args.push_back(parse_expr());
            }
        }
        return args;
    }

    ExprPtr parse_primary() {
        Token tk = current_;

        // Número
        if (tk.type == TK::NUMBER) {
            do_advance();
            if (tk.value.find('.') != std::string::npos) {
                return std::make_unique<Expr>(FloatLit{
                    std::stod(tk.value), tk.line
                });
            }
            return std::make_unique<Expr>(NumberLit{
                std::stoi(tk.value), tk.line
            });
        }

        // String simples
        if (tk.type == TK::STRING) {
            do_advance();
            return std::make_unique<Expr>(StringLit{
                tk.value, tk.line
            });
        }

        // String interpolada
        if (tk.type == TK::STRING_INTERP) {
            do_advance();
            std::vector<InterpPart> parts;
            for (auto& p : tk.interp_parts) {
                if (!p.is_var) {
                    parts.push_back(InterpPart{false, p.value, nullptr});
                } else {
                    // Tenta parsear como expressão completa
                    // Se é um identificador simples (sem operadores), mantém como string
                    bool is_simple = true;
                    for (char ch : p.value) {
                        if (ch != '_' && !std::isalnum(static_cast<unsigned char>(ch))
                            && ch != '.') {
                            is_simple = false;
                            break;
                        }
                    }

                    if (is_simple) {
                        // Nome simples ou "auto.attr" — mantém como antes
                        parts.push_back(InterpPart{true, p.value, nullptr});
                    } else {
                        // Expressão complexa — parsear com sub-lexer/parser
                        Lexer sub_lex(p.value, lang_config_);
                        Parser sub_parser(sub_lex);
                        auto expr = sub_parser.parse_expr_public();
                        if (expr && !sub_parser.had_error()) {
                            parts.push_back(InterpPart{true, "", std::move(expr)});
                        } else {
                            // Fallback: trata como texto
                            parts.push_back(InterpPart{true, p.value, nullptr});
                        }
                    }
                }
            }
            return std::make_unique<Expr>(StringInterp{
                std::move(parts), tk.line
            });
        }

        // Bool
        if (tk.type == TK::BOOL) {
            do_advance();
            return std::make_unique<Expr>(BoolLit{
                tk.value == "verdadeiro", tk.line
            });
        }

        // Nulo
        if (tk.type == TK::NULO) {
            do_advance();
            return std::make_unique<Expr>(NullLit{tk.line});
        }

        // Auto (self)
        if (tk.type == TK::AUTO) {
            do_advance();
            return std::make_unique<Expr>(AutoExpr{tk.line});
        }

        // Identificador
        if (tk.type == TK::IDENT) {
            do_advance();
            return std::make_unique<Expr>(VarExpr{
                tk.value, tk.line
            });
        }

        // Parênteses
        if (tk.type == TK::LPAREN) {
            do_advance();
            auto expr = parse_expr();
            expect(TK::RPAREN, "Esperado ')'");
            return expr;
        }

        // Lista literal: [expr, expr, ...]
        if (tk.type == TK::LBRACKET) {
            int line = tk.line;
            do_advance();  // consome '['
            std::vector<ExprPtr> elements;
            if (!check(TK::RBRACKET)) {
                elements.push_back(parse_expr());
                while (match(TK::COMMA)) {
                    elements.push_back(parse_expr());
                }
            }
            expect(TK::RBRACKET, "Esperado ']'");
            return std::make_unique<Expr>(ListLitExpr{
                std::move(elements), line
            });
        }

        error("Expressão inesperada: '" + tk.value + "'");
        do_advance();
        return std::make_unique<Expr>(NumberLit{0, tk.line});
    }

    // ========================================================================
    // HELPERS PARA CHECAGEM DE NÓ
    // ========================================================================

    static bool is_var_expr(const ExprPtr& expr) {
        return std::holds_alternative<VarExpr>(expr->node);
    }

    static std::string get_var_name(const ExprPtr& expr) {
        return std::get<VarExpr>(expr->node).name;
    }

    // ========================================================================
    // STATEMENTS
    // ========================================================================

    StmtPtr parse_statement() {
        Token tk = current_;

        // saida / saidal (incluindo variantes com cor) — usa prefixo do idioma
        if (tk.type == TK::IDENT && is_saida_command(tk.value)) {
            return parse_saida();
        }

        if (tk.type == TK::SE)        return parse_if();
        if (tk.type == TK::REPETIR)   return parse_repetir();
        if (tk.type == TK::ENQUANTO)  return parse_enquanto();
        if (tk.type == TK::PARA)      return parse_para();

        if (tk.type == TK::PARAR) {
            do_advance();
            return std::make_unique<Stmt>(PararStmt{tk.line});
        }

        if (tk.type == TK::CONTINUAR) {
            do_advance();
            return std::make_unique<Stmt>(ContinuarStmt{tk.line});
        }

        if (tk.type == TK::FUNCAO)    return parse_funcao();
        if (tk.type == TK::RETORNA)   return parse_retorna();
        if (tk.type == TK::CLASSE)    return parse_classe();
        if (tk.type == TK::NATIVO)    return parse_nativo();
        if (tk.type == TK::IMPORTAR)  return parse_importar();
        if (tk.type == TK::IDENT)     return parse_ident_stmt();
        if (tk.type == TK::AUTO)      return parse_auto_stmt();

        error("Statement inesperado: '" + tk.value + "'");
        do_advance();
        return nullptr;
    }

    // ========================================================================
    // HELPER: verifica se um identificador é comando de saída
    // Suporta: prefixo, prefixo + sufixo_sem_quebra, prefixo + cor,
    //          prefixo + sufixo_sem_quebra + cor
    // Ex: "saida", "saidal", "saida_amarelo", "saidal_verde"
    //     "output", "outputl", "output_yellow", "outputl_green"
    //     "salida", "salidal", "salida_rojo", "salidal_verde"
    // ========================================================================

    bool is_saida_command(const std::string& name) const {
        const std::string& prefix = lang_config_.saida_prefix;
        const std::string& nl_suffix = lang_config_.saida_no_newline_suffix;

        if (name == prefix) return true;
        if (name == prefix + nl_suffix) return true;

        // prefixo + cor (ex: saida_amarelo, output_yellow, salida_rojo)
        if (name.size() > prefix.size() && name.substr(0, prefix.size()) == prefix
            && name[prefix.size()] == '_') {
            return true;
        }

        // prefixo + l + cor (ex: saidal_verde, outputl_yellow, salidal_rojo)
        std::string prefix_nl = prefix + nl_suffix;
        if (name.size() > prefix_nl.size() && name.substr(0, prefix_nl.size()) == prefix_nl
            && name[prefix_nl.size()] == '_') {
            return true;
        }

        return false;
    }

    // ========================================================================
    // SAIDA
    // ========================================================================

    StmtPtr parse_saida() {
        Token tk = current_;
        std::string name = tk.value;
        do_advance();
        expect(TK::LPAREN, "Esperado '(' após " + name);

        const std::string& prefix = lang_config_.saida_prefix;
        const std::string& nl_suffix = lang_config_.saida_no_newline_suffix;
        std::string prefix_nl = prefix + nl_suffix;

        // Detecta newline e cor pelo nome da função
        bool newline = true;
        Cor cor = Cor::Nenhuma;

        if (name.size() > prefix_nl.size() + 1 &&
            name.substr(0, prefix_nl.size()) == prefix_nl &&
            name[prefix_nl.size()] == '_') {
            // prefixol_cor (ex: saidal_verde, salidal_rojo, outputl_yellow)
            newline = false;
            std::string cor_suffix = name.substr(prefix_nl.size()); // "_verde"
            cor = resolve_cor(cor_suffix);
        } else if (name.size() > prefix.size() + 1 &&
                   name.substr(0, prefix.size()) == prefix &&
                   name[prefix.size()] == '_') {
            // prefixo_cor (ex: saida_amarelo, salida_rojo, output_yellow)
            newline = true;
            std::string cor_suffix = name.substr(prefix.size()); // "_amarelo"
            cor = resolve_cor(cor_suffix);
        } else if (name == prefix_nl) {
            // prefixol (ex: saidal, salidal, outputl)
            newline = false;
        }
        // else: prefixo puro (ex: saida, salida, output) → newline=true, cor=Nenhuma

        // Parsear primeiro argumento
        auto value = parse_expr();

        // Se houver mais argumentos separados por vírgula,
        // combiná-los em cadeia de ConcatExpr
        while (match(TK::COMMA)) {
            auto next = parse_expr();
            value = std::make_unique<Expr>(ConcatExpr{
                std::move(value), std::move(next), tk.line
            });
        }

        expect(TK::RPAREN, "Esperado ')'");

        return std::make_unique<Stmt>(SaidaStmt{
            std::move(value), newline, cor, tk.line
        });
    }

    // Resolve sufixo de cor (ex: "_amarelo", "_rojo", "_yellow") → Cor
    Cor resolve_cor(const std::string& suffix) const {
        // Primeiro: tentar mapa de cores do idioma
        auto it = lang_config_.saida_cores.find(suffix);
        if (it != lang_config_.saida_cores.end()) {
            return cor_from_name(it->second);
        }
        // Fallback: tentar diretamente como nome PT (ex: "_amarelo" → "amarelo")
        if (suffix.size() > 1 && suffix[0] == '_') {
            return cor_from_name(suffix.substr(1));
        }
        return Cor::Nenhuma;
    }

    // ========================================================================
    // CONDICIONAL
    // ========================================================================

    StmtPtr parse_if() {
        int line = cur_line();
        do_advance();  // consome 'se'

        auto cond = parse_expr();
        expect(TK::COLON, "Esperado ':' após condição");
        auto body = parse_block();

        std::vector<CondBranch> branches;

        // Primeiro ramo: se
        branches.push_back(CondBranch{std::move(cond), std::move(body)});

        // Ramos ou_se / senao se
        while (check(TK::OU_SE) || (check(TK::SENAO) && peek_is(TK::SE))) {
            if (check(TK::SENAO)) {
                do_advance();  // consome 'senao'
            }
            do_advance();  // consome 'ou_se' ou 'se'

            auto elif_cond = parse_expr();
            expect(TK::COLON, "Esperado ':' após condição");
            auto elif_body = parse_block();

            branches.push_back(CondBranch{
                std::move(elif_cond), std::move(elif_body)
            });
        }

        // Ramo senao
        if (check(TK::SENAO)) {
            do_advance();
            expect(TK::COLON, "Esperado ':' após senao");
            auto else_body = parse_block();

            // senao: condition = nullptr
            branches.push_back(CondBranch{nullptr, std::move(else_body)});
        }

        return std::make_unique<Stmt>(IfStmt{std::move(branches), line});
    }

    // ========================================================================
    // LOOPS
    // ========================================================================

    StmtPtr parse_repetir() {
        int line = cur_line();
        do_advance();  // consome 'repetir'

        auto count = parse_expr();
        expect(TK::COLON, "Esperado ':' após repetir N");
        auto body = parse_block();

        return std::make_unique<Stmt>(RepetirStmt{
            std::move(count), std::move(body), line
        });
    }

    StmtPtr parse_enquanto() {
        int line = cur_line();
        do_advance();  // consome 'enquanto'

        auto cond = parse_expr();
        expect(TK::COLON, "Esperado ':' após condição");
        auto body = parse_block();

        return std::make_unique<Stmt>(EnquantoStmt{
            std::move(cond), std::move(body), line
        });
    }

    StmtPtr parse_para() {
        int line = cur_line();
        do_advance();  // consome 'para'

        if (!check(TK::IDENT)) {
            error("Esperado nome de variável após 'para'");
            return nullptr;
        }
        std::string var_name = current_.value;
        do_advance();

        expect(TK::EM, "Esperado 'em' após variável");
        expect(TK::INTERVALO, "Esperado 'intervalo' após 'em'");
        expect(TK::LPAREN, "Esperado '(' após intervalo");

        auto start = parse_expr();
        expect(TK::COMMA, "Esperado ',' em intervalo");
        auto end = parse_expr();

        ExprPtr step = nullptr;
        if (match(TK::COMMA)) {
            step = parse_expr();
        }

        expect(TK::RPAREN, "Esperado ')' em intervalo");
        expect(TK::COLON, "Esperado ':'");
        auto body = parse_block();

        return std::make_unique<Stmt>(ParaStmt{
            var_name, std::move(start), std::move(end),
            std::move(step), std::move(body), line
        });
    }

    // ========================================================================
    // FUNÇÕES
    // ========================================================================

    StmtPtr parse_funcao() {
        int line = cur_line();
        do_advance();  // consome 'funcao'

        if (!check(TK::IDENT)) {
            error("Esperado nome da função");
            return nullptr;
        }
        std::string name = current_.value;
        do_advance();

        expect(TK::LPAREN, "Esperado '('");
        std::vector<std::string> params;

        if (!check(TK::RPAREN)) {
            if (!check(TK::IDENT)) {
                error("Esperado nome de parâmetro");
            } else {
                params.push_back(current_.value);
                do_advance();
            }
            while (match(TK::COMMA)) {
                if (!check(TK::IDENT)) {
                    error("Esperado nome de parâmetro");
                } else {
                    params.push_back(current_.value);
                    do_advance();
                }
            }
        }

        expect(TK::RPAREN, "Esperado ')'");
        expect(TK::COLON, "Esperado ':'");
        auto body = parse_block();

        return std::make_unique<Stmt>(FuncaoStmt{
            name, std::move(params), std::move(body), line
        });
    }

    StmtPtr parse_retorna() {
        int line = cur_line();
        do_advance();  // consome 'retorna'

        ExprPtr value = nullptr;
        if (!check(TK::NEWLINE) && !check(TK::TK_EOF) && !check(TK::DEDENT)) {
            value = parse_expr();
        }

        return std::make_unique<Stmt>(RetornaStmt{std::move(value), line});
    }

    // ========================================================================
    // CLASSES
    // ========================================================================

    StmtPtr parse_classe() {
        int line = cur_line();
        do_advance();  // consome 'classe'

        if (!check(TK::IDENT)) {
            error("Esperado nome da classe");
            return nullptr;
        }
        std::string name = current_.value;
        do_advance();

        expect(TK::COLON, "Esperado ':'");
        auto body = parse_block();

        return std::make_unique<Stmt>(ClasseStmt{
            name, std::move(body), line
        });
    }

    // ========================================================================
    // NATIVOS
    // ========================================================================

    StmtPtr parse_nativo() {
        int line = cur_line();
        do_advance();  // consome 'nativo'

        if (!check(TK::STRING)) {
            error("Esperado caminho da biblioteca");
            return nullptr;
        }
        std::string lib_path = current_.value;
        do_advance();

        // Sintaxe com lista explícita: nativo "dll" importar func1, func2
        if (check(TK::IMPORTAR)) {
            do_advance();
            std::vector<std::string> funcs;
            while (true) {
                if (!check(TK::IDENT)) {
                    error("Esperado nome da função");
                    break;
                }
                funcs.push_back(current_.value);
                do_advance();
                if (!match(TK::COMMA)) break;
            }
            return std::make_unique<Stmt>(NativoStmt{
                lib_path, std::move(funcs), false, line
            });
        }

        // Sintaxe automática: nativo "bibliotecas/tempo"
        return std::make_unique<Stmt>(NativoStmt{
            lib_path, {}, true, line
        });
    }

    // ========================================================================
    // IMPORTAR
    // ========================================================================

    StmtPtr parse_importar() {
        int line = cur_line();
        do_advance();  // consome 'importar'

        if (check(TK::IDENT)) {
            std::string name = current_.value;
            do_advance();

            // Verifica se vem .jpd (tokens: DOT + IDENT "jpd")
            if (check(TK::DOT) && peek_is(TK::IDENT)) {
                // Salva posição caso não seja .jpd
                std::string dot_ext;
                do_advance();  // consome '.'
                dot_ext = current_.value;
                do_advance();  // consome extensão

                if (dot_ext == "jpd") {
                    // importar ola.jpd → trata como DLL dinâmica
                    std::string lib_path = name + ".jpd";
                    return std::make_unique<Stmt>(NativoStmt{
                        lib_path, {}, true, line
                    });
                }

                // Não é .jpd, erro
                error("Extensão desconhecida: ." + dot_ext);
                return nullptr;
            }

            // Sem extensão: importar janela → bibliotecas/janela (estático .obj)
            std::string lib_path = "bibliotecas/" + name;
            return std::make_unique<Stmt>(NativoStmt{
                lib_path, {}, true, line
            });
        }

        if (check(TK::STRING)) {
            std::string path = current_.value;
            do_advance();

            // Se termina em .jp, importa como código-fonte
            if (path.size() > 3 && path.substr(path.size() - 3) == ".jp") {
                // Tenta abrir o path direto primeiro, depois relativo ao base_dir
                std::string full_path = path;
                std::ifstream file(full_path);
                if (!file.is_open() && !base_dir_.empty()) {
                    full_path = base_dir_ + "/" + path;
                    file.open(full_path);
                }
                if (!file.is_open()) {
                    error("Não foi possível abrir: " + path);
                    return nullptr;
                }
                std::stringstream buf;
                buf << file.rdbuf();
                std::string source = buf.str();

                Lexer imp_lex(source, lang_config_);
                // Calcula diretório do arquivo importado para imports aninhados
                std::string imp_dir;
                size_t last_sep = full_path.find_last_of("/\\");
                if (last_sep != std::string::npos) {
                    imp_dir = full_path.substr(0, last_sep);
                }
                Parser imp_parser(imp_lex, imp_dir);
                auto result = imp_parser.parse();

                if (!result || imp_parser.had_error()) {
                    error("Erro ao parsear: " + path);
                    return nullptr;
                }

                // Insere todos os statements do arquivo no pending
                for (auto& s : result->statements) {
                    pending_imports_.push_back(std::move(s));
                }

                // Retorna nullptr (os statements reais vão pelo pending)
                return nullptr;
            }

            // Se termina em .jpd entre aspas: importar "caminho/lib.jpd"
            if (path.size() > 4 && path.substr(path.size() - 4) == ".jpd") {
                return std::make_unique<Stmt>(NativoStmt{
                    path, {}, true, line
                });
            }

            // Senão, trata como biblioteca nativa
            return std::make_unique<Stmt>(NativoStmt{
                path, {}, true, line
            });
        }

        error("Esperado nome do módulo");
        return nullptr;
    }

    // ========================================================================
    // STATEMENTS DE IDENTIFICADOR
    // ========================================================================

    StmtPtr parse_ident_stmt() {
        Token tk = current_;
        std::string name = tk.value;
        do_advance();

        // Atribuição: nome = expr
        if (match(TK::EQUALS)) {
            auto value = parse_expr();
            return std::make_unique<Stmt>(AssignStmt{
                name, std::move(value), tk.line
            });
        }

        // Acesso por índice: nome[index]...
        if (check(TK::LBRACKET)) {
            do_advance();  // consome '['
            auto index = parse_expr();
            expect(TK::RBRACKET, "Esperado ']'");

            // nome[index] = valor (atribuição simples)
            if (match(TK::EQUALS)) {
                auto value = parse_expr();
                return std::make_unique<Stmt>(IndexSetStmt{
                    name, std::move(index), std::move(value), tk.line
                });
            }

            // Constrói expressão base: nome[index]
            ExprPtr node = std::make_unique<Expr>(IndexGetExpr{
                std::make_unique<Expr>(VarExpr{name, tk.line}),
                std::move(index), tk.line
            });

            // Encadeia postfix: .attr, .metodo(), [index]
            node = parse_postfix_chain(std::move(node));

            return std::make_unique<Stmt>(ExprStmt{
                std::move(node), tk.line
            });
        }

        // Acesso a atributo
        if (check(TK::DOT)) {
            do_advance();
            if (!check(TK::IDENT)) {
                error("Esperado nome após '.'");
                return nullptr;
            }
            std::string attr = current_.value;
            do_advance();

            // nome.attr = expr
            if (match(TK::EQUALS)) {
                auto value = parse_expr();
                auto obj = std::make_unique<Expr>(VarExpr{name, tk.line});
                return std::make_unique<Stmt>(AttrSetStmt{
                    std::move(obj), attr, std::move(value), tk.line
                });
            }

            // nome.metodo(args)
            if (check(TK::LPAREN)) {
                do_advance();
                auto args = parse_args();
                expect(TK::RPAREN, "Esperado ')'");
                auto obj = std::make_unique<Expr>(VarExpr{name, tk.line});
                auto call = std::make_unique<Expr>(MetodoChamadaExpr{
                    std::move(obj), attr, std::move(args), tk.line
                });
                return std::make_unique<Stmt>(ExprStmt{
                    std::move(call), tk.line
                });
            }

            error("Esperado '=' ou '(' após '" + name + "." + attr + "'");
            return nullptr;
        }

        // Chamada de função: nome(args)
        if (check(TK::LPAREN)) {
            do_advance();
            auto args = parse_args();
            expect(TK::RPAREN, "Esperado ')'");
            auto call = std::make_unique<Expr>(ChamadaExpr{
                name, std::move(args), tk.line
            });
            return std::make_unique<Stmt>(ExprStmt{
                std::move(call), tk.line
            });
        }

        error("Statement inesperado: '" + name + "'");
        return nullptr;
    }

    // ========================================================================
    // AUTO (self) STATEMENTS
    // ========================================================================

    StmtPtr parse_auto_stmt() {
        Token tk = current_;
        do_advance();  // consome 'auto'

        expect(TK::DOT, "Esperado '.' após 'auto'");
        if (!check(TK::IDENT)) {
            error("Esperado nome de atributo após 'auto.'");
            return nullptr;
        }
        std::string attr = current_.value;
        do_advance();

        // auto.attr = expr
        if (match(TK::EQUALS)) {
            auto value = parse_expr();
            auto obj = std::make_unique<Expr>(AutoExpr{tk.line});
            return std::make_unique<Stmt>(AttrSetStmt{
                std::move(obj), attr, std::move(value), tk.line
            });
        }

        // auto.metodo(args)
        if (check(TK::LPAREN)) {
            do_advance();
            auto args = parse_args();
            expect(TK::RPAREN, "Esperado ')'");
            auto obj = std::make_unique<Expr>(AutoExpr{tk.line});
            auto call = std::make_unique<Expr>(MetodoChamadaExpr{
                std::move(obj), attr, std::move(args), tk.line
            });
            return std::make_unique<Stmt>(ExprStmt{
                std::move(call), tk.line
            });
        }

        error("Esperado '=' ou '(' após 'auto." + attr + "'");
        return nullptr;
    }
};

} // namespace jplang

#endif // JPLANG_PARSER_HPP