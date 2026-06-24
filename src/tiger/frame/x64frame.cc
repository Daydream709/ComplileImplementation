#include "tiger/frame/x64frame.h"

#include <array>
#include <set>
#include <sstream>

extern frame::RegManager *reg_manager;

namespace frame {

X64RegManager::X64RegManager() : RegManager() {
  const char *reg_names[REG_COUNT] = {
      "%rax", "%rcx", "%rdx", "%rsi", "%rdi", "%r8", "%r9",
      "%rbx", "%rbp", "%r10", "%r11", "%r12", "%r13", "%r14", "%r15", "%rsp",
      "%rbp"};
  for (int i = 0; i < REG_COUNT; i++) {
    regs_.push_back(temp::TempFactory::NewTemp());
    temp_map_->Enter(regs_[i], new std::string(reg_names[i]));
  }
}

temp::TempList *X64RegManager::Registers() {
  const std::array<X64Reg, 15> reg_array{
      REG_RAX, REG_RBX, REG_RCX, REG_RDX, REG_RSI, REG_RDI,
      REG_RBP, REG_R8,  REG_R9,  REG_R10, REG_R11, REG_R12, REG_R13, REG_R14, REG_R15,
  };
  auto *list = new temp::TempList();
  for (auto &reg : reg_array)
    list->Append(regs_[reg]);
  return list;
}

temp::TempList *X64RegManager::ArgRegs() {
  const std::array<X64Reg, 6> reg_array{REG_RDI, REG_RSI, REG_RDX, REG_RCX, REG_R8, REG_R9};
  auto *list = new temp::TempList();
  for (auto &reg : reg_array)
    list->Append(regs_[reg]);
  return list;
}

temp::TempList *X64RegManager::CallerSaves() {
  const std::array<X64Reg, 9> reg_array{
      REG_RAX, REG_RDI, REG_RSI, REG_RDX, REG_RCX, REG_R8, REG_R9, REG_R10, REG_R11};
  auto *list = new temp::TempList();
  for (auto &reg : reg_array)
    list->Append(regs_[reg]);
  return list;
}

temp::TempList *X64RegManager::CalleeSaves() {
  const std::array<X64Reg, 6> reg_array{REG_RBP, REG_RBX, REG_R12, REG_R13, REG_R14, REG_R15};
  auto *list = new temp::TempList();
  for (auto &reg : reg_array)
    list->Append(regs_[reg]);
  return list;
}

temp::TempList *X64RegManager::ReturnSink() {
  temp::TempList *list = CalleeSaves();
  list->Append(regs_[REG_RSP]);
  list->Append(regs_[REG_RAX]);
  return list;
}

int X64RegManager::WordSize() { return 8; }

temp::Temp *X64RegManager::FramePointer() { return regs_[REG_FP]; }

temp::Temp *X64RegManager::StackPointer() { return regs_[REG_RSP]; }

temp::Temp *X64RegManager::ReturnValue() { return regs_[REG_RAX]; }

class InFrameAccess : public Access {
public:
  int offset;
  explicit InFrameAccess(int offset) : offset(offset) {}
  tree::Exp *ToExp(tree::Exp *frame_ptr) const override {
    return new tree::MemExp(
        new tree::BinopExp(tree::PLUS_OP, frame_ptr, new tree::ConstExp(offset)));
  }
};

class InRegAccess : public Access {
public:
  temp::Temp *reg;
  explicit InRegAccess(temp::Temp *reg) : reg(reg) {}
  tree::Exp *ToExp(tree::Exp *framePtr) const override {
    return new tree::TempExp(reg);
  }
};

class X64Frame : public Frame {
public:
  tree::Stm *view_shift;

  X64Frame(temp::Label *name, std::list<frame::Access *> *formals)
      : Frame(8, 0, name, formals), view_shift(nullptr) {}

  [[nodiscard]] std::string GetLabel() const override { return name_->Name(); }
  [[nodiscard]] temp::Label *Name() const override { return name_; }
  [[nodiscard]] std::list<frame::Access *> *Formals() const override { return formals_; }
  frame::Access *AllocLocal(bool escape) override {
    frame::Access *access;
    if (escape) {
      local_offset_ -= word_size_;
      access = new InFrameAccess(local_offset_);
    } else {
      access = new InRegAccess(temp::TempFactory::NewTemp());
    }
    locals_->push_back(access);
    return access;
  }
  void AllocOutgoSpace(int size) override { outgoing_size_ = size; }
};

Frame *NewFrame(temp::Label *name, std::list<bool> formals) {
  auto *access_list = new std::list<Access *>();
  auto *frame = new X64Frame(name, access_list);

  // formals list = [arg1, arg2, ..., sl] (sl is at the end)
  // IR args = [sl, arg1, arg2, ...] (sl is first)
  // First 6 args (including sl) are passed in registers: rdi, rsi, rdx, rcx, r8, r9
  // Beyond 6, args are on stack (caller pushes them).

  int total_args = formals.size();
  int num_stack_args = total_args > 6 ? total_args - 6 : 0;
  if (num_stack_args > 0) {
    frame->AllocOutgoSpace(num_stack_args * 8);
  }

  // Allocate accesses in [arg1, arg2, ..., sl] order
  // For each arg, decide if it's a register arg (first 5: arg1..arg5) or stack arg (arg6+)
  // sl is always in IR args[0] (register rdi), not the stack
  int arg_idx = 0;  // 0 = arg1, 1 = arg2, ..., total_args-1 = sl
  int stack_arg_num = 0;
  bool is_last = false;
  int list_size = formals.size();
  for (bool escape : formals) {
    is_last = (arg_idx == list_size - 1);
    // IR position: sl is at IR args[0], arg1 is at IR args[1], etc.
    // For arg_idx (in formals list), the IR position is:
    //   - if is_last (sl): IR position 0
    //   - else: IR position arg_idx + 1
    int ir_position = is_last ? 0 : (arg_idx + 1);
    Access *access;
    if (is_last) {
      // sl is always a register arg
      access = frame->AllocLocal(true);
    } else if (ir_position >= 6) {
      // Stack argument: InFrameAccess with positive offset relative to callee's rbp
      int offset = 16 + stack_arg_num * 8;
      access = new InFrameAccess(offset);
      stack_arg_num++;
    } else if (escape) {
      // Register arg, escape=true: InFrameAccess in callee frame
      access = frame->AllocLocal(true);
    } else {
      // Register arg, escape=false: InRegAccess
      access = frame->AllocLocal(false);
    }
    access_list->push_back(access);
    arg_idx++;
  }

  // Build view_shift
  // IR args order: [sl, arg1, arg2, ...]
  // rdi = sl, rsi = arg1, rdx = arg2, ..., r9 = arg5, then stack
  temp::TempList *arg_regs = reg_manager->ArgRegs();
  auto arg_it = arg_regs->GetList().begin();
  tree::Stm *shift = nullptr;
  int ir_pos = 0;  // 0 = sl, 1 = arg1, 2 = arg2, ...
  int stack_arg_idx = 0;

  // Process sl first (always in register)
  if (!formals.empty() && arg_it != arg_regs->GetList().end()) {
    Access *sl = access_list->back();
    temp::Temp *sl_actual = *arg_it++;
    tree::Stm *sl_move = new tree::MoveStm(
        sl->ToExp(new tree::TempExp(reg_manager->FramePointer())),
        new tree::TempExp(sl_actual));
    shift = sl_move;
    ir_pos = 1;
  }

  // Process arg1, arg2, ...
  auto acc_it = access_list->begin();
  for (bool escape : formals) {
    if (acc_it == access_list->end()) break;
    Access *access = *acc_it++;
    // Skip sl (last element)
    if (acc_it == access_list->end()) break;

    if (ir_pos < 6 && arg_it != arg_regs->GetList().end()) {
      temp::Temp *actual = *arg_it++;
      tree::Stm *move = new tree::MoveStm(
          access->ToExp(new tree::TempExp(reg_manager->FramePointer())),
          new tree::TempExp(actual));
      shift = shift ? new tree::SeqStm(shift, move) : move;
    }
    // Stack args: no view_shift needed (caller pushes, callee reads via InFrameAccess)
    ir_pos++;
  }

  frame->view_shift = shift;
  return frame;
}

tree::Exp *ExternalCall(std::string_view s, tree::ExpList *args) {
  static const std::set<std::string> runtime_funcs = {
    "init_array", "alloc_record", "string_equal", "print",
    "printi", "flush", "ord", "chr", "size", "substring",
    "concat", "not", "getchar"
  };

  std::string func_name(s);
  if (runtime_funcs.find(func_name) == runtime_funcs.end()) {
    args->Insert(new tree::NameExp(temp::LabelFactory::NamedLabel("staticLink")));
  }
  return new tree::CallExp(new tree::NameExp(temp::LabelFactory::NamedLabel(s)), args);
}

tree::Stm *ProcEntryExit1(Frame *frame, tree::Stm *stm) {
  auto *x64 = static_cast<X64Frame *>(frame);
  if (!x64->view_shift)
    return stm;
  return new tree::SeqStm(x64->view_shift, stm);
}

assem::Proc *ProcEntryExit3(Frame *frame, assem::InstrList *body) {
  const std::array<X64Reg, 5> saved_regs{
      X64Reg::REG_RBX, X64Reg::REG_R12, X64Reg::REG_R13,
      X64Reg::REG_R14, X64Reg::REG_R15};
  int save_area_size = saved_regs.size() * frame->word_size_;
  int locals_size = -frame->local_offset_;
  if (locals_size < 0) locals_size = 0;
  int frame_size = locals_size + frame->outgoing_size_ + save_area_size + 8;
  int remainder = frame_size % 16;
  if (remainder <= 8)
    frame_size += 8 - remainder;
  else
    frame_size += 24 - remainder;
  std::string label = frame->GetLabel();
  std::string fs_label = label + "_framesize";

  std::ostringstream prolog;
  prolog << ".set " << fs_label << ", " << frame_size << "\n";
  prolog << label << ":\n";
  prolog << "pushq %rbp\n";
  prolog << "movq %rsp, %rbp\n";
  prolog << "subq $" << (frame_size - 8) << ", %rsp\n";
  for (std::size_t i = 0; i < saved_regs.size(); ++i) {
    int offset = -(int)(locals_size + save_area_size) + i * (int)frame->word_size_;
    prolog << "movq " << *reg_manager->temp_map_->Look(reg_manager->GetRegister(saved_regs[i]))
           << ", " << offset << "(%rbp)\n";
  }

  std::ostringstream epilog;
  for (std::size_t i = 0; i < saved_regs.size(); ++i) {
    int offset = -(int)(locals_size + save_area_size) + i * (int)frame->word_size_;
    epilog << "movq " << offset << "(%rbp), "
           << *reg_manager->temp_map_->Look(reg_manager->GetRegister(saved_regs[i]))
           << "\n";
  }
  epilog << "leave\n";
  epilog << "retq\n";

  return new assem::Proc(prolog.str(), body, epilog.str());
}

assem::Proc *BuildCompleteProcedure(Frame *frame, assem::InstrList *body) {
  return ProcEntryExit3(frame, body);
}

} // namespace frame
