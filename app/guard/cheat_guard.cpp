#include "guard/cheat_guard.h"

#include <chrono>
#include <ratio>
#include <string>
#include <utility>

#include "ro/core/log.h"
#include "ro/net/cheat_report.h"

#if defined(_WIN32)
#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <windows.h>
// Après <windows.h>, qu'ils supposent déjà lu.
#include <SDL3/SDL_system.h>
#include <hidusage.h>
#include <tlhelp32.h>
#endif

namespace moonclient {

namespace {

// Le délai entre deux balayages des noms connus.
constexpr auto kScanInterval = std::chrono::minutes(1);

#if defined(_WIN32)

constexpr std::uint64_t kNanosecondsInSecond = std::nano::den;

// LA PAGE QUE LE NOYAU PARTAGE AVEC CHAQUE PROCESSUS, à la même adresse
// partout, et son temps d'interruption : trois mots de 32 bits, en centaines de
// nanosecondes. Le noyau écrit le mot haut deux fois, avant et après le mot
// bas : une lecture où les deux hauts diffèrent a croisé une écriture.
constexpr std::uintptr_t kKernelSharedPage = 0x7FFE0000;
constexpr std::uintptr_t kInterruptTimeOffset = 0x08;
constexpr std::uint64_t kNanosecondsPerKernelUnit = 100;

struct KernelTime {
  ULONG low;
  LONG high_first;
  LONG high_second;
};

std::uint64_t KernelInterruptNs() {
  const auto* time =
      reinterpret_cast<const volatile KernelTime*>(kKernelSharedPage + kInterruptTimeOffset);
  LONG high = 0;
  ULONG low = 0;
  do {
    high = time->high_first;
    low = time->low;
  } while (high != time->high_second);
  const std::uint64_t units = (static_cast<std::uint64_t>(static_cast<ULONG>(high)) << 32) | low;
  return units * kNanosecondsPerKernelUnit;
}

std::uint64_t PerformanceCounterNs() {
  LARGE_INTEGER frequency{};
  LARGE_INTEGER counter{};
  QueryPerformanceFrequency(&frequency);
  QueryPerformanceCounter(&counter);
  const auto ticks = static_cast<std::uint64_t>(counter.QuadPart);
  const auto per_second = static_cast<std::uint64_t>(frequency.QuadPart);
  if (per_second == 0) {
    return 0;
  }
  // En deux temps : `ticks * 10^9` déborderait au bout de quelques heures.
  return ticks / per_second * kNanosecondsInSecond +
         ticks % per_second * kNanosecondsInSecond / per_second;
}

std::string Narrow(const wchar_t* wide) {
  const int bytes = WideCharToMultiByte(CP_UTF8, 0, wide, -1, nullptr, 0, nullptr, nullptr);
  if (bytes <= 1) {
    return {};
  }
  std::string narrow(static_cast<std::size_t>(bytes - 1), '\0');
  WideCharToMultiByte(CP_UTF8, 0, wide, -1, narrow.data(), bytes, nullptr, nullptr);
  return narrow;
}

// LE CROCHET DES MESSAGES DE LA FENÊTRE, appelé par SDL sur le thread principal
// avant chaque traduction. Il observe et laisse toujours passer.
bool SDLCALL WatchWindowMessage(void* userdata, MSG* message) {
  auto* guard = static_cast<CheatGuard*>(userdata);
  if (message->message == WM_INPUT) {
    RAWINPUT input{};
    UINT size = sizeof(input);
    if (GetRawInputData(reinterpret_cast<HRAWINPUT>(message->lParam), RID_INPUT, &input, &size,
                        sizeof(RAWINPUTHEADER)) != static_cast<UINT>(-1) &&
        input.header.dwType == RIM_TYPEKEYBOARD &&
        (input.data.keyboard.Flags & RI_KEY_BREAK) == 0) {
      guard->NoteRawKey(input.header.hDevice != nullptr);
    }
  } else if (message->message == WM_KEYDOWN || message->message == WM_SYSKEYDOWN) {
    const auto virtual_key = static_cast<int>(message->wParam);
    // La saisie d'une méthode d'entrée (IME) ne nomme pas la touche frappée.
    if (virtual_key != VK_PROCESSKEY) {
      guard->NoteKeyMessage(GetAsyncKeyState(virtual_key) < 0);
    }
  }
  return true;
}

// L'ENTRÉE BRUTE DU CLAVIER, pour la fenêtre qui a le focus. Les messages de
// touche ordinaires continuent d'arriver : SDL lit ceux-là.
bool RegisterRawKeyboard() {
  RAWINPUTDEVICE keyboard{};
  keyboard.usUsagePage = HID_USAGE_PAGE_GENERIC;
  keyboard.usUsage = HID_USAGE_GENERIC_KEYBOARD;
  keyboard.dwFlags = 0;
  keyboard.hwndTarget = nullptr;
  return RegisterRawInputDevices(&keyboard, 1, sizeof(keyboard)) != FALSE;
}

template <typename Found>
void ScanProcesses(Found&& found) {
  HANDLE snapshot = CreateToolhelp32Snapshot(TH32CS_SNAPPROCESS, 0);
  if (snapshot == INVALID_HANDLE_VALUE) {
    return;
  }
  PROCESSENTRY32W entry{};
  entry.dwSize = sizeof(entry);
  for (BOOL more = Process32FirstW(snapshot, &entry); more != FALSE;
       more = Process32NextW(snapshot, &entry)) {
    if (auto finding =
            MatchSignature(KnownSignatures(), GuardProbe::kProcess, Narrow(entry.szExeFile))) {
      found(std::move(*finding));
    }
  }
  CloseHandle(snapshot);
}

template <typename Found>
void ScanModules(Found&& found) {
  HANDLE snapshot = CreateToolhelp32Snapshot(TH32CS_SNAPMODULE, 0);
  if (snapshot == INVALID_HANDLE_VALUE) {
    return;
  }
  MODULEENTRY32W entry{};
  entry.dwSize = sizeof(entry);
  for (BOOL more = Module32FirstW(snapshot, &entry); more != FALSE;
       more = Module32NextW(snapshot, &entry)) {
    if (auto finding =
            MatchSignature(KnownSignatures(), GuardProbe::kModule, Narrow(entry.szModule))) {
      found(std::move(*finding));
    }
  }
  CloseHandle(snapshot);
}

std::vector<GuardFinding> ScanWindows() {
  std::vector<GuardFinding> findings;
  EnumWindows(
      [](HWND window, LPARAM context) -> BOOL {
        constexpr int kClassNameChars = 256;
        wchar_t class_name[kClassNameChars] = {};
        if (GetClassNameW(window, class_name, kClassNameChars) > 0) {
          if (auto finding =
                  MatchSignature(KnownSignatures(), GuardProbe::kWindowClass, Narrow(class_name))) {
            reinterpret_cast<std::vector<GuardFinding>*>(context)->push_back(std::move(*finding));
          }
        }
        return TRUE;
      },
      reinterpret_cast<LPARAM>(&findings));
  return findings;
}

#endif

}  // namespace

CheatGuard::~CheatGuard() {
#if defined(_WIN32)
  if (started_) {
    SDL_SetWindowsMessageHook(nullptr, nullptr);
  }
#endif
}

void CheatGuard::Start() {
  if (started_) {
    return;
  }
  started_ = true;
#if defined(_WIN32)
  if (!RegisterRawKeyboard()) {
    ro::core::LogWarning("Garde : l'entrée brute du clavier est refusée (erreur {})",
                         GetLastError());
  }
  SDL_SetWindowsMessageHook(&WatchWindowMessage, this);
  // Sans table de noms, le balayage n'a rien à chercher.
  if (KnownSignatures().empty()) {
    ro::core::LogInfo("Garde : aucune table de noms à la bâtisse, le balayage est éteint");
  } else {
    scan_thread_ = std::jthread([this](const std::stop_token& stop) { ScanLoop(stop); });
  }
#endif
}

void CheatGuard::ScanLoop(const std::stop_token& stop) {
#if defined(_WIN32)
  while (!stop.stop_requested()) {
    std::vector<GuardFinding> found = ScanWindows();
    const auto keep = [&found](GuardFinding finding) { found.push_back(std::move(finding)); };
    ScanProcesses(keep);
    ScanModules(keep);
    std::unique_lock lock(scanned_mutex_);
    for (GuardFinding& finding : found) {
      scanned_.push_back(std::move(finding));
    }
    scan_wake_.wait_for(lock, stop, kScanInterval, [] { return false; });
  }
#else
  static_cast<void>(stop);
#endif
}

void CheatGuard::Report(GuardFinding finding) {
  const std::string tool = finding.tool;
  const std::string detail = finding.detail;
  if (spokesman_.Note(std::move(finding))) {
    ro::core::LogWarning("Garde : {} ({})", tool, detail);
  }
}

void CheatGuard::Pump(ro::net::Connection& link, bool session_verified, std::uint32_t now_ms) {
#if defined(_WIN32)
  if (started_) {
    if (auto finding = clocks_.Note(PerformanceCounterNs(), KernelInterruptNs())) {
      Report(std::move(*finding));
    }
  }
#endif
  if (auto finding = keys_.TakeVerdict()) {
    Report(std::move(*finding));
  }
  std::vector<GuardFinding> scanned;
  {
    std::scoped_lock lock(scanned_mutex_);
    scanned.swap(scanned_);
  }
  for (GuardFinding& finding : scanned) {
    Report(std::move(finding));
  }
  if (auto due = spokesman_.Due(session_verified, now_ms)) {
    ro::net::SendCheatReport(link, due->tool, due->detail);
    ro::core::LogInfo("Garde : signalé au serveur — {} ({})", due->tool, due->detail);
  }
}

}  // namespace moonclient
