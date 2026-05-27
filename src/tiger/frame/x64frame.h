#ifndef TIGER_COMPILER_X64FRAME_H
#define TIGER_COMPILER_X64FRAME_H

#include "tiger/frame/frame.h"

namespace frame {

// x86-64 register indices
enum X64Reg {
  REG_RAX = 0,
  REG_RCX,
  REG_RDX,
  REG_RSI,
  REG_RDI,
  REG_R8,
  REG_R9,
  REG_RBX,
  REG_RBP,
  REG_R10,
  REG_R11,
  REG_R12,
  REG_R13,
  REG_R14,
  REG_R15,
  REG_RSP,
  REG_COUNT
};

class X64RegManager : public RegManager {
public:
  X64RegManager();

  [[nodiscard]] temp::TempList *Registers() override;

  [[nodiscard]] temp::TempList *ArgRegs() override;

  [[nodiscard]] temp::TempList *CallerSaves() override;

  [[nodiscard]] temp::TempList *CalleeSaves() override;

  [[nodiscard]] temp::TempList *ReturnSink() override;

  [[nodiscard]] int WordSize() override;

  [[nodiscard]] temp::Temp *FramePointer() override;

  [[nodiscard]] temp::Temp *StackPointer() override;

  [[nodiscard]] temp::Temp *ReturnValue() override;
};

} // namespace frame
#endif // TIGER_COMPILER_X64FRAME_H
