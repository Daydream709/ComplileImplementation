#include "tiger/frame/x64frame.h"

extern frame::RegManager *reg_manager;

namespace frame {

X64RegManager::X64RegManager() : RegManager() {
    /* TODO: Put your lab5 code here */
}

temp::TempList *X64RegManager::Registers() {
    /* TODO: Put your lab5 code here */
}

temp::TempList *X64RegManager::ArgRegs() {
    /* TODO: Put your lab5 code here */
}

temp::TempList *X64RegManager::CallerSaves() {
    /* TODO: Put your lab5 code here */
}

temp::TempList *X64RegManager::CalleeSaves() {
    /* TODO: Put your lab5 code here */
}

temp::TempList *X64RegManager::ReturnSink() {
    /* TODO: Put your lab5 code here */
}

int X64RegManager::WordSize() { /* TODO: Put your lab5 code here */ }

temp::Temp *X64RegManager::FramePointer() { /* TODO: Put your lab5 code here */ }

temp::Temp *X64RegManager::StackPointer() { /* TODO: Put your lab5 code here */ }

temp::Temp *X64RegManager::ReturnValue() { /* TODO: Put your lab5 code here */ }

class InFrameAccess : public Access {
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

/* TODO: Put your lab5 code here */

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
    /* TODO: Put your lab5 code here */
}


/* End for lab5 code */

} // namespace frame
