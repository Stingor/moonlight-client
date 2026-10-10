#include "ro/core/console.h"

#include <cstdio>
#include <cstdlib>
#include <iostream>

#if defined(_WIN32)
#include <io.h>
#include <windows.h>
#else
#include <termios.h>
#include <unistd.h>
#endif

namespace ro::core {

void UseUtf8Console() {
#if defined(_WIN32)
  // ⚠ LES DEUX : la SORTIE pour ce qu'on écrit, l'ENTRÉE pour ce qu'on lit —
  // un compte saisi avec un accent, à la question de `ReadSecret`, reviendrait
  // sinon en octets d'une autre page.
  SetConsoleOutputCP(CP_UTF8);
  SetConsoleCP(CP_UTF8);
#endif
}

namespace {

// Interdit — puis rétablit — l'écho des frappes, le temps d'une saisie.
//
// Le rétablissement passe par un destructeur : une saisie interrompue laisserait
// sinon le terminal muet pour tout ce qui suit, y compris le shell.
class EchoOff {
 public:
  EchoOff() {
#if defined(_WIN32)
    handle_ = GetStdHandle(STD_INPUT_HANDLE);
    if (handle_ != INVALID_HANDLE_VALUE && GetConsoleMode(handle_, &previous_) != 0) {
      restore_ = true;
      SetConsoleMode(handle_, previous_ & ~static_cast<DWORD>(ENABLE_ECHO_INPUT));
    }
#else
    if (tcgetattr(STDIN_FILENO, &previous_) == 0) {
      restore_ = true;
      termios quiet = previous_;
      quiet.c_lflag &= ~static_cast<tcflag_t>(ECHO);
      tcsetattr(STDIN_FILENO, TCSAFLUSH, &quiet);
    }
#endif
  }

  ~EchoOff() {
    if (!restore_) {
      return;
    }
#if defined(_WIN32)
    SetConsoleMode(handle_, previous_);
#else
    tcsetattr(STDIN_FILENO, TCSAFLUSH, &previous_);
#endif
  }

  EchoOff(const EchoOff&) = delete;
  EchoOff& operator=(const EchoOff&) = delete;
  EchoOff(EchoOff&&) = delete;
  EchoOff& operator=(EchoOff&&) = delete;

 private:
  bool restore_ = false;
#if defined(_WIN32)
  HANDLE handle_ = nullptr;
  DWORD previous_ = 0;
#else
  termios previous_ = {};
#endif
};

bool StandardInputIsTerminal() {
#if defined(_WIN32)
  return _isatty(_fileno(stdin)) != 0;
#else
  return isatty(STDIN_FILENO) != 0;
#endif
}

}  // namespace

std::optional<std::string> Environment(const char* name) {
#if defined(_WIN32)
  char* value = nullptr;
  std::size_t length = 0;
  if (_dupenv_s(&value, &length, name) != 0 || value == nullptr) {
    return std::nullopt;
  }
  std::string copy(value);
  std::free(value);
  return copy;
#else
  const char* value = std::getenv(name);
  return value == nullptr ? std::nullopt : std::optional<std::string>(value);
#endif
}

std::optional<std::string> ReadSecret(std::string_view question) {
  if (!StandardInputIsTerminal()) {
    return std::nullopt;
  }

  std::fwrite(question.data(), 1, question.size(), stderr);
  std::fflush(stderr);

  std::string answer;
  {
    const EchoOff quiet;
    if (!std::getline(std::cin, answer)) {
      std::fputc('\n', stderr);
      return std::nullopt;
    }
  }
  // L'écho était coupé : le retour à la ligne de l'utilisateur ne s'est pas
  // affiché, et sans celui-ci la ligne suivante se collerait à la question.
  std::fputc('\n', stderr);

  // Windows laisse le retour chariot des fins de ligne CRLF sur les entrées qui
  // n'ont pas été ouvertes en mode texte.
  if (!answer.empty() && answer.back() == '\r') {
    answer.pop_back();
  }
  return answer;
}

}  // namespace ro::core
