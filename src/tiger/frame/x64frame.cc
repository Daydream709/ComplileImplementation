#include "tiger/frame/x64frame.h"

extern frame::RegManager *reg_manager;

namespace frame {

// x86-64 register names in System V ABI order
static const char *reg_names[] = {
    "%rax", "%rcx", "%rdx", "%rsi", "%rdi", "%r8", "%r9",
    "%rbx", "%rbp", "%r10", "%r11", "%r12", "%r13", "%r14", "%r15", "%rsp"};

X64RegManager::X64RegManager() : RegManager() {
  regs_.resize(REG_COUNT);
  for (int i = 0; i < REG_COUNT; i++) {
    regs_[i] = temp::TempFactory::NewTemp();
    temp_map_->Enter(regs_[i], new std::string(reg_names[i]));
  }
}

temp::TempList *X64RegManager::Registers() {
  return new temp::TempList(
      {regs_[REG_RAX], regs_[REG_RCX], regs_[REG_RDX], regs_[REG_RSI],
       regs_[REG_RDI], regs_[REG_R8], regs_[REG_R9], regs_[REG_RBX],
       regs_[REG_RBP], regs_[REG_R10], regs_[REG_R11], regs_[REG_R12],
       regs_[REG_R13], regs_[REG_R14], regs_[REG_R15]});
}

temp::TempList *X64RegManager::ArgRegs() {
  // First 6 argument registers: RDI, RSI, RDX, RCX, R8, R9
  return new temp::TempList({regs_[REG_RDI], regs_[REG_RSI], regs_[REG_RDX],
                             regs_[REG_RCX], regs_[REG_R8], regs_[REG_R9]});
}

temp::TempList *X64RegManager::CallerSaves() {
  return new temp::TempList(
      {regs_[REG_RAX], regs_[REG_RCX], regs_[REG_RDX], regs_[REG_RSI],
       regs_[REG_RDI], regs_[REG_R8], regs_[REG_R9], regs_[REG_R10],
       regs_[REG_R11]});
}

temp::TempList *X64RegManager::CalleeSaves() {
  return new temp::TempList({regs_[REG_RBX], regs_[REG_RBP], regs_[REG_R12],
                             regs_[REG_R13], regs_[REG_R14], regs_[REG_R15]});
}

temp::TempList *X64RegManager::ReturnSink() {
  return new temp::TempList(
      {regs_[REG_RAX], regs_[REG_RBX], regs_[REG_RBP], regs_[REG_R12],
       regs_[REG_R13], regs_[REG_R14], regs_[REG_R15]});
}

int X64RegManager::WordSize() { return 8; }

temp::Temp *X64RegManager::FramePointer() { return regs_[REG_RBP]; }

temp::Temp *X64RegManager::StackPointer() { return regs_[REG_RSP]; }

temp::Temp *X64RegManager::ReturnValue() { return regs_[REG_RAX]; }

class InFrameAccess : public Access {
public:
  int offset;
  explicit InFrameAccess(int offset) : offset(offset) {}

  tree::Exp *ToExp(tree::Exp *frame_ptr) const override {
    return new tree::MemExp(
        new tree::BinopExp(tree::PLUS_OP, frame_ptr,
                           new tree::ConstExp(offset)));
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

  [[nodiscard]] std::string GetLabel() const override {
    return name_->Name();
  }
  [[nodiscard]] temp::Label *Name() const override { return name_; }
  [[nodiscard]] std::list<frame::Access *> *Formals() const override {
    return formals_;
  }
  frame::Access *AllocLocal(bool escape) override {
    if (escape) {
      offset_ -= word_size_;
      return new InFrameAccess(offset_);
    } else {
      return new InRegAccess(temp::TempFactory::NewTemp());
    }
  }
  void AllocOutgoSpace(int size) override {
    if (size > -offset_) {
      offset_ = -size;
    }
  }
};

frame::Frame *NewFrame(temp::Label *name, std::list<bool> formals) {
  auto *access_list = new std::list<frame::Access *>();
  X64Frame *frame = new X64Frame(name, access_list);

  // Allocate accesses for each formal parameter
  // formals = [user_formals..., static_link(true)]
  for (bool esc : formals) {
    frame::Access *acc;
    if (esc) {
      frame->offset_ -= frame->word_size_;
      acc = new InFrameAccess(frame->offset_);
    } else {
      acc = new InRegAccess(temp::TempFactory::NewTemp());
    }
    access_list->push_back(acc);
  }

  // Build view_shift: move args from registers to their frame locations
  tree::Exp *fp = new tree::TempExp(reg_manager->FramePointer());
  temp::TempList *arg_regs = reg_manager->ArgRegs();
  auto reg_it = arg_regs->GetList().begin();

  tree::Stm *shift = nullptr;

  // First register (RDI) maps to the last formal (static link)
  if (reg_it != arg_regs->GetList().end() && !access_list->empty()) {
    frame::Access *sl_acc = access_list->back();
    tree::Stm *move = new tree::MoveStm(sl_acc->ToExp(fp),
                                         new tree::TempExp(*reg_it));
    shift = move;
    ++reg_it;
  }

  // Remaining registers map to user formals (all but last)
  auto acc_it = access_list->begin();
  auto acc_end = access_list->end();
  --acc_end; // skip static link
  while (acc_it != acc_end && reg_it != arg_regs->GetList().end()) {
    tree::Stm *move = new tree::MoveStm(
        (*acc_it)->ToExp(fp), new tree::TempExp(*reg_it));
    if (!shift)
      shift = move;
    else
      shift = new tree::SeqStm(shift, move);
    ++acc_it;
    ++reg_it;
  }

  frame->view_shift = shift;
  return frame;
}

tree::Exp *ExternalCall(std::string_view s, tree::ExpList *args) {
  return new tree::CallExp(new tree::NameExp(temp::LabelFactory::NamedLabel(s)),
                           args);
}

tree::Stm *ProcEntryExit1(Frame *frame, tree::Stm *stm) {
  X64Frame *x64frame = static_cast<X64Frame *>(frame);

  // Calculate frame size
  int frame_size = -x64frame->offset_;
  if (frame_size % 16 != 0) {
    frame_size += 16 - (frame_size % 16);
  }

  // Build the full sequence:
  // save callee-saved → set %rbp=%rsp → subq $framesize,%rsp → view_shift → body → addq $framesize,%rsp → restore callee-saved

  // Step 1: Save callee-saved registers into temporaries
  temp::TempList *callee_saves = reg_manager->CalleeSaves();
  tree::Stm *save_stm = nullptr;
  std::vector<std::pair<temp::Temp *, temp::Temp *>> save_pairs; // (save_temp, reg_temp)

  for (auto reg : callee_saves->GetList()) {
    temp::Temp *t = temp::TempFactory::NewTemp();
    save_pairs.push_back({t, reg});
    tree::Stm *move =
        new tree::MoveStm(new tree::TempExp(t), new tree::TempExp(reg));
    if (!save_stm)
      save_stm = move;
    else
      save_stm = new tree::SeqStm(save_stm, move);
  }

  // Step 2: Set %rbp = %rsp (establish frame pointer)
  tree::Stm *setup_fp = new tree::MoveStm(
      new tree::TempExp(reg_manager->FramePointer()),
      new tree::TempExp(reg_manager->StackPointer()));

  // Step 3: Allocate frame space: subq $framesize, %rsp
  tree::Stm *alloc_frame = nullptr;
  if (frame_size > 0) {
    alloc_frame = new tree::MoveStm(
        new tree::TempExp(reg_manager->StackPointer()),
        new tree::BinopExp(tree::MINUS_OP,
                           new tree::TempExp(reg_manager->StackPointer()),
                           new tree::ConstExp(frame_size)));
  }

  // Step 6 (at end): Deallocate frame space: addq $framesize, %rsp
  tree::Stm *dealloc_frame = nullptr;
  if (frame_size > 0) {
    dealloc_frame = new tree::MoveStm(
        new tree::TempExp(reg_manager->StackPointer()),
        new tree::BinopExp(tree::PLUS_OP,
                           new tree::TempExp(reg_manager->StackPointer()),
                           new tree::ConstExp(frame_size)));
  }

  // Step 7 (at end): Restore callee-saved registers from temporaries
  tree::Stm *restore_stm = nullptr;
  for (auto &pair : save_pairs) {
    tree::Stm *move =
        new tree::MoveStm(new tree::TempExp(pair.second),
                          new tree::TempExp(pair.first));
    if (!restore_stm)
      restore_stm = move;
    else
      restore_stm = new tree::SeqStm(restore_stm, move);
  }

  // Assemble the full sequence
  // Append teardown + restore after body
  if (dealloc_frame) {
    stm = new tree::SeqStm(stm, dealloc_frame);
  }
  if (restore_stm) {
    stm = new tree::SeqStm(stm, restore_stm);
  }

  // Prepend view shift
  if (x64frame->view_shift) {
    stm = new tree::SeqStm(x64frame->view_shift, stm);
  }

  // Prepend frame allocation
  if (alloc_frame) {
    stm = new tree::SeqStm(alloc_frame, stm);
  }

  // Prepend frame pointer setup
  stm = new tree::SeqStm(setup_fp, stm);

  // Prepend callee-save
  if (save_stm) {
    stm = new tree::SeqStm(save_stm, stm);
  }

  return stm;
}

assem::Proc *ProcEntryExit3(Frame *frame, assem::InstrList *body) {
  X64Frame *x64frame = static_cast<X64Frame *>(frame);

  // Calculate frame size
  int frame_size = -x64frame->offset_;
  if (frame_size % 16 != 0) {
    frame_size += 16 - (frame_size % 16);
  }

  std::string func_name = frame->GetLabel();

  // Prologue: label + .set framesize directive
  // The interpreter uses .set framesize_xxx, N for frame memory address calculation
  std::string prolog;
  prolog += func_name + ":\n";
  prolog += ".set framesize_" + func_name + ", " + std::to_string(frame_size) + "\n";

  // Epilogue: retq to return from function
  std::string epilog = "retq\n\n";

  return new assem::Proc(prolog, body, epilog);
}

/* End for lab5 code */

} // namespace frame
