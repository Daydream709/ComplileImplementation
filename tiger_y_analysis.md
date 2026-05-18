# Tiger 编译器 Parser 语法文件逐行解析

> 文件路径：`src/tiger/parse/tiger.y`
> 用途：使用 Bison（属于 Yacc 家族）定义 Tiger 语言的**上下文无关文法**，同时嵌入语义动作来构建**抽象语法树（AST）**。

---

## 目录

1. [第一部分：声明区（Prologue / Declarations）](#第一部分声明区)
2. [第二部分：文法规则区（Grammar Rules）](#第二部分文法规则区)
3. [总结与问辩要点](#总结与问辩要点)

---

## 第一部分：声明区

### 1.1 Bison 配置指令（第 1–7 行）

```yacc
%filenames parser
%scanner tiger/lex/scanner.h
%baseclass-preinclude tiger/absyn/absyn.h
```

| 行号 | 指令 | 含义 |
|------|------|------|
| 1 | `%filenames parser` | 指定生成的解析器类名为 `parser`， Bison 会生成 `parser.h` 和 `parser.cc`。 |
| 2 | `%scanner tiger/lex/scanner.h` | 指定词法分析器（Scanner/Lexer）的头文件路径。解析器通过 `scanner_` 成员来调用词法分析器获取 Token。 |
| 3 | `%baseclass-preinclude tiger/absyn/absyn.h` | 在生成的 parser 基类之前预包含 `absyn.h`，确保语义动作中可以直接使用所有 AST 节点类型（如 `absyn::VarExp`）。 |

> **问辩提示**：这三个指令连接了编译器的三大模块 —— **词法分析**（Scanner）、**语法分析**（Parser）和**抽象语法树**（Absyn）。

---

### 1.2 联合体声明 `%union`（第 9–27 行）

```yacc
%union {
  int ival;
  std::string* sval;
  sym::Symbol *sym;
  absyn::Exp *exp;
  absyn::ExpList *explist;
  absyn::Var *var;
  absyn::DecList *declist;
  absyn::Dec *dec;
  absyn::EFieldList *efieldlist;
  absyn::EField *efield;
  absyn::NameAndTyList *tydeclist;
  absyn::NameAndTy *tydec;
  absyn::FieldList *fieldlist;
  absyn::Field *field;
  absyn::FunDecList *fundeclist;
  absyn::FunDec *fundec;
  absyn::Ty *ty;
}
```

`%union` 定义了解析器栈上**每个 Token 或非终结符可以携带的语义值类型**。Bison 会用这个 union 来声明 `YYSTYPE`（即 `$$`、`$1`、`$2` 等语义值变量的类型）。

| 字段名 | C++ 类型 | 用途 |
|--------|----------|------|
| `ival` | `int` | 整数字面量的值 |
| `sval` | `std::string*` | 字符串字面量的值 |
| `sym` | `sym::Symbol*` | 标识符（变量名、函数名、类型名），使用符号表中的 Symbol 类 |
| `exp` | `absyn::Exp*` | 表达式 AST 节点（所有表达式的基类） |
| `explist` | `absyn::ExpList*` | 表达式列表 |
| `var` | `absyn::Var*` | 左值（变量）AST 节点 |
| `declist` | `absyn::DecList*` | 声明列表 |
| `dec` | `absyn::Dec*` | 单个声明 AST 节点 |
| `efieldlist` | `absyn::EFieldList*` | 记录表达式中的字段列表 |
| `efield` | `absyn::EField*` | 记录表达式中的单个字段（`id = exp`） |
| `tydeclist` | `absyn::NameAndTyList*` | 类型声明列表（支持递归/互递归类型声明） |
| `tydec` | `absyn::NameAndTy*` | 单个类型声明（类型名 → 类型定义的映射） |
| `fieldlist` | `absyn::FieldList*` | 类型定义中记录字段的列表 |
| `field` | `absyn::Field*` | 类型定义中的单个字段（`id : type`） |
| `fundeclist` | `absyn::FunDecList*` | 函数声明列表（支持递归/互递归函数声明） |
| `fundec` | `absyn::FunDec*` | 单个函数声明 |
| `ty` | `absyn::Ty*` | 类型定义 AST 节点 |

> **问辩提示**：`%union` 是语法分析与语义分析的桥梁。Token 携带词法层面的值（如整数、字符串、标识符），而语义动作则利用这些值构建 AST 节点。所有 AST 指针类型都使用了对应继承体系的**基类指针**，实现多态。

---

### 1.3 终结符（Token）声明（第 29–38 行）

```yacc
%token <sym> ID
%token <sval> STRING
%token <ival> INT
```

这三行声明了**带语义值**的终结符：

| Token | 语义值类型 | 说明 | 示例 |
|-------|-----------|------|------|
| `ID` | `sym::Symbol*` | 标识符 | `x`, `foo`, `myFunc` |
| `STRING` | `std::string*` | 字符串字面量 | `"hello"` |
| `INT` | `int` | 整数字面量 | `42`, `0`, `100` |

尖括号中的类型名对应 `%union` 中的字段名，指示该 Token 在解析栈上的语义值取 union 的哪个成员。

```yacc
%token
  COMMA COLON SEMICOLON LPAREN RPAREN LBRACK RBRACK
  LBRACE RBRACE DOT
  ARRAY IF WHILE FOR TO DO LET IN END OF
  BREAK NIL
  FUNCTION VAR TYPE
```

这些是**不带语义值**的关键字和标点符号终结符。每个 Token 对应 Tiger 语言中的一个保留字或标点：

| Token | 字面量 | 用途 |
|-------|--------|------|
| `COMMA` | `,` | 分隔参数/字段 |
| `COLON` | `:` | 类型标注 |
| `SEMICOLON` | `;` | 分隔序列表达式 |
| `LPAREN` / `RPAREN` | `(` `)` | 分组/函数调用 |
| `LBRACK` / `RBRACK` | `[` `]` | 数组下标 |
| `LBRACE` / `RBRACE` | `{` `}` | 记录创建 |
| `DOT` | `.` | 字段访问 |
| `ARRAY` | `array` | 数组类型声明 |
| `IF` / `THEN` / `ELSE` | `if` / `then` / `else` | 条件表达式 |
| `WHILE` / `DO` | `while` / `do` | while 循环 |
| `FOR` / `TO` | `for` / `to` | for 循环 |
| `LET` / `IN` / `END` | `let` / `in` / `end` | let 表达式 |
| `OF` | `of` | 数组创建 `type[size] of init` |
| `BREAK` | `break` | 跳出循环 |
| `NIL` | `nil` | 空值 |
| `FUNCTION` | `function` | 函数声明 |
| `VAR` | `var` | 变量声明 |
| `TYPE` | `type` | 类型声明 |

> **问辩提示**：终结符是词法分析器（Scanner）输出的 Token，是文法中最基本的不可再分的符号。注意 `ID`、`STRING`、`INT` 是唯一携带语义值的终结符。

---

### 1.4 运算符优先级与结合性（第 40–50 行）

```yacc
%right ASSIGN
%left OR
%left AND
%nonassoc EQ NEQ LT LE GT GE
%nonassoc THEN
%nonassoc ELSE
%left PLUS MINUS
%left TIMES DIVIDE
%nonassoc UMINUS
```

Bison 使用这些声明来**解决移进-归约冲突**（特别是表达式文法中的二义性）。**排在后面的行优先级更高**；同一行中 `%left` 表示左结合，`%right` 表示右结合，`%nonassoc` 表示不可结合。

| 行号 | 结合性 | 运算符 | 优先级（低→高） | 说明 |
|------|--------|--------|-----------------|------|
| 42 | 右结合 | `:=` (ASSIGN) | 1（最低） | 赋值是右结合：`a := b := c` 等价于 `a := (b := c)` |
| 43 | 左结合 | `or` | 2 | 逻辑或 |
| 44 | 左结合 | `&` (AND) | 3 | 逻辑与 |
| 45 | 不可结合 | `=` `<>` `<` `<=` `>` `>=` | 4 | 比较运算符，不可连写 |
| 46 | 不可结合 | `then` | 5 | 用于解决悬空 else |
| 47 | 不可结合 | `else` | 6 | 优先匹配最近的 then |
| 48 | 左结合 | `+` `-` | 7 | 加减 |
| 49 | 左结合 | `*` `/` | 8 | 乘除 |
| 50 | 不可结合 | 一元 `-` (UMINUS) | 9（最高） | 一元取负 |

**悬空 else 的解决**：`THEN` 和 `ELSE` 被声明为 `%nonassoc`，且 `ELSE` 的优先级高于 `THEN`。当解析器看到 `IF ... THEN ... IF ... THEN ... ELSE` 时，`ELSE` 会与最近的 `THEN` 匹配（移进 `ELSE` 优先于按 `ifexp → IF exp THEN exp` 归约），这正是我们期望的行为。

**一元减号**：通过 `%prec UMINUS` 指示（见第 112 行），使得 `-3 + 4` 解析为 `(-3) + 4` 而不是 `-(3 + 4)`。

> **问辩提示**：优先级声明让 Bison 能用**简单递归的文法规则**（如 `exp: exp PLUS exp`）而不是**嵌套的非终结符层次**（如 `expr → term → factor`）来解决二义性，使文法更简洁。

---

### 1.5 非终结符类型声明（第 53–66 行）

```yacc
%type <exp> exp expseq ifexp whileexp callexp recordexp
%type <explist> actuals nonemptyactuals sequencing sequencing_exps
%type <var> lvalue
%type <declist> decs decs_nonempty
%type <dec> decs_nonempty_s vardec
%type <efieldlist> rec rec_nonempty
%type <efield> rec_one
%type <tydeclist> tydec
%type <tydec> tydec_one
%type <fieldlist> tyfields tyfields_nonempty
%type <field> tyfield
%type <ty> ty
%type <fundeclist> fundec
%type <fundec> fundec_one
```

`%type` 声明每个**非终结符**归约后产生的语义值类型。例如 `exp` 归约后产生一个 `absyn::Exp*`，`lvalue` 产生一个 `absyn::Var*`。这些类型同样对应 `%union` 中的字段。

按功能分组：

| 类别 | 非终结符 | 语义值类型 | 含义 |
|------|----------|-----------|------|
| 表达式 | `exp` | `Exp*` | 表达式（核心非终结符） |
| | `ifexp` | `Exp*` | if 条件表达式 |
| | `whileexp` | `Exp*` | while 循环表达式 |
| | `callexp` | `Exp*` | 函数调用表达式 |
| | `recordexp` | `Exp*` | 记录创建表达式 |
| | `expseq` | `Exp*` | 表达式序列（带分号） |
| 表达式列表 | `actuals` | `ExpList*` | 函数实参列表 |
| | `nonemptyactuals` | `ExpList*` | 非空实参列表 |
| | `sequencing` | `ExpList*` | let-in-end 中的序列 |
| | `sequencing_exps` | `ExpList*` | 非空序列（内部辅助） |
| 左值 | `lvalue` | `Var*` | 变量/左值 |
| 声明 | `decs` | `DecList*` | 声明列表 |
| | `decs_nonempty` | `DecList*` | 非空声明列表 |
| | `decs_nonempty_s` | `Dec*` | 单个声明 |
| | `vardec` | `Dec*` | 变量声明 |
| 类型声明 | `tydec` | `NameAndTyList*` | 类型声明组 |
| | `tydec_one` | `NameAndTy*` | 单个类型声明 |
| 类型字段 | `tyfields` | `FieldList*` | 函数参数/记录字段列表 |
| | `tyfields_nonempty` | `FieldList*` | 非空字段列表 |
| | `tyfield` | `Field*` | 单个字段 |
| 类型定义 | `ty` | `Ty*` | 类型定义 |
| 函数声明 | `fundec` | `FunDecList*` | 函数声明组 |
| | `fundec_one` | `FunDec*` | 单个函数声明 |
| 记录字段 | `rec` | `EFieldList*` | 记录初始化字段列表 |
| | `rec_nonempty` | `EFieldList*` | 非空字段列表 |
| | `rec_one` | `EField*` | 单个字段初始化 `id = exp` |

---

### 1.6 起始符号（第 68 行）

```yacc
%start program
```

指定文法的起始符号为 `program`。整个解析过程的目标就是将输入归约为 `program`。

---

## 第二部分：文法规则区

第 70 行的 `%%` 是 Bison 的分隔符，标记声明区结束、文法规则区开始。

---

### 2.1 顶层规则：program（第 71 行）

```yacc
program:  exp  {absyn_tree_ = std::make_unique<absyn::AbsynTree>($1);};
```

- **产生式**：`program → exp`
- **语义动作**：用解析得到的表达式 `$1` 构造 `AbsynTree` 根节点，赋值给 `absyn_tree_`（解析器的成员变量）。
- **含义**：一个 Tiger 程序就是一个表达式。整个程序被包装成一个 `AbsynTree` 对象。

> **问辩提示**：Tiger 语言的一个特点 —— 程序本身就是一个表达式，而不是语句的序列。一切皆表达式。

---

### 2.2 表达式：exp（第 73–152 行）

`exp` 是文法中**最大、最核心**的非终结符，定义了 Tiger 语言所有种类的表达式。

#### 2.2.1 左值表达式（第 74–76 行）

```yacc
    lvalue {
      $$ = new absyn::VarExp(scanner_.GetTokPos(), $1);
    }
```

- **产生式**：`exp → lvalue`
- **语义动作**：将左值 `$1` 包装成 `VarExp` 节点。
- **含义**：变量引用（如 `x`、`a.b`、`a[i]`）本身也是表达式。

#### 2.2.2 空值字面量（第 77–79 行）

```yacc
  | NIL {
      $$ = new absyn::NilExp(scanner_.GetTokPos());
    }
```

- **产生式**：`exp → NIL`
- **语义动作**：创建 `NilExp` 节点。
- **含义**：`nil` 表示空值，通常用于记录类型的默认值。

#### 2.2.3 整数字面量（第 80–82 行）

```yacc
  | INT {
      $$ = new absyn::IntExp(scanner_.GetTokPos(), $1);
    }
```

- **产生式**：`exp → INT`
- **语义动作**：`$1` 是 `int` 类型的整数值，创建 `IntExp` 节点。

#### 2.2.4 字符串字面量（第 83–85 行）

```yacc
  | STRING {
      $$ = new absyn::StringExp(scanner_.GetTokPos(), $1);
    }
```

- **产生式**：`exp → STRING`
- **语义动作**：`$1` 是 `std::string*` 类型，创建 `StringExp` 节点。

#### 2.2.5 函数调用与记录创建（第 86–87 行）

```yacc
  | callexp
  | recordexp
```

- 这两条规则直接透传子规则的语义值（`$$ = $1` 是隐式的）。
- 函数调用和记录创建的定义分别见 `callexp`（第 169 行）和 `recordexp`（第 175 行）。

#### 2.2.6 数组创建（第 88–90 行）

```yacc
  | ID LBRACK exp RBRACK OF exp {
      $$ = new absyn::ArrayExp(scanner_.GetTokPos(), $1, $3, $6);
    }
```

- **产生式**：`exp → ID [ exp ] of exp`
- **语义值**：`$1` = 类型名（Symbol*），`$3` = 数组大小（Exp*），`$6` = 初始值（Exp*）
- **含义**：如 `intarray [10] of 0` —— 创建一个长度为 10 的 int 数组，所有元素初始化为 0。
- **AST 节点**：`ArrayExp(pos, typ, size, init)`

#### 2.2.7 赋值表达式（第 91–93 行）

```yacc
  | lvalue ASSIGN exp {
      $$ = new absyn::AssignExp(scanner_.GetTokPos(), $1, $3);
    }
```

- **产生式**：`exp → lvalue := exp`
- **含义**：如 `x := 5`、`a.b := 10`。
- **AST 节点**：`AssignExp(pos, var, exp)`

#### 2.2.8 条件与循环（第 94–95 行）

```yacc
  | ifexp
  | whileexp
```

透传 `ifexp` 和 `whileexp`。

#### 2.2.9 for 循环（第 96–98 行）

```yacc
  | FOR ID ASSIGN exp TO exp DO exp {
      $$ = new absyn::ForExp(scanner_.GetTokPos(), $2, $4, $6, $8);
    }
```

- **产生式**：`exp → for ID := exp to exp do exp`
- **语义值**：`$2` = 循环变量（Symbol*），`$4` = 下界（Exp*），`$6` = 上界（Exp*），`$8` = 循环体（Exp*）
- **含义**：如 `for i := 1 to 10 do (print(i))`。
- **注意**：Tiger 的 for 循环变量 `i` 的作用域限定在循环体内，且不可被赋值。

#### 2.2.10 break 表达式（第 99–101 行）

```yacc
  | BREAK {
      $$ = new absyn::BreakExp(scanner_.GetTokPos());
    }
```

- **产生式**：`exp → break`
- **含义**：跳出最近的 `while` 或 `for` 循环。语义分析阶段会检查它是否在循环体内。

#### 2.2.11 let 表达式（第 102–105 行）

```yacc
  | LET decs IN sequencing END {
      $$ = new absyn::LetExp(scanner_.GetTokPos(), $2,
                             new absyn::SeqExp(scanner_.GetTokPos(), $4));
    }
```

- **产生式**：`exp → let decs in sequencing end`
- **语义值**：`$2` = 声明列表（DecList*），`$4` = 表达式序列（ExpList*）
- **含义**：Tiger 的 let 表达式定义局部绑定。如 `let var x := 5 in x + 1 end`。
- **AST 构造**：`sequencing`（ExpList*）被包装成 `SeqExp`，作为 `LetExp` 的 body。这是为了让 `LetExp` 的 body 统一为一个 `Exp*` 类型。

#### 2.2.12 空括号 / unit 表达式（第 106–108 行）

```yacc
  | LPAREN RPAREN {
      $$ = new absyn::VoidExp(scanner_.GetTokPos());
    }
```

- **产生式**：`exp → ( )`
- **含义**：`()` 表示什么都不做，相当于 unit/void 值。类似于 C 语言中的 `void`。

#### 2.2.13 括号表达式序列（第 109–111 行）

```yacc
  | LPAREN expseq RPAREN {
      $$ = $2;
    }
```

- **产生式**：`exp → ( expseq )`
- **含义**：括号内的表达式序列，直接透传 `expseq` 的值。

#### 2.2.14 一元取负（第 112–115 行）

```yacc
  | MINUS exp %prec UMINUS {
      $$ = new absyn::OpExp(scanner_.GetTokPos(), absyn::Oper::MINUS_OP,
                            new absyn::IntExp(scanner_.GetTokPos(), 0), $2);
    }
```

- **产生式**：`exp → - exp`（优先级为 `UMINUS`）
- **`%prec UMINUS`**：指示这条规则使用 `UMINUS` 的优先级（最高），而不是 `MINUS` 的优先级。
- **语义动作**：将 `-e` 转换为 `0 - e`，即用二元减法实现一元取负。这样就不需要单独的一元运算 AST 节点。
- **示例**：`-3` → `OpExp(MINUS_OP, IntExp(0), IntExp(3))`

#### 2.2.15 二元运算（第 116–151 行）

```yacc
  | exp PLUS exp    { $$ = new absyn::OpExp(..., absyn::Oper::PLUS_OP,  $1, $3); }
  | exp MINUS exp   { $$ = new absyn::OpExp(..., absyn::Oper::MINUS_OP, $1, $3); }
  | exp TIMES exp   { $$ = new absyn::OpExp(..., absyn::Oper::TIMES_OP, $1, $3); }
  | exp DIVIDE exp  { $$ = new absyn::OpExp(..., absyn::Oper::DIVIDE_OP,$1, $3); }
  | exp EQ exp      { $$ = new absyn::OpExp(..., absyn::Oper::EQ_OP,    $1, $3); }
  | exp NEQ exp     { $$ = new absyn::OpExp(..., absyn::Oper::NEQ_OP,   $1, $3); }
  | exp LT exp      { $$ = new absyn::OpExp(..., absyn::Oper::LT_OP,    $1, $3); }
  | exp LE exp      { $$ = new absyn::OpExp(..., absyn::Oper::LE_OP,    $1, $3); }
  | exp GT exp      { $$ = new absyn::OpExp(..., absyn::Oper::GT_OP,    $1, $3); }
  | exp GE exp      { $$ = new absyn::OpExp(..., absyn::Oper::GE_OP,    $1, $3); }
  | exp AND exp     { $$ = new absyn::OpExp(..., absyn::Oper::AND_OP,   $1, $3); }
  | exp OR exp      { $$ = new absyn::OpExp(..., absyn::Oper::OR_OP,    $1, $3); }
```

所有二元运算采用统一模式：`exp OP exp → OpExp(pos, Oper::XX_OP, left, right)`。

运算符优先级和结合性由前面的 `%left`/`%right`/`%nonassoc` 声明控制，而非文法规则的嵌套层次。

> **问辩提示**：`AND` 和 `OR` 在 Tiger 语言规范中定义为**不短路求值**（与 C 语言不同）。这里将它们当作普通二元运算符处理，在语义分析阶段没有特殊处理。Tiger 语言参考手册明确规定了这一点。

---

### 2.3 if 条件表达式：ifexp（第 154–161 行）

```yacc
ifexp:
    IF exp THEN exp %prec THEN {
      $$ = new absyn::IfExp(scanner_.GetTokPos(), $2, $4, nullptr);
    }
  | IF exp THEN exp ELSE exp {
      $$ = new absyn::IfExp(scanner_.GetTokPos(), $2, $4, $6);
    }
  ;
```

| 产生式 | 含义 | AST 节点 |
|--------|------|----------|
| `IF exp THEN exp` | 无 else 分支 | `IfExp(pos, test, then, nullptr)` |
| `IF exp THEN exp ELSE exp` | 有 else 分支 | `IfExp(pos, test, then, else)` |

**关于 `%prec THEN`**（第 155 行）：

第一个产生式添加了 `%prec THEN`，这用于解决悬空 else 问题。考虑如下代码：

```
if a then if b then c else d
```

解析器需要决定 `else d` 属于哪个 `if`：
- **不加 `%prec THEN`**：该产生式的优先级由其最后一个终结符 `THEN` 决定（优先级为 5），与 `ELSE`（优先级为 6）比较时，`ELSE` 更高，所以解析器会移进 `ELSE`，将其与内层 `if` 匹配。
- 实际上这里 `%prec THEN` 是冗余的（因为最后一个终结符就是 `THEN`），但显式写出可以增强可读性，明确表示这是一个"无 else"的 if。

> **问辩提示**：悬空 else 的经典解决方案 —— `ELSE` 优先级高于 `THEN`，使得 `else` 总是被移进（而非按无 else 的规则归约），从而与最近的 `then` 匹配。

---

### 2.4 while 循环：whileexp（第 163–167 行）

```yacc
whileexp:
    WHILE exp DO exp {
      $$ = new absyn::WhileExp(scanner_.GetTokPos(), $2, $4);
    }
  ;
```

- **产生式**：`while exp do exp`
- `$2` = 循环条件，`$4` = 循环体。
- **AST**：`WhileExp(pos, test, body)`

---

### 2.5 函数调用：callexp（第 169–173 行）

```yacc
callexp:
    ID LPAREN actuals RPAREN {
      $$ = new absyn::CallExp(scanner_.GetTokPos(), $1, $3);
    }
  ;
```

- **产生式**：`ID ( actuals )`
- `$1` = 函数名（Symbol*），`$3` = 实参列表（ExpList*）。
- **AST**：`CallExp(pos, func, args)`
- **示例**：`print("hello")` → `CallExp(print, [StringExp("hello")])`

---

### 2.6 记录创建：recordexp（第 175–179 行）

```yacc
recordexp:
    ID LBRACE rec RBRACE {
      $$ = new absyn::RecordExp(scanner_.GetTokPos(), $1, $3);
    }
  ;
```

- **产生式**：`ID { rec }`
- `$1` = 记录类型名，`$3` = 字段初始化列表。
- **AST**：`RecordExp(pos, typ, fields)`
- **示例**：`point {x=1, y=2}` → `RecordExp(point, [EField(x,IntExp(1)), EField(y,IntExp(2))])`

---

### 2.7 表达式序列：expseq（第 181–188 行）

```yacc
expseq:
    exp {
      $$ = $1;
    }
  | exp SEMICOLON sequencing_exps {
      $$ = new absyn::SeqExp(scanner_.GetTokPos(), $3->Prepend($1));
    }
  ;
```

| 产生式 | 含义 |
|--------|------|
| `exp` | 单个表达式，直接透传 |
| `exp ; sequencing_exps` | 以分号分隔的表达式序列 |

**第二条规则的工作方式**：
- `$1` = 第一个表达式
- `$3` = 后续表达式列表（`ExpList*`）
- `$3->Prepend($1)` 将第一个表达式插入列表头部，然后用整个列表创建 `SeqExp`

**注意**：`expseq` 用于括号内的序列 `(exp1; exp2; exp3)`，而 `sequencing` 用于 `let ... in ... end` 中的序列。两者功能类似但使用场景不同。

---

### 2.8 函数实参列表：actuals（第 190–204 行）

```yacc
actuals:
    { $$ = new absyn::ExpList(); }
  | nonemptyactuals
  ;

nonemptyactuals:
    exp {
      $$ = new absyn::ExpList($1);
    }
  | exp COMMA nonemptyactuals {
      $$ = $3->Prepend($1);
    }
  ;
```

| 非终结符 | 产生式 | 含义 |
|----------|--------|------|
| `actuals` | 空 | 无实参 → 空列表 |
| `actuals` | `nonemptyactuals` | 透传非空列表 |
| `nonemptyactuals` | `exp` | 单个实参 |
| `nonemptyactuals` | `exp , nonemptyactuals` | 递归：在列表前插入一个实参 |

**递归构建列表的模式**：利用 `Prepend` 方法在列表头部插入元素。这是**左递归列表构建**的标准模式，但这里用**右递归文法** + `Prepend` 实现了相同的效果。解析时从左到右扫描，但通过 `Prepend` 最终保持正确的从左到右顺序。

---

### 2.9 let 内部的表达式序列：sequencing / sequencing_exps（第 206–220 行）

```yacc
sequencing:
    { $$ = new absyn::ExpList(); }
  | sequencing_exps
  ;

sequencing_exps:
    exp {
      $$ = new absyn::ExpList($1);
    }
  | exp SEMICOLON sequencing_exps {
      $$ = $3->Prepend($1);
    }
  ;
```

结构与 `actuals` / `nonemptyactuals` 完全相同，只是用于不同上下文（`let ... in sequencing end`）。

> **问辩提示**：`sequencing` 和 `actuals` 的文法结构几乎一样，但设计为不同的非终结符，因为它们在语义上表示不同的概念（语句序列 vs 函数实参），在后续阶段可能需要不同处理。

---

### 2.10 左值：lvalue（第 222–236 行）

```yacc
lvalue:
    ID {
      $$ = new absyn::SimpleVar(scanner_.GetTokPos(), $1);
    }
  | lvalue DOT ID {
      $$ = new absyn::FieldVar(scanner_.GetTokPos(), $1, $3);
    }
  | ID LBRACK exp RBRACK {
      auto base = new absyn::SimpleVar(scanner_.GetTokPos(), $1);
      $$ = new absyn::SubscriptVar(scanner_.GetTokPos(), base, $3);
    }
  | lvalue LBRACK exp RBRACK {
      $$ = new absyn::SubscriptVar(scanner_.GetTokPos(), $1, $3);
    }
  ;
```

| 产生式 | 含义 | AST 节点 | 示例 |
|--------|------|----------|------|
| `ID` | 简单变量 | `SimpleVar(pos, sym)` | `x` |
| `lvalue . ID` | 记录字段访问 | `FieldVar(pos, var, sym)` | `r.name` |
| `ID [ exp ]` | 数组下标（基础为 ID） | `SubscriptVar(pos, SimpleVar(ID), exp)` | `a[0]` |
| `lvalue [ exp ]` | 数组下标（链式） | `SubscriptVar(pos, var, exp)` | `a.b[0]`、`a[0][1]` |

**为什么要两条数组下标规则？**

因为左值的开头要么是 `ID`（第 223 行），要么已经是 `lvalue`（第 226 行）。当解析器看到 `ID [` 时，它需要判断这是一个数组下标（`SubscriptVar`）还是简单变量后面跟了其他东西。通过两条规则，处理了这两种入口情况。

**链式左值示例**：`a.b[0].c`
1. `a` → `SimpleVar(a)`
2. `a.b` → `FieldVar(SimpleVar(a), b)`
3. `a.b[0]` → `SubscriptVar(FieldVar(SimpleVar(a), b), IntExp(0))`
4. `a.b[0].c` → `FieldVar(SubscriptVar(...), c)`

---

### 2.11 声明列表：decs / decs_nonempty（第 238–252 行）

```yacc
decs:
    { $$ = new absyn::DecList(); }
  | decs_nonempty
  ;

decs_nonempty:
    decs_nonempty_s {
      $$ = new absyn::DecList($1);
    }
  | decs_nonempty_s decs_nonempty {
      $$ = $2->Prepend($1);
    }
  ;
```

- `decs`：声明列表，可以为空（`let in exp end` 中没有任何声明）。
- `decs_nonempty`：非空声明列表，使用 `Prepend` 构建列表。
- `decs_nonempty_s`：单个声明（变量声明、类型声明组或函数声明组）。

---

### 2.12 单个声明：decs_nonempty_s（第 254–262 行）

```yacc
decs_nonempty_s:
    vardec
  | tydec {
      $$ = new absyn::TypeDec(scanner_.GetTokPos(), $1);
    }
  | fundec {
      $$ = new absyn::FunctionDec(scanner_.GetTokPos(), $1);
    }
  ;
```

| 产生式 | 含义 | 说明 |
|--------|------|------|
| `vardec` | 变量声明 | 直接透传，`vardec` 本身返回 `Dec*` |
| `tydec` | 类型声明组 | 将 `NameAndTyList*` 包装成 `TypeDec` |
| `fundec` | 函数声明组 | 将 `FunDecList*` 包装成 `FunctionDec` |

> **问辩提示**：`tydec` 和 `fundec` 返回的是**列表**（`NameAndTyList*` 和 `FunDecList*`），因为 Tiger 允许**相邻的同类型声明形成一组**（用于互递归类型/函数）。这里用 `TypeDec` / `FunctionDec` 将整个列表包装成一个 `Dec*`，放入 `DecList` 中。

---

### 2.13 变量声明：vardec（第 264–271 行）

```yacc
vardec:
    VAR ID ASSIGN exp {
      $$ = new absyn::VarDec(scanner_.GetTokPos(), $2, nullptr, $4);
    }
  | VAR ID COLON ID ASSIGN exp {
      $$ = new absyn::VarDec(scanner_.GetTokPos(), $2, $4, $6);
    }
  ;
```

| 产生式 | 含义 | AST |
|--------|------|-----|
| `var id := exp` | 无类型标注的变量声明 | `VarDec(pos, name, nullptr, init)` |
| `var id : typeid := exp` | 有类型标注的变量声明 | `VarDec(pos, name, typ, init)` |

- **无类型标注**：类型由初始化表达式推断。`typ` 为 `nullptr`。
- **有类型标注**：显式指定类型，如 `var x : int := 5`。`$4` 是类型名的 Symbol。

---

### 2.14 记录字段初始化：rec / rec_nonempty / rec_one（第 273–293 行）

```yacc
rec:
    { $$ = new absyn::EFieldList(); }
  | rec_nonempty
  ;

rec_nonempty:
    rec_one {
      $$ = new absyn::EFieldList($1);
    }
  | rec_one COMMA rec_nonempty {
      $$ = $3->Prepend($1);
    }
  ;

rec_one:
    ID EQ exp {
      $$ = new absyn::EField($1, $3);
    }
  ;
```

- `rec`：记录字段初始化列表，可以为空（如 `point {}`）。
- `rec_one`：单个字段初始化 `ID = exp`，创建 `EField(name, exp)`。
- **示例**：`{x=1, y=2}` → `EFieldList([EField(x, IntExp(1)), EField(y, IntExp(2))])`

---

### 2.15 类型声明：tydec / tydec_one（第 295–308 行）

```yacc
tydec:
    tydec_one {
      $$ = new absyn::NameAndTyList($1);
    }
  | tydec_one tydec {
      $$ = $2->Prepend($1);
    }
  ;

tydec_one:
    TYPE ID EQ ty {
      $$ = new absyn::NameAndTy($2, $4);
    }
  ;
```

- `tydec_one`：单个类型声明 `type id = ty`，创建 `NameAndTy(name, ty)`。
- `tydec`：类型声明组，使用**左递归模式**（`tydec_one tydec`）将相邻的类型声明组合在一起。

**互递归类型示例**：

```
type tree = {key: int, children: treelist}
type treelist = {head: tree, tail: treelist}
```

这两个相邻的类型声明会被归约为一个 `NameAndTyList`，包含两个 `NameAndTy`，然后包装成一个 `TypeDec`。这表示它们是**互递归的**，语义分析时需要一起处理。

> **问辩提示**：Tiger 语言规定，相邻的同类型声明（连续的 `type` 或连续的 `function`）构成一个声明组，可以相互引用（递归/互递归）。不同类型的声明之间不能交叉引用。

---

### 2.16 类型字段列表：tyfields / tyfields_nonempty / tyfield（第 310–324 行）

```yacc
tyfields:
    { $$ = new absyn::FieldList(); }
  | tyfields_nonempty
  ;

tyfields_nonempty:
    tyfield {
      $$ = new absyn::FieldList($1);
    }
  | tyfield COMMA tyfields_nonempty {
      $$ = $3->Prepend($1);
    }
  ;

tyfield:
    ID COLON ID {
      $$ = new absyn::Field(scanner_.GetTokPos(), $1, $3);
    }
  ;
```

- `tyfield`：单个字段声明 `ID : ID`（字段名 : 类型名），创建 `Field(pos, name, typ)`。
- 用于记录类型定义和函数参数列表中。
- **示例**：`x: int, y: string` → `FieldList([Field(x, int), Field(y, string)])`

---

### 2.17 类型定义：ty（第 332–342 行）

```yacc
ty:
    ID {
      $$ = new absyn::NameTy(scanner_.GetTokPos(), $1);
    }
  | LBRACE tyfields RBRACE {
      $$ = new absyn::RecordTy(scanner_.GetTokPos(), $2);
    }
  | ARRAY OF ID {
      $$ = new absyn::ArrayTy(scanner_.GetTokPos(), $3);
    }
  ;
```

| 产生式 | 含义 | AST 节点 | 示例 |
|--------|------|----------|------|
| `ID` | 类型别名 | `NameTy(pos, name)` | `type myint = int` |
| `{ tyfields }` | 记录类型 | `RecordTy(pos, fields)` | `type point = {x: int, y: int}` |
| `array of ID` | 数组类型 | `ArrayTy(pos, element_type)` | `type intarray = array of int` |

这三种类型定义方式覆盖了 Tiger 语言的全部类型声明能力。

---

### 2.18 函数声明：fundec / fundec_one（第 344–360 行）

```yacc
fundec:
    fundec_one {
      $$ = new absyn::FunDecList($1);
    }
  | fundec_one fundec {
      $$ = $2->Prepend($1);
    }
  ;

fundec_one:
    FUNCTION ID LPAREN tyfields RPAREN EQ exp {
      $$ = new absyn::FunDec(scanner_.GetTokPos(), $2, $4, nullptr, $7);
    }
  | FUNCTION ID LPAREN tyfields RPAREN COLON ID EQ exp {
      $$ = new absyn::FunDec(scanner_.GetTokPos(), $2, $4, $7, $9);
    }
  ;
```

`fundec` 与 `tydec` 结构类似，相邻的函数声明组合成一组。

`fundec_one` 有两种形式：

| 产生式 | 含义 | AST |
|--------|------|-----|
| `function id (params) = exp` | 无返回类型的函数（过程） | `FunDec(pos, name, params, nullptr, body)` |
| `function id (params) : typeid = exp` | 有返回类型的函数 | `FunDec(pos, name, params, result, body)` |

- **过程**：返回类型为 `nullptr`，实际上返回 `unit`/`void`。
- **函数**：`$7` 是返回类型的 Symbol。

**示例**：

```
function print_int(i: int) = print(i)          /* 过程 */
function add(a: int, b: int) : int = a + b     /* 函数 */
```

---

## 总结与问辩要点

### 文法设计总结

1. **文法范式**：采用 Bison 的声明式优先级机制，用扁平的 `exp OP exp` 规则 + 优先级声明替代传统的嵌套层次（`expr → term → factor`），文法更简洁。
2. **AST 构建**：每个语义动作都 `new` 一个 AST 节点，利用 Bison 的 `$$` 和 `$n` 传递语义值。
3. **列表构建模式**：统一使用右递归 + `Prepend` 方法构建列表，保证从左到右的正确顺序。
4. **声明分组**：相邻的 `type` 和 `function` 声明被自动分组，支持互递归定义。

### 问辩高频问题

| 问题 | 答案要点 |
|------|----------|
| 悬空 else 怎么解决？ | `ELSE` 优先级高于 `THEN`，解析器优先移进 `ELSE`，与最近的 `then` 匹配 |
| 一元减号怎么处理？ | `%prec UMINUS` 赋予最高优先级，语义上转换为 `0 - exp` |
| 类型声明为什么要分组？ | Tiger 允许互递归类型（如链表和树），相邻的 type 声明构成一组，可以相互引用 |
| AND/OR 是否短路？ | Tiger 规范中 **不短路**，作为普通二元运算符处理 |
| `sequencing` 和 `expseq` 有什么区别？ | `expseq` 用于括号内 `(e1; e2)`，`sequencing` 用于 `let-in-end` 中，结构类似但语义上下文不同 |
| 为什么用 `Prepend` 而不是 `Append`？ | 文法是右递归的，解析时先得到列表尾部的元素，用 `Prepend` 从头部逐步插入，时间复杂度 O(1)/次 |
| nil 的含义？ | 空值，通常用于记录类型的默认值。语义分析时会检查 nil 只能赋给记录类型 |
| for 循环变量的作用域？ | 循环变量只在循环体中可见，且不可被赋值（只读），这由语义分析阶段检查 |
| `scanner_.GetTokPos()` 是什么？ | 获取当前 Token 在源代码中的位置（行号/列号），用于错误报告 |
