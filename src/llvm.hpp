#include <cstdint>
#include <string>
#include <vector>

namespace ib::llvm {

void init();

std::vector<uint8_t> compile(const std::string &asmStr);

} // namespace ib::llvm
