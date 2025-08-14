#include <cstring>
#include <memory>
#include <optional>
#include <spdlog/spdlog.h>
#include <string>

#include "llvm.hpp"
#include "llvm/CodeGen/AsmPrinter.h"
#include "llvm/IR/LLVMContext.h"
#include "llvm/MC/MCAsmBackend.h"
#include "llvm/MC/MCAsmInfo.h"
#include "llvm/MC/MCAssembler.h"
#include "llvm/MC/MCCodeEmitter.h"
#include "llvm/MC/MCContext.h"
#include "llvm/MC/MCInst.h"
#include "llvm/MC/MCInstPrinter.h"
#include "llvm/MC/MCInstrInfo.h"
#include "llvm/MC/MCObjectFileInfo.h"
#include "llvm/MC/MCObjectWriter.h"
#include "llvm/MC/MCParser/MCAsmParser.h"
#include "llvm/MC/MCParser/MCTargetAsmParser.h"
#include "llvm/MC/MCRegisterInfo.h"
#include "llvm/MC/MCStreamer.h"
#include "llvm/MC/MCSubtargetInfo.h"
#include "llvm/MC/MCTargetOptions.h"
#include "llvm/MC/TargetRegistry.h"
#include "llvm/Support/SMLoc.h"
#include "llvm/Support/SourceMgr.h"
#include "llvm/Support/TargetSelect.h"
#include "llvm/Target/TargetMachine.h"
#include "llvm/Target/TargetOptions.h"
#include "llvm/TargetParser/Host.h"
#include "machine_code.hpp"

using namespace llvm;

class IbStreamer : public MCStreamer {
  std::unique_ptr<MCCodeEmitter> code_emitter_;

public:
  SmallString<256> code_;

  IbStreamer(MCContext &Context, std::unique_ptr<MCCodeEmitter> code_emitter)
      : MCStreamer(Context), code_emitter_(std::move(code_emitter)) {}

  void initSections(bool NoExecStack, const MCSubtargetInfo &STI) override {}

  void emitInstruction(const MCInst &inst,
                       const MCSubtargetInfo &sub_target_info) override {
    SmallVector<MCFixup, 4> Fixups;
    code_emitter_->encodeInstruction(inst, code_, Fixups, sub_target_info);
  }

  bool hasRawTextSupport() const override { return true; }
  void emitRawTextImpl(StringRef String) override {}

  bool emitSymbolAttribute(MCSymbol *Symbol, MCSymbolAttr Attribute) override {
    return true;
  }

  void emitCommonSymbol(MCSymbol *Symbol, uint64_t Size,
                        Align ByteAlignment) override {}
  void emitZerofill(MCSection *Section, MCSymbol *Symbol = nullptr,
                    uint64_t Size = 0, Align ByteAlignment = Align(1),
                    SMLoc Loc = SMLoc()) override {}
  void beginCOFFSymbolDef(const MCSymbol *Symbol) override {}
  void emitCOFFSymbolStorageClass(int StorageClass) override {}
  void emitCOFFSymbolType(int Type) override {}
  void endCOFFSymbolDef() override {}
  void emitXCOFFSymbolLinkageWithVisibility(MCSymbol *Symbol,
                                            MCSymbolAttr Linkage,
                                            MCSymbolAttr Visibility) override {}
};

static Target const *getTarget() {
  using namespace llvm;
  std::string error;
  const Target *target =
      TargetRegistry::lookupTarget(llvm::sys::getDefaultTargetTriple(), error);
  if (!target) {
    spdlog::error("LLVM Target lookup failed: {}", error);
    abort();
  }
  return target;
}

void ib::llvm::init() {
  LLVMContext context;
  InitializeNativeTargetAsmParser();
  InitializeAllTargetInfos();
  InitializeAllTargetMCs();
  InitializeNativeTargetAsmParser();
  spdlog::info("LLVM initialized successfully!");
}

std::unique_ptr<ib::MachineCode> ib::llvm::compile(const std::string &asmStr) {
  const Target *target = getTarget();
  Triple triple{sys::getDefaultTargetTriple()};

  std::unique_ptr<MCRegisterInfo> register_info(
      target->createMCRegInfo(sys::getDefaultTargetTriple()));
  if (!register_info) {
    spdlog::error("Unable to create MCRegisterInfo");
    abort();
  }
  std::unique_ptr<MCAsmInfo> asm_info(target->createMCAsmInfo(
      *register_info, sys::getDefaultTargetTriple(), MCTargetOptions{}));
  if (!asm_info) {
    spdlog::error("Unable to create MCAsmInfo");
    abort();
  }
  std::unique_ptr<MCInstrInfo> instr_info(target->createMCInstrInfo());
  if (!instr_info) {
    spdlog::error("Unable to create MCInstrInfo");
    abort();
  }
  std::unique_ptr<MCSubtargetInfo> sub_target_info(
      target->createMCSubtargetInfo(sys::getDefaultTargetTriple(), "", ""));
  if (!sub_target_info) {
    spdlog::error("Unable to create MCSubtargetInfo");
    abort();
  }

  SourceMgr source_mgr;
  source_mgr.AddNewSourceBuffer(MemoryBuffer::getMemBuffer(asmStr, "<inline>"),
                                SMLoc());

  MCTargetOptions target_options;
  MCContext context{Triple{sys::getDefaultTargetTriple()},
                    asm_info.get(),
                    register_info.get(),
                    sub_target_info.get(),
                    &source_mgr,
                    &target_options};

  std::unique_ptr<TargetMachine> target_machine{
      target->createTargetMachine(Triple{sys::getDefaultTargetTriple()}, "", "",
                                  TargetOptions{}, std::nullopt)};

  // std::unique_ptr<MCAsmBackend> asm_backend{target->createMCAsmBackend(
  //     *sub_target_info, *register_info, target_options)};
  // std::unique_ptr<MCCodeEmitter> code_emitter{
  //     target->createMCCodeEmitter(*instr_info, context)};
  // std::unique_ptr<MCObjectWriter> object_writer{};

  // MCAssembler assembler{context, std::move(asm_backend),
  //                       std::move(code_emitter), std::move(object_writer)};

  std::unique_ptr<IbStreamer> ib_streamer{new IbStreamer{
      context, std::unique_ptr<MCCodeEmitter>{
                   target->createMCCodeEmitter(*instr_info, context)}}};
  std::unique_ptr<MCAsmParser> asm_parser{
      createMCAsmParser(source_mgr, context, *ib_streamer, *asm_info)};
  MCTargetAsmParser *target_asm_parser = target->createMCAsmParser(
      *sub_target_info, *asm_parser, *instr_info, target_options);
  asm_parser->setTargetParser(*target_asm_parser);

  // start
  MCInst inst;

  int Res = asm_parser->Run(true);

  std::unique_ptr<ib::MachineCode> ret{new ib::MachineCode()};
  ret->resize(ib_streamer->code_.size());
  std::memcpy(ret->data(), ib_streamer->code_.data(),
              ib_streamer->code_.size());
  return ret;
}
