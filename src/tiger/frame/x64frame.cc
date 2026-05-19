#include "tiger/frame/x64frame.h"

extern frame::RegManager *reg_manager;

namespace frame {

X64RegManager::X64RegManager() : RegManager() {
  for (int i = 0; i < REG_COUNT; i++)
    regs_.push_back(temp::TempFactory::NewTemp());

  // Note: no frame pointer in tiger compiler
  std::array<std::string_view, REG_COUNT> reg_name{
      "%rax", "%rbx", "%rcx", "%rdx", "%rsi", "%rdi", "%rbp", "%rsp",
      "%r8",  "%r9",  "%r10", "%r11", "%r12", "%r13", "%r14", "%r15"};
  int reg = RAX;
  for (auto &name : reg_name) {
    temp_map_->Enter(regs_[reg], new std::string(name));
    reg++;
  }
}

temp::TempList *X64RegManager::Registers() {
  const std::array reg_array{
      RAX, RBX, RCX, RDX, RSI, RDI, RBP, R8, R9, R10, R11, R12, R13, R14, R15,
  };
  auto *temp_list = new temp::TempList();
  for (auto &reg : reg_array)
    temp_list->Append(regs_[reg]);
  return temp_list;
}

temp::TempList *X64RegManager::ArgRegs() {
  const std::array reg_array{RDI, RSI, RDX, RCX, R8, R9};
  auto *temp_list = new temp::TempList();
  ;
  for (auto &reg : reg_array)
    temp_list->Append(regs_[reg]);
  return temp_list;
}

temp::TempList *X64RegManager::CallerSaves() {
  std::array reg_array{RAX, RDI, RSI, RDX, RCX, R8, R9, R10, R11};
  auto *temp_list = new temp::TempList();
  ;
  for (auto &reg : reg_array)
    temp_list->Append(regs_[reg]);
  return temp_list;
}

temp::TempList *X64RegManager::CalleeSaves() {
  std::array reg_array{RBP, RBX, R12, R13, R14, R15};
  auto *temp_list = new temp::TempList();
  ;
  for (auto &reg : reg_array)
    temp_list->Append(regs_[reg]);
  return temp_list;
}

temp::TempList *X64RegManager::ReturnSink() {
  temp::TempList *temp_list = CalleeSaves();
  temp_list->Append(regs_[SP]);
  temp_list->Append(regs_[RV]);
  return temp_list;
}

int X64RegManager::WordSize() { return 8; }

temp::Temp *X64RegManager::FramePointer() { return regs_[FP]; }

temp::Temp *X64RegManager::StackPointer() { return regs_[SP]; }

temp::Temp *X64RegManager::ReturnValue() { return regs_[RV]; }

class InFrameAccess : public Access {
public:
  int offset;

  explicit InFrameAccess(int offset) : offset(offset) {}
  /* TODO: Put your lab5 code here */
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
  /* TODO: Put your lab5 code here */
  tree::Exp *ToExp(tree::Exp *framePtr) const override {
    return new tree::TempExp(reg);
  }
};

class X64Frame : public Frame {
  /* TODO: Put your lab5 code here */
public:
  tree::Stm *view_shift;

  X64Frame(temp::Label *name, std::list<frame::Access *> *formals)
      : Frame(8, 0, name, formals), view_shift(nullptr) {}

  [[nodiscard]] std::string GetLabel() const override { return name_->Name(); }
  [[nodiscard]] temp::Label *Name() const override { return name_; }
  [[nodiscard]] std::list<frame::Access *> *Formals() const override {
    return formals_;
  }
  frame::Access *AllocLocal(bool escape) override {
    /* TODO: Put your lab5 code here */
    if (escape) {
      offset_ -= word_size_;
      return new InFrameAccess(offset_);
    } else {
      return new InRegAccess(temp::TempFactory::NewTemp());
    }
  }
  void AllocOutgoSpace(int size) override {
    /* TODO: Put your lab5 code here */
    if (size > -offset_) {
      offset_ = -size;
    }
  }
};

frame::Frame *NewFrame(temp::Label *name, std::list<bool> formals) {
  /* TODO: Put your lab5 code here */
  auto *access_list = new std::list<frame::Access *>();
  // formals = [user_formals..., static_link(true)]
  // The static link is the last element, pushed by Level::NewLevel
  X64Frame *frame = new X64Frame(name, access_list);

  // Allocate accesses for each formal parameter in order
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
  // At call time, args are passed as: [static_link, user_arg1, user_arg2, ...]
  // So registers: RDI=static_link, RSI=user_arg1, RDX=user_arg2, ...
  // But formals list order: [user_acc1, user_acc2, ..., sl_acc]
  // We need to map RDI->sl_acc(last), RSI->user_acc1(first), etc.
  tree::Exp *fp = new tree::TempExp(reg_manager->FramePointer());
  temp::TempList *arg_regs = reg_manager->ArgRegs();
  auto reg_it = arg_regs->GetList().begin();
  tree::Stm *shift = nullptr;

  // First register (RDI) maps to the last formal (static link)
  if (reg_it != arg_regs->GetList().end() && !access_list->empty()) {
    frame::Access *sl_acc = access_list->back();
    if (typeid(*sl_acc) == typeid(InFrameAccess)) {
      shift = new tree::MoveStm(sl_acc->ToExp(fp),
                                new tree::TempExp(*reg_it));
    }
    ++reg_it;
  }

  // Remaining registers map to user formals (all but last)
  auto acc_it = access_list->begin();
  auto acc_end = access_list->end();
  --acc_end; // skip static link
  while (acc_it != acc_end && reg_it != arg_regs->GetList().end()) {
    if (typeid(**acc_it) == typeid(InFrameAccess)) {
      tree::Stm *move = new tree::MoveStm(
          (*acc_it)->ToExp(fp), new tree::TempExp(*reg_it));
      if (!shift)
        shift = move;
      else
        shift = new tree::SeqStm(shift, move);
    }
    ++acc_it;
    ++reg_it;
  }

  frame->view_shift = shift;
  return frame;
}

tree::Exp *ExternalCall(std::string_view s, tree::ExpList *args) {
  // Prepend a magic exp at first arg, indicating do not pass static link on
  // stack
  args->Insert(new tree::NameExp(temp::LabelFactory::NamedLabel("staticLink")));
  return new tree::CallExp(new tree::NameExp(temp::LabelFactory::NamedLabel(s)),
                           args);
}

/**
 * Moving incoming formal parameters, the saving and restoring of callee-save
 * Registers
 * @param frame curruent frame
 * @param stm statements
 * @return statements with saving, restoring and view shift
 */
tree::Stm *ProcEntryExit1(frame::Frame *frame, tree::Stm *stm) {
  auto x64_frame = dynamic_cast<frame::X64Frame *>(frame);
  assert(x64_frame);

  auto callee_list = new tree::ExpList();

  // Save callee-saved register
  tree::Stm *save_stm = nullptr;
  temp::TempList *callees = reg_manager->CalleeSaves();
  for (auto callee : callees->GetList()) {
    temp::Temp *r = temp::TempFactory::NewTemp();
    if (!save_stm)
      save_stm =
          new tree::MoveStm(new tree::TempExp(r), new tree::TempExp(callee));
    else
      save_stm = new tree::SeqStm(
          save_stm,
          new tree::MoveStm(new tree::TempExp(r), new tree::TempExp(callee)));
    callee_list->Append(new tree::TempExp(r));
  }

  // Restore callee-saved register
  tree::Stm *restore_stm = nullptr;
  callees = reg_manager->CalleeSaves();
  auto callee_it = callee_list->GetList().begin();
  for (auto callee : callees->GetList()) {
    assert(callee_it != callee_list->GetList().end());
    if (!restore_stm)
      restore_stm = new tree::MoveStm(new tree::TempExp(callee), *callee_it++);
    else
      restore_stm = new tree::SeqStm(
          restore_stm,
          new tree::MoveStm(new tree::TempExp(callee), *callee_it++));
  }

  // Add view shift for arguments
  tree::Stm *exit_stm;
  if (x64_frame->view_shift == nullptr) {
    // Outermost frame and functions with no formals do not have formal access_
    // list and view shift
    exit_stm = new tree::SeqStm(save_stm, new tree::SeqStm(stm, restore_stm));
  } else
    exit_stm = new tree::SeqStm(
        save_stm, new tree::SeqStm(x64_frame->view_shift,
                                   new tree::SeqStm(stm, restore_stm)));
  return exit_stm;
}

/* End for lab5 code */

} // namespace frame
