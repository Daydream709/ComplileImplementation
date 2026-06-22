# codegen.cc 逐行详解

## 文件概述

本文件实现的是 Tiger 编译器 **Lab5-2：代码生成（Code Generation）** 的核心部分。

它的职责是将经过规范化（canonicalization）、基本块划分（basic block）和 trace 调度之后的 **IR 树（中间表示树）** 翻译为 **x86-64 汇编指令序列**。

文件分为两大部分：
- `namespace cg`：代码生成的入口驱动逻辑
- `namespace tree`：每个 IR 树节点的 "Munch"（指令选择）方法

---

## 全局常量与外部依赖（第 1–14 行）

```cpp
#include "tiger/codegen/codegen.h"
#include <cassert>
#include <sstream>

extern frame::RegManager *reg_manager;   // 全局寄存器管理器，提供寄存器编号与名称的映射

namespace {
constexpr int maxlen = 1024;       // sprintf 缓冲区大小
constexpr int WORD_SIZE = 8;       // x86-64 一个字（64 位）= 8 字节
constexpr int NUM_ARG_REGS = 6;    // x86-64 System V ABI 用 6 个寄存器传参（RDI, RSI, RDX, RCX, R8, R9）
} // namespace
```

---

## 第一部分：`namespace cg` — 代码生成入口（第 16–34 行）

### `CodeGen::Codegen()`（第 18–27 行）

```cpp
void CodeGen::Codegen() {
  auto *instr_list = new assem::InstrList();          // 1. 创建空指令列表
  tree::StmList *stm_list = traces_->GetStmList();    // 2. 获取 trace 调度后的语句列表

  for (auto *stm : stm_list->GetList()) {             // 3. 对每条语句调用 Munch
    stm->Munch(*instr_list, fs_);                     //    Munch 会向 instr_list 追加汇编指令
  }

  assem_instr_ = std::make_unique<AssemInstr>(instr_list);  // 4. 包装结果
}
```

- `traces_` 是 trace 调度阶段输出的语句列表
- `fs_` 是当前函数的 framesize 字符串（用于帧指针偏移计算）
- 每个 `stm->Munch()` 会根据语句类型选择对应的 x86-64 指令

### `AssemInstr::Print()`（第 29–33 行）

```cpp
void AssemInstr::Print(FILE *out, temp::Map *map) const {
  for (auto instr : instr_list_->GetList())
    instr->Print(out, map);    // 逐条打印汇编指令，map 将 temp 编号映射为寄存器名
  fprintf(out, "\n");
}
```

调试/输出用，将生成的指令序列打印到文件。

---

## 第二部分：`namespace tree` — IR 节点的 Munch 方法

### 指令中的占位符约定

在汇编指令模板中使用以下占位符：
- `` `s0 ``、`` `s1 `` ... — 源操作数（source），对应 `TempList` 中的第 0、1... 个 temp
- `` `d0 ``、`` `d1 `` ... — 目标操作数（destination），对应 `TempList` 中的第 0、1... 个 temp
- `` `j0 `` — 跳转目标标签

---

### `ExpList::MunchArgs` — 函数参数传递（第 44–79 行）

```cpp
temp::TempList *ExpList::MunchArgs(assem::InstrList &instr_list, std::string_view fs) {
  temp::TempList *arg_temps = new temp::TempList();   // 记录所有参数所在的 temp
  temp::TempList *arg_regs = reg_manager->ArgRegs();  // 获取 6 个参数寄存器 [RDI, RSI, RDX, RCX, R8, R9]

  auto reg_it = arg_regs->GetList().begin();
  int i = 0;

  for (auto *arg : exp_list_) {
    temp::Temp *arg_temp = arg->Munch(instr_list, fs);  // 先对参数表达式求值

    if (i < NUM_ARG_REGS) {
      // 前 6 个参数：移入对应寄存器
      // movq `s0, `d0  →  movq arg_temp, RDI/RSI/...
      instr_list.Append(new assem::OperInstr(
          "movq `s0, `d0",
          new temp::TempList({*reg_it}),       // dst: 参数寄存器
          new temp::TempList({arg_temp}),      // src: 参数值
          nullptr));
      arg_temps->Append(*reg_it);
      ++reg_it;
    } else {
      // 第 7 个及之后的参数：写入栈上
      // movq `s0, offset(%rsp)
      int stack_offset = (i - NUM_ARG_REGS) * WORD_SIZE;  // 0, 8, 16, ...
      char buf[maxlen];
      sprintf(buf, "movq `s0, %d(%%rsp)", stack_offset);
      instr_list.Append(new assem::OperInstr(
          buf,
          nullptr,                              // dst: 无（内存目标）
          new temp::TempList({arg_temp}),        // src: 参数值
          nullptr));
      arg_temps->Append(arg_temp);
    }
    i++;
  }

  return arg_temps;
}
```

**关键逻辑**：x86-64 调用约定中，前 6 个参数走寄存器，之后的走栈。调用方（caller）需要在调用前用 `subq` 分配好栈空间（见 `CallExp::Munch`）。

---

### `SeqStm::Munch` — 顺序语句（第 81–85 行）

```cpp
void SeqStm::Munch(assem::InstrList &instr_list, std::string_view fs) {
  left_->Munch(instr_list, fs);    // 先处理左子语句
  right_->Munch(instr_list, fs);   // 再处理右子语句
}
```

规范化后理论上不应出现 `SeqStm`，但做防御性处理。

---

### `LabelStm::Munch` — 标签（第 87–89 行）

```cpp
void LabelStm::Munch(assem::InstrList &instr_list, std::string_view fs) {
  instr_list.Append(new assem::LabelInstr(
      label_->Name(), label_));    // 生成 "L0:" 这样的标签
}
```

---

### `JumpStm::Munch` — 无条件跳转（第 92–99 行）

```cpp
void JumpStm::Munch(assem::InstrList &instr_list, std::string_view fs) {
  temp::Label *target = exp_->name_;
  instr_list.Append(new assem::OperInstr(
      "jmp `j0",                   // jmp L0
      nullptr,                     // dst: 无
      nullptr,                     // src: 无
      new assem::Targets(new std::vector<temp::Label *>({target}))));  // 跳转目标
}
```

---

### `CjumpStm::Munch` — 条件跳转（第 101–132 行）

```cpp
void CjumpStm::Munch(assem::InstrList &instr_list, std::string_view fs) {
  temp::Temp *left = left_->Munch(instr_list, fs);    // 求值左操作数
  temp::Temp *right = right_->Munch(instr_list, fs);  // 求值右操作数

  // 先比较：cmpq right, left（注意 x86 的 operand order：cmpq src1, src0 设置 flags）
  instr_list.Append(new assem::OperInstr(
      "cmpq `s1, `s0",
      nullptr,
      new temp::TempList({left, right}),     // s0=left, s1=right
      nullptr));

  // 根据比较运算符选择对应的条件跳转指令
  std::string jmp_op;
  switch (op_) {
  case EQ_OP:  jmp_op = "je"; break;    // ==
  case NE_OP:  jmp_op = "jne"; break;   // !=
  case LT_OP:  jmp_op = "jl"; break;    // <
  case GT_OP:  jmp_op = "jg"; break;    // >
  case LE_OP:  jmp_op = "jle"; break;   // <=
  case GE_OP:  jmp_op = "jge"; break;   // >=
  default: assert(0); break;
  }

  // 条件成立则跳到 true_label_
  instr_list.Append(new assem::OperInstr(
      jmp_op + " `j0", nullptr, nullptr,
      new assem::Targets(new std::vector<temp::Label *>({true_label_}))));

  // 条件不成立则跳到 false_label_
  instr_list.Append(new assem::OperInstr(
      "jmp `j0", nullptr, nullptr,
      new assem::Targets(new std::vector<temp::Label *>({false_label_}))));
}
```

**注意**：每条 `CjumpStm` 生成 3 条指令：`cmpq` + 条件跳转 + 无条件跳转。这是因为 trace 调度后的 cjump 只有一个"期望"的目标，另一个需要显式跳转。

---

### `MoveStm::Munch` — 赋值/移动（第 134–239 行）

这是最复杂的方法之一，需要处理多种模式：

#### 模式 1：`MOVE(MEM(...), e2)` — 存储到内存

```cpp
if (typeid(*dst_) == typeid(MemExp)) {
    auto *mem = static_cast<MemExp *>(dst_);
    temp::Temp *src = src_->Munch(instr_list, fs);   // 先求值源操作数
```

**子模式 1a**：`MOVE(MEM(BINOP(PLUS, base, CONST(n))), src)` — 带偏移的存储

```cpp
    // movq src, offset(base)
    sprintf(buf, "movq `s0, %d(`s1)", offset);
    // s0 = src值, s1 = base地址
```

**子模式 1b**：`MOVE(MEM(BINOP(PLUS, CONST(n), base)), src)` — 偏移在左

```cpp
    // 同上，加法可交换
    sprintf(buf, "movq `s0, %d(`s1)", offset);
```

**子模式 1c**：`MOVE(MEM(addr), src)` — 通用存储

```cpp
    // movq src, (addr)
    instr_list.Append(new assem::OperInstr(
        "movq `s0, (`s1)", ...));
```

#### 模式 2：`MOVE(TEMP(t), e2)` — 存到临时变量

**子模式 2a**：`MOVE(TEMP(t), MEM(BINOP(PLUS, base, CONST(n))))` — 带偏移的加载

```cpp
    // movq offset(base), dst
    sprintf(buf, "movq %d(`s0), `d0", offset);
    // s0 = base地址, d0 = 目标temp
```

**子模式 2b**：`MOVE(TEMP(t), MEM(addr))` — 通用加载

```cpp
    // movq (addr), dst
    instr_list.Append(new assem::MoveInstr(
        "movq (`s0), `d0", ...));
```

**子模式 2c**：`MOVE(TEMP(t), e)` — 通用寄存器移动

```cpp
    // movq src, dst
    instr_list.Append(new assem::MoveInstr(
        "movq `s0, `d0", ...));
```

#### 模式 3：兜底通用移动

```cpp
  temp::Temp *dst_temp = dst_->Munch(instr_list, fs);
  temp::Temp *src_temp = src_->Munch(instr_list, fs);
  instr_list.Append(new assem::MoveInstr("movq `s0, `d0", ...));
```

---

### `ExpStm::Munch` — 表达式语句（第 241–243 行）

```cpp
void ExpStm::Munch(assem::InstrList &instr_list, std::string_view fs) {
  exp_->Munch(instr_list, fs);   // 只求值，丢弃结果
}
```

用于函数调用 `print(x)` 这种"只为副作用"的场景。

---

### `BinopExp::Munch` — 二元运算（第 245–463 行）

对每种运算符生成不同的指令序列。所有情况都返回一个新 temp 保存结果。

#### `PLUS_OP` — 加法（第 249–302 行）

三种优化路径：

1. **`BINOP(PLUS, e, CONST(n))`** → `leaq n(%temp), %result`（一条指令）
2. **`BINOP(PLUS, CONST(n), e)`** → `leaq n(%temp), %result`（加法可交换）
3. **通用** → `movq left, result; addq right, result`（两条指令）

`leaq` 是 x86-64 的"取有效地址"指令，常用于快速加法（不修改 flags）。

#### `MINUS_OP` — 减法（第 304–317 行）

```cpp
// movq left, result
// subq right, result    (result = left - right)
```

#### `MUL_OP` — 乘法（第 319–338 行）

```cpp
// movq left, %rax
// imulq right           (%rax = %rax * right)
// movq %rax, result
```

x86 的 `imulq` 单操作数形式要求被乘数在 `%rax`，结果也在 `%rax`。

#### `DIV_OP` — 除法（第 340–365 行）

```cpp
// movq left, %rax       被除数放 %rax
// cqto                  符号扩展 %rax → %rdx:%rax（128 位被除数）
// idivq right           %rax = 商, %rdx = 余数
// movq %rax, result
```

#### `XOR_OP` — 异或（第 367–392 行）

```cpp
// movq left, result
// xorq right, result    (若 right 是常量，用 xorq $imm 形式)
```

#### `LSHIFT_OP` / `RSHIFT_OP` — 移位（第 397–447 行）

```cpp
// movq left, result
// movq right, %rcx      x86 要求移位量在 %cl（%rcx 低 8 位）
// shlq %cl, result      左移
// shrq %cl, result      右移（逻辑右移）
```

---

### `MemExp::Munch` — 内存读取（第 465–532 行）

从内存地址加载一个 64 位值到 temp。

#### 模式：`MEM(BINOP(PLUS, base, CONST(n)))` — 带偏移加载

```cpp
// movq offset(%base), %result
```

#### 模式：`MEM(BINOP(PLUS, e1, e2))` — 双寄存器寻址

```cpp
// movq e1, result
// addq e2, result       计算地址
// movq (result), result  加载值
```

#### 模式：`MEM(addr)` — 通用加载

```cpp
// movq (%addr), %result
```

---

### `TempExp::Munch` — 临时变量（第 534–536 行）

```cpp
temp::Temp *TempExp::Munch(...) {
  return temp_;    // 直接返回 temp 本身，不生成任何指令
}
```

---

### `EseqExp::Munch` — ESEQ 表达式（第 538–542 行）

```cpp
temp::Temp *EseqExp::Munch(...) {
  stm_->Munch(instr_list, fs);    // 先执行副作用语句
  return exp_->Munch(instr_list, fs);  // 再求值表达式部分
}
```

规范化后理论上不应出现 `EseqExp`，做防御性处理。

---

### `NameExp::Munch` — 标签名/符号地址（第 544–554 行）

```cpp
temp::Temp *NameExp::Munch(...) {
  temp::Temp *result = temp::TempFactory::NewTemp();
  // leaq label(%rip), %result   — 使用 RIP 相对寻址获取标签地址
  sprintf(buf, "leaq %s(%%rip), `d0", name_->Name().data());
  instr_list.Append(new assem::OperInstr(buf, ...));
  return result;
}
```

用于字符串常量地址、函数名等。

---

### `ConstExp::Munch` — 整数常量（第 556–566 行）

```cpp
temp::Temp *ConstExp::Munch(...) {
  temp::Temp *result = temp::TempFactory::NewTemp();
  // movq $const, %result   — 立即数加载
  sprintf(buf, "movq $%d, `d0", consti_);
  instr_list.Append(new assem::OperInstr(buf, ...));
  return result;
}
```

---

### `CallExp::Munch` — 函数调用（第 568–627 行）

这是整个文件中最复杂的方法，负责完整的函数调用序列：

```cpp
temp::Temp *CallExp::Munch(...) {
  // 1. 计算栈参数数量
  int nargs = args_->GetList().size();
  int stack_args = nargs > NUM_ARG_REGS ? nargs - NUM_ARG_REGS : 0;

  // 2. 为栈参数分配空间（subq $N, %rsp）
  if (stack_args > 0) {
    sprintf(buf0, "subq $%d, `d0", stack_args * WORD_SIZE);
    // 例：subq $24, %rsp  — 分配 3 个参数的栈空间
  }

  // 3. 处理所有参数（MunchArgs 生成 movq 到寄存器/栈的指令）
  temp::TempList *arg_temps = args_->MunchArgs(instr_list, fs);

  // 4. 收集 caller-saved 寄存器列表（call 会破坏这些寄存器）
  temp::TempList *caller_saves = reg_manager->CallerSaves();
  temp::TempList *src_list = new temp::TempList();
  for (auto t : caller_saves->GetList()) {
    src_list->Append(t);
  }

  // 5. 生成 callq 指令
  // callq func_name
  // dst: %rax（返回值）, src: 所有 caller-saved 寄存器（被破坏）
  sprintf(buf, "callq %s", func_name.data());
  instr_list.Append(new assem::OperInstr(
      buf,
      new temp::TempList({reg_manager->ReturnValue()}),  // 定义: %rax
      src_list,                                           // 使用: caller-saved
      nullptr));

  // 6. 将返回值从 %rax 移到新 temp
  // movq %rax, result
  temp::Temp *result = temp::TempFactory::NewTemp();
  instr_list.Append(new assem::MoveInstr(
      "movq `s0, `d0",
      new temp::TempList({result}),
      new temp::TempList({reg_manager->ReturnValue()})));

  // 7. 清理栈空间（addq $N, %rsp）
  if (stack_args > 0) {
    sprintf(buf2, "addq $%d, `d0", stack_args * WORD_SIZE);
    // 例：addq $24, %rsp
  }

  return result;
}
```

**完整调用序列**：`subq` → 参数加载 → `callq` → 取返回值 → `addq`

---

## 数据流总结

```
IR 树节点
    │
    │  Munch() 方法递归调用
    ▼
assem::InstrList（汇编指令列表）
    │
    │  每条指令中的占位符 (`s0, `d0, `j0)
    │  在后续的寄存器分配阶段（Lab6）被替换为真实寄存器
    ▼
最终 x86-64 汇编代码 (.s 文件)
```

## 指令类说明

| 类 | 用途 | 示例 |
|----|------|------|
| `LabelInstr` | 标签定义 | `L0:` |
| `MoveInstr` | 寄存器间移动（对后续 liveness 分析重要） | `movq `s0, `d0` |
| `OperInstr` | 一般操作（运算、跳转等） | `addq`, `jmp`, `callq` |

`MoveInstr` 和 `OperInstr` 的区别在于：`MoveInstr` 的 `dst` 和 `src` 之间是移动关系（`dst ← src`），这对 liveness 分析（判断死代码）很重要——`MoveInstr` 中 `src` 和 `dst` 可以用同一个寄存器而不算"使用"。
