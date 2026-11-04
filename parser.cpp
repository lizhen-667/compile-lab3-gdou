// parser_ll1_table.cpp —— 严格 LL(1) 表驱动语法分析器
// 编译: g++ -std=c++11 parser_ll1_table.cpp -o parser_ll1
// 用法: ./parser_ll1 [tokens.txt] [syntax_result.txt]
#include <iostream>
#include <fstream>
#include <sstream>
#include <string>
#include <vector>
#include <map>
#include <stack>
#include <set>

// ==================== 数据结构 ====================
struct Token {
    std::string type;   // KEYWORD / IDENTIFIER / CONSTANT / STRING / CHAR_CONST / OPERATOR / DELIMITER / EOF
    std::string value;
    int pos;
};

static std::string tokDescStatic(const Token& t) {
    return "<" + t.type + ", " + t.value + ">";
}

// ==================== LL(1) 表驱动分析器 ====================
class Parser {
public:
    explicit Parser(const std::vector<Token>& t) : tokens_(t) { initGrammar(); }

    bool parse() {
        try {
            std::stack<std::string> stk;
            stk.push("$");        // 栈底：结束标记
            stk.push("Program");  // 文法起始符号

            while (!stk.empty()) {
                std::string top = stk.top();

                // 栈底符号：要求输入必须已耗尽
                if (top == "$") {
                    if (peek().type == "EOF") {
                        logAction("✓ 输入耗尽，接受");
                        return true;
                    }
                    throw SyntaxError(peek().pos,
                        "文件末尾仍有未分析的符号 " + tokDesc(peek()));
                }

                // 非终结符：查表 → 展开
                if (isNonTerminal(top)) {
                    const Production* p = select(top, peek());
                    if (!p) {
                        throw SyntaxError(peek().pos,
                            "在非终结符 " + top + " 处无法匹配输入 " + tokDesc(peek()));
                    }
                    std::ostringstream oss;
                    oss << "展开 " << top << " →";
                    if (p->rhs.empty()) oss << " ε";
                    else for (const auto& s : p->rhs) oss << " " << s;
                    logAction(oss.str());

                    stk.pop();
                    // 逆序压栈：保证右部按从左到右匹配
                    for (auto it = p->rhs.rbegin(); it != p->rhs.rend(); ++it)
                        stk.push(*it);
                }
                // 终结符：必须匹配当前 token
                else {
                    if (matchTerm(top, peek())) {
                        logAction("匹配 " + top);
                        advance();
                        stk.pop();
                    } else {
                        throw SyntaxError(peek().pos,
                            "期望 " + top + "，但遇到 " + tokDesc(peek()));
                    }
                }
            }
            return true;
        } catch (const SyntaxError& e) {
            errMsg_ = e.msg;
            errPos_ = e.pos;
            return false;
        }
    }

    const std::vector<std::string>& trace() const { return trace_; }
    const std::string& errMsg() const { return errMsg_; }
    int errPos() const { return errPos_; }

private:
    struct SyntaxError {
        int pos; std::string msg;
        SyntaxError(int p, std::string m) : pos(p), msg(std::move(m)) {}
    };
    struct Production { std::vector<std::string> rhs; };

    const std::vector<Token>& tokens_;
    size_t idx_ = 0;
    int step_ = 0;
    std::vector<std::string> trace_;
    std::string errMsg_;
    int errPos_ = -1;
    std::set<std::string> nonTerminals_;
    std::map<std::string, std::map<std::string, Production>> M_;  // 分析表

    // ---------- 基础工具 ----------
    const Token& peek(int off = 0) const {
        size_t i = idx_ + off;
        if (i < tokens_.size()) return tokens_[i];
        static Token eof{ "EOF", "EOF", (int)tokens_.size() };
        return eof;
    }
    Token advance() {
        if (idx_ >= tokens_.size()) return peek();
        return tokens_[idx_++];
    }
    std::string tokDesc(const Token& t) const {
        return "<" + t.type + ", " + t.value + ">";
    }
    void logAction(const std::string& s) {
        std::ostringstream oss;
        oss << "[" << (++step_) << "] " << s;
        trace_.push_back(oss.str());
    }
    bool isNonTerminal(const std::string& s) const {
        return nonTerminals_.count(s) > 0;
    }
    // 终结符匹配："TYPE:VALUE" 精确 / "TYPE:*" 任意值 / "EOF" 结束
    bool matchTerm(const std::string& term, const Token& t) const {
        if (term == "EOF") return t.type == "EOF";
        size_t colon = term.find(':');
        if (colon == std::string::npos) return false;
        std::string ty = term.substr(0, colon);
        std::string v = term.substr(colon + 1);
        if (t.type != ty) return false;
        return v == "*" || t.value == v;
    }
    // ★ 严格 LL(1)：用当前 token 查 M[N, t] —— 不做任何多 token 前瞻
    const Production* select(const std::string& N, const Token& t) const {
        auto it = M_.find(N);
        if (it == M_.end()) return nullptr;
        auto& row = it->second;
        if (t.type == "EOF") {
            auto e = row.find("EOF");
            return (e != row.end()) ? &e->second : nullptr;
        }
        auto j = row.find(t.type + ":" + t.value);  // 精确匹配
        if (j != row.end()) return &j->second;
        auto k = row.find(t.type + ":*");            // 通配匹配
        if (k != row.end()) return &k->second;
        return nullptr;
    }
    void set(const std::string& N, const std::string& t, std::vector<std::string> rhs) {
        nonTerminals_.insert(N);
        M_[N][t] = { std::move(rhs) };
    }

    // ---------- 填表：每个 (N, t) 一条产生式 ----------
    void initGrammar() {
        const std::vector<std::string> TY = {
            "KEYWORD:int", "KEYWORD:float", "KEYWORD:char",
            "KEYWORD:void", "KEYWORD:double", "KEYWORD:long", "KEYWORD:short"
        };
        // Expr 首符集（Factor 首符集）
        const std::vector<std::string> EXPR_FIRST = {
            "IDENTIFIER:*", "CONSTANT:*", "STRING:*", "CHAR_CONST:*",
            "DELIMITER:(", "OPERATOR:+", "OPERATOR:-"
        };
        // ε 分支的 FOLLOW 集：TermTail / ExprTail / FactorRest / IdTail
        // = {+ - * / < > <= >= == != && || ) ; , } EOF}
        const std::vector<std::string> FOLLOW_OP = {
            "OPERATOR:+", "OPERATOR:-", "OPERATOR:*", "OPERATOR:/",
            "OPERATOR:<", "OPERATOR:>", "OPERATOR:<=", "OPERATOR:>=",
            "OPERATOR:==", "OPERATOR:!=", "OPERATOR:&&", "OPERATOR:||"
        };
        const std::vector<std::string> FOLLOW_DELIM = {
            "DELIMITER:)", "DELIMITER:;", "DELIMITER:,", "DELIMITER:}"
        };

        // ==================== Program ====================
        // Program → TypeSpec IDENTIFIER AfterTypeId | NonTypeStmt StmtList | ε
        for (const auto& ty : TY)
            set("Program", ty, {"TypeSpec", "IDENTIFIER:*", "AfterTypeId"});
        set("Program", "KEYWORD:if",     {"NonTypeStmt", "StmtList"});
        set("Program", "KEYWORD:return", {"NonTypeStmt", "StmtList"});
        set("Program", "DELIMITER:{",    {"NonTypeStmt", "StmtList"});
        set("Program", "DELIMITER:;",    {"NonTypeStmt", "StmtList"});
        set("Program", "DELIMITER:(",    {"NonTypeStmt", "StmtList"});
        set("Program", "IDENTIFIER:*",   {"NonTypeStmt", "StmtList"});
        set("Program", "CONSTANT:*",     {"NonTypeStmt", "StmtList"});
        set("Program", "EOF",            {});

        // ==================== AfterTypeId ====================
        // AfterTypeId → '(' ParenRest | DeclRest StmtList
        set("AfterTypeId", "DELIMITER:(", {"DELIMITER:(", "ParenRest"});
        set("AfterTypeId", "DELIMITER:,", {"DeclRest", "StmtList"});
        set("AfterTypeId", "DELIMITER:;", {"DeclRest", "StmtList"});

        // ==================== ParenRest ====================
        // ParenRest → ParamList ')' Block | ')' Block
        set("ParenRest", "DELIMITER:)", {"DELIMITER:)", "Block"});
        for (const auto& ty : TY)
            set("ParenRest", ty, {"ParamList", "DELIMITER:)", "Block"});

        // ==================== ParamList / ParamRest ====================
        for (const auto& ty : TY)
            set("ParamList", ty, {"TypeSpec", "IDENTIFIER:*", "ParamRest"});
        set("ParamRest", "DELIMITER:,", {"DELIMITER:,", "ParamList"});
        set("ParamRest", "DELIMITER:)", {});

        // ==================== TypeSpec ====================
        for (const auto& ty : TY) set("TypeSpec", ty, {ty});

        // ==================== Block / StmtList ====================
        set("Block", "DELIMITER:{", {"DELIMITER:{", "StmtList", "DELIMITER:}"});

        // StmtList → Stmt StmtList | ε（ε 的 FOLLOW 是 } 和 EOF）
        for (const auto& ty : TY)          set("StmtList", ty, {"Stmt", "StmtList"});
        set("StmtList", "KEYWORD:if",     {"Stmt", "StmtList"});
        set("StmtList", "KEYWORD:return", {"Stmt", "StmtList"});
        set("StmtList", "DELIMITER:{",    {"Stmt", "StmtList"});
        set("StmtList", "DELIMITER:;",    {"Stmt", "StmtList"});
        set("StmtList", "DELIMITER:(",    {"Stmt", "StmtList"});
        set("StmtList", "IDENTIFIER:*",   {"Stmt", "StmtList"});
        set("StmtList", "CONSTANT:*",     {"Stmt", "StmtList"});
        set("StmtList", "DELIMITER:}",    {});   // ε
        set("StmtList", "EOF",            {});   // ε

        // ==================== Stmt（含 NonTypeStmt：不含 DeclStmt 分支） ====================
        // Stmt → IfStmt | Block | ReturnStmt | DeclStmt | ';'
        //      | IDENTIFIER IdStmt | CONSTANT TermTail ExprTail ';'
        //      | '(' Expr ')' TermTail ExprTail ';'
        auto fillStmt = [&](const std::string& N, bool includeDecl) {
            set(N, "KEYWORD:if",     {"IfStmt"});
            set(N, "KEYWORD:return", {"ReturnStmt"});
            set(N, "DELIMITER:{",    {"Block"});
            set(N, "DELIMITER:;",    {"DELIMITER:;"});
            set(N, "IDENTIFIER:*",   {"IDENTIFIER:*", "IdStmt"});
            set(N, "CONSTANT:*",     {"CONSTANT:*", "TermTail", "ExprTail", "DELIMITER:;"});
            set(N, "DELIMITER:(",    {"DELIMITER:(", "Expr", "DELIMITER:)", "TermTail", "ExprTail", "DELIMITER:;"});
            if (includeDecl)
                for (const auto& ty : TY) set(N, ty, {"DeclStmt"});
        };
        fillStmt("Stmt",        true);
        fillStmt("NonTypeStmt", false);

        // ==================== DeclStmt ====================
        for (const auto& ty : TY)
            set("DeclStmt", ty, {"TypeSpec", "IDENTIFIER:*", "DeclRest"});
        set("DeclRest", "DELIMITER:,", {"DELIMITER:,", "IDENTIFIER:*", "DeclRest"});
        set("DeclRest", "DELIMITER:;", {"DELIMITER:;"});

        // ==================== IdStmt（IDENTIFIER 已经被 Stmt 消费） ====================
        set("IdStmt", "OPERATOR:=", {"OPERATOR:=", "Expr", "DELIMITER:;"});
        set("IdStmt", "DELIMITER:;", {"DELIMITER:;"});
        set("IdStmt", "OPERATOR:+", {"OPERATOR:+", "Term", "ExprTail", "DELIMITER:;"});
        set("IdStmt", "OPERATOR:-", {"OPERATOR:-", "Term", "ExprTail", "DELIMITER:;"});
        set("IdStmt", "OPERATOR:*", {"OPERATOR:*", "Factor", "TermTail", "ExprTail", "DELIMITER:;"});
        set("IdStmt", "OPERATOR:/", {"OPERATOR:/", "Factor", "TermTail", "ExprTail", "DELIMITER:;"});
        set("IdStmt", "DELIMITER:(", {"DELIMITER:(", "Args", "DELIMITER:)", "TermTail", "ExprTail", "DELIMITER:;"});

        // ==================== IfStmt / ElsePart ====================
        set("IfStmt", "KEYWORD:if",
            {"KEYWORD:if", "DELIMITER:(", "Cond", "DELIMITER:)", "Stmt", "ElsePart"});
        set("ElsePart", "KEYWORD:else", {"KEYWORD:else", "Stmt"});
        // ε 的 FOLLOW(ElsePart) = FOLLOW(IfStmt) = StmtList 首符 ∪ { }, EOF }
        for (const auto& ty : TY)          set("ElsePart", ty, {});
        set("ElsePart", "KEYWORD:if",     {});
        set("ElsePart", "KEYWORD:return", {});
        set("ElsePart", "DELIMITER:{",    {});
        set("ElsePart", "DELIMITER:;",    {});
        set("ElsePart", "DELIMITER:(",    {});
        set("ElsePart", "IDENTIFIER:*",   {});
        set("ElsePart", "CONSTANT:*",     {});
        set("ElsePart", "DELIMITER:}",    {});
        set("ElsePart", "EOF",            {});

        // ==================== ReturnStmt / ReturnTail ====================
        set("ReturnStmt", "KEYWORD:return", {"KEYWORD:return", "ReturnTail"});
        set("ReturnTail", "DELIMITER:;", {"DELIMITER:;"});
        for (const auto& f : EXPR_FIRST)
            set("ReturnTail", f, {"Expr", "DELIMITER:;"});

        // ==================== Cond ====================
        for (const auto& f : EXPR_FIRST)
            set("Cond", f, {"Expr", "OptRel", "LogicLoop"});

        // ==================== OptRel ====================
        // OptRel → RelOp Expr | ε；ε 的 FOLLOW = {&&, ||, )}
        set("OptRel", "OPERATOR:<",  {"RelOp", "Expr"});
        set("OptRel", "OPERATOR:>",  {"RelOp", "Expr"});
        set("OptRel", "OPERATOR:<=", {"RelOp", "Expr"});
        set("OptRel", "OPERATOR:>=", {"RelOp", "Expr"});
        set("OptRel", "OPERATOR:==", {"RelOp", "Expr"});
        set("OptRel", "OPERATOR:!=", {"RelOp", "Expr"});
        set("OptRel", "OPERATOR:&&", {});
        set("OptRel", "OPERATOR:||", {});
        set("OptRel", "DELIMITER:)", {});

        // ==================== LogicLoop ====================
        set("LogicLoop", "OPERATOR:&&", {"OPERATOR:&&", "Expr", "OptRel", "LogicLoop"});
        set("LogicLoop", "OPERATOR:||", {"OPERATOR:||", "Expr", "OptRel", "LogicLoop"});
        set("LogicLoop", "DELIMITER:)", {});

        // ==================== RelOp ====================
        set("RelOp", "OPERATOR:<",  {"OPERATOR:<"});
        set("RelOp", "OPERATOR:>",  {"OPERATOR:>"});
        set("RelOp", "OPERATOR:<=", {"OPERATOR:<="});
        set("RelOp", "OPERATOR:>=", {"OPERATOR:>="});
        set("RelOp", "OPERATOR:==", {"OPERATOR:=="});
        set("RelOp", "OPERATOR:!=", {"OPERATOR:!="});

        // ==================== Expr / ExprTail ====================
        for (const auto& f : EXPR_FIRST) set("Expr", f, {"Term", "ExprTail"});
        set("ExprTail", "OPERATOR:+", {"OPERATOR:+", "Term", "ExprTail"});
        set("ExprTail", "OPERATOR:-", {"OPERATOR:-", "Term", "ExprTail"});
        // ε 的 FOLLOW(ExprTail) = FOLLOW(Expr) = {< > <= >= == != && || ) ; , }
        for (const auto& op : {"OPERATOR:<","OPERATOR:>","OPERATOR:<=","OPERATOR:>=",
                               "OPERATOR:==","OPERATOR:!=","OPERATOR:&&","OPERATOR:||"})
            set("ExprTail", op, {});
        for (const auto& d : {"DELIMITER:)","DELIMITER:;","DELIMITER:,","DELIMITER:}"})
            set("ExprTail", d, {});
        set("ExprTail", "EOF", {});

        // ==================== Term / TermTail ====================
        for (const auto& f : EXPR_FIRST) set("Term", f, {"Factor", "TermTail"});
        set("TermTail", "OPERATOR:*", {"OPERATOR:*", "Factor", "TermTail"});
        set("TermTail", "OPERATOR:/", {"OPERATOR:/", "Factor", "TermTail"});
        // ε 的 FOLLOW(TermTail) = FOLLOW(Term) = {+ - < > <= >= == != && || ) ; , }
        for (const auto& op : {"OPERATOR:+","OPERATOR:-","OPERATOR:<","OPERATOR:>",
                               "OPERATOR:<=","OPERATOR:>=","OPERATOR:==","OPERATOR:!=",
                               "OPERATOR:&&","OPERATOR:||"})
            set("TermTail", op, {});
        for (const auto& d : {"DELIMITER:)","DELIMITER:;","DELIMITER:,","DELIMITER:}"})
            set("TermTail", d, {});
        set("TermTail", "EOF", {});

        // ==================== Factor / FactorRest ====================
        set("Factor", "IDENTIFIER:*", {"IDENTIFIER:*", "FactorRest"});
        set("Factor", "CONSTANT:*",   {"CONSTANT:*"});
        set("Factor", "STRING:*",     {"STRING:*"});
        set("Factor", "CHAR_CONST:*", {"CHAR_CONST:*"});
        set("Factor", "DELIMITER:(",  {"DELIMITER:(", "Expr", "DELIMITER:)"});
        set("Factor", "OPERATOR:+",   {"OPERATOR:+", "Factor"});
        set("Factor", "OPERATOR:-",   {"OPERATOR:-", "Factor"});

        set("FactorRest", "DELIMITER:(", {"DELIMITER:(", "Args", "DELIMITER:)"});
        // ε 的 FOLLOW(FactorRest) = FOLLOW(Factor) ⊇ FOLLOW(Term) ∪ {*,/}
        for (const auto& op : FOLLOW_OP) set("FactorRest", op, {});
        for (const auto& d : FOLLOW_DELIM) set("FactorRest", d, {});
        set("FactorRest", "EOF", {});

        // ==================== Args / ArgsList ====================
        for (const auto& f : EXPR_FIRST) set("Args", f, {"Expr", "ArgsList"});
        set("Args", "DELIMITER:)", {});                       // ε，FOLLOW = { )}
        set("ArgsList", "DELIMITER:,", {"DELIMITER:,", "Expr", "ArgsList"});
        set("ArgsList", "DELIMITER:)", {});                   // ε，FOLLOW = { )}
    }
};

// ==================== 加载二元式文件 ====================
static std::vector<Token> loadTokens(const std::string& filename) {
    std::vector<Token> tokens;
    std::ifstream fin(filename);
    if (!fin) { std::cerr << "错误：无法打开 token 文件 " << filename << "\n"; return tokens; }

    std::string line;
    int pos = 0;
    while (std::getline(fin, line)) {
        if (line.empty()) continue;
        if (line.find("错误信息") != std::string::npos ||
            line.find("Lexical Error") != std::string::npos) break;
        if (line[0] != '<') continue;
        size_t comma = line.find(',');
        size_t rp = line.rfind('>');
        if (comma == std::string::npos || rp == std::string::npos) continue;
        std::string type = line.substr(1, comma - 1);
        std::string value = line.substr(comma + 1, rp - comma - 1);
        while (!type.empty() && type.front() == ' ')  type.erase(type.begin());
        while (!type.empty() && type.back()  == ' ')  type.pop_back();
        while (!value.empty() && value.front() == ' ') value.erase(value.begin());
        while (!value.empty() && value.back()  == ' ') value.pop_back();
        tokens.push_back({ type, value, pos++ });
    }
    fin.close();
    if (tokens.empty() || tokens.back().type != "EOF")
        tokens.push_back({ "EOF", "EOF", (int)tokens.size() });
    return tokens;
}

// ==================== main ====================
int main(int argc, char* argv[]) {
    std::string inputFile  = (argc > 1) ? argv[1] : "tokens.txt";
    std::string outputFile = (argc > 2) ? argv[2] : "syntax_result.txt";

    auto tokens = loadTokens(inputFile);
    if (tokens.size() <= 1) {
        std::cerr << "错误：token 文件为空或未读到有效 token\n";
        return 1;
    }

    std::ofstream fout(outputFile);
    if (!fout) { std::cerr << "错误：无法创建 " << outputFile << "\n"; return 1; }

    fout << "===== 输入 token 流 =====\n";
    for (const auto& t : tokens)
        fout << "第" << (t.pos + 1) << "个 token：" << tokDescStatic(t) << "\n";

    fout << "\n===== 分析过程（LL(1) 表驱动）=====\n";
    Parser parser(tokens);
    bool ok = parser.parse();
    for (const auto& s : parser.trace()) fout << s << "\n";

    fout << "\n===== 分析结果 =====\n";
    if (ok) {
        fout << "✓ 语法正确！返回结果值：0\n";
        std::cout << "语法分析完成：✓ 输入串合法（严格 LL(1) 表驱动）\n";
    } else {
        int p = parser.errPos();
        fout << "✗ 语法错误！\n";
        fout << "错误位置：第 " << (p + 1) << " 个 token";
        if (p >= 0 && p < (int)tokens.size())
            fout << "（即 " << tokDescStatic(tokens[p]) << "）";
        fout << "\n错误信息：" << parser.errMsg() << "\n";
        fout << "返回结果值：1\n";
        std::cout << "语法分析完成：✗ 输入串非法\n"
                  << "  错误位置：第 " << (p + 1) << " 个 token\n"
                  << "  错误信息：" << parser.errMsg() << "\n";
    }
    fout.close();
    std::cout << "详细过程已写入 " << outputFile << "\n";
    return ok ? 0 : 1;
}