#ifndef SAQUT_CLI_EXIT_CODES
#define SAQUT_CLI_EXIT_CODES

namespace saqut::exit_code {
constexpr int kSuccess = 0;
constexpr int kUsageError = 64;
constexpr int kDataError = 65;
constexpr int kSoftwareError = 70;
}  // namespace saqut::exit_code

#endif  // SAQUT_CLI_EXIT_CODES
