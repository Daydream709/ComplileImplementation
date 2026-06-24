#include "tiger/regalloc/regalloc.h"

#include "tiger/frame/x64frame.h"
#include "tiger/output/logger.h"

#include <cassert>
#include <map>
#include <set>
#include <sstream>
#include <vector>

extern frame::RegManager *reg_manager;

namespace ra {
namespace {

temp::TempList *TL(temp::Temp *t) { return new temp::TempList({t}); }

std::string FrameOffset(int offset, const std::string &fs) {
  std::ostringstream out;
  out << offset << "(%rbp)";
  return out.str();
}

} // namespace

Result::~Result() = default;

temp::TempList *RegAllocator::AllocatableRegs() {
  return new temp::TempList({
      reg_manager->GetRegister(frame::X64Reg::REG_RBX),
      reg_manager->GetRegister(frame::X64Reg::REG_R12),
      reg_manager->GetRegister(frame::X64Reg::REG_R13),
      reg_manager->GetRegister(frame::X64Reg::REG_R14),
      reg_manager->GetRegister(frame::X64Reg::REG_R15),
  });
}

std::string RegAllocator::SpillAddress(temp::Temp *temp) {
  auto it = spill_offsets_.find(temp);
  if (it == spill_offsets_.end()) {
    frame_->local_offset_ -= frame_->word_size_;
    it = spill_offsets_.emplace(temp, frame_->local_offset_).first;
  }
  return FrameOffset(it->second, frame_->GetLabel() + "_framesize");
}

void RegAllocator::RewriteProgram(live::INodeListPtr spills) {
  std::set<temp::Temp *> spill_temps;
  for (auto node : spills->GetList())
    spill_temps.insert(node->NodeInfo());

  auto *rewritten = new assem::InstrList();

  for (auto instr : il_->GetList()) {
    std::vector<assem::Instr *> before, after;
    std::map<temp::Temp *, temp::Temp *> replacement;
    std::set<temp::Temp *> loaded, stored;

    auto fresh_for = [&](temp::Temp *t) -> temp::Temp * {
      auto it = replacement.find(t);
      if (it != replacement.end())
        return it->second;
      temp::Temp *fresh = temp::TempFactory::NewTemp();
      replacement[t] = fresh;
      return fresh;
    };

    auto replace_list = [&](temp::TempList *list, bool is_use,
                            bool is_def) -> temp::TempList * {
      if (!list)
        return nullptr;
      auto *result = new temp::TempList();
      for (auto t : list->GetList()) {
        if (!spill_temps.count(t)) {
          result->Append(t);
          continue;
        }
        temp::Temp *fresh = fresh_for(t);
        if (is_use && !loaded.count(t)) {
          before.push_back(new assem::MoveInstr(
              "movq " + SpillAddress(t) + ", `d0", TL(fresh), nullptr));
          loaded.insert(t);
        }
        if (is_def && !stored.count(t)) {
          after.push_back(new assem::MoveInstr(
              "movq `s0, " + SpillAddress(t), nullptr, TL(fresh)));
          stored.insert(t);
        }
        result->Append(fresh);
      }
      return result;
    };

    if (auto oper = dynamic_cast<assem::OperInstr *>(instr)) {
      oper->src_ = replace_list(oper->src_, true, false);
      oper->dst_ = replace_list(oper->dst_, false, true);
    } else if (auto move = dynamic_cast<assem::MoveInstr *>(instr)) {
      move->src_ = replace_list(move->src_, true, false);
      move->dst_ = replace_list(move->dst_, false, true);
    }

    for (auto load : before)
      rewritten->Append(load);
    rewritten->Append(instr);
    for (auto store : after)
      rewritten->Append(store);
  }

  il_ = rewritten;
}

void RegAllocator::RegAlloc() {
  constexpr int max_iterations = 64;
  for (int iter = 0; iter < max_iterations; ++iter) {
    fg::FlowGraphFactory flow_factory(il_);
    flow_factory.AssemFlowGraph();

    live::LiveGraphFactory live_factory(flow_factory.GetFlowGraph());
    live_factory.Liveness();

    col::Color color(live_factory.GetLiveGraph(), reg_manager->temp_map_,
                     AllocatableRegs());
    col::Result result = color.Coloring();
    if (result.spills->GetList().empty()) {
      coloring_ = result.coloring;
      return;
    }

    RewriteProgram(result.spills);
  }

  assert(false);
}

std::unique_ptr<Result> RegAllocator::BuildAllocationResult() {
  return std::make_unique<Result>(coloring_, il_);
}

} // namespace ra
