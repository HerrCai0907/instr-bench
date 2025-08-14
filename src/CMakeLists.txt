set(CMAKE_CXX_STANDARD 20)

enable_language(ASM)

aux_source_directory(${CMAKE_CURRENT_SOURCE_DIR} instr_bench_src)

add_executable(instr_bench ${instr_bench_src} trampoline.s)
add_dependencies(instr_bench llvm-project-build spdlog-build)

target_include_directories(instr_bench PRIVATE SYSTEM ${LLVM_INCLUDE_DIRS})
target_compile_definitions(instr_bench PRIVATE ${LLVM_DEFINITIONS})

llvm_map_components_to_libnames(LLVM_LIBS
  AllTargetsAsmParsers
  AllTargetsDescs
  AllTargetsDisassemblers
  AllTargetsInfos
  MC
  MCParser
  Support
  TargetParser
  DWARFCFIChecker)
target_link_libraries(instr_bench PRIVATE ${LLVM_LIBS} spdlog::spdlog)
