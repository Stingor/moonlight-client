#include "ro/core/program_arguments.h"

#ifdef _WIN32
#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
// clang-format off
// L'ordre est SÉMANTIQUE : shellapi.h ne compile pas sans les types de
// windows.h. Les trier alphabétiquement casse la compilation.
#include <windows.h>
#include <shellapi.h>
// clang-format on
#endif

namespace ro::core {

std::vector<std::string> ProgramArguments(int argc, char** argv) {
  std::vector<std::string> args;
#ifdef _WIN32
  int wide_count = 0;
  LPWSTR* wide = CommandLineToArgvW(GetCommandLineW(), &wide_count);
  if (wide != nullptr) {
    for (int index = 1; index < wide_count; ++index) {
      const int bytes =
          WideCharToMultiByte(CP_UTF8, 0, wide[index], -1, nullptr, 0, nullptr, nullptr);
      if (bytes <= 1) {
        args.emplace_back();
        continue;
      }
      std::string utf8(static_cast<std::size_t>(bytes - 1), '\0');
      WideCharToMultiByte(CP_UTF8, 0, wide[index], -1, utf8.data(), bytes, nullptr, nullptr);
      args.push_back(std::move(utf8));
    }
    LocalFree(wide);
    return args;
  }
  // La lecture de la ligne de commande large a échoué : on se rabat sur argv,
  // dégradé mais mieux que rien.
#endif
  for (int index = 1; index < argc; ++index) {
    args.emplace_back(argv[index]);
  }
  return args;
}

}  // namespace ro::core
