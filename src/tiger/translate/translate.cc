#include "tiger/translate/translate.h"

#include <tiger/absyn/absyn.h>

#include "tiger/env/env.h"
#include "tiger/errormsg/errormsg.h"
#include "tiger/frame/x64frame.h"
#include "tiger/frame/temp.h"
#include "tiger/frame/frame.h"

extern frame::Frags *frags;
extern frame::RegManager *reg_manager;

namespace {
frame::ProcFrag *ProcEntryExit(tr::Level *level, tr::Exp *body);
} // namespace

namespace tr {

Access *Access::AllocLocal(Level *level, bool escape) {
  /* TODO: Put your lab5 code here */
  return new Access(level, level->frame_->AllocLocal(escape));
}

class Cx {
public:
  PatchList trues_;
  PatchList falses_;
  tree::Stm *stm_;

  Cx(PatchList trues, PatchList falses, tree::Stm *stm)
      : trues_(trues), falses_(falses), stm_(stm) {}
};

class Exp {
public:
  [[nodiscard]] virtual tree::Exp *UnEx() const = 0;
  [[nodiscard]] virtual tree::Stm *UnNx() const = 0;
  [[nodiscard]] virtual Cx UnCx(err::ErrorMsg *errormsg) const = 0;
};

class ExpAndTy {
public:
  tr::Exp *exp_;
  type::Ty *ty_;

  ExpAndTy(tr::Exp *exp, type::Ty *ty) : exp_(exp), ty_(ty) {}
};

class ExExp : public Exp {
public:
  tree::Exp *exp_;

  explicit ExExp(tree::Exp *exp) : exp_(exp) {}

  [[nodiscard]] tree::Exp *UnEx() const override {
    /* TODO: Put your lab5 code here */
    return exp_; }
  [[nodiscard]] tree::Stm *UnNx() const override {
    /* TODO: Put your lab5 code here */
    return new tree::ExpStm(exp_);
  }
  [[nodiscard]] Cx UnCx(err::ErrorMsg *errormsg) const override {
    /* TODO: Put your lab5 code here */
    tree::CjumpStm *cjump = new tree::CjumpStm(
        tree::NE_OP, exp_, new tree::ConstExp(0),
        temp::LabelFactory::NewLabel(), temp::LabelFactory::NewLabel());
    tr::PatchList trues = tr::PatchList({&cjump->true_label_});
    tr::PatchList falses = tr::PatchList({&cjump->false_label_});
    return Cx(trues, falses, cjump);
  }
};

class NxExp : public Exp {
public:
  tree::Stm *stm_;

  explicit NxExp(tree::Stm *stm) : stm_(stm) {}

  [[nodiscard]] tree::Exp *UnEx() const override {
    /* TODO: Put your lab5 code here */
    return new tree::EseqExp(stm_, new tree::ConstExp(0));
  }
  [[nodiscard]] tree::Stm *UnNx() const override {
    /* TODO: Put your lab5 code here */
    return stm_; }
  [[nodiscard]] Cx UnCx(err::ErrorMsg *errormsg) const override {
    /* TODO: Put your lab5 code here */
    tree::NameExp *name_exp = new tree::NameExp(temp::LabelFactory::NewLabel());
    auto *targets = new std::vector<temp::Label *>({name_exp->name_});
    tree::Stm *stm = new tree::SeqStm(stm_, new tree::JumpStm(name_exp, targets));
    tr::PatchList trues = tr::PatchList();
    tr::PatchList falses = tr::PatchList({&name_exp->name_});
    return Cx(trues, falses, stm);
  }
};

class CxExp : public Exp {
public:
  Cx cx_;

  CxExp(PatchList trues, PatchList falses, tree::Stm *stm)
      : cx_(trues, falses, stm) {}

  [[nodiscard]] tree::Exp *UnEx() const override {
    /* TODO: Put your lab5 code here */
    temp::Temp *r = temp::TempFactory::NewTemp();
    temp::Label *t = temp::LabelFactory::NewLabel();
    temp::Label *f = temp::LabelFactory::NewLabel();
    cx_.trues_.DoPatch(t);
    cx_.falses_.DoPatch(f);
    return new tree::EseqExp(
        new tree::MoveStm(new tree::TempExp(r), new tree::ConstExp(1)),
        new tree::EseqExp(
            cx_.stm_,
            new tree::EseqExp(
                new tree::LabelStm(f),
                new tree::EseqExp(
                    new tree::MoveStm(new tree::TempExp(r),
                                      new tree::ConstExp(0)),
                    new tree::EseqExp(new tree::LabelStm(t),
                                      new tree::TempExp(r))))));
  }
  [[nodiscard]] tree::Stm *UnNx() const override {
    /* TODO: Put your lab5 code here */
    temp::Label *dummy = temp::LabelFactory::NewLabel();
    cx_.trues_.DoPatch(dummy);
    cx_.falses_.DoPatch(dummy);
    return new tree::SeqStm(cx_.stm_, new tree::LabelStm(dummy));
  }
  [[nodiscard]] Cx UnCx(err::ErrorMsg *errormsg) const override {
    /* TODO: Put your lab5 code here */
    return cx_;
  }
};

static tree::Exp *StaticLink(tr::Level *current, tr::Level *target) {
  tree::Exp *fp = new tree::TempExp(reg_manager->FramePointer());
  while (current != target) {
    auto formals = current->frame_->Formals();
    assert(!formals->empty());
    frame::Access *sl_access = formals->back();
    fp = sl_access->ToExp(fp);
    current = current->parent_;
  }
  return fp;
}

void ProgTr::Translate() {
  /* TODO: Put your lab5 code here */
  FillBaseVEnv();
  FillBaseTEnv();
  tr::ExpAndTy *result = absyn_tree_->Translate(
      venv_.get(), tenv_.get(), main_level_.get(),
      temp::LabelFactory::NamedLabel("main"), errormsg_.get());
  if (result && result->exp_) {
    frags->PushBack(ProcEntryExit(main_level_.get(), result->exp_));
  }
}

} // namespace tr

namespace {

frame::ProcFrag *ProcEntryExit(tr::Level *level, tr::Exp *body) {
  /* TODO: Put your lab5 code here */
  tree::Stm *stm = frame::ProcEntryExit1(level->frame_, body->UnNx());
  return new frame::ProcFrag(stm, level->frame_);
}

} // namespace

namespace absyn {

tr::ExpAndTy *AbsynTree::Translate(env::VEnvPtr venv, env::TEnvPtr tenv,
                                   tr::Level *level, temp::Label *label,
                                   err::ErrorMsg *errormsg) const {
  /* TODO: Put your lab5 code here */
  return root_->Translate(venv, tenv, level, label, errormsg);
}

tr::ExpAndTy *SimpleVar::Translate(env::VEnvPtr venv, env::TEnvPtr tenv,
                                   tr::Level *level, temp::Label *label,
                                   err::ErrorMsg *errormsg) const {
  /* TODO: Put your lab5 code here */
  env::EnvEntry *entry = venv->Look(sym_);
  if (!entry || typeid(*entry) != typeid(env::VarEntry)) {
    errormsg->Error(pos_, "undefined variable %s", sym_->Name().data());
    return new tr::ExpAndTy(new tr::ExExp(new tree::ConstExp(0)),
                            type::IntTy::Instance());
  }
  env::VarEntry *var_entry = static_cast<env::VarEntry *>(entry);
  tr::Access *access = var_entry->access_;
  tree::Exp *fp = tr::StaticLink(level, access->level_);
  tree::Exp *exp = access->access_->ToExp(fp);
  return new tr::ExpAndTy(new tr::ExExp(exp), var_entry->ty_);
}

tr::ExpAndTy *FieldVar::Translate(env::VEnvPtr venv, env::TEnvPtr tenv,
                                  tr::Level *level, temp::Label *label,
                                  err::ErrorMsg *errormsg) const {
  tr::ExpAndTy *exp_ty =
    var_->Translate(venv, tenv, level, label, errormsg);
  tr::Exp *exp = exp_ty->exp_;
  type::Ty *ty = exp_ty->ty_->ActualTy();

  if (typeid(*ty) != typeid(type::RecordTy)) {
    errormsg->Error(pos_, "not a record type");
    return new tr::ExpAndTy(new tr::ExExp(new tree::ConstExp(0)),
                            type::VoidTy::Instance());
  }

  if (typeid(*exp) != typeid(tr::ExExp)) {
    errormsg->Error(pos_, "field var's exp must be an expression");
    return new tr::ExpAndTy(new tr::ExExp(new tree::ConstExp(0)),
                            type::VoidTy::Instance());
  }
  auto record_ty = static_cast<type::RecordTy *>(ty);
  type::FieldList *field_list = record_ty->fields_;
  int order = 0;
  for (auto field : field_list->GetList()) {
    if (field->name_ == sym_) {
      tree::Exp *texp = new tree::MemExp(new tree::BinopExp(
          tree::PLUS_OP, exp->UnEx(),
          new tree::ConstExp(order * reg_manager->WordSize())));
      return new tr::ExpAndTy(new tr::ExExp(texp), field->ty_->ActualTy());
    }
    order++;
  }
  errormsg->Error(pos_, "field %s doesn't exist", sym_->Name().data());
  return new tr::ExpAndTy(nullptr, type::IntTy::Instance());
}

tr::ExpAndTy *SubscriptVar::Translate(env::VEnvPtr venv, env::TEnvPtr tenv,
                                      tr::Level *level, temp::Label *label,
                                      err::ErrorMsg *errormsg) const {
  /* TODO: Put your lab5 code here */
  tr::ExpAndTy *var_ty = var_->Translate(venv, tenv, level, label, errormsg);
  tr::ExpAndTy *sub_ty = subscript_->Translate(venv, tenv, level, label, errormsg);

  type::Ty *ty = var_ty->ty_->ActualTy();
  if (typeid(*ty) != typeid(type::ArrayTy)) {
    errormsg->Error(pos_, "array type required");
    return new tr::ExpAndTy(new tr::ExExp(new tree::ConstExp(0)),
                            type::IntTy::Instance());
  }

  tree::Exp *exp = new tree::MemExp(new tree::BinopExp(
      tree::PLUS_OP, var_ty->exp_->UnEx(),
      new tree::BinopExp(tree::MUL_OP, sub_ty->exp_->UnEx(),
                         new tree::ConstExp(reg_manager->WordSize()))));

  return new tr::ExpAndTy(new tr::ExExp(exp),
                          static_cast<type::ArrayTy *>(ty)->ty_);
}

tr::ExpAndTy *VarExp::Translate(env::VEnvPtr venv, env::TEnvPtr tenv,
                                tr::Level *level, temp::Label *label,
                                err::ErrorMsg *errormsg) const {
  /* TODO: Put your lab5 code here */
  return var_->Translate(venv, tenv, level, label, errormsg);
}

tr::ExpAndTy *NilExp::Translate(env::VEnvPtr venv, env::TEnvPtr tenv,
                                tr::Level *level, temp::Label *label,
                                err::ErrorMsg *errormsg) const {
  /* TODO: Put your lab5 code here */
  return new tr::ExpAndTy(new tr::ExExp(new tree::ConstExp(0)),
                          type::NilTy::Instance());
}

tr::ExpAndTy *IntExp::Translate(env::VEnvPtr venv, env::TEnvPtr tenv,
                                tr::Level *level, temp::Label *label,
                                err::ErrorMsg *errormsg) const {
  /* TODO: Put your lab5 code here */
  return new tr::ExpAndTy(new tr::ExExp(new tree::ConstExp(val_)),
                          type::IntTy::Instance());
}

tr::ExpAndTy *StringExp::Translate(env::VEnvPtr venv, env::TEnvPtr tenv,
                                   tr::Level *level, temp::Label *label,
                                   err::ErrorMsg *errormsg) const {
  /* TODO: Put your lab5 code here */
  temp::Label *lab = temp::LabelFactory::NewLabel();
  frags->PushBack(new frame::StringFrag(lab, str_));
  return new tr::ExpAndTy(new tr::ExExp(new tree::NameExp(lab)),
                          type::StringTy::Instance());
}

tr::ExpAndTy *CallExp::Translate(env::VEnvPtr venv, env::TEnvPtr tenv,
                                 tr::Level *level, temp::Label *label,
                                 err::ErrorMsg *errormsg) const {
  /* TODO: Put your lab5 code here */
  env::EnvEntry *entry = venv->Look(func_);
  if (!entry || typeid(*entry) != typeid(env::FunEntry)) {
    errormsg->Error(pos_, "undefined function %s", func_->Name().data());
    return new tr::ExpAndTy(new tr::ExExp(new tree::ConstExp(0)),
                            type::IntTy::Instance());
  }
  env::FunEntry *fun_entry = static_cast<env::FunEntry *>(entry);

  auto *args = new tree::ExpList();
  if (fun_entry->label_ == nullptr) {
    for (Exp *arg : args_->GetList()) {
      tr::ExpAndTy *arg_ty = arg->Translate(venv, tenv, level, label, errormsg);
      args->Append(arg_ty->exp_->UnEx());
    }
    return new tr::ExpAndTy(
        new tr::ExExp(frame::ExternalCall(func_->Name(), args)),
        fun_entry->result_);
  } else {
    tree::Exp *sl = tr::StaticLink(level, fun_entry->level_->parent_);
    args->Append(sl);
    for (Exp *arg : args_->GetList()) {
      tr::ExpAndTy *arg_ty = arg->Translate(venv, tenv, level, label, errormsg);
      args->Append(arg_ty->exp_->UnEx());
    }
    return new tr::ExpAndTy(
        new tr::ExExp(new tree::CallExp(
            new tree::NameExp(fun_entry->label_), args)),
        fun_entry->result_);
  }
}

tr::ExpAndTy *OpExp::Translate(env::VEnvPtr venv, env::TEnvPtr tenv,
                               tr::Level *level, temp::Label *label,
                               err::ErrorMsg *errormsg) const {
  /* TODO: Put your lab5 code here */
  tr::ExpAndTy *left_ty = left_->Translate(venv, tenv, level, label, errormsg);
  tr::ExpAndTy *right_ty = right_->Translate(venv, tenv, level, label, errormsg);
  tree::Exp *left_exp = left_ty->exp_->UnEx();
  tree::Exp *right_exp = right_ty->exp_->UnEx();

  if (oper_ == absyn::PLUS_OP || oper_ == absyn::MINUS_OP ||
      oper_ == absyn::TIMES_OP || oper_ == absyn::DIVIDE_OP) {
    tree::BinOp op;
    if (oper_ == absyn::PLUS_OP) op = tree::PLUS_OP;
    else if (oper_ == absyn::MINUS_OP) op = tree::MINUS_OP;
    else if (oper_ == absyn::TIMES_OP) op = tree::MUL_OP;
    else op = tree::DIV_OP;
    return new tr::ExpAndTy(
        new tr::ExExp(new tree::BinopExp(op, left_exp, right_exp)),
        type::IntTy::Instance());
  }

  if (oper_ == absyn::AND_OP) {
    temp::Label *t = temp::LabelFactory::NewLabel();
    temp::Label *f = temp::LabelFactory::NewLabel();
    temp::Label *join = temp::LabelFactory::NewLabel();
    temp::Temp *r = temp::TempFactory::NewTemp();

    tr::Cx left_cx = left_ty->exp_->UnCx(errormsg);
    left_cx.trues_.DoPatch(t);
    left_cx.falses_.DoPatch(f);

    tree::Stm *stm = new tree::SeqStm(
        left_cx.stm_,
        new tree::SeqStm(
            new tree::LabelStm(t),
            new tree::SeqStm(
                new tree::MoveStm(new tree::TempExp(r), right_ty->exp_->UnEx()),
                new tree::SeqStm(
                    new tree::JumpStm(new tree::NameExp(join),
                                      new std::vector<temp::Label *>({join})),
                    new tree::SeqStm(
                        new tree::LabelStm(f),
                        new tree::SeqStm(
                            new tree::MoveStm(new tree::TempExp(r),
                                              new tree::ConstExp(0)),
                            new tree::LabelStm(join)))))));
    return new tr::ExpAndTy(new tr::ExExp(new tree::EseqExp(stm, new tree::TempExp(r))),
                            type::IntTy::Instance());
  }

  if (oper_ == absyn::OR_OP) {
    temp::Label *t = temp::LabelFactory::NewLabel();
    temp::Label *f = temp::LabelFactory::NewLabel();
    temp::Label *join = temp::LabelFactory::NewLabel();
    temp::Temp *r = temp::TempFactory::NewTemp();

    tr::Cx left_cx = left_ty->exp_->UnCx(errormsg);
    left_cx.trues_.DoPatch(t);
    left_cx.falses_.DoPatch(f);

    tree::Stm *stm = new tree::SeqStm(
        left_cx.stm_,
        new tree::SeqStm(
            new tree::LabelStm(t),
            new tree::SeqStm(
                new tree::MoveStm(new tree::TempExp(r), new tree::ConstExp(1)),
                new tree::SeqStm(
                    new tree::JumpStm(new tree::NameExp(join),
                                      new std::vector<temp::Label *>({join})),
                    new tree::SeqStm(
                        new tree::LabelStm(f),
                        new tree::SeqStm(
                            new tree::MoveStm(new tree::TempExp(r),
                                              right_ty->exp_->UnEx()),
                            new tree::LabelStm(join)))))));
    return new tr::ExpAndTy(new tr::ExExp(new tree::EseqExp(stm, new tree::TempExp(r))),
                            type::IntTy::Instance());
  }

  tree::RelOp relop;
  if (oper_ == absyn::EQ_OP) relop = tree::EQ_OP;
  else if (oper_ == absyn::NEQ_OP) relop = tree::NE_OP;
  else if (oper_ == absyn::LT_OP) relop = tree::LT_OP;
  else if (oper_ == absyn::LE_OP) relop = tree::LE_OP;
  else if (oper_ == absyn::GT_OP) relop = tree::GT_OP;
  else relop = tree::GE_OP;

  type::Ty *actual_left = left_ty->ty_->ActualTy();
  if (typeid(*actual_left) == typeid(type::StringTy)) {
    auto *args = new tree::ExpList();
    args->Append(left_exp);
    args->Append(right_exp);
    tree::Exp *call = frame::ExternalCall("string_equal", args);
    if (oper_ == absyn::EQ_OP) {
      return new tr::ExpAndTy(new tr::ExExp(call), type::IntTy::Instance());
    } else if (oper_ == absyn::NEQ_OP) {
      return new tr::ExpAndTy(
          new tr::ExExp(new tree::BinopExp(tree::XOR_OP, call,
                                           new tree::ConstExp(1))),
          type::IntTy::Instance());
    }
  }

  tree::CjumpStm *cjump = new tree::CjumpStm(relop, left_exp, right_exp,
      temp::LabelFactory::NewLabel(), temp::LabelFactory::NewLabel());
  tr::PatchList trues = tr::PatchList({&cjump->true_label_});
  tr::PatchList falses = tr::PatchList({&cjump->false_label_});
  return new tr::ExpAndTy(new tr::CxExp(trues, falses, cjump),
                          type::IntTy::Instance());
}

tr::ExpAndTy *RecordExp::Translate(env::VEnvPtr venv, env::TEnvPtr tenv,
                                   tr::Level *level, temp::Label *label,
                                   err::ErrorMsg *errormsg) const {
  /* TODO: Put your lab5 code here */
  type::Ty *ty = tenv->Look(typ_);
  if (!ty || typeid(*(ty->ActualTy())) != typeid(type::RecordTy)) {
    errormsg->Error(pos_, "undefined type %s", typ_->Name().data());
    return new tr::ExpAndTy(new tr::ExExp(new tree::ConstExp(0)),
                            type::IntTy::Instance());
  }

  int n = fields_->GetList().size();
  auto *alloc_args = new tree::ExpList();
  alloc_args->Append(new tree::ConstExp(n * reg_manager->WordSize()));
  tree::Exp *alloc = frame::ExternalCall("alloc_record", alloc_args);

  temp::Temp *r = temp::TempFactory::NewTemp();

  tree::Stm *moves = nullptr;
  int i = 0;
  for (EField *field : fields_->GetList()) {
    tr::ExpAndTy *field_ty = field->exp_->Translate(venv, tenv, level, label, errormsg);
    tree::Exp *dst = new tree::MemExp(new tree::BinopExp(
        tree::PLUS_OP, new tree::TempExp(r),
        new tree::ConstExp(i * reg_manager->WordSize())));
    tree::Stm *move = new tree::MoveStm(dst, field_ty->exp_->UnEx());
    if (!moves)
      moves = move;
    else
      moves = new tree::SeqStm(moves, move);
    i++;
  }

  if (moves) {
    return new tr::ExpAndTy(
        new tr::ExExp(new tree::EseqExp(
            new tree::SeqStm(
                new tree::MoveStm(new tree::TempExp(r), alloc),
                moves),
            new tree::TempExp(r))),
        ty);
  } else {
    return new tr::ExpAndTy(
        new tr::ExExp(new tree::EseqExp(
            new tree::MoveStm(new tree::TempExp(r), alloc),
            new tree::TempExp(r))),
        ty);
  }
}

tr::ExpAndTy *SeqExp::Translate(env::VEnvPtr venv, env::TEnvPtr tenv,
                                tr::Level *level, temp::Label *label,
                                err::ErrorMsg *errormsg) const {
  /* TODO: Put your lab5 code here */
  if (seq_->GetList().empty()) {
    return new tr::ExpAndTy(new tr::ExExp(new tree::ConstExp(0)),
                            type::VoidTy::Instance());
  }

  auto &list = seq_->GetList();
  tree::Stm *stm = nullptr;

  auto it = list.begin();
  auto last_it = std::prev(list.end());
  for (; it != last_it; ++it) {
    tr::ExpAndTy *exp_ty = (*it)->Translate(venv, tenv, level, label, errormsg);
    if (stm) {
      stm = new tree::SeqStm(stm, exp_ty->exp_->UnNx());
    } else {
      stm = exp_ty->exp_->UnNx();
    }
  }

  tr::ExpAndTy *result = (*last_it)->Translate(venv, tenv, level, label, errormsg);
  if (stm) {
    return new tr::ExpAndTy(
        new tr::ExExp(new tree::EseqExp(stm, result->exp_->UnEx())),
        result->ty_);
  } else {
    return result;
  }
}

tr::ExpAndTy *AssignExp::Translate(env::VEnvPtr venv, env::TEnvPtr tenv,
                                   tr::Level *level, temp::Label *label,
                                   err::ErrorMsg *errormsg) const {
  /* TODO: Put your lab5 code here */
  tr::ExpAndTy *var_ty = var_->Translate(venv, tenv, level, label, errormsg);
  tr::ExpAndTy *exp_ty = exp_->Translate(venv, tenv, level, label, errormsg);
  return new tr::ExpAndTy(
      new tr::NxExp(new tree::MoveStm(var_ty->exp_->UnEx(),
                                       exp_ty->exp_->UnEx())),
      type::VoidTy::Instance());
}

tr::ExpAndTy *IfExp::Translate(env::VEnvPtr venv, env::TEnvPtr tenv,
                               tr::Level *level, temp::Label *label,
                               err::ErrorMsg *errormsg) const {
  /* TODO: Put your lab5 code here */
  tr::ExpAndTy *test_ty = test_->Translate(venv, tenv, level, label, errormsg);
  tr::ExpAndTy *then_ty = then_->Translate(venv, tenv, level, label, errormsg);

  if (elsee_) {
    tr::ExpAndTy *else_ty = elsee_->Translate(venv, tenv, level, label, errormsg);

    temp::Label *t = temp::LabelFactory::NewLabel();
    temp::Label *f = temp::LabelFactory::NewLabel();
    temp::Label *join = temp::LabelFactory::NewLabel();

    tr::Cx test_cx = test_ty->exp_->UnCx(errormsg);
    test_cx.trues_.DoPatch(t);
    test_cx.falses_.DoPatch(f);

    temp::Temp *r = temp::TempFactory::NewTemp();
    type::Ty *result_ty = then_ty->ty_;

    tree::Stm *stm = new tree::SeqStm(
        test_cx.stm_,
        new tree::SeqStm(
            new tree::LabelStm(t),
            new tree::SeqStm(
                new tree::MoveStm(new tree::TempExp(r), then_ty->exp_->UnEx()),
                new tree::SeqStm(
                    new tree::JumpStm(new tree::NameExp(join),
                                      new std::vector<temp::Label *>({join})),
                    new tree::SeqStm(
                        new tree::LabelStm(f),
                        new tree::SeqStm(
                            new tree::MoveStm(new tree::TempExp(r),
                                              else_ty->exp_->UnEx()),
                            new tree::LabelStm(join)))))));

    return new tr::ExpAndTy(
        new tr::ExExp(new tree::EseqExp(stm, new tree::TempExp(r))),
        result_ty);
  } else {
    temp::Label *t = temp::LabelFactory::NewLabel();
    temp::Label *f = temp::LabelFactory::NewLabel();

    tr::Cx test_cx = test_ty->exp_->UnCx(errormsg);
    test_cx.trues_.DoPatch(t);
    test_cx.falses_.DoPatch(f);

    tree::Stm *stm = new tree::SeqStm(
        test_cx.stm_,
        new tree::SeqStm(
            new tree::LabelStm(t),
            new tree::SeqStm(then_ty->exp_->UnNx(),
                             new tree::LabelStm(f))));

    return new tr::ExpAndTy(new tr::NxExp(stm), type::VoidTy::Instance());
  }
}

tr::ExpAndTy *WhileExp::Translate(env::VEnvPtr venv, env::TEnvPtr tenv,
                                  tr::Level *level, temp::Label *label,
                                  err::ErrorMsg *errormsg) const {
  /* TODO: Put your lab5 code here */
  temp::Label *test_label = temp::LabelFactory::NewLabel();
  temp::Label *body_label = temp::LabelFactory::NewLabel();
  temp::Label *done_label = temp::LabelFactory::NewLabel();

  tr::ExpAndTy *test_ty = test_->Translate(venv, tenv, level, done_label, errormsg);
  tr::ExpAndTy *body_ty = body_->Translate(venv, tenv, level, done_label, errormsg);

  tr::Cx test_cx = test_ty->exp_->UnCx(errormsg);
  test_cx.trues_.DoPatch(body_label);
  test_cx.falses_.DoPatch(done_label);

  tree::Stm *stm = new tree::SeqStm(
      new tree::LabelStm(test_label),
      new tree::SeqStm(
          test_cx.stm_,
          new tree::SeqStm(
              new tree::LabelStm(body_label),
              new tree::SeqStm(
                  body_ty->exp_->UnNx(),
                  new tree::SeqStm(
                      new tree::JumpStm(new tree::NameExp(test_label),
                                        new std::vector<temp::Label *>({test_label})),
                      new tree::LabelStm(done_label))))));

  return new tr::ExpAndTy(new tr::NxExp(stm), type::VoidTy::Instance());
}

tr::ExpAndTy *ForExp::Translate(env::VEnvPtr venv, env::TEnvPtr tenv,
                                tr::Level *level, temp::Label *label,
                                err::ErrorMsg *errormsg) const {
  /* TODO: Put your lab5 code here */
  temp::Label *loop_label = temp::LabelFactory::NewLabel();
  temp::Label *body_label = temp::LabelFactory::NewLabel();
  temp::Label *done_label = temp::LabelFactory::NewLabel();

  tr::ExpAndTy *lo_ty = lo_->Translate(venv, tenv, level, label, errormsg);
  tr::ExpAndTy *hi_ty = hi_->Translate(venv, tenv, level, label, errormsg);

  tr::Access *access = tr::Access::AllocLocal(level, escape_);
  venv->BeginScope();
  venv->Enter(var_, new env::VarEntry(access, type::IntTy::Instance(), true));

  temp::Temp *limit = temp::TempFactory::NewTemp();

  tree::Stm *init = new tree::MoveStm(
      access->access_->ToExp(new tree::TempExp(reg_manager->FramePointer())),
      lo_ty->exp_->UnEx());

  tree::Stm *set_limit = new tree::MoveStm(
      new tree::TempExp(limit), hi_ty->exp_->UnEx());

  tree::Exp *var_exp = access->access_->ToExp(
      new tree::TempExp(reg_manager->FramePointer()));
  tree::Stm *test = new tree::CjumpStm(
      tree::LE_OP, var_exp, new tree::TempExp(limit),
      body_label, done_label);

  tr::ExpAndTy *body_ty = body_->Translate(venv, tenv, level, done_label, errormsg);

  tree::Stm *incr = new tree::MoveStm(
      access->access_->ToExp(new tree::TempExp(reg_manager->FramePointer())),
      new tree::BinopExp(tree::PLUS_OP, var_exp, new tree::ConstExp(1)));

  tree::Stm *stm = new tree::SeqStm(
      init,
      new tree::SeqStm(
          set_limit,
          new tree::SeqStm(
              new tree::LabelStm(loop_label),
              new tree::SeqStm(
                  test,
                  new tree::SeqStm(
                      new tree::LabelStm(body_label),
                      new tree::SeqStm(
                          body_ty->exp_->UnNx(),
                          new tree::SeqStm(
                              incr,
                              new tree::SeqStm(
                                  new tree::JumpStm(
                                      new tree::NameExp(loop_label),
                                      new std::vector<temp::Label *>({loop_label})),
                                  new tree::LabelStm(done_label)))))))));

  venv->EndScope();
  return new tr::ExpAndTy(new tr::NxExp(stm), type::VoidTy::Instance());
}

tr::ExpAndTy *BreakExp::Translate(env::VEnvPtr venv, env::TEnvPtr tenv,
                                  tr::Level *level, temp::Label *label,
                                  err::ErrorMsg *errormsg) const {
  /* TODO: Put your lab5 code here */
  return new tr::ExpAndTy(
      new tr::NxExp(new tree::JumpStm(
          new tree::NameExp(label),
          new std::vector<temp::Label *>({label}))),
      type::VoidTy::Instance());
}

tr::ExpAndTy *LetExp::Translate(env::VEnvPtr venv, env::TEnvPtr tenv,
                                tr::Level *level, temp::Label *label,
                                err::ErrorMsg *errormsg) const {
  /* TODO: Put your lab5 code here */
  venv->BeginScope();
  tenv->BeginScope();

  tree::Stm *decs_stm = nullptr;
  for (Dec *dec : decs_->GetList()) {
    tr::Exp *dec_exp = dec->Translate(venv, tenv, level, label, errormsg);
    if (dec_exp) {
      tree::Stm *dec_stm = dec_exp->UnNx();
      if (decs_stm) {
        decs_stm = new tree::SeqStm(decs_stm, dec_stm);
      } else {
        decs_stm = dec_stm;
      }
    }
  }

  tr::ExpAndTy *body_ty = body_->Translate(venv, tenv, level, label, errormsg);

  tenv->EndScope();
  venv->EndScope();

  if (decs_stm) {
    return new tr::ExpAndTy(
        new tr::ExExp(new tree::EseqExp(decs_stm, body_ty->exp_->UnEx())),
        body_ty->ty_);
  } else {
    return body_ty;
  }
}

tr::ExpAndTy *ArrayExp::Translate(env::VEnvPtr venv, env::TEnvPtr tenv,
                                  tr::Level *level, temp::Label *label,
                                  err::ErrorMsg *errormsg) const {
  /* TODO: Put your lab5 code here */
  type::Ty *ty = tenv->Look(typ_);
  if (!ty || typeid(*(ty->ActualTy())) != typeid(type::ArrayTy)) {
    errormsg->Error(pos_, "array type required");
    return new tr::ExpAndTy(new tr::ExExp(new tree::ConstExp(0)),
                            type::IntTy::Instance());
  }

  tr::ExpAndTy *size_ty = size_->Translate(venv, tenv, level, label, errormsg);
  tr::ExpAndTy *init_ty = init_->Translate(venv, tenv, level, label, errormsg);

  auto *args = new tree::ExpList();
  args->Append(size_ty->exp_->UnEx());
  args->Append(init_ty->exp_->UnEx());
  tree::Exp *alloc = frame::ExternalCall("init_array", args);

  return new tr::ExpAndTy(new tr::ExExp(alloc), ty);
}

tr::ExpAndTy *VoidExp::Translate(env::VEnvPtr venv, env::TEnvPtr tenv,
                                 tr::Level *level, temp::Label *label,
                                 err::ErrorMsg *errormsg) const {
  /* TODO: Put your lab5 code here */
  return new tr::ExpAndTy(new tr::ExExp(new tree::ConstExp(0)),
                          type::VoidTy::Instance());
}

tr::Exp *FunctionDec::Translate(env::VEnvPtr venv, env::TEnvPtr tenv,
                                tr::Level *level, temp::Label *label,
                                err::ErrorMsg *errormsg) const {
  /* TODO: Put your lab5 code here */
  for (FunDec *fun_dec : functions_->GetList()) {
    type::TyList *formal_tys = new type::TyList();
    std::list<bool> formals_esc;
    for (Field *field : fun_dec->params_->GetList()) {
      type::Ty *ty = tenv->Look(field->typ_);
      if (!ty) {
        errormsg->Error(field->pos_, "undefined type %s",
                        field->typ_->Name().data());
      }
      formal_tys->Append(ty);
      formals_esc.push_back(field->escape_);
    }

    type::Ty *result_ty = type::VoidTy::Instance();
    if (fun_dec->result_) {
      result_ty = tenv->Look(fun_dec->result_);
      if (!result_ty) {
        errormsg->Error(fun_dec->pos_, "undefined type %s",
                        fun_dec->result_->Name().data());
      }
    }

    temp::Label *fun_label = temp::LabelFactory::NamedLabel(fun_dec->name_->Name());
    tr::Level *new_level = tr::Level::NewLevel(level, fun_label, formals_esc);

    venv->Enter(fun_dec->name_,
                new env::FunEntry(new_level, fun_label, formal_tys, result_ty));
  }

  for (FunDec *fun_dec : functions_->GetList()) {
    env::FunEntry *fun_entry =
        static_cast<env::FunEntry *>(venv->Look(fun_dec->name_));
    tr::Level *fun_level = fun_entry->level_;

    venv->BeginScope();

    auto formal_it = fun_entry->formals_->GetList().begin();
    auto tr_acc_it = fun_level->Formals()->begin();
    for (Field *field : fun_dec->params_->GetList()) {
      venv->Enter(field->name_,
                  new env::VarEntry(*tr_acc_it, *formal_it));
      ++formal_it;
      ++tr_acc_it;
    }

    tr::ExpAndTy *body_ty = fun_dec->body_->Translate(
        venv, tenv, fun_level, label, errormsg);

    tree::Stm *body_stm;
    if (typeid(*(fun_entry->result_->ActualTy())) == typeid(type::VoidTy)) {
      body_stm = body_ty->exp_->UnNx();
    } else {
      body_stm = new tree::MoveStm(
          new tree::TempExp(reg_manager->ReturnValue()),
          body_ty->exp_->UnEx());
    }

    frags->PushBack(ProcEntryExit(fun_level, new tr::NxExp(body_stm)));
    venv->EndScope();
  }

  return new tr::NxExp(new tree::ExpStm(new tree::ConstExp(0)));
}

tr::Exp *VarDec::Translate(env::VEnvPtr venv, env::TEnvPtr tenv,
                           tr::Level *level, temp::Label *label,
                           err::ErrorMsg *errormsg) const {
  tr::ExpAndTy *init_exp_ty =
      init_->Translate(venv, tenv, level, label, errormsg);
  type::Ty *init_ty = init_exp_ty->ty_;

  if (typ_) {
    type::Ty *ty = tenv->Look(typ_);
    if (!ty) {
      errormsg->Error(pos_, "undefined type %s", typ_->Name().data());
    }

    if (!ty->IsSameType(init_ty)) {
      errormsg->Error(pos_, "type and init type mismatch");
    }
  } else {
    auto actual_init_ty = init_ty->ActualTy();
    if (typeid(*actual_init_ty) == typeid(type::NilTy)) {
      errormsg->Error(pos_, "init should not be nil without type specified");
    }
  }

  tr::Access *access = tr::Access::AllocLocal(level, escape_);
  venv->Enter(var_, new env::VarEntry(access, init_ty));

  return new tr::NxExp(
      new tree::MoveStm(access->access_->ToExp(new tree::TempExp(
                            reg_manager->FramePointer())),
                        init_exp_ty->exp_->UnEx()));
}

tr::Exp *TypeDec::Translate(env::VEnvPtr venv, env::TEnvPtr tenv,
                            tr::Level *level, temp::Label *label,
                            err::ErrorMsg *errormsg) const {
  /* TODO: Put your lab5 code here */
  for (NameAndTy *name_and_ty : types_->GetList()) {
    tenv->Enter(name_and_ty->name_,
                new type::NameTy(name_and_ty->name_, nullptr));
  }

  for (NameAndTy *name_and_ty : types_->GetList()) {
    type::Ty *ty = tenv->Look(name_and_ty->name_);
    type::NameTy *name_ty = static_cast<type::NameTy *>(ty);
    name_ty->ty_ = name_and_ty->ty_->Translate(tenv, errormsg);
  }

  return nullptr;
}

type::Ty *NameTy::Translate(env::TEnvPtr tenv, err::ErrorMsg *errormsg) const {
  /* TODO: Put your lab5 code here */
  type::Ty *ty = tenv->Look(name_);
  if (!ty) {
    errormsg->Error(pos_, "undefined type %s", name_->Name().data());
    return type::IntTy::Instance();
  }
  return new type::NameTy(name_, ty);
}

type::Ty *RecordTy::Translate(env::TEnvPtr tenv,
                              err::ErrorMsg *errormsg) const {
  /* TODO: Put your lab5 code here */
  type::FieldList *field_list = new type::FieldList();
  for (Field *field : record_->GetList()) {
    type::Ty *ty = tenv->Look(field->typ_);
    if (!ty) {
      errormsg->Error(field->pos_, "undefined type %s",
                      field->typ_->Name().data());
    }
    field_list->Append(new type::Field(field->name_, ty));
  }
  return new type::RecordTy(field_list);
}

type::Ty *ArrayTy::Translate(env::TEnvPtr tenv, err::ErrorMsg *errormsg) const {
  /* TODO: Put your lab5 code here */
  type::Ty *ty = tenv->Look(array_);
  if (!ty) {
    errormsg->Error(pos_, "undefined type %s", array_->Name().data());
    return type::IntTy::Instance();
  }
  return new type::ArrayTy(ty);
}

} // namespace absyn
