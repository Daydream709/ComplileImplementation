#include "tiger/semant/semant.h"
#include <set>
#include "tiger/absyn/absyn.h"

namespace absyn {

void AbsynTree::SemAnalyze(env::VEnvPtr venv, env::TEnvPtr tenv,
                           err::ErrorMsg *errormsg) const {
  /* TODO: Put your lab4 code here */
  root_->SemAnalyze(venv, tenv, 0, errormsg);
}

type::Ty *SimpleVar::SemAnalyze(env::VEnvPtr venv, env::TEnvPtr tenv,
                                int labelcount, err::ErrorMsg *errormsg) const {
  /* TODO: Put your lab4 code here */
  env::EnvEntry *entry = venv->Look(sym_);
  if (entry && typeid(*entry) == typeid(env::VarEntry)) {
    return (static_cast<env::VarEntry *>(entry))->ty_;
  } else {
    errormsg->Error(pos_, "undefined variable %s", sym_->Name().data());
    return type::IntTy::Instance();
  }
}

type::Ty *FieldVar::SemAnalyze(env::VEnvPtr venv, env::TEnvPtr tenv,
                               int labelcount, err::ErrorMsg *errormsg) const {
  /* TODO: Put your lab4 code here */
  type::Ty *var_ty =
      var_->SemAnalyze(venv, tenv, labelcount, errormsg)->ActualTy();
  if (typeid(*var_ty) != typeid(type::RecordTy)) {
    errormsg->Error(pos_, "not a record type");
    return type::IntTy::Instance();
  }
  type::RecordTy *record_ty = static_cast<type::RecordTy *>(var_ty);
  for (type::Field *field : record_ty->fields_->GetList()) {
    if (field->name_ == sym_) {
      return field->ty_;
    }
  }
  errormsg->Error(pos_, " field %s doesn't exist", sym_->Name().data());
  return type::IntTy::Instance();
}

type::Ty *SubscriptVar::SemAnalyze(env::VEnvPtr venv, env::TEnvPtr tenv,
                                   int labelcount,
                                   err::ErrorMsg *errormsg) const {
  /* TODO: Put your lab4 code here */
  type::Ty *var_ty =
      var_->SemAnalyze(venv, tenv, labelcount, errormsg)->ActualTy();
  if (typeid(*var_ty) != typeid(type::ArrayTy)) {
    errormsg->Error(pos_, "array type required");
    return type::IntTy::Instance();
  }
  type::Ty *exp_ty =
      subscript_->SemAnalyze(venv, tenv, labelcount, errormsg)->ActualTy();
  if (typeid(*exp_ty) != typeid(type::IntTy)) {
    errormsg->Error(pos_, "integer required");
  }
  return static_cast<type::ArrayTy *>(var_ty)->ty_;
}

type::Ty *VarExp::SemAnalyze(env::VEnvPtr venv, env::TEnvPtr tenv,
                             int labelcount, err::ErrorMsg *errormsg) const {
  /* TODO: Put your lab4 code here */
  return var_->SemAnalyze(venv, tenv, labelcount, errormsg);
}

type::Ty *NilExp::SemAnalyze(env::VEnvPtr venv, env::TEnvPtr tenv,
                             int labelcount, err::ErrorMsg *errormsg) const {
  /* TODO: Put your lab4 code here */
  return type::NilTy::Instance();
}

type::Ty *IntExp::SemAnalyze(env::VEnvPtr venv, env::TEnvPtr tenv,
                             int labelcount, err::ErrorMsg *errormsg) const {
  /* TODO: Put your lab4 code here */
  return type::IntTy::Instance();
}

type::Ty *StringExp::SemAnalyze(env::VEnvPtr venv, env::TEnvPtr tenv,
                                int labelcount, err::ErrorMsg *errormsg) const {
  /* TODO: Put your lab4 code here */
  return type::StringTy::Instance();
}

type::Ty *CallExp::SemAnalyze(env::VEnvPtr venv, env::TEnvPtr tenv,
                              int labelcount, err::ErrorMsg *errormsg) const {
  /* TODO: Put your lab4 code here */
  env::EnvEntry *entry = venv->Look(func_);
  if (!entry || typeid(*entry) != typeid(env::FunEntry)) {
    errormsg->Error(pos_, "undefined function %s", func_->Name().data());
    return type::IntTy::Instance();
  }
  env::FunEntry *fun_entry = static_cast<env::FunEntry *>(entry);

  size_t formal_count = fun_entry->formals_->GetList().size();
  size_t arg_count = args_->GetList().size();

  if (arg_count < formal_count) {
    errormsg->Error(pos_, "too few params in function %s",
                    func_->Name().data());
  } else if (arg_count > formal_count) {
    errormsg->Error(pos_, "too many params in function %s",
                    func_->Name().data());
  }

  auto formal_it = fun_entry->formals_->GetList().begin();
  auto arg_it = args_->GetList().begin();
  while (formal_it != fun_entry->formals_->GetList().end() &&
         arg_it != args_->GetList().end()) {
    type::Ty *arg_ty =
        (*arg_it)->SemAnalyze(venv, tenv, labelcount, errormsg)->ActualTy();
    type::Ty *formal_ty = (*formal_it)->ActualTy();
    if (!arg_ty->IsSameType(formal_ty)) {
      errormsg->Error((*arg_it)->pos_, "para type mismatch");
    }
    ++formal_it;
    ++arg_it;
  }

  return fun_entry->result_;
}

type::Ty *OpExp::SemAnalyze(env::VEnvPtr venv, env::TEnvPtr tenv,
                            int labelcount, err::ErrorMsg *errormsg) const {
  /* TODO: Put your lab4 code here */
  type::Ty *left_ty =
      left_->SemAnalyze(venv, tenv, labelcount, errormsg)->ActualTy();
  type::Ty *right_ty =
      right_->SemAnalyze(venv, tenv, labelcount, errormsg)->ActualTy();
  if (oper_ == PLUS_OP || oper_ == MINUS_OP || oper_ == TIMES_OP ||
      oper_ == DIVIDE_OP) {
    if (typeid(*left_ty) != typeid(type::IntTy)) {
      errormsg->Error(left_->pos_, "integer required");
    }
    if (typeid(*right_ty) != typeid(type::IntTy)) {
      errormsg->Error(right_->pos_, "integer required");
    }
  } else if (oper_ == EQ_OP || oper_ == NEQ_OP) {
    if (!left_ty->IsSameType(right_ty)) {
      errormsg->Error(pos_, "same type required");
    }
  } else {
    if (typeid(*left_ty) != typeid(type::IntTy) &&
        typeid(*left_ty) != typeid(type::StringTy)) {
      errormsg->Error(left_->pos_, "same type required");
    }
    if (!left_ty->IsSameType(right_ty)) {
      errormsg->Error(right_->pos_, "same type required");
    }
  }
  return type::IntTy::Instance();
}

type::Ty *RecordExp::SemAnalyze(env::VEnvPtr venv, env::TEnvPtr tenv,
                                int labelcount, err::ErrorMsg *errormsg) const {
  /* TODO: Put your lab4 code here */
  type::Ty *ty = tenv->Look(typ_);
  if (!ty || typeid(*(ty->ActualTy())) != typeid(type::RecordTy)) {
    errormsg->Error(pos_, " undefined type %s", typ_->Name().data());
    return type::IntTy::Instance();
  }
  type::RecordTy *record_ty = static_cast<type::RecordTy *>(ty->ActualTy());
  auto expected_it = record_ty->fields_->GetList().begin();
  auto actual_it = fields_->GetList().begin();

  while (expected_it != record_ty->fields_->GetList().end() &&
         actual_it != fields_->GetList().end()) {
    if ((*expected_it)->name_ != (*actual_it)->name_) {
      errormsg->Error(pos_, " field %s doesn't exist",
                      (*actual_it)->name_->Name().data());
      return type::IntTy::Instance();
    }
    type::Ty *expected_ty = (*expected_it)->ty_->ActualTy();
    type::Ty *actual_ty =
        (*actual_it)
            ->exp_->SemAnalyze(venv, tenv, labelcount, errormsg)
            ->ActualTy();
    if (!actual_ty->IsSameType(expected_ty)) {
      errormsg->Error((*actual_it)->exp_->pos_, "para type mismatch");
    }
    ++expected_it;
    ++actual_it;
  }
  return ty;
}

type::Ty *SeqExp::SemAnalyze(env::VEnvPtr venv, env::TEnvPtr tenv,
                             int labelcount, err::ErrorMsg *errormsg) const {
  /* TODO: Put your lab4 code here */
  type::Ty *ty = type::VoidTy::Instance();
  for (Exp *exp : seq_->GetList()) {
    ty = exp->SemAnalyze(venv, tenv, labelcount, errormsg);
  }
  return ty;
}

type::Ty *IfExp::SemAnalyze(env::VEnvPtr venv, env::TEnvPtr tenv,
                            int labelcount, err::ErrorMsg *errormsg) const {
  /* TODO: Put your lab4 code here */
  type::Ty *test_ty =
      test_->SemAnalyze(venv, tenv, labelcount, errormsg)->ActualTy();
  if (typeid(*test_ty) != typeid(type::IntTy)) {
    errormsg->Error(test_->pos_, "integer required");
  }
  type::Ty *then_ty =
      then_->SemAnalyze(venv, tenv, labelcount, errormsg)->ActualTy();
  if (elsee_) {
    type::Ty *else_ty =
        elsee_->SemAnalyze(venv, tenv, labelcount, errormsg)->ActualTy();
    if (!then_ty->IsSameType(else_ty)) {
      errormsg->Error(pos_, "then exp and else exp type mismatch");
    }
    return then_ty;
  }
  if (typeid(*then_ty) != typeid(type::VoidTy)) {
    errormsg->Error(then_->pos_, "if-then exp's body must produce no value");
  }
  return type::VoidTy::Instance();
}

type::Ty *WhileExp::SemAnalyze(env::VEnvPtr venv, env::TEnvPtr tenv,
                               int labelcount, err::ErrorMsg *errormsg) const {
  /* TODO: Put your lab4 code here */
  type::Ty *test_ty =
      test_->SemAnalyze(venv, tenv, labelcount, errormsg)->ActualTy();
  if (typeid(*test_ty) != typeid(type::IntTy)) {
    errormsg->Error(test_->pos_, "integer required");
  }
  type::Ty *body_ty =
      body_->SemAnalyze(venv, tenv, labelcount + 1, errormsg)->ActualTy();
  if (typeid(*body_ty) != typeid(type::VoidTy)) {
    errormsg->Error(pos_, "while body must produce no value");
  }
  return type::VoidTy::Instance();
}

type::Ty *BreakExp::SemAnalyze(env::VEnvPtr venv, env::TEnvPtr tenv,
                               int labelcount, err::ErrorMsg *errormsg) const {
  /* TODO: Put your lab4 code here */
  if (labelcount == 0) {
    errormsg->Error(pos_, "break is not inside any loop");
  }
  return type::VoidTy::Instance();
}

type::Ty *LetExp::SemAnalyze(env::VEnvPtr venv, env::TEnvPtr tenv,
                             int labelcount, err::ErrorMsg *errormsg) const {
  /* TODO: Put your lab4 code here */
  venv->BeginScope();
  tenv->BeginScope();
  for (Dec *dec : decs_->GetList()) {
    dec->SemAnalyze(venv, tenv, labelcount, errormsg);
  }
  type::Ty *ty = body_->SemAnalyze(venv, tenv, labelcount, errormsg);
  tenv->EndScope();
  venv->EndScope();
  return ty;
}

type::Ty *ArrayExp::SemAnalyze(env::VEnvPtr venv, env::TEnvPtr tenv,
                               int labelcount, err::ErrorMsg *errormsg) const {
  /* TODO: Put your lab4 code here */
  type::Ty *ty = tenv->Look(typ_);
  if (!ty || typeid(*(ty->ActualTy())) != typeid(type::ArrayTy)) {
    errormsg->Error(pos_, "array type required");
    return type::IntTy::Instance();
  }
  type::ArrayTy *array_ty = static_cast<type::ArrayTy *>(ty->ActualTy());
  type::Ty *size_ty =
      size_->SemAnalyze(venv, tenv, labelcount, errormsg)->ActualTy();
  if (typeid(*size_ty) != typeid(type::IntTy)) {
    errormsg->Error(size_->pos_, "integer required");
  }
  type::Ty *init_ty =
      init_->SemAnalyze(venv, tenv, labelcount, errormsg)->ActualTy();
  if (!init_ty->IsSameType(array_ty->ty_->ActualTy())) {
    errormsg->Error(init_->pos_, "type mismatch");
  }
  return ty;
}

type::Ty *VoidExp::SemAnalyze(env::VEnvPtr venv, env::TEnvPtr tenv,
                              int labelcount, err::ErrorMsg *errormsg) const {
  /* TODO: Put your lab4 code here */
  return type::VoidTy::Instance();
}

type::Ty *AssignExp::SemAnalyze(env::VEnvPtr venv, env::TEnvPtr tenv,
                                int labelcount, err::ErrorMsg *errormsg) const {
  /* TODO: Put your lab4 code here */
  if (typeid(*var_) == typeid(SimpleVar)) {
    SimpleVar *sim_var = static_cast<SimpleVar *>(var_);
    env::EnvEntry *entry = venv->Look(sim_var->sym_);
    if (entry && entry->readonly_) {
      errormsg->Error(pos_, " loop variable can't be assigned");
    }
  }
  type::Ty *var_ty =
      var_->SemAnalyze(venv, tenv, labelcount, errormsg)->ActualTy();
  type::Ty *exp_ty =
      exp_->SemAnalyze(venv, tenv, labelcount, errormsg)->ActualTy();
  if (!var_ty->IsSameType(exp_ty)) {
    errormsg->Error(pos_, " unmatched assign exp");
  }
  return type::VoidTy::Instance();
}

void FunctionDec::SemAnalyze(env::VEnvPtr venv, env::TEnvPtr tenv,
                             int labelcount, err::ErrorMsg *errormsg) const {
  /* TODO: Put your lab4 code here */
    std::set<std::string> seen_names;
    for (FunDec *fun_dec : functions_->GetList()) {
      if (seen_names.count(fun_dec->name_->Name())) {
        errormsg->Error(fun_dec->pos_, "two functions have the same name");
      }
      seen_names.insert(fun_dec->name_->Name());
    type::TyList *formal_tys = new type::TyList();
    for (Field *field : fun_dec->params_->GetList()) {
      type::Ty *ty = tenv->Look(field->typ_);
      if (!ty) {
        errormsg->Error(field->pos_, " undefined type %s",
                        field->typ_->Name().data());
      }
      formal_tys->Append(ty);
    }
    type::Ty *result_ty = type::VoidTy::Instance();
    if (fun_dec->result_) {
      result_ty = tenv->Look(fun_dec->result_);
      if (!result_ty) {
        errormsg->Error(fun_dec->pos_, " undefined type %s",
                        fun_dec->result_->Name().data());
      }
    }
    venv->Enter(fun_dec->name_, new env::FunEntry(formal_tys, result_ty));
  }

  for (FunDec *fun_dec : functions_->GetList()) {
    venv->BeginScope();
    env::FunEntry *fun_entry =
        static_cast<env::FunEntry *>(venv->Look(fun_dec->name_));
    auto formal_it = fun_entry->formals_->GetList().begin();
    for (Field *field : fun_dec->params_->GetList()) {
      venv->Enter(field->name_, new env::VarEntry(*formal_it));
      ++formal_it;
    }
    type::Ty *body_ty =
        fun_dec->body_->SemAnalyze(venv, tenv, labelcount, errormsg)
            ->ActualTy();
    if (!body_ty->IsSameType(fun_entry->result_->ActualTy())) {
      if (typeid(*(fun_entry->result_->ActualTy())) == typeid(type::VoidTy)) {
        errormsg->Error(fun_dec->body_->pos_, "procedure returns value");
      } else {
        errormsg->Error(fun_dec->body_->pos_, "function return type mismatch");
      }
    }
    venv->EndScope();
  }
}

void VarDec::SemAnalyze(env::VEnvPtr venv, env::TEnvPtr tenv, int labelcount,
                        err::ErrorMsg *errormsg) const {
  /* TODO: Put your lab4 code here */
  type::Ty *exp_ty =
      init_->SemAnalyze(venv, tenv, labelcount, errormsg)->ActualTy();
  if (typ_) {
    type::Ty *declared_ty = tenv->Look(typ_);
    if (!declared_ty) {
      errormsg->Error(pos_, " undefined type %s", typ_->Name().data());
      return;
    }
    if (!exp_ty->IsSameType(declared_ty->ActualTy())) {
      errormsg->Error(pos_, "type mismatch");
    }
    venv->Enter(var_, new env::VarEntry(declared_ty, false));
  } else {
    if (typeid(*exp_ty) == typeid(type::NilTy)) {
      errormsg->Error(pos_, "init should not be nil without type specified");
    }
    venv->Enter(var_, new env::VarEntry(exp_ty, false));
  }
}

void TypeDec::SemAnalyze(env::VEnvPtr venv, env::TEnvPtr tenv, int labelcount,
                         err::ErrorMsg *errormsg) const {
  /* TODO: Put your lab4 code here */
    std::set<std::string> seen_names;
    for (NameAndTy *name_and_ty : types_->GetList()) {
      if (seen_names.count(name_and_ty->name_->Name())) {
        errormsg->Error(pos_, "two types have the same name");
      }
      seen_names.insert(name_and_ty->name_->Name());
    tenv->Enter(name_and_ty->name_,
                new type::NameTy(name_and_ty->name_, nullptr));
  }

  for (NameAndTy *name_and_ty : types_->GetList()) {
    type::Ty *ty = tenv->Look(name_and_ty->name_);
    type::NameTy *name_ty = static_cast<type::NameTy *>(ty);
    name_ty->ty_ = name_and_ty->ty_->SemAnalyze(tenv, errormsg);
  }

  for (NameAndTy *name_and_ty : types_->GetList()) {
    type::Ty *ty = tenv->Look(name_and_ty->name_);
    type::Ty *cur = ty;
    bool has_cycle = false;

    if (cur != nullptr && typeid(*cur) == typeid(type::NameTy)) {
      type::Ty *tortoise = cur;
      type::Ty *hare = cur;

      while (hare != nullptr && typeid(*hare) == typeid(type::NameTy)) {
        type::NameTy *hare_name_ty = static_cast<type::NameTy *>(hare);
        hare = hare_name_ty->ty_;
        if (hare != nullptr && typeid(*hare) == typeid(type::NameTy)) {
          hare_name_ty = static_cast<type::NameTy *>(hare);
          hare = hare_name_ty->ty_;
        } else {
          break;
        }

        if (tortoise != nullptr && typeid(*tortoise) == typeid(type::NameTy)) {
          type::NameTy *tort_name_ty = static_cast<type::NameTy *>(tortoise);
          tortoise = tort_name_ty->ty_;
        } else {
          break;
        }

        if (tortoise == hare && tortoise != nullptr) {
          has_cycle = true;
          break;
        }
      }
    }

    if (has_cycle) {
      errormsg->Error(pos_, "illegal type cycle");
      break;
    }
  }
}

type::Ty *NameTy::SemAnalyze(env::TEnvPtr tenv, err::ErrorMsg *errormsg) const {
  /* TODO: Put your lab4 code here */
  type::Ty *ty = tenv->Look(name_);
  if (!ty) {
    errormsg->Error(pos_, " undefined type %s", name_->Name().data());
    return type::IntTy::Instance();
  }
  return new type::NameTy(name_, ty);
}

type::Ty *RecordTy::SemAnalyze(env::TEnvPtr tenv,
                               err::ErrorMsg *errormsg) const {
  /* TODO: Put your lab4 code here */
  type::FieldList *field_list = new type::FieldList();
  for (Field *field : record_->GetList()) {
    type::Ty *ty = tenv->Look(field->typ_);
    if (!ty) {
      errormsg->Error(field->pos_, " undefined type %s",
                      field->typ_->Name().data());
    }
    field_list->Append(new type::Field(field->name_, ty));
  }
  return new type::RecordTy(field_list);
}

type::Ty *ArrayTy::SemAnalyze(env::TEnvPtr tenv,
                              err::ErrorMsg *errormsg) const {
  /* TODO: Put your lab4 code here */
  type::Ty *ty = tenv->Look(array_);
  if (!ty) {
    errormsg->Error(pos_, " undefined type %s", array_->Name().data());
    return type::IntTy::Instance();
  }
  return new type::ArrayTy(ty);
}

type::Ty *ForExp::SemAnalyze(env::VEnvPtr venv, env::TEnvPtr tenv,
                             int labelcount, err::ErrorMsg *errormsg) const {
  type::Ty *lo_ty =
      lo_->SemAnalyze(venv, tenv, labelcount, errormsg)->ActualTy();
  type::Ty *hi_ty =
      hi_->SemAnalyze(venv, tenv, labelcount, errormsg)->ActualTy();

  if (typeid(*lo_ty) != typeid(type::IntTy)) {
    errormsg->Error(lo_->pos_, "for exp's range type is not integer");
  }
  if (typeid(*hi_ty) != typeid(type::IntTy)) {
    errormsg->Error(hi_->pos_, "for exp's range type is not integer");
  }

  venv->BeginScope();
  venv->Enter(var_, new env::VarEntry(type::IntTy::Instance(), true));

  if (body_) {
    body_->SemAnalyze(venv, tenv, labelcount + 1, errormsg);
  }

  venv->EndScope();
  return type::VoidTy::Instance();
}
} // namespace absyn

namespace sem {

void ProgSem::SemAnalyze() {
  FillBaseVEnv();
  FillBaseTEnv();
  absyn_tree_->SemAnalyze(venv_.get(), tenv_.get(), errormsg_.get());
}

} // namespace tr
