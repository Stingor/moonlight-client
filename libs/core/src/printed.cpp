#include "ro/core/printed.h"

#include <cstdarg>
#include <cstddef>
#include <cstdio>

namespace ro::core {

std::string Printed(const char* format, ...) {
  std::va_list measuring;
  va_start(measuring, format);
  std::va_list writing;
  va_copy(writing, measuring);
#if defined(__GNUC__)
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wformat-nonliteral"
#endif
  // La première passe ne fait que compter ; la seconde écrit, zéro final
  // compris, dans un texte qui a déjà sa taille.
  const int needed = std::vsnprintf(nullptr, 0, format, measuring);
  std::string text;
  if (needed > 0) {
    text.resize(static_cast<std::size_t>(needed));
    std::vsnprintf(text.data(), text.size() + 1, format, writing);
  }
#if defined(__GNUC__)
#pragma GCC diagnostic pop
#endif
  va_end(writing);
  va_end(measuring);
  return text;
}

}  // namespace ro::core
