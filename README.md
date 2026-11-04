# 编译原理实验三：LL(1) 表驱动语法分析器

> 读入实验二词法分析器输出的 `<种别, 属性值>` token 流，使用 **LL(1) 预测分析表 + 显式栈** 判断源程序是否符合给定 C 语言子集的文法，并输出完整的分析（推导/匹配）过程与错误定位。

---

## 1. 实验目标

本实验实现一个**表驱动的 LL(1) 语法分析器**，完成以下工作：

1. 读取词法分析器产生的二元式 token 文件；
2. 根据预先构造的 LL(1) 预测分析表，使用一个显式栈完成最左推导；
3. 每一步只查看**当前一个 token**（严格单符号前瞻），不做回溯；
4. 输出：
   - 输入 token 流；
   - 每一步的分析动作（展开非终结符 / 匹配终结符）；
   - 最终结论（语法正确 / 语法错误）；
   - 错误时的 token 位置和具体原因；
   - 进程退出码：`0` 表示合法，`1` 表示非法或输入输出失败。

整个程序只依赖 C++11 标准库，全部逻辑集中在单文件 `parser.cpp` 中，便于阅读、调试和答辩讲解。

---

## 2. 仓库结构

```text
实验三/
├── parser.cpp                  # LL(1) 表驱动语法分析器（单文件实现）
├── tokens-实验二输出.txt        # 实验二词法分析器输出，作为本实验的输入样例
├── syntax_result-最终输出.txt   # 样例运行结果（279 步分析过程 + 接受）
└── README.md                   # 本文档
```

---

## 3. 快速开始

### 3.1 编译

源码文件名是 `parser.cpp`。在项目根目录执行：

```bash
g++ -std=c++11 parser.cpp -o parser
```

Windows（MinGW / MSYS2）下可执行文件通常带 `.exe`：

```bash
g++ -std=c++11 parser.cpp -o parser.exe
```

使用 MSVC：

```bat
cl /std:c++14 /utf-8 /EHsc parser.cpp
```

> 说明：`parser.cpp` 文件头部的注释仍沿用早期文件名 `parser_ll1_table.cpp`，实际编译时请以当前文件名 `parser.cpp` 为准。

### 3.2 运行

```bash
./parser <tokens.txt> <syntax_result.txt>
```

- 第 1 个参数：输入 token 文件，缺省为 `tokens.txt`；
- 第 2 个参数：分析结果输出文件，缺省为 `syntax_result.txt`。

用仓库自带的样例运行：

```bash
./parser "tokens-实验二输出.txt" syntax_result.txt
```

控制台输出（合法输入）：

```text
语法分析完成：✓ 输入串合法（严格 LL(1) 表驱动）
详细过程已写入 syntax_result.txt
```

退出码为 `0`。如果输入非法，控制台会打印错误位置和原因，退出码为 `1`。

### 3.3 与实验二的衔接

本实验的输入就是实验二词法分析器的输出。完整的处理链如下：

```bash
# 实验二：词法分析，生成 token 文件
g++ -std=c++11 ../实验二/lexer.cpp -o lexer
./lexer input.c tokens.txt

# 实验三：语法分析，读取 token 文件
g++ -std=c++11 parser.cpp -o parser
./parser tokens.txt syntax_result.txt
```

### 3.4 编码说明

- `parser.cpp` 是 UTF-8 编码；
- 在中文 Windows 控制台中，如果中文显示为乱码，可先执行 `chcp 65001`，或使用 Windows Terminal；
- 仓库中现有的 `syntax_result-最终输出.txt` 是 **GBK 编码**（用 `file` 检测为 Non-ISO extended-ASCII），GitHub 网页预览时中文可能显示为乱码。重新运行程序即可生成 UTF-8 版本；
- MinGW 下可显式指定执行字符集：`g++ -std=c++11 -finput-charset=UTF-8 -fexec-charset=UTF-8 parser.cpp -o parser`；
- MSVC 编译时加 `/utf-8`，保证源码中的中文字符串按 UTF-8 处理。

---

## 4. 整体数据流

```text
┌──────────────────────┐        ┌───────────────────────┐
│ 源程序 input.c       │        │ 实验二 lexer.cpp      │
└──────────┬───────────┘        └───────────┬───────────┘
           │                                │
           └───────────────┬────────────────┘
                           ▼
              ┌──────────────────────────┐
              │ tokens.txt               │
              │ <TYPE, VALUE> 逐行保存    │
              └────────────┬─────────────┘
                           │ loadTokens()
                           ▼
              ┌──────────────────────────┐
              │ Parser（LL(1) 表驱动）    │
              │ 显式栈 + 预测分析表 M     │
              └────────────┬─────────────┘
                           │ trace_ / errMsg_ / errPos_
                           ▼
              ┌──────────────────────────┐
              │ syntax_result.txt        │
              │ token 流 + 分析过程 + 结论│
              └──────────────────────────┘
```

程序内部可以分成四层：

| 层次 | 代码位置 | 职责 |
|---|---|---|
| 词法结果加载 | `loadTokens()` | 把文本形式的二元式解析成 `Token` 数组 |
| 文法定义 | `Parser::initGrammar()`、`Parser::set()` | 把文法规则写成 LL(1) 分析表表项 |
| 语法分析 | `Parser::parse()`、`select()`、`matchTerm()` | 查表、压栈/弹栈、匹配 token |
| 结果输出 | `main()` | 写 token 流、分析轨迹、错误信息，设置退出码 |

---

## 5. 输入：token 文件格式

### 5.1 基本格式

实验二的词法分析器按行输出二元式：

```text
<种别, 属性值>
```

例如：

```text
<KEYWORD, void>
<IDENTIFIER, main>
<DELIMITER, (>
<DELIMITER, )>
<DELIMITER, {>
<KEYWORD, int>
<IDENTIFIER, num1>
<DELIMITER, ;>
...
<EOF, EOF>
```

### 5.2 种别（token type）

| 种别 | 含义 | 示例 |
|---|---|---|
| `KEYWORD` | 关键字 | `int`、`void`、`if`、`else`、`return` |
| `IDENTIFIER` | 标识符 | `main`、`num1`、`printf` |
| `CONSTANT` | 数值常量 | `0`、`5`、`3.14` |
| `STRING` | 字符串常量 | `"hello"` |
| `CHAR_CONST` | 字符常量 | `'a'` |
| `OPERATOR` | 运算符 | `=`、`+`、`-`、`*`、`/`、`<`、`<=`、`==`、`&&` |
| `DELIMITER` | 界符 | `(`、`)`、`{`、`}`、`;`、`,` |
| `EOF` | 输入结束标志 | `EOF` |

### 5.3 `loadTokens()` 的解析规则

`loadTokens()` 逐行读取 token 文件，处理逻辑如下：

1. 跳过空行；
2. 遇到包含 `错误信息` 或 `Lexical Error` 的行就停止读取，防止把实验二的词法错误文本当成 token；
3. 只处理以 `<` 开头的行；
4. 用**第一个逗号**分隔种别和属性值，用**最后一个 `>`** 确定行尾；
5. 去掉种别和属性值两端的空格；
6. 每个 token 记录一个从 0 开始的位置 `pos`；
7. 如果文件末尾没有 `EOF` token，自动补一个；
8. 文件打不开或没有读到有效 token 时，`main()` 会报错并返回 `1`。

第 4 条特别重要：字符串 token 的属性值本身可能包含逗号（例如 `"a,b"`），用“第一个逗号 + 最后一个 `>`”切分可以完整保留属性值。

---

## 6. 文法设计

### 6.1 终结符的编码方式

词法分析器已经把每个单词归类为“种别 + 属性值”。语法分析表如果直接以属性值为列，列数会无限多（标识符、常量、字符串都有无穷多种取值），表就无法写出来。因此本程序采用两级编码：

| 编码 | 含义 | 例子 |
|---|---|---|
| `TYPE:VALUE` | 精确匹配“种别 + 值” | `KEYWORD:int`、`OPERATOR:;`、`DELIMITER:(` |
| `TYPE:*` | 匹配某个种别下的任意值 | `IDENTIFIER:*`、`CONSTANT:*`、`STRING:*`、`CHAR_CONST:*` |
| `EOF` | 输入结束 | 单独处理，不带冒号 |

这样既保留了关键字/运算符之间的区别（`int` 和 `if` 必须区分），又把无限多的标识符、常量收敛成一个有限列。

在本文后续的 BNF 中，使用下面的简写：

```text
type       = int | float | char | void | double | long | short
id         = <IDENTIFIER, *>
const      = <CONSTANT, *>
str        = <STRING, *>
charconst  = <CHAR_CONST, *>
"("        = <DELIMITER, (>
...
```

### 6.2 完整文法

下面是 `initGrammar()` 中实际实现的文法（`ε` 表示空串）：

```text
Program      → TypeSpec id AfterTypeId
             | NonTypeStmt StmtList
             | ε

AfterTypeId  → "(" ParenRest
             | DeclRest StmtList

ParenRest    → ")" Block
             | ParamList ")" Block

ParamList    → TypeSpec id ParamRest
ParamRest    → "," ParamList
             | ε

TypeSpec     → int | float | char | void | double | long | short

Block        → "{" StmtList "}"

StmtList     → Stmt StmtList
             | ε

Stmt         → DeclStmt
             | IfStmt
             | ReturnStmt
             | Block
             | ";"
             | id IdStmt
             | const TermTail ExprTail ";"
             | "(" Expr ")" TermTail ExprTail ";"

NonTypeStmt  → IfStmt
             | ReturnStmt
             | Block
             | ";"
             | id IdStmt
             | const TermTail ExprTail ";"
             | "(" Expr ")" TermTail ExprTail ";"

DeclStmt     → TypeSpec id DeclRest
DeclRest     → "," id DeclRest
             | ";"

IdStmt       → "=" Expr ";"
             | ";"
             | "+" Term ExprTail ";"
             | "-" Term ExprTail ";"
             | "*" Factor TermTail ExprTail ";"
             | "/" Factor TermTail ExprTail ";"
             | "(" Args ")" TermTail ExprTail ";"

IfStmt       → if "(" Cond ")" Stmt ElsePart
ElsePart     → else Stmt
             | ε

ReturnStmt   → return ReturnTail
ReturnTail   → ";"
             | Expr ";"

Cond         → Expr OptRel LogicLoop
OptRel       → RelOp Expr
             | ε
LogicLoop    → "&&" Expr OptRel LogicLoop
             | "||" Expr OptRel LogicLoop
             | ε
RelOp        → "<" | ">" | "<=" | ">=" | "==" | "!="

Expr         → Term ExprTail
ExprTail     → "+" Term ExprTail
             | "-" Term ExprTail
             | ε

Term         → Factor TermTail
TermTail     → "*" Factor TermTail
             | "/" Factor TermTail
             | ε

Factor       → id FactorRest
             | const
             | str
             | charconst
             | "(" Expr ")"
             | "+" Factor
             | "-" Factor

FactorRest   → "(" Args ")"
             | ε

Args         → Expr ArgsList
             | ε
ArgsList     → "," Expr ArgsList
             | ε
```

### 6.3 文法构造要点

这份文法不是直接照抄 C 语言文法，而是为 **LL(1) 分析** 做了针对性改造。

#### 1）消除左递归

加减、乘除、语句列表都采用“首项 + 尾部”的写法：

```text
Expr  → Term ExprTail
ExprTail → "+" Term ExprTail | "-" Term ExprTail | ε

Term  → Factor TermTail
TermTail → "*" Factor TermTail | "/" Factor TermTail | ε
```

这样既消除了左递归（LL(1) 不允许左递归），又通过右递归的 `ExprTail/TermTail` 保持左结合：

```text
1 - 2 - 3  等价于  (1 - 2) - 3
```

#### 2）提取公共左因子

C 语言的顶层声明和函数定义都以 `类型 标识符` 开头。如果直接写成：

```text
Program → TypeSpec id "(" ...
        | TypeSpec id DeclRest ...
```

第一个 token 都是类型关键字，LL(1) 无法只凭一个 token 决定分支。因此把公共前缀提取出来：

```text
Program     → TypeSpec id AfterTypeId
AfterTypeId → "(" ParenRest          // 函数定义
            | DeclRest StmtList      // 变量声明
```

看到类型和标识符后，再看下一个 token：是 `(` 就按函数定义处理，是 `,` 或 `;` 就按变量声明处理。

#### 3）用分层文法表达优先级

表达式按优先级分成三层：

```text
Expr  → Term ExprTail      // 加减层
Term  → Factor TermTail    // 乘除层
Factor → ...               // 原子、括号、一元正负号
```

乘除被包在 `Term` 里，因此天然比加减结合得更紧。例如 `1 + 2 * 3` 会解析成 `1 + (2 * 3)`。

#### 4）把“标识符开头的语句”单独提取

语句可能以标识符开头，但标识符后面可能是赋值、函数调用或普通表达式：

```text
id = Expr ;          // 赋值
id ( Args ) ;        // 函数调用
id + Term ... ;      // 表达式语句
id * Factor ... ;    // 表达式语句
id ;                 // 空表达式语句
```

因此用 `Stmt → id IdStmt` 先把 `id` 吃掉，再由 `IdStmt` 根据下一个 token（`=`、`(`、`+`、`-`、`*`、`/`、`;`）决定后续。这是对“以 id 开头”的公共前缀做的提取，避免 LL(1) 冲突。

以常量或左括号开头的语句没有这种歧义，所以直接在 `Stmt` 中写完整形式：

```text
Stmt → const TermTail ExprTail ";"
     | "(" Expr ")" TermTail ExprTail ";"
```

#### 5）`Stmt` 与 `NonTypeStmt`

`NonTypeStmt` 和 `Stmt` 几乎完全一样，唯一区别是 **不包含 `DeclStmt`**。

原因是 `Program` 有两个入口：

- 第一个 token 是类型关键字：交给 `TypeSpec id AfterTypeId`，专门处理“第一个顶层声明 / 第一个函数定义”；
- 第一个 token 不是类型关键字：交给 `NonTypeStmt StmtList`，此时不可能出现声明，所以用 `NonTypeStmt` 排除声明分支，避免重复匹配。

后续的声明由 `StmtList → Stmt → DeclStmt` 正常处理。

#### 6）悬挂 else 的消歧

`ElsePart → else Stmt | ε` 是经典的“悬挂 else”问题。按严格 FOLLOW 计算，`else` 也会出现在 `FOLLOW(ElsePart)` 中，因此 `else Stmt` 与 `ε` 在 `else` 上存在潜在冲突。

本程序采用 C 语言的标准约定：**`else` 与最近的未匹配 `if` 结合**。实现上就是：

- 当 lookahead 是 `else` 时，只填 `ElsePart → else Stmt`；
- `ElsePart` 的 `ε` 表项中**不包含** `else`。

这样分析表仍然是确定性的，且语义符合直觉。

---

## 7. LL(1) 预测分析表

### 7.1 数据结构

分析表定义在 `Parser` 类中：

```cpp
std::map<std::string, std::map<std::string, Production>> M_;
```

- 外层 key：非终结符名，如 `"Expr"`、`"Stmt"`；
- 内层 key：终结符编码，如 `"KEYWORD:int"`、`"IDENTIFIER:*"`、`"EOF"`；
- 值：一条产生式（只保存右部 `rhs`，左部就是外层 key）。

这是一个**稀疏表**：没有填写的 `(N, t)` 组合就表示“此处不允许出现 t”，分析时直接报错。

非终结符集合单独保存在：

```cpp
std::set<std::string> nonTerminals_;
```

每次调用 `set(N, ...)` 时都会把 `N` 插入这个集合，`isNonTerminal()` 就通过集合成员判断当前栈顶符号的类型。这样即使终结符编码里含有冒号、非终结符名不含冒号，也不会因为字符串格式而误判。

### 7.2 表的构建

表不是在运行时由 FIRST/FOLLOW 自动计算的，而是在 `initGrammar()` 中通过 `set()` 一条条写死的：

```cpp
void set(const std::string& N,
         const std::string& t,
         std::vector<std::string> rhs) {
    nonTerminals_.insert(N);
    M_[N][t] = { std::move(rhs) };
}
```

例如：

```cpp
// Expr → Term ExprTail
for (const auto& f : EXPR_FIRST)
    set("Expr", f, {"Term", "ExprTail"});

// AfterTypeId → "(" ParenRest
set("AfterTypeId", "DELIMITER:(", {"DELIMITER:(", "ParenRest"});

// ElsePart → ε（在 FOLLOW 集上）
set("ElsePart", "DELIMITER:}", {});
```

`initGrammar()` 中的注释标明了每个非终结符对应的文法规则，便于对照检查。当前文法共有 **29 个非终结符、200 余条表项**。

这种“静态建表”的方式优点是直观、可审计、启动快；缺点是文法改动时需要同步维护 FIRST/FOLLOW 相关的所有表项。后文“可扩展方向”给出了自动生成表的思路。

### 7.3 查表规则 `select()`

`select(N, lookahead)` 负责在表中找到 `(N, lookahead)` 对应的产生式，查找顺序是：

1. **输入结束**：如果 lookahead 是 `EOF`，直接查内层 key `"EOF"`；
2. **精确匹配**：查 `type + ":" + value`，例如 `KEYWORD:int`、`OPERATOR:;`；
3. **通配匹配**：查 `type + ":*"`，例如 `IDENTIFIER:*`、`CONSTANT:*`；
4. 都没找到：返回空指针，分析器报“无法匹配”的错误。

精确匹配优先于通配匹配很重要：如果某一行同时存在 `KEYWORD:int` 和 `KEYWORD:*`，那么 `int` 必须走精确表项，不能被通配项“吞掉”。

### 7.4 表项示例

下表只截取几个有代表性的表项（`—` 表示没有表项，遇到就会报错）：

| N \ 输入 t | `(` | `)` | `;` | `,` | `+` | `id` |
|---|---|---|---|---|---|---|
| `AfterTypeId` | `( ParenRest` | — | `DeclRest StmtList` | `DeclRest StmtList` | — | — |
| `ExprTail` | — | ε | ε | ε | `+ Term ExprTail` | — |
| `TermTail` | — | ε | ε | ε | ε | — |
| `FactorRest` | `( Args )` | ε | ε | ε | ε | — |
| `IdStmt` | `( Args ) TermTail ExprTail ;` | — | `;` | — | `+ Term ExprTail ;` | — |

例如 `AfterTypeId` 行：

- 看到 `(`：说明前面是 `类型 标识符 (`，按函数定义处理；
- 看到 `,` 或 `;`：说明是变量声明，进入 `DeclRest StmtList`。

`ExprTail` 行：

- 看到 `+`：展开为 `+ Term ExprTail`；
- 看到 `)`、`;`、`,`：说明表达式已经结束，展开为 ε。

### 7.5 FIRST 集与 FOLLOW 集

每个非终结符的候选式首符集（FIRST）如下（简写）：

| 非终结符 | FIRST 集 |
|---|---|
| `Program` | `type`、`if`、`return`、`{`、`;`、`(`、`id`、`const`、`ε` |
| `AfterTypeId` | `(`、`,`、`;` |
| `ParenRest` | `)`、`type` |
| `ParamList` | `type` |
| `ParamRest` | `,`、`ε` |
| `StmtList` | `type`、`if`、`return`、`{`、`;`、`(`、`id`、`const`、`ε` |
| `Stmt` | `type`、`if`、`return`、`{`、`;`、`(`、`id`、`const` |
| `NonTypeStmt` | `if`、`return`、`{`、`;`、`(`、`id`、`const` |
| `DeclStmt` | `type` |
| `DeclRest` | `,`、`;` |
| `IdStmt` | `=`、`;`、`+`、`-`、`*`、`/`、`(` |
| `IfStmt` | `if` |
| `ElsePart` | `else`、`ε` |
| `ReturnStmt` | `return` |
| `ReturnTail` | `;`、`id`、`const`、`str`、`charconst`、`(`、`+`、`-` |
| `Cond` | `id`、`const`、`str`、`charconst`、`(`、`+`、`-` |
| `OptRel` | `<`、`>`、`<=`、`>=`、`==`、`!=`、`ε` |
| `LogicLoop` | `&&`、`||`、`ε` |
| `RelOp` | `<`、`>`、`<=`、`>=`、`==`、`!=` |
| `Expr` | `id`、`const`、`str`、`charconst`、`(`、`+`、`-` |
| `ExprTail` | `+`、`-`、`ε` |
| `Term` | `id`、`const`、`str`、`charconst`、`(`、`+`、`-` |
| `TermTail` | `*`、`/`、`ε` |
| `Factor` | `id`、`const`、`str`、`charconst`、`(`、`+`、`-` |
| `FactorRest` | `(`、`ε` |
| `Args` | FIRST(`Expr`) ∪ `{ε}` |
| `ArgsList` | `,`、`ε` |

对于可能推导出 ε 的非终结符，ε 分支应在 **FOLLOW 集** 上触发。本程序实际填写的 ε 触发符号如下：

| 非终结符 | ε 分支的 lookahead（FOLLOW） |
|---|---|
| `Program` | `EOF` |
| `StmtList` | `}`、`EOF` |
| `ElsePart` | `type`、`if`、`return`、`{`、`;`、`(`、`id`、`const`、`}`、`EOF` |
| `ExprTail` | 关系运算符、`&&`、`||`、`)`、`;`、`,`、`}`、`EOF` |
| `TermTail` | `+`、`-`、关系运算符、`&&`、`||`、`)`、`;`、`,`、`}`、`EOF` |
| `FactorRest` | `+`、`-`、`*`、`/`、关系运算符、`&&`、`||`、`)`、`;`、`,`、`}`、`EOF` |
| `OptRel` | `&&`、`||`、`)` |
| `LogicLoop` | `)` |
| `ParamRest` | `)` |
| `Args` | `)` |
| `ArgsList` | `)` |

> 注意：严格按定义计算，`FOLLOW(ElsePart)` 还会包含 `else`。本程序有意不填这一项，把 `else` 留给 `ElsePart → else Stmt`，从而按“就近匹配”消解悬挂 else 冲突。这是唯一一处需要人为消歧的地方。

除悬挂 else 外，其余非终结符各候选式的 FIRST 集两两不交；ε 候选式在 FOLLOW 集上触发。代码中 `ExprTail`、`TermTail`、`FactorRest` 的 ε 表项还额外包含 `}` 和 `EOF`，这是为了在语句/文件结束时尽早触发 ε 的宽松项；它们不与任何非 ε 候选式冲突，也不会造成错误接受，只会把报错位置推迟到后续符号。因此分析表仍然是确定性的，不需要回溯或多符号前瞻。

---

## 8. 运行原理：分析主循环

核心函数是 `Parser::parse()`。

### 8.1 初始化

```cpp
std::stack<std::string> stk;
stk.push("$");        // 栈底：结束标记
stk.push("Program");  // 栈顶：文法起始符号
```

栈中保存的是符号字符串，可能是：

- 非终结符，如 `"Expr"`、`"Stmt"`；
- 终结符编码，如 `"KEYWORD:int"`、`"IDENTIFIER:*"`；
- 栈底结束标记 `"$"`。

输入指针 `idx_` 初始指向 token 数组的第一个 token，`peek()` 返回当前 lookahead。

### 8.2 三种栈顶情况

每一轮循环只检查栈顶符号：

#### 情况 1：栈顶是 `$`

`$` 是栈底哨兵，不是文法符号。看到 `$` 时：

- 如果当前输入是 `EOF`：说明输入已经全部匹配完，**接受**；
- 如果当前输入不是 `EOF`：说明起始符号已经推导完，但还有多余 token，报“文件末尾仍有未分析的符号”。

这就是“双重结束检查”：输入侧看 `EOF`，栈侧看 `$`。

#### 情况 2：栈顶是非终结符

调用 `select(top, peek())` 查表：

- 找到产生式：把栈顶弹出，再把产生式右部**逆序压栈**；
- 找不到：报“在非终结符 X 处无法匹配输入 …”。

如果产生式右部为空（ε 产生式），就只弹出栈顶，不压任何东西。这一步对应“用空串替换非终结符”。

#### 情况 3：栈顶是终结符

调用 `matchTerm(top, peek())` 检查当前 token 是否匹配：

- 匹配：弹出栈顶，`idx_` 后移一个 token；
- 不匹配：报“期望 X，但遇到 …”。

### 8.3 为什么右部要逆序压栈

栈是后进先出（LIFO）。产生式 `A → X Y Z` 要求后续先匹配 `X`、再 `Y`、最后 `Z`，因此 `X` 必须在栈顶。如果按 `X Y Z` 顺序压栈，栈顶会变成 `Z`，匹配顺序就反了。

正确做法：

```cpp
for (auto it = p->rhs.rbegin(); it != p->rhs.rend(); ++it)
    stk.push(*it);
```

先压 `Z`，再压 `Y`，最后压 `X`，于是 `X` 位于栈顶。

### 8.4 伪代码

```text
parse():
    stack.push("$")
    stack.push("Program")

    while stack is not empty:
        X = stack.top()

        if X == "$":
            if lookahead.type == EOF:
                return ACCEPT
            else:
                error("文件末尾仍有未分析的符号 " + lookahead)

        else if isNonTerminal(X):
            p = M[X][encode(lookahead)]
            if p == null:
                error("在非终结符 X 处无法匹配输入 " + lookahead)

            stack.pop()
            for sym in reverse(p.rhs):
                stack.push(sym)

        else:  // X 是终结符
            if matchTerm(X, lookahead):
                stack.pop()
                advance()
            else:
                error("期望 X，但遇到 " + lookahead)
```

其中 `encode(lookahead)` 的规则就是：

```text
EOF token          → "EOF"
其他 token         → "TYPE:VALUE"（精确查找）
                   → "TYPE:*"（精确查找失败后的通配查找）
```

### 8.5 每一步都做了什么记录

`logAction()` 会把每一步动作写入 `trace_`，并加上序号：

```cpp
void logAction(const std::string& s) {
    std::ostringstream oss;
    oss << "[" << (++step_) << "] " << s;
    trace_.push_back(oss.str());
}
```

三类动作：

- `展开 N → ...`：非终结符展开；
- `匹配 X`：终结符匹配成功；
- `✓ 输入耗尽，接受`：分析成功。

`parse()` 成功返回 `true`；失败时捕获内部抛出的 `SyntaxError`，把错误信息和位置保存到 `errMsg_`、`errPos_`，返回 `false`。因此 `main()` 不需要处理异常，只需要看返回值。

### 8.6 复杂度

设 token 总数为 `n`：

- 每个 token 只被消费一次；
- 每次展开把一个非终结符替换成常数长度的右部，总压栈次数与 `n` 同阶；
- 分析表查找使用 `std::map`，单次 `O(log T)`，`T` 是表项数（约 200 余条，对给定文法是常数）；
- 因此总时间 **O(n)**（严格说是 `O(n log T)`，`T` 固定时即 `O(n)`）；
- 空间：栈最坏 `O(n)`，轨迹 `O(n)`，token 数组 `O(n)`，分析表 `O(T)`。

---

## 9. 推演示例

### 9.1 最小例子：`int x;`

输入 token：

```text
<KEYWORD, int> <IDENTIFIER, x> <DELIMITER, ;> <EOF, EOF>
```

栈的演化如下（栈顶在左，栈底为 `$`；表中“栈”是**执行动作前**的状态）：

| 步骤 | 栈（栈顶 → 栈底） | 当前 lookahead | 动作 |
|---|---|---|---|
| 0 | `Program $` | `int` | 初始状态 |
| 1 | `TypeSpec id AfterTypeId $` | `int` | 展开 `Program → TypeSpec id AfterTypeId` |
| 2 | `int id AfterTypeId $` | `int` | 展开 `TypeSpec → int` |
| 3 | `id AfterTypeId $` | `x` | 匹配 `int`，前进 |
| 4 | `AfterTypeId $` | `;` | 匹配 `id`，前进 |
| 5 | `DeclRest StmtList $` | `;` | 展开 `AfterTypeId → DeclRest StmtList` |
| 6 | `; StmtList $` | `;` | 展开 `DeclRest → ;` |
| 7 | `StmtList $` | `EOF` | 匹配 `;`，前进 |
| 8 | `$` | `EOF` | 展开 `StmtList → ε` |
| 9 | `$` | `EOF` | 栈顶是 `$` 且输入是 `EOF`，接受 |

这个例子覆盖了：

- 非终结符展开；
- 终结符匹配与前进；
- ε 产生式（只弹栈，不压栈）；
- 双重结束检查（`$` + `EOF`）。

### 9.2 表达式：`1 + 2 * 3`

对应的最左推导可以直观看出优先级：

```text
Expr
⇒ Term ExprTail
⇒ Factor TermTail ExprTail
⇒ 1 TermTail ExprTail
⇒ 1 ExprTail
⇒ 1 + Term ExprTail
⇒ 1 + Factor TermTail ExprTail
⇒ 1 + 2 TermTail ExprTail
⇒ 1 + 2 * Factor TermTail ExprTail
⇒ 1 + 2 * 3 TermTail ExprTail
⇒ 1 + 2 * 3 ExprTail
⇒ 1 + 2 * 3
```

`2 * 3` 在 `Term` 内部完成，再回到 `ExprTail` 做加法，因此等价于 `1 + (2 * 3)`。

### 9.3 样例程序的实际轨迹

`tokens-实验二输出.txt` 对应的程序是 `void main()`，包含三个变量、嵌套 if/else 和 `printf` 调用。程序共输出 279 步，开头如下：

```text
[1] 展开 Program → TypeSpec IDENTIFIER:* AfterTypeId
[2] 展开 TypeSpec → KEYWORD:void
[3] 匹配 KEYWORD:void
[4] 匹配 IDENTIFIER:*
[5] 展开 AfterTypeId → DELIMITER:( ParenRest
[6] 匹配 DELIMITER:(
[7] 展开 ParenRest → DELIMITER:) Block
[8] 匹配 DELIMITER:)
[9] 展开 Block → DELIMITER:{ StmtList DELIMITER:}
[10] 匹配 DELIMITER:{
[11] 展开 StmtList → Stmt StmtList
[12] 展开 Stmt → DeclStmt
...
```

结尾：

```text
[278] 匹配 DELIMITER:}
[279] ✓ 输入耗尽，接受
```

从轨迹可以清楚看到：每遇到一个非终结符就展开一次，每匹配一个 token 就前进一步，直到栈底 `$` 与输入 `EOF` 同时到达。

---

## 10. 错误处理

### 10.1 三类语法错误

| 场景 | 触发位置 | 错误信息 |
|---|---|---|
| 查表失败 | 栈顶是非终结符，但 `M[N][lookahead]` 为空 | `在非终结符 X 处无法匹配输入 <TYPE, VALUE>` |
| 终结符不匹配 | 栈顶是终结符，但当前 token 不等于它 | `期望 X，但遇到 <TYPE, VALUE>` |
| 输入有剩余 | 栈顶已经是 `$`，但 lookahead 不是 `EOF` | `文件末尾仍有未分析的符号 <TYPE, VALUE>` |

错误位置 `errPos_` 记录的是**当前 lookahead 在 token 数组中的下标**。输出时统一转换成从 1 开始的编号。

### 10.2 示例：缺少分号

输入（`int x` 后面直接结束）：

```text
<KEYWORD, int>
<IDENTIFIER, x>
<EOF, EOF>
```

输出：

```text
===== 分析结果 =====
✗ 语法错误！
错误位置：第 3 个 token（即 <EOF, EOF>）
错误信息：在非终结符 AfterTypeId 处无法匹配输入 <EOF, EOF>
返回结果值：1
```

解释：`int x` 之后，`AfterTypeId` 只接受 `(`、`,`、`;`。遇到 `EOF` 时表中没有对应表项，于是报错并定位到第 3 个 token。

### 10.3 示例：多个顶层函数

输入包含 `void f(){} void g(){}` 时，第一个函数体解析完后，栈顶已经是 `$`，但输入还剩 `void`：

```text
错误位置：第 7 个 token（即 <KEYWORD, void>）
错误信息：文件末尾仍有未分析的符号 <KEYWORD, void>
```

这是本实验文法的范围限制（只支持一个顶层函数定义），不是分析算法的问题，详见“已知限制”。

### 10.4 退出码

| 情况 | 退出码 |
|---|---|
| 语法正确 | `0` |
| 语法错误 | `1` |
| token 文件打不开 / 为空 / 输出文件无法创建 | `1` |

---

## 11. 输出文件说明

分析结果文件分为三段。

### 11.1 输入 token 流

```text
===== 输入 token 流 =====
第1个 token：<KEYWORD, void>
第2个 token：<IDENTIFIER, main>
第3个 token：<DELIMITER, (>
...
```

编号从 1 开始，方便和错误位置对应。

### 11.2 分析过程

```text
===== 分析过程（LL(1) 表驱动）=====
[1] 展开 Program → TypeSpec IDENTIFIER:* AfterTypeId
[2] 展开 TypeSpec → KEYWORD:void
[3] 匹配 KEYWORD:void
...
```

每一步都带序号，记录“展开”或“匹配”动作。

### 11.3 分析结果

合法时：

```text
===== 分析结果 =====
✓ 语法正确！返回结果值：0
```

非法时：

```text
===== 分析结果 =====
✗ 语法错误！
错误位置：第 N 个 token（即 <TYPE, VALUE>）
错误信息：...
返回结果值：1
```

控制台只打印简要结论，详细信息写入输出文件。

---

## 12. 实现细节与设计取舍

### 12.1 为什么用显式栈，而不是递归下降？

递归下降把“选择哪条产生式”写死在代码的 if/else 里，分析逻辑和文法结构耦合较强；表驱动把选择放在数据（分析表）里，主循环只有一份，新增/修改文法主要改表，不改算法。显式栈还便于打印栈状态、做可视化，也避免了深层嵌套时的函数调用栈压力。

### 12.2 为什么把分析表写死在代码里？

- 文法固定且规模小（29 个非终结符、200 余条表项）；
- 手写表项可以直接对照文法规则检查，便于实验答辩；
- 启动时不需要计算 FIRST/FOLLOW，逻辑更简单。

代价是文法改动时要同步维护相关的 ε 表项。若把文法规则写成数据结构，再自动计算 FIRST/FOLLOW、检测冲突并生成表，可维护性会更好，这也是后续可扩展方向之一。

### 12.3 为什么用 `std::map`？

分析表规模很小，`std::map` 的有序性和稳定的查找行为足够；代码也更直观。对性能要求更高的场景可以换成 `std::unordered_map`，把单次查找从 `O(log T)` 降到平均 `O(1)`。

### 12.4 `Stmt` 与 `NonTypeStmt` 的复用

两者只差“是否包含声明分支”。源码里通过一个 lambda 复用同一份填表逻辑：

```cpp
auto fillStmt = [&](const std::string& N, bool includeDecl) {
    // 填入 if / return / block / ; / id / const / ( 等公共分支
    if (includeDecl)
        for (const auto& ty : TY)
            set(N, ty, {"DeclStmt"});
};

fillStmt("Stmt",        true);
fillStmt("NonTypeStmt", false);
```

这样既保证两份规则同步，又避免复制粘贴导致的不一致。

### 12.5 为什么只保存 token 的种别和值？

语法分析阶段不关心变量名具体是 `num1` 还是 `max`，只关心它是不是一个 `IDENTIFIER`；也不关心常量具体是 `5` 还是 `6`，只关心它是 `CONSTANT`。因此终结符编码用 `TYPE:*` 把这一类 token 收敛成一列。只有关键字和运算符需要精确到值，因为 `int` 和 `if` 虽然都是 `KEYWORD`，但走的是完全不同的文法分支。

### 12.6 没有构建语法树

本实验的目标是“判断输入串是否合法”，因此只做识别，不保存推导树。若需要后续语义分析，可以在每次展开非终结符时创建内部节点，把右部符号作为子节点；终结符匹配时创建叶子节点。实现上可以用一个与符号栈平行的“节点栈”。

---

## 13. 已知限制

本程序实现的是**教学用 C 语言子集**，不是完整的 C 语言编译器前端。以下输入会报语法错误，属于文法范围限制：

### 13.1 文法覆盖范围

- 类型只支持 `int`、`float`、`char`、`void`、`double`、`long`、`short`；
- 语句只支持声明、赋值、`if/else`、`return`、代码块、函数调用语句和部分表达式语句；
- 表达式支持 `+ - * /`、一元 `+ -`、括号、标识符、常量、字符串、字符常量、函数调用；
- 条件中支持关系运算 `< > <= >= == !=` 和逻辑运算 `&& ||`；
- 不支持 `while`、`for`、`do-while`、`switch`、`break`、`continue`（实验二词法分析器能识别这些关键字，但本实验的分析表没有对应产生式）；
- 不支持数组、下标、指针、结构体、联合体、枚举、`typedef`；
- 不支持 `%`、`!`（逻辑非）、`++`、`--`、复合赋值（如 `+=`）、三目运算符 `?:`、逗号运算符、强制类型转换、`sizeof`；
- 声明不支持初始化，例如 `int x = 1;` 会报错；请写成 `int x; x = 1;`；
- 函数参数只支持 `类型 标识符` 形式，不支持指针、数组参数，也不支持 `void` 作为空参数表标记；
- 不支持函数原型声明，函数定义必须带函数体；
- 只支持**一个顶层函数定义**；第二个顶层函数会触发“文件末尾仍有未分析的符号”；
- 顶层允许出现非声明的语句（`Program → NonTypeStmt StmtList`），这是为了方便实验测试，不完全等价于标准 C 的顶层结构；
- 以字符串、字符常量或一元运算符开头的语句不受支持（例如 `"hi";`、`-x;`），因为 `Stmt` 的首符集没有包含它们；它们可以作为表达式的一部分出现在赋值右侧或函数实参中。

### 13.2 分析器行为

- **单符号前瞻**：只使用当前 lookahead，不做多 token 试探；如果文法出现 LL(1) 冲突，必须通过改造文法解决；
- **无错误恢复**：遇到第一个语法错误就停止，不进行恐慌模式同步，也不会继续报告后续错误；
- **错误信息为单点信息**：只报告当前 token 和当前非终结符，不列出完整的期望符号集合；
- `Program → ε` 在文法中存在，但 `main()` 对“只有 EOF”的空 token 流会直接报错，因此该分支实际不会被执行；
- 词法错误不会被语法分析器修复：`loadTokens()` 遇到 `错误信息` 行就停止，语法分析只针对词法正确的部分。

---

## 14. 答辩速查

### Q1：为什么选择 LL(1)？

LL(1) 只需要一个 token 的前瞻，不需要回溯；分析过程是线性的，易于用显式栈和一张表实现；每一步都可以打印出来，便于验证和讲解。本实验的文法可以通过消除左递归、提取左因子改造成 LL(1) 形式。

### Q2：怎么保证只向前看一个 token？

`Parser::parse()` 中只调用 `peek()`（默认偏移 0）。虽然 `peek(int off)` 支持偏移，但分析逻辑从未传入非零偏移；`select()` 只根据当前 lookahead 的种别和值查表。因此是严格的单符号前瞻。

### Q3：预测分析表是怎么得到的？

对产生式 `A → α`：

- 对 `a ∈ FIRST(α)`，令 `M[A, a] = A → α`；
- 若 `α` 能推导出 ε，则对 `b ∈ FOLLOW(A)`，令 `M[A, b] = A → ε`。

本程序把计算结果直接写成 `initGrammar()` 中的 `set()` 调用。

### Q4：ε 产生式在运行时怎么执行？

ε 产生式的右部为空。执行时只把栈顶非终结符弹出，不压入任何符号。触发它的 lookahead 属于该非终结符的 FOLLOW 集，因此不会被消费，而是留给后面的符号匹配。

### Q5：悬挂 else 怎么处理？

采用“else 与最近的 if 匹配”的约定。表中 `ElsePart` 在 lookahead 为 `else` 时选择 `else Stmt`，并故意不填 `else` 的 ε 表项。这样内层 `if` 会先消费 `else`，符合 C 语言语义。

### Q6：为什么产生式右部要逆序压栈？

栈是 LIFO。为了让最左符号先匹配，必须让它位于栈顶。逆序压栈后，右部第一个符号最后入栈，因而最先出栈。

### Q7：为什么用 `TYPE:*` 通配？

标识符、常量、字符串的取值是无限的，不可能为每个值建一列。用 `TYPE:*` 把同一词法种别收敛成一个终结符类，分析表才是有限的。关键字和运算符则需要精确到值，因为不同值对应不同的文法分支。

### Q8：如果分析表出现冲突怎么办？

那说明文法不是 LL(1) 的。常见处理：

1. 消除左递归；
2. 提取左因子；
3. 重新设计文法（把公共前缀/后缀提取成新的非终结符）；
4. 若仍无法消解，改用 LL(k)、LR(1)/LALR(1) 等更强的分析方法。

本实验除悬挂 else 就近消歧外，各候选式的 FIRST 集互不相交，ε 分支只在 FOLLOW 集上触发，因此表是确定性的。

### Q9：错误位置是怎么确定的？

在查表失败或终结符不匹配的那一刻，`peek()` 指向的 token 就是出错位置。`errPos_` 保存其 0 基下标，输出时加 1 显示。

### Q10：时间复杂度是多少？

对固定文法，每个 token 只消费一次，每个非终结符展开只压入常数个符号，因此总时间是 `O(n)`；分析表查找是 `O(log T)`，`T` 对固定文法是常数。空间上，栈、轨迹和 token 数组都是 `O(n)`，分析表是 `O(T)`。

### Q11：怎么扩展成语法树 / AST？

为每个栈项关联一个语法树节点。展开 `A → X Y Z` 时创建父节点 `A`，把 `X/Y/Z` 的节点作为子节点；匹配终结符时创建叶子节点。也可以维护一个与符号栈平行的节点栈，展开和匹配时同步操作。

### Q12：这个程序能直接分析完整的 C 程序吗？

不能。它面向实验给定的 C 语言子集，主要覆盖函数定义、变量声明、赋值、if/else、return、函数调用和基本表达式。完整 C 语言还需要循环、数组、指针、结构体、类型系统等大量文法规则，并且需要更复杂的消歧和语义处理。

---

## 15. 可扩展方向

1. **自动生成分析表**：把文法规则写成数据结构，程序自动计算 FIRST/FOLLOW、检测冲突并填表，减少手工维护 ε 表项的错误；
2. **构建语法树 / AST**：在展开和匹配时同步构建节点，为后续语义分析、类型检查、代码生成做准备；
3. **错误恢复**：引入同步符号集合（FOLLOW 集），在出错后跳过部分 token 继续分析，一次报告多个错误；
4. **更完整的 C 文法**：加入 `while`、`for`、数组、指针、结构体、初始化声明、函数原型等；
5. **可视化**：把栈、剩余输入和分析表做成网页动画，逐步展示移进/展开过程；
6. **单元测试**：为合法程序、缺少分号、括号不匹配、悬挂 else、多函数等场景建立自动化测试用例。

---

## 16. 小结

这份实现的核心可以概括为一句话：

> **用一张“非终结符 × 终结符”的 LL(1) 预测分析表，驱动一个显式栈；每步只看一个 token，遇到非终结符就查表展开（右部逆序压栈），遇到终结符就匹配并前进，直到栈底 `$` 与输入 `EOF` 同时到达。**

文法层面通过消除左递归、提取左因子和分层表达式，把 C 语言子集改造成 LL(1) 形式；工程层面用 `std::map` 保存稀疏分析表，用 `trace_` 记录每一步动作，用 `SyntaxError` 统一处理错误定位。整个程序结构清晰、可追踪，适合作为编译原理语法分析实验的实现与讲解材料。
