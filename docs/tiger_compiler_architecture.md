# Tiger 编译器架构详解

> 本文档面向编译原理初学者，按照编译器的实际执行顺序，从源代码到可执行程序的完整旅程讲解每一个模块的作用与相互关系。

---

## 目录

- [整体流程概览](#整体流程概览)
- [1. lex/ -- 词法分析器](#1-lex----词法分析器lexerscanner)
- [2. parse/ -- 语法分析器](#2-parse----语法分析器parser)
- [3. absyn/ -- 抽象语法树](#3-absyn----抽象语法树abstract-syntax-tree)
- [4. symbol/ -- 符号表基础](#4-symbol----符号表基础)
- [5. env/ -- 环境定义](#5-env----环境定义符号表内容)
- [6. semant/ + types/ -- 语义分析](#6-semant--types----语义分析)
- [7. escape/ -- 逃逸分析](#7-escape----逃逸分析)
- [8. frame/ + x64frame/ -- 栈帧管理](#8-frame--x64frame----栈帧管理)
- [9. translate/ + tree/ -- IR 树翻译](#9-translate--tree----ir-树翻译)
- [10. errormsg/ -- 错误信息](#10-errormsg----错误信息)
- [11. main/ -- 主程序](#11-main----主程序)
- [模块间数据流总结](#模块间数据流总结)

---

## 整体流程概览

想象你写了一段 Tiger 程序：

```tiger
let
  var x := 10
  function add(a: int, b: int): int = a + b
in
  print_int(add(x, 5))
end
```

编译器需要把这段文本变成机器能执行的指令。这个过程就像**翻译一篇外语文章**，要经过多个"翻译站"：

```
源代码(.tig)
    |
    v
+-----------+    +-----------+    +------------+
| 词法分析  |--->| 语法分析  |--->| 抽象语法树 |
|  (lex)    |    |  (parse)  |    |  (absyn)   |
+-----------+    +-----------+    +------------+
                                       |
                       +---------------+---------------+
                       v               v               v
                 +-----------+  +-----------+    +-----------+
                 | 逃逸分析  |  | 语义分析  |    |  符号表   |
                 | (escape)  |  | (semant)  |    | (symbol)  |
                 +-----------+  +-----------+    |  (env)    |
                      |                          +-----------+
                      v
                 +--------------------------------------+
                 |       IR 树翻译 (translate)           |
                 |  +---------+  +----------------+     |
                 |  | tree    |  | frame/x64frame |     |
                 |  | (IR树)  |  |  (栈帧管理)    |     |
                 |  +---------+  +----------------+     |
                 +--------------------------------------+
```

---

## 1. lex/ -- 词法分析器（Lexer/Scanner）

**类比：把句子拆成一个个词**

词法分析器的任务是"认字"。它逐个字符地读入源代码文本，把字符流切分成有意义的**词法单元（Token）**。

```
输入: "let var x := 10 + 5"
输出: [LET] [VAR] [ID:x] [ASSIGN] [INT:10] [PLUS] [INT:5]
```

**相关文件：**

| 文件 | 作用 |
|------|------|
| `tiger.lex` | 用正则表达式定义每种 Token 的模式（flex 语法） |
| `scanner.h` | 自动生成的扫描器基类 |
| `lex.cc` | flex 自动生成的词法分析器实现 |

**类比理解：** 就像你读英文时，眼睛自动把 `let` 识别为一个关键字，`x` 识别为一个标识符。词法分析器不做任何语法判断，只是"分词"。

---

## 2. parse/ -- 语法分析器（Parser）

**类比：把词组装成有语法结构的句子**

语法分析器接收 Token 流，按照**语法规则（文法）** 判断这些 Token 是否构成合法的程序，并构建出一棵**抽象语法树（AST）**。

```
Token流: [LET] [VAR] [x] [:=] [10]
         |
     LetExp
      |-- VarDec(x, init=IntExp(10))
      +-- body...
```

**相关文件：**

| 文件 | 作用 |
|------|------|
| `tiger.y` | 语法规则定义（Yacc/Bison 格式），每条规则在匹配时创建 AST 节点 |
| `parser.h` | 自动生成的解析器基类 |
| `parse.cc` | Bison 自动生成的语法分析器实现 |

**类比理解：** 就像语文课上分析句子结构 -- "主语+谓语+宾语"。语法分析器检查的是"这些词的组合方式是否合乎语法"，但不管"意思对不对"。

---

## 3. absyn/ -- 抽象语法树（Abstract Syntax Tree）

**类比：文章的骨架大纲**

这是语法分析的产物。AST 是一棵**树形结构**，精确表示程序的结构，丢弃了括号、分号等无关细节。

### 关键 AST 节点类型

| 类名 | 含义 | Tiger 示例 |
|------|------|------------|
| `IntExp` | 整数表达式 | `42` |
| `StringExp` | 字符串表达式 | `"hello"` |
| `NilExp` | nil 表达式 | `nil` |
| `VarExp` | 变量表达式 | `x` |
| `CallExp` | 函数调用 | `f(1, 2)` |
| `OpExp` | 二元运算 | `a + b` |
| `IfExp` | if 条件表达式 | `if a then b else c` |
| `WhileExp` | while 循环 | `while a do b` |
| `ForExp` | for 循环 | `for i := 1 to 10 do ...` |
| `LetExp` | let 块 | `let ... in ... end` |
| `SeqExp` | 表达式序列 | `(a; b; c)` |
| `AssignExp` | 赋值表达式 | `x := 5` |
| `RecordExp` | 记录创建 | `r {name="a", age=1}` |
| `ArrayExp` | 数组创建 | `arr [10] of 0` |
| `BreakExp` | break 语句 | `break` |
| `VoidExp` | 空表达式（无操作） | -- |
| `VarDec` | 变量声明 | `var x := 10` |
| `FunctionDec` | 函数声明（可多个） | `function f(...)` |
| `TypeDec` | 类型声明（可多个） | `type t = int` |

### 变量访问的三种方式

| 类名 | 含义 | 示例 |
|------|------|------|
| `SimpleVar` | 简单变量名 | `x` |
| `FieldVar` | 记录字段访问 | `r.name` |
| `SubscriptVar` | 数组下标访问 | `arr[3]` |

**重要设计 -- 访问者模式：**

每个 AST 节点都有多种"访问方法"：

| 方法 | 对应阶段 | 作用 |
|------|----------|------|
| `Print()` | 调试 | 打印 AST 结构 |
| `SemAnalyze()` | Lab4 语义分析 | 类型检查 |
| `Traverse()` | Lab5 逃逸分析 | 标记逃逸变量 |
| `Translate()` | Lab5 IR翻译 | 生成IR树 |

同一棵 AST 树可以被不同阶段以不同方式遍历处理。

---

## 4. symbol/ -- 符号表基础

**类比：字典 / 名字查询系统**

在编译器中，需要频繁用"名字"（如变量名 `x`、函数名 `add`）来查找信息。`symbol` 模块提供了：

- **`Symbol`**：表示一个名字（如 `"x"`）。相同字符串只创建一个 Symbol 对象（内部用哈希表实现去重），可以用 `==` 直接比较
- **`Table<ValueType>`**：一个支持**作用域**的哈希表，提供以下操作：

```cpp
tab->Enter(sym, value);   // 插入名字-值映射
tab->Look(sym);            // 查找名字对应的值
tab->BeginScope();         // 开始新作用域（压入哨兵标记）
tab->EndScope();           // 结束作用域（弹出当前作用域的所有条目）
```

**作用域栈的运作方式：**

```
作用域栈（从外到内）：

  全局: { print: FunEntry, int: IntTy, string: StringTy }
    |
    +-- 函数f: { a: VarEntry(Int), b: VarEntry(Int) }
          |
          +-- let块: { x: VarEntry(Int) }   <-- BeginScope 压入
          |
          (EndScope 弹出 x, 回到函数f的作用域)
```

**相关文件：**

| 文件 | 作用 |
|------|------|
| `symbol.h` | Symbol 类和 Table 模板类的定义 |
| `symbol.cc` | Symbol 的实现（唯一化、哈希） |
| `util/table.h` | 底层通用哈希表 |

---

## 5. env/ -- 环境定义（符号表内容）

**类比：字典里的词条解释**

`env` 定义了符号表中存什么内容。当你查到变量 `x` 时，得到的是一个 `VarEntry`；查到函数 `f` 时得到 `FunEntry`。

### 环境条目类型

```
EnvEntry (基类)
  |
  +-- VarEntry:   { access_: 变量的内存位置, ty_: 变量的类型 }
  |     readonly_ 标记是否为只读变量（for循环变量不可赋值）
  |
  +-- FunEntry:   { level_: 函数所在的嵌套层级
                    label_: 函数入口标签
                    formals_: 参数类型列表
                    result_: 返回类型 }
```

### 内置函数和类型

`FillBaseVEnv()` 注册内置函数：

| 函数 | 参数类型 | 返回类型 | 说明 |
|------|----------|----------|------|
| `print` | string | void | 打印字符串 |
| `printi` | int | void | 打印整数 |
| `flush` | -- | void | 刷新输出 |
| `getchar` | -- | string | 读一个字符 |
| `ord` | string | int | 字符转ASCII码 |
| `chr` | int | string | ASCII码转字符 |
| `size` | string | int | 字符串长度 |
| `substring` | string, int, int | string | 子串 |
| `concat` | string, string | string | 字符串拼接 |
| `exit` | int | void | 退出程序 |

`FillBaseTEnv()` 注册内置类型：`int`, `string`。

---

## 6. semant/ + types/ -- 语义分析

**类比：检查文章"说得对不对"**

语法分析只检查"语法结构是否合法"，但不会告诉你：
- `x` 是否已声明？（变量查找）
- `a + b` 的类型对不对？（类型检查）
- 函数调用的参数个数和类型对不对？（参数匹配）
- `break` 是否在循环内部？（上下文检查）

语义分析遍历 AST，使用符号表做这些检查。

### Tiger 的类型系统（types/）

```
type::Ty (类型基类)
  +-- IntTy      int 类型（单例）
  +-- StringTy   string 类型（单例）
  +-- VoidTy     void / 无返回值（单例）
  +-- NilTy      nil / 空记录（单例）
  +-- RecordTy   记录类型 { name: type, ... }
  |     fields_: FieldList
  +-- ArrayTy    数组类型 type []
  |     ty_: 元素类型
  +-- NameTy     类型别名 / 命名类型
        name_: 类型名
        ty_: 指向实际类型（可能需要链式跟随）
```

**关键方法：**
- `ActualTy()`：跟随 NameTy 链，找到底层实际类型
- `IsSameType()`：判断两个类型是否兼容（nil 可赋值给任何 record 类型）

### 语义分析的工作流程

以 `a + b`（OpExp）为例：

```
1. 递归分析 left_ (a) -> 查符号表 -> 得到类型 IntTy
2. 递归分析 right_ (b) -> 查符号表 -> 得到类型 IntTy
3. 检查 + 要求两边都是 int -> 通过
4. 返回表达式类型 IntTy
```

以 `let var x := 10 in x end`（LetExp）为例：

```
1. venv->BeginScope()  tenv->BeginScope()
2. 处理声明: VarDec(x) -> 分析 init (10) -> 类型 IntTy -> venv->Enter(x, VarEntry(IntTy))
3. 处理 body: x -> 查符号表 -> VarEntry -> 类型 IntTy
4. tenv->EndScope()  venv->EndScope()
5. 返回 body 的类型 IntTy
```

### 相关文件

| 文件 | 作用 |
|------|------|
| `semant.h` | ProgSem 类定义（语义分析入口） |
| `semant.cc` | 所有 AST 节点的 SemAnalyze() 实现 |
| `types.h` | 类型系统定义（Ty, RecordTy, ArrayTy 等） |
| `types.cc` | ActualTy() 和 IsSameType() 实现 |

---

## 7. escape/ -- 逃逸分析

**类比：决定变量应该放在"公共储物柜"还是"随身口袋"**

在语义分析之后、IR 翻译之前运行。核心问题是：一个变量应该分配在**栈上**（内存）还是**寄存器**中？

- **不逃逸（Non-escape）：** 变量只在定义它的函数内部使用 -> 可以优化为放在寄存器里，访问更快
- **逃逸（Escape）：** 变量被内层嵌套函数引用（即"逃出"了定义它的作用域）-> 必须放在栈上，因为内层函数需要通过地址访问它

### 判定规则

```
变量定义深度 < 变量使用深度  =>  逃逸

let                           <- 深度 0
  var x := 10                 <- x 定义在深度 0
  function f() =              <- f 定义在深度 1（body 在深度 1 执行）
    print_int(x)              <- x 在深度 1 被使用，1 > 0 => x 逃逸！
in
  f()
end
```

### 算法

遍历 AST，维护一个逃逸环境（`EscEnv`：符号名 -> `{depth, &escape_}`）：
1. 遇到变量声明（`VarDec`, `ForExp`, 函数参数 `Field`）：将 `变量名 -> {当前深度, &escape_布尔}` 存入环境
2. 遇到变量使用（`SimpleVar`）：查找环境，如果使用深度 > 定义深度，设置 `*escape_ = true`
3. 进入 `let` 块或函数体时 `BeginScope()`，离开时 `EndScope()`
4. 进入函数体时深度 +1

### 相关文件

| 文件 | 作用 |
|------|------|
| `escape.h` | EscapeEntry 和 EscFinder 类定义 |
| `escape.cc` | 所有 AST 节点的 Traverse() 实现 |

---

## 8. frame/ + x64frame/ -- 栈帧管理

**类比：为每次函数调用分配一张"办公桌"**

当程序运行时，每调用一个函数，系统会在栈上分配一块连续空间叫**栈帧（Stack Frame）**。这个模块负责管理栈帧的布局。

### x86-64 栈帧结构

```
+--------------------+ <- 高地址
|  调用者的栈帧       |
+--------------------+
|  第7个参数(栈传递)  |  <- 超过6个的参数通过栈传递
|  第8个参数(栈传递)  |
+--------------------+
|  返回地址           |  <- call 指令自动压入
+--------------------+ <- 函数入口时的 RSP
|  局部变量1 (逃逸)   |  <- offset = -8
|  局部变量2 (逃逸)   |  <- offset = -16
|  ...               |
+--------------------+
|  callee-saved 寄存器 |  <- 保存的 RBX, R12-R15 等
|  保存区             |
+--------------------+
|  outgoing 参数区    |  <- 调用其他函数时传递超过6个的参数
+--------------------+ <- 低地址（当前 RSP）
```

### x86-64 寄存器约定

| 寄存器 | 角色 | 说明 |
|--------|------|------|
| RAX | 返回值 (RV) | 函数返回值放在这里 |
| RDI, RSI, RDX, RCX, R8, R9 | 参数传递 | 前6个整数参数 |
| RAX, RDI, RSI, RDX, RCX, R8, R9, R10, R11 | Caller-saved | 调用函数前需要保存（如果还要用） |
| RBX, RBP, R12, R13, R14, R15 | Callee-saved | 被调用函数负责保存恢复 |
| RSP | 栈指针 | 始终指向栈顶 |

### 关键概念

**Access -- 变量的位置**

```
Access (基类)
  +-- InFrameAccess(offset)  -- 在栈帧中，通过 FP+offset 访问
  |     ToExp(fp) => MemExp(BinopExp(+, fp, Const(offset)))
  |
  +-- InRegAccess(temp)      -- 在寄存器中
        ToExp(fp) => TempExp(temp)
```

**静态链接（Static Link）**

Tiger 支持函数嵌套定义，内层函数需要访问外层函数的变量。静态链接是每个函数的**第一个隐藏参数**，指向**定义它的外层函数的栈帧**，形成一条链：

```
tigermain 的栈帧 (深度 0)
    ^ 静态链接
    |
函数 g 的栈帧 (深度 1, 在 tigermain 中定义)
    ^ 静态链接
    |
函数 f 的栈帧 (深度 2, 在 g 中定义)
    -- f 中访问 g 的变量时，通过静态链接链跳到 g 的栈帧
```

当变量 `x` 在外层函数中定义，内层函数要访问它时：
1. 从当前栈帧的静态链接出发
2. 逐层向上跳，直到到达 `x` 定义所在的那层栈帧
3. 在该栈帧中按偏移量读取变量

**View Shift -- 参数搬家**

函数刚被调用时，参数在寄存器中（RDI, RSI, ...）。但如果参数变量逃逸了（需要取地址），就必须把它搬到栈上。View Shift 就是做这件事的一系列 `Move` 指令：

```
view_shift:
  MOV [FP-8], RDI     ; 第1个参数搬到栈上
  MOV [FP-16], RSI    ; 第2个参数搬到栈上
  ...
```

### 相关文件

| 文件 | 作用 |
|------|------|
| `frame.h` | Access、Frame、Frag 等基类定义 |
| `x64frame.h` | X64RegManager 定义（寄存器管理） |
| `x64frame.cc` | NewFrame、AllocLocal、ProcEntryExit1 等实现 |
| `temp.h` / `temp.cc` | 临时变量(Temp)和标签(Label)的工厂 |

---

## 9. translate/ + tree/ -- IR 树翻译

**类比：把"中文文章"翻译成"英文文章"，但还不是机器码**

这是 Lab5 的核心模块。它把 AST 翻译成**中间表示树（IR Tree）** -- 一种更接近机器但仍然与具体 CPU 无关的表示。每一条 IR 指令大致对应一条或几条机器指令。

### IR 树节点类型

**语句（tree::Stm）：产生副作用，不返回值**

| 类名 | 含义 | 类比 |
|------|------|------|
| `SeqStm(s1, s2)` | 顺序执行 s1, s2 | `{ s1; s2; }` |
| `LabelStm(label)` | 放一个标签（跳转目标） | `L0:` |
| `JumpStm(exp)` | 无条件跳转 | `goto L0;` |
| `CjumpStm(op, l, r, t, f)` | 条件跳转 | `if (l op r) goto t else goto f` |
| `MoveStm(dst, src)` | 赋值 | `dst = src;` |
| `ExpStm(exp)` | 执行表达式，丢弃结果 | `f();` |

**表达式（tree::Exp）：产生一个值**

| 类名 | 含义 | 类比 |
|------|------|------|
| `BinopExp(op, l, r)` | 二元运算 | `l + r` |
| `MemExp(addr)` | 读内存 | `*addr` / `M[addr]` |
| `TempExp(temp)` | 读临时变量/寄存器 | `t1` |
| `ConstExp(n)` | 整数常量 | `42` |
| `NameExp(label)` | 标签地址 | `&&L0` |
| `CallExp(func, args)` | 函数调用 | `func(args...)` |
| `EseqExp(stm, exp)` | 先执行 stm，再求值 exp | `({stm; exp;})` |

### 翻译示例

Tiger 代码：
```tiger
var x := 10 + 5
```

翻译成 IR 树：
```
MoveStm(                                    -- 赋值语句
  MemExp(                                   -- 目标: 栈上某个位置
    BinopExp(PLUS,
      TempExp(FP),                          -- 栈帧指针
      ConstExp(-8)                          -- 偏移量 -8
    )
  ),
  BinopExp(PLUS, ConstExp(10), ConstExp(5)) -- 源: 10 + 5
)
```

### 三种 IR 表达式包装器

这是 IR 翻译中最精妙的设计。翻译过程中，一个表达式的结果可能有三种"形态"：

| 包装器 | 含义 | 何时产生 | 示例 |
|--------|------|----------|------|
| `ExExp` | 产生一个**值**的表达式 | 算术运算、变量、常量 | `1 + 2` -> `BinopExp(+, 1, 2)` |
| `NxExp` | 产生一个**语句**（无返回值） | 赋值、声明 | `x := 5` -> `MoveStm(...)` |
| `CxExp` | 产生一个**条件跳转** | 比较运算 | `a < b` -> `CjumpStm(LT, a, b, t, f)` |

### 包装器之间的转换

它们之间可以互相转换（`UnEx()`, `UnNx()`, `UnCx()`）：

**ExExp -> Cx：把值与0比较**
```
exp != 0  =>  true
exp == 0  =>  false
```

**CxExp -> Ex：用临时变量保存 0/1 结果**
```
CxExp(a < b => CjumpStm)
  |
  v  UnEx()
EseqExp(
  MoveStm(r, 1),            -- 先设 r = 1（假设成立）
  CjumpStm(a<b, t, f),      -- 如果 a<b 跳到 t
  LabelStm(f),
  MoveStm(r, 0),            -- 否则设 r = 0
  LabelStm(t),
  TempExp(r)                 -- 返回 r 的值
)
```

**NxExp -> Ex：返回常量0**
```
NxExp(stm)
  |
  v  UnEx()
EseqExp(stm, ConstExp(0))   -- 执行语句，返回 0
```

### 翻译各语法结构的策略

#### 变量访问 -- `SimpleVar::Translate`

```tiger
let function f() = ( ... x ... )  (* x 在外层定义 *)
```
翻译时需要沿静态链接链从当前层级向上跳到 `x` 的定义层级：
```cpp
tree::Exp *fp = new TempExp(FP);
while (current_level != x's_level) {
    fp = static_link_of(current_level)->ToExp(fp);
    current_level = current_level->parent;
}
return x's_access->ToExp(fp);
```

#### 函数调用 -- `CallExp::Translate`

- **外部函数**（print, exit 等运行时库函数）：用 `ExternalCall`
- **内部函数**：第一个参数传静态链接，后面跟实际参数

```
// 调用 add(x, 5)
CallExp(
  NameExp(add_label),
  [static_link, x的值, ConstExp(5)]
)
```

#### if-then-else -- `IfExp::Translate`

```
CjumpStm(test, true_label, false_label)
LabelStm(true_label)
MoveStm(r, then_value)
JumpStm(join)
LabelStm(false_label)
MoveStm(r, else_value)
LabelStm(join)
return TempExp(r)
```

#### while 循环 -- `WhileExp::Translate`

```
LabelStm(test_label)
CjumpStm(test, body_label, done_label)
LabelStm(body_label)
(body statements)
JumpStm(test_label)
LabelStm(done_label)
```

#### for 循环 -- `ForExp::Translate`

```
MoveStm(loop_var, lo_value)       -- i := lo
MoveStm(limit_temp, hi_value)     -- limit := hi
LabelStm(loop_label)
CjumpStm(LE, i, limit, body, done) -- i <= limit ?
LabelStm(body_label)
(body statements)
MoveStm(loop_var, i+1)            -- i := i + 1
JumpStm(loop_label)
LabelStm(done_label)
```

#### 记录创建 -- `RecordExp::Translate`

```
// 调用运行时分配内存
r = ExternalCall("alloc_record", [n * wordSize])
// 逐个字段赋值
MoveStm(Mem(r + 0), field1_value)
MoveStm(Mem(r + 8), field2_value)
...
return r
```

#### 数组创建 -- `ArrayExp::Translate`

```
// 调用运行时分配并初始化
ExternalCall("init_array", [size, init_value])
```

### 代码片段（Fragments）

翻译的最终产物是一组 Fragment：

```
Frag (基类)
  +-- StringFrag:  { label_: 字符串标签, str_: 字符串内容 }
  |     -- 每个字符串常量生成一个，放在数据段
  |
  +-- ProcFrag:    { body_: IR树(语句), frame_: 栈帧信息 }
        -- 每个函数生成一个，包含完整的IR树和栈帧布局
```

### 相关文件

| 文件 | 作用 |
|------|------|
| `translate.h` | Level, Access, ProgTr, PatchList 等定义 |
| `translate.cc` | 所有 Translate() 方法和 Exp 转换实现 |
| `tree.h` | IR 树节点定义（Stm, Exp 及其子类） |
| `tree.cc` | IR 树打印和辅助函数 |

---

## 10. errormsg/ -- 错误信息

**类比：批改作业时用红笔标注错误位置**

提供错误报告功能，记录出错的文件位置（行号、列号），输出带位置的错误信息。

```cpp
errormsg->Error(pos, "undefined variable %s", name);
// 输出类似: test.tig:3.5: undefined variable x
```

---

## 11. main/ -- 主程序

`main.cc` 把所有模块串联起来，形成完整的编译管线：

```cpp
int main(int argc, char **argv) {
    // 初始化全局对象
    reg_manager = new frame::X64RegManager();
    frags = new frame::Frags();

    // 第1步：词法分析 + 语法分析 -> AST
    Parser parser(fname);
    parser.parse();
    absyn_tree = parser.TransferAbsynTree();

    // 第2步：逃逸分析 -> 标记哪些变量需要放在栈上
    esc::EscFinder esc_finder(std::move(absyn_tree));
    esc_finder.FindEscape();
    absyn_tree = esc_finder.TransferAbsynTree();

    // 第3步：IR 树翻译 -> 生成 IR + 栈帧 + 代码片段
    tr::ProgTr prog_tr(std::move(absyn_tree));
    prog_tr.Translate();
    errormsg = prog_tr.TransferErrormsg();

    // 如果有错误就停止
    if (errormsg->AnyErrors()) return 1;

    // 第4步：（后续 Lab）代码生成 -> 汇编
    output::AssemGen assem_gen(fname);
    assem_gen.GenAssem(true);
}
```

### 测试程序

| 文件 | 作用 |
|------|------|
| `test_lex.cc` | 仅测试词法分析 |
| `test_parse.cc` | 仅测试语法分析（构建 AST 并打印） |
| `test_semant.cc` | 测试语义分析（类型检查） |
| `test_translate.cc` | 测试 IR 翻译（生成完整 IR 树） |

---

## 模块间数据流总结

```
.tig 源文件
  |
  v  [词法分析 lex/tiger.lex]
Token 流
  |
  v  [语法分析 parse/tiger.y]
抽象语法树 (absyn::AbsynTree, absyn::Exp, absyn::Dec ...)
  |
  +---- [语义分析 semant] ----> 类型检查结果 (Lab4)
  |
  v  [逃逸分析 escape]
AST + escape 标记 (VarDec::escape_, ForExp::escape_, Field::escape_)
  |
  v  [IR 翻译 translate]
  使用 symbol/env (符号表) + frame/x64frame (栈帧) + tree (IR节点)
  |
  v
IR 树 (tree::Stm, tree::Exp)
  + 栈帧 (frame::X64Frame)
  + 代码片段集合 (frame::Frags: StringFrag + ProcFrag)
  |
  v  [后续 Lab: 指令选择 / 寄存器分配 / 代码发射]
x86-64 汇编代码
```

---

## 核心设计思想

### 1. 分层翻译

编译器是一个**多级翻译**的过程。每一级都把程序从一种表示转换成更接近机器的另一种表示，同时在每一级做相应的分析和检查。这种分层设计让每一层只需要关注自己的问题，降低了整体复杂度。

### 2. 数据结构是桥梁

```
源代码文本
  --(Token)--> 词法分析器
  --(AST)--> 语法分析器
  --(带escape标记的AST)--> 逃逸分析
  --(IR Tree + Frame)--> IR翻译器
  --(汇编)--> 代码生成器
```

每个阶段之间的数据结构（Token、AST、IR Tree）都是前一阶段的产物、后一阶段的输入。

### 3. 访问者模式

AST 节点（`absyn.h`）不包含任何编译逻辑，只存储结构信息。通过虚函数 `SemAnalyze()`、`Translate()`、`Traverse()` 让不同阶段以不同方式遍历同一棵树。这是经典的访问者模式，好处是：
- AST 定义稳定不变
- 各编译阶段的代码相互独立
- 新增阶段只需添加新的遍历方法

### 4. 符号表贯穿始终

从语义分析到 IR 翻译，符号表（`symbol::Table`）是最核心的数据结构之一。它支撑了：
- 变量/函数的声明与查找
- 类型信息的存储
- 作用域管理（嵌套的 begin/end scope）
- 从变量名到内存位置的映射
