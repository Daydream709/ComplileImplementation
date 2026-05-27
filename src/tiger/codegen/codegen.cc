#include "tiger/codegen/codegen.h"

#include <cassert>
#include <sstream>

extern frame::RegManager *reg_manager;

namespace {

constexpr int maxlen = 1024;
constexpr int WORD_SIZE = 8;
constexpr int NUM_ARG_REGS = 6;

} // namespace

namespace cg {

void CodeGen::Codegen() {
  auto *instr_list = new assem::InstrList();
  tree::StmList *stm_list = traces_->GetStmList();

  for (auto *stm : stm_list->GetList()) {
    stm->Munch(*instr_list, fs_);
  }

  assem_instr_ = std::make_unique<AssemInstr>(instr_list);
}

void AssemInstr::Print(FILE *out, temp::Map *map) const {
  for (auto instr : instr_list_->GetList())
    instr->Print(out, map);
  fprintf(out, "\n");
}
} // namespace cg

namespace tree {

/**
 * Generate code for passing arguments
 * @param args argument list
 * @param instr_list instruction holder
 * @return temp list to hold arguments
 */
temp::TempList *ExpList::MunchArgs(assem::InstrList &instr_list, std::string_view fs) {
  temp::TempList *arg_temps = new temp::TempList();
  temp::TempList *arg_regs = reg_manager->ArgRegs();

  auto reg_it = arg_regs->GetList().begin();
  int i = 0;

  for (auto *arg : exp_list_) {
    temp::Temp *arg_temp = arg->Munch(instr_list, fs);

    if (i < NUM_ARG_REGS) {
      // Move to argument register
      instr_list.Append(new assem::OperInstr(
          "movq `s0, `d0",
          new temp::TempList({*reg_it}),
          new temp::TempList({arg_temp}),
          nullptr));
      arg_temps->Append(*reg_it);
      ++reg_it;
    } else {
      // Push onto stack (args beyond 6th)
      instr_list.Append(new assem::OperInstr(
          "pushq `s0",
          new temp::TempList({reg_manager->StackPointer()}),
          new temp::TempList({arg_temp, reg_manager->StackPointer()}),
          nullptr));
      arg_temps->Append(arg_temp);
    }
    i++;
  }

  return arg_temps;
}

void SeqStm::Munch(assem::InstrList &instr_list, std::string_view fs) {
  // SeqStm should not exist after canonicalization, but handle gracefully
  left_->Munch(instr_list, fs);
  right_->Munch(instr_list, fs);
}

void LabelStm::Munch(assem::InstrList &instr_list, std::string_view fs) {
  instr_list.Append(new assem::LabelInstr(
      label_->Name(), label_));
}

void JumpStm::Munch(assem::InstrList &instr_list, std::string_view fs) {
  temp::Label *target = exp_->name_;
  instr_list.Append(new assem::OperInstr(
      "jmp `j0",
      nullptr,
      nullptr,
      new assem::Targets(new std::vector<temp::Label *>({target}))));
}

void CjumpStm::Munch(assem::InstrList &instr_list, std::string_view fs) {
  temp::Temp *left = left_->Munch(instr_list, fs);
  temp::Temp *right = right_->Munch(instr_list, fs);

  instr_list.Append(new assem::OperInstr(
      "cmpq `s1, `s0",
      nullptr,
      new temp::TempList({left, right}),
      nullptr));

  std::string jmp_op;
  switch (op_) {
  case EQ_OP:  jmp_op = "je"; break;
  case NE_OP:  jmp_op = "jne"; break;
  case LT_OP:  jmp_op = "jl"; break;
  case GT_OP:  jmp_op = "jg"; break;
  case LE_OP:  jmp_op = "jle"; break;
  case GE_OP:  jmp_op = "jge"; break;
  default: assert(0); break;
  }

  instr_list.Append(new assem::OperInstr(
      jmp_op + " `j0",
      nullptr,
      nullptr,
      new assem::Targets(new std::vector<temp::Label *>({true_label_}))));
  instr_list.Append(new assem::OperInstr(
      "jmp `j0",
      nullptr,
      nullptr,
      new assem::Targets(new std::vector<temp::Label *>({false_label_}))));
}

void MoveStm::Munch(assem::InstrList &instr_list, std::string_view fs) {
  // MOVE(MEM(e1), e2) - store
  if (typeid(*dst_) == typeid(MemExp)) {
    auto *mem = static_cast<MemExp *>(dst_);
    temp::Temp *src = src_->Munch(instr_list, fs);

    // MOVE(MEM(BINOP(PLUS, e1, CONST(n))), e2) - store with offset
    if (typeid(*mem->exp_) == typeid(BinopExp)) {
      auto *binop = static_cast<BinopExp *>(mem->exp_);
      if (binop->op_ == PLUS_OP && typeid(*binop->right_) == typeid(ConstExp)) {
        int offset = static_cast<ConstExp *>(binop->right_)->consti_;
        temp::Temp *base = binop->left_->Munch(instr_list, fs);
        char buf[maxlen];
        sprintf(buf, "movq `s0, %d(`s1)", offset);
        instr_list.Append(new assem::OperInstr(
            buf,
            nullptr,
            new temp::TempList({src, base}),
            nullptr));
        return;
      }
      // BINOP(PLUS, CONST(n), e1)
      if (binop->op_ == PLUS_OP && typeid(*binop->left_) == typeid(ConstExp)) {
        int offset = static_cast<ConstExp *>(binop->left_)->consti_;
        temp::Temp *base = binop->right_->Munch(instr_list, fs);
        char buf[maxlen];
        sprintf(buf, "movq `s0, %d(`s1)", offset);
        instr_list.Append(new assem::OperInstr(
            buf,
            nullptr,
            new temp::TempList({src, base}),
            nullptr));
        return;
      }
    }

    temp::Temp *dst_addr = mem->exp_->Munch(instr_list, fs);
    instr_list.Append(new assem::OperInstr(
        "movq `s0, (`s1)",
        nullptr,
        new temp::TempList({src, dst_addr}),
        nullptr));
    return;
  }

  // MOVE(TEMP(t1), e2)
  if (typeid(*dst_) == typeid(TempExp)) {
    auto *temp_exp = static_cast<TempExp *>(dst_);
    temp::Temp *dst_temp = temp_exp->temp_;

    // MOVE(TEMP(t), MEM(e)) - load
    if (typeid(*src_) == typeid(MemExp)) {
      auto *mem = static_cast<MemExp *>(src_);

      // MOVE(TEMP(t), MEM(BINOP(PLUS, e, CONST(n))))
      if (typeid(*mem->exp_) == typeid(BinopExp)) {
        auto *binop = static_cast<BinopExp *>(mem->exp_);
        if (binop->op_ == PLUS_OP && typeid(*binop->right_) == typeid(ConstExp)) {
          int offset = static_cast<ConstExp *>(binop->right_)->consti_;
          temp::Temp *base = binop->left_->Munch(instr_list, fs);
          char buf[maxlen];
          sprintf(buf, "movq %d(`s0), `d0", offset);
          instr_list.Append(new assem::MoveInstr(
              buf,
              new temp::TempList({dst_temp}),
              new temp::TempList({base})));
          return;
        }
        if (binop->op_ == PLUS_OP && typeid(*binop->left_) == typeid(ConstExp)) {
          int offset = static_cast<ConstExp *>(binop->left_)->consti_;
          temp::Temp *base = binop->right_->Munch(instr_list, fs);
          char buf[maxlen];
          sprintf(buf, "movq %d(`s0), `d0", offset);
          instr_list.Append(new assem::MoveInstr(
              buf,
              new temp::TempList({dst_temp}),
              new temp::TempList({base})));
          return;
        }
      }

      temp::Temp *src_addr = mem->exp_->Munch(instr_list, fs);
      instr_list.Append(new assem::MoveInstr(
          "movq (`s0), `d0",
          new temp::TempList({dst_temp}),
          new temp::TempList({src_addr})));
      return;
    }

    // MOVE(TEMP(t), e) - general case
    temp::Temp *src_temp = src_->Munch(instr_list, fs);
    instr_list.Append(new assem::MoveInstr(
        "movq `s0, `d0",
        new temp::TempList({dst_temp}),
        new temp::TempList({src_temp})));
    return;
  }

  // Fallback: general move
  temp::Temp *dst_temp = dst_->Munch(instr_list, fs);
  temp::Temp *src_temp = src_->Munch(instr_list, fs);
  instr_list.Append(new assem::MoveInstr(
      "movq `s0, `d0",
      new temp::TempList({dst_temp}),
      new temp::TempList({src_temp})));
}

void ExpStm::Munch(assem::InstrList &instr_list, std::string_view fs) {
  exp_->Munch(instr_list, fs);
}

temp::Temp *BinopExp::Munch(assem::InstrList &instr_list, std::string_view fs) {
  temp::Temp *result = temp::TempFactory::NewTemp();

  switch (op_) {
  case PLUS_OP: {
    // Handle LEA for BINOP(PLUS, e1, CONST(n))
    if (typeid(*right_) == typeid(ConstExp)) {
      int c = static_cast<ConstExp *>(right_)->consti_;
      temp::Temp *left = left_->Munch(instr_list, fs);
      if (c == 0) {
        instr_list.Append(new assem::MoveInstr(
            "movq `s0, `d0",
            new temp::TempList({result}),
            new temp::TempList({left})));
      } else {
        char buf[maxlen];
        sprintf(buf, "leaq %d(`s0), `d0", c);
        instr_list.Append(new assem::OperInstr(
            buf,
            new temp::TempList({result}),
            new temp::TempList({left}),
            nullptr));
      }
      return result;
    }
    if (typeid(*left_) == typeid(ConstExp)) {
      int c = static_cast<ConstExp *>(left_)->consti_;
      temp::Temp *right_t = right_->Munch(instr_list, fs);
      if (c == 0) {
        instr_list.Append(new assem::MoveInstr(
            "movq `s0, `d0",
            new temp::TempList({result}),
            new temp::TempList({right_t})));
      } else {
        char buf[maxlen];
        sprintf(buf, "leaq %d(`s0), `d0", c);
        instr_list.Append(new assem::OperInstr(
            buf,
            new temp::TempList({result}),
            new temp::TempList({right_t}),
            nullptr));
      }
      return result;
    }

    temp::Temp *left = left_->Munch(instr_list, fs);
    temp::Temp *right = right_->Munch(instr_list, fs);
    instr_list.Append(new assem::MoveInstr(
        "movq `s0, `d0",
        new temp::TempList({result}),
        new temp::TempList({left})));
    instr_list.Append(new assem::OperInstr(
        "addq `s0, `d0",
        new temp::TempList({result}),
        new temp::TempList({right, result}),
        nullptr));
    return result;
  }

  case MINUS_OP: {
    temp::Temp *left = left_->Munch(instr_list, fs);
    temp::Temp *right = right_->Munch(instr_list, fs);
    instr_list.Append(new assem::MoveInstr(
        "movq `s0, `d0",
        new temp::TempList({result}),
        new temp::TempList({left})));
    instr_list.Append(new assem::OperInstr(
        "subq `s0, `d0",
        new temp::TempList({result}),
        new temp::TempList({right, result}),
        nullptr));
    return result;
  }

  case MUL_OP: {
    temp::Temp *left = left_->Munch(instr_list, fs);
    temp::Temp *right = right_->Munch(instr_list, fs);
    temp::Temp *rax = reg_manager->GetRegister(frame::REG_RAX);
    // Use 1-operand imulq: %rax *= src, result in %rax
    instr_list.Append(new assem::MoveInstr(
        "movq `s0, `d0",
        new temp::TempList({rax}),
        new temp::TempList({left})));
    instr_list.Append(new assem::OperInstr(
        "imulq `s0",
        new temp::TempList({rax}),
        new temp::TempList({right, rax}),
        nullptr));
    instr_list.Append(new assem::MoveInstr(
        "movq `s0, `d0",
        new temp::TempList({result}),
        new temp::TempList({rax})));
    return result;
  }

  case DIV_OP: {
    temp::Temp *left = left_->Munch(instr_list, fs);
    temp::Temp *right = right_->Munch(instr_list, fs);
    temp::Temp *rax = reg_manager->GetRegister(frame::REG_RAX);
    temp::Temp *rdx = reg_manager->GetRegister(frame::REG_RDX);

    instr_list.Append(new assem::MoveInstr(
        "movq `s0, `d0",
        new temp::TempList({rax}),
        new temp::TempList({left})));
    instr_list.Append(new assem::OperInstr(
        "cqto",
        new temp::TempList({rdx}),
        new temp::TempList({rax}),
        nullptr));
    instr_list.Append(new assem::OperInstr(
        "idivq `s0",
        new temp::TempList({rax}),
        new temp::TempList({right, rax, rdx}),
        nullptr));
    instr_list.Append(new assem::MoveInstr(
        "movq `s0, `d0",
        new temp::TempList({result}),
        new temp::TempList({rax})));
    return result;
  }

  case XOR_OP: {
    temp::Temp *left = left_->Munch(instr_list, fs);
    temp::Temp *right = right_->Munch(instr_list, fs);
    instr_list.Append(new assem::MoveInstr(
        "movq `s0, `d0",
        new temp::TempList({result}),
        new temp::TempList({left})));
    // XOR with immediate 1 for boolean negation
    if (typeid(*right_) == typeid(ConstExp)) {
      int c = static_cast<ConstExp *>(right_)->consti_;
      char buf[maxlen];
      sprintf(buf, "xorq $%d, `d0", c);
      instr_list.Append(new assem::OperInstr(
          buf,
          new temp::TempList({result}),
          new temp::TempList({result}),
          nullptr));
    } else {
      instr_list.Append(new assem::OperInstr(
          "xorq `s0, `d0",
          new temp::TempList({result}),
          new temp::TempList({right, result}),
          nullptr));
    }
    return result;
  }

  default:
    // For other ops (AND, OR, LSHIFT, RSHIFT, ARSHIFT)
    // Handle left shift and right shift
    if (op_ == LSHIFT_OP) {
      temp::Temp *left = left_->Munch(instr_list, fs);
      temp::Temp *right = right_->Munch(instr_list, fs);
      // Shift amount must be in RCX
      temp::Temp *rcx = reg_manager->GetRegister(frame::REG_RCX);
      instr_list.Append(new assem::MoveInstr(
          "movq `s0, `d0",
          new temp::TempList({result}),
          new temp::TempList({left})));
      instr_list.Append(new assem::MoveInstr(
          "movq `s0, `d0",
          new temp::TempList({rcx}),
          new temp::TempList({right})));
      // Use constant shift if possible
      if (typeid(*right_) == typeid(ConstExp)) {
        int c = static_cast<ConstExp *>(right_)->consti_;
        char buf[maxlen];
        sprintf(buf, "shlq $%d, `d0", c);
        instr_list.Append(new assem::OperInstr(
            buf,
            new temp::TempList({result}),
            new temp::TempList({result}),
            nullptr));
      } else {
        instr_list.Append(new assem::OperInstr(
            "shlq `s0, `d0",
            new temp::TempList({result}),
            new temp::TempList({rcx, result}),
            nullptr));
      }
      return result;
    }
    if (op_ == RSHIFT_OP) {
      temp::Temp *left = left_->Munch(instr_list, fs);
      temp::Temp *right = right_->Munch(instr_list, fs);
      temp::Temp *rcx = reg_manager->GetRegister(frame::REG_RCX);
      instr_list.Append(new assem::MoveInstr(
          "movq `s0, `d0",
          new temp::TempList({result}),
          new temp::TempList({left})));
      instr_list.Append(new assem::MoveInstr(
          "movq `s0, `d0",
          new temp::TempList({rcx}),
          new temp::TempList({right})));
      instr_list.Append(new assem::OperInstr(
          "shrq `s0, `d0",
          new temp::TempList({result}),
          new temp::TempList({rcx, result}),
          nullptr));
      return result;
    }

    // Default: load left, apply op with right
    temp::Temp *left = left_->Munch(instr_list, fs);
    temp::Temp *right = right_->Munch(instr_list, fs);
    instr_list.Append(new assem::MoveInstr(
        "movq `s0, `d0",
        new temp::TempList({result}),
        new temp::TempList({left})));
    instr_list.Append(new assem::OperInstr(
        "addq `s0, `d0",
        new temp::TempList({result}),
        new temp::TempList({right, result}),
        nullptr));
    return result;
  }
}

temp::Temp *MemExp::Munch(assem::InstrList &instr_list, std::string_view fs) {
  temp::Temp *result = temp::TempFactory::NewTemp();

  // MEM(BINOP(PLUS, e, CONST(n))) - memory access with offset
  if (typeid(*exp_) == typeid(BinopExp)) {
    auto *binop = static_cast<BinopExp *>(exp_);
    if (binop->op_ == PLUS_OP && typeid(*binop->right_) == typeid(ConstExp)) {
      int offset = static_cast<ConstExp *>(binop->right_)->consti_;
      temp::Temp *base = binop->left_->Munch(instr_list, fs);
      char buf[maxlen];
      sprintf(buf, "movq %d(`s0), `d0", offset);
      instr_list.Append(new assem::MoveInstr(
          buf,
          new temp::TempList({result}),
          new temp::TempList({base})));
      return result;
    }
    if (binop->op_ == PLUS_OP && typeid(*binop->left_) == typeid(ConstExp)) {
      int offset = static_cast<ConstExp *>(binop->left_)->consti_;
      temp::Temp *base = binop->right_->Munch(instr_list, fs);
      char buf[maxlen];
      sprintf(buf, "movq %d(`s0), `d0", offset);
      instr_list.Append(new assem::MoveInstr(
          buf,
          new temp::TempList({result}),
          new temp::TempList({base})));
      return result;
    }
    // BINOP(PLUS, e1, e2) -> lea
    if (binop->op_ == PLUS_OP) {
      temp::Temp *left = binop->left_->Munch(instr_list, fs);
      temp::Temp *right = binop->right_->Munch(instr_list, fs);
      // Compute address first
      char buf[maxlen];
      sprintf(buf, "leaq (`s0,`s1,1), `d0");
      // Actually, let's just add them and load
      instr_list.Append(new assem::MoveInstr(
          "movq `s0, `d0",
          new temp::TempList({result}),
          new temp::TempList({left})));
      instr_list.Append(new assem::OperInstr(
          "addq `s0, `d0",
          new temp::TempList({result}),
          new temp::TempList({right, result}),
          nullptr));
      instr_list.Append(new assem::MoveInstr(
          "movq (`s0), `d0",
          new temp::TempList({result}),
          new temp::TempList({result})));
      return result;
    }
  }

  // MEM(CONST(n)) - absolute memory address (rare)
  if (typeid(*exp_) == typeid(ConstExp)) {
    int c = static_cast<ConstExp *>(exp_)->consti_;
    // Load address 0 + offset: use RBP as base with offset
    // Fallback: treat as general case
  }

  // General case: MEM(e)
  temp::Temp *addr = exp_->Munch(instr_list, fs);
  instr_list.Append(new assem::MoveInstr(
      "movq (`s0), `d0",
      new temp::TempList({result}),
      new temp::TempList({addr})));
  return result;
}

temp::Temp *TempExp::Munch(assem::InstrList &instr_list, std::string_view fs) {
  return temp_;
}

temp::Temp *EseqExp::Munch(assem::InstrList &instr_list, std::string_view fs) {
  // EseqExp should not exist after canonicalization
  stm_->Munch(instr_list, fs);
  return exp_->Munch(instr_list, fs);
}

temp::Temp *NameExp::Munch(assem::InstrList &instr_list, std::string_view fs) {
  temp::Temp *result = temp::TempFactory::NewTemp();
  char buf[maxlen];
  sprintf(buf, "leaq %s(%%rip), `d0", name_->Name().data());
  instr_list.Append(new assem::OperInstr(
      buf,
      new temp::TempList({result}),
      nullptr,
      nullptr));
  return result;
}

temp::Temp *ConstExp::Munch(assem::InstrList &instr_list, std::string_view fs) {
  temp::Temp *result = temp::TempFactory::NewTemp();
  char buf[maxlen];
  sprintf(buf, "movq $%d, `d0", consti_);
  instr_list.Append(new assem::OperInstr(
      buf,
      new temp::TempList({result}),
      nullptr,
      nullptr));
  return result;
}

temp::Temp *CallExp::Munch(assem::InstrList &instr_list, std::string_view fs) {
  // Munch args first
  temp::TempList *arg_temps = args_->MunchArgs(instr_list, fs);

  // Count stack args (beyond 6th)
  int nargs = args_->GetList().size();
  int stack_args = nargs > NUM_ARG_REGS ? nargs - NUM_ARG_REGS : 0;

  // Collect caller-saved registers (they will be clobbered)
  temp::TempList *caller_saves = reg_manager->CallerSaves();
  temp::TempList *src_list = new temp::TempList();
  for (auto t : caller_saves->GetList()) {
    src_list->Append(t);
  }

  // Generate the call
  // The function name should be from NameExp
  std::string func_name;
  if (typeid(*fun_) == typeid(NameExp)) {
    func_name = static_cast<NameExp *>(fun_)->name_->Name();
  }

  char buf[maxlen];
  sprintf(buf, "callq %s", func_name.data());
  instr_list.Append(new assem::OperInstr(
      buf,
      new temp::TempList({reg_manager->ReturnValue()}),
      src_list,
      nullptr));

  // Move return value to a new temp
  temp::Temp *result = temp::TempFactory::NewTemp();
  instr_list.Append(new assem::MoveInstr(
      "movq `s0, `d0",
      new temp::TempList({result}),
      new temp::TempList({reg_manager->ReturnValue()})));

  // Clean up stack if we pushed extra args
  if (stack_args > 0) {
    char buf2[maxlen];
    sprintf(buf2, "addq $%d, `d0", stack_args * WORD_SIZE);
    instr_list.Append(new assem::OperInstr(
        buf2,
        new temp::TempList({reg_manager->StackPointer()}),
        new temp::TempList({reg_manager->StackPointer()}),
        nullptr));
  }

  return result;
}

} // namespace tree
