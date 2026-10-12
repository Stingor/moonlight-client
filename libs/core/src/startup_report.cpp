#include "ro/core/startup_report.h"

#include <array>
#include <cstdint>
#include <format>
#include <fstream>
#include <iterator>
#include <string>
#include <system_error>
#include <thread>
#include <vector>

#include "ro/core/log.h"
#include "ro/core/utf8_path.h"

#if defined(_WIN32)
#include <windows.h>
// `EnumProcessModules` vit ici, et non dans `windows.h`.
#include <psapi.h>
#endif
#if defined(_MSC_VER)
#include <intrin.h>
#endif

namespace ro::core {
namespace {

constexpr std::uint64_t kBytesPerMebibyte = 1024U * 1024U;
// Le fichier d'essai d'écriture, et ce qu'on y écrit puis relit.
constexpr std::string_view kProbeFileName = "moonclient.ecriture.tmp";
constexpr std::string_view kProbeText = "moonclient : essai d'écriture\n";

[[nodiscard]] std::string CurrentFolder() {
  std::error_code failed;
  const std::filesystem::path folder = std::filesystem::current_path(failed);
  return failed ? std::string("(inconnu)") : Utf8FromPath(folder);
}

#if defined(_MSC_VER)

// Les feuilles de `cpuid` qu'on interroge, et les registres qu'elles rendent.
constexpr int kCpuidBasicFeatures = 1;
constexpr int kCpuidExtendedFeatures = 7;
constexpr unsigned kCpuidHighestExtended = 0x80000000U;
constexpr unsigned kCpuidBrandFirst = 0x80000002U;
constexpr unsigned kCpuidBrandLast = 0x80000004U;
constexpr std::size_t kEbx = 1;
constexpr std::size_t kEcx = 2;
// Les bits de capacité, tels que les documente le manuel du processeur.
constexpr int kSse42Bit = 20;       // feuille 1, ECX
constexpr int kOsSavesAvxBit = 27;  // feuille 1, ECX : le système sauve les registres AVX
constexpr int kAvxBit = 28;         // feuille 1, ECX
constexpr int kAvx2Bit = 5;         // feuille 7, EBX

using CpuidRegisters = std::array<int, 4>;

[[nodiscard]] CpuidRegisters Cpuid(int leaf) {
  CpuidRegisters registers{};
  __cpuidex(registers.data(), leaf, 0);
  return registers;
}

[[nodiscard]] bool HasBit(int value, int bit) {
  return ((static_cast<unsigned>(value) >> static_cast<unsigned>(bit)) & 1U) != 0;
}

// Le nom commercial du processeur, et les jeux d'instructions qu'un pilote ou
// une bibliothèque peut exiger sans le dire.
[[nodiscard]] std::string DescribeProcessor() {
  std::string brand;
  if (static_cast<unsigned>(Cpuid(static_cast<int>(kCpuidHighestExtended))[0]) >= kCpuidBrandLast) {
    for (unsigned leaf = kCpuidBrandFirst; leaf <= kCpuidBrandLast; ++leaf) {
      const CpuidRegisters part = Cpuid(static_cast<int>(leaf));
      brand.append(reinterpret_cast<const char*>(part.data()), sizeof(part));
    }
    brand = brand.c_str();
  }
  const CpuidRegisters basic = Cpuid(kCpuidBasicFeatures);
  const CpuidRegisters extended = Cpuid(kCpuidExtendedFeatures);
  const bool avx = HasBit(basic[kEcx], kAvxBit) && HasBit(basic[kEcx], kOsSavesAvxBit);
  return std::format("{} — SSE4.2 {}, AVX {}, AVX2 {}", brand.empty() ? "(inconnu)" : brand,
                     HasBit(basic[kEcx], kSse42Bit) ? "oui" : "non", avx ? "oui" : "non",
                     avx && HasBit(extended[kEbx], kAvx2Bit) ? "oui" : "non");
}

#else

[[nodiscard]] std::string DescribeProcessor() { return "(non relevé sur cette plateforme)"; }

#endif

#if defined(_WIN32)

// Un chemin Windows tient dans 32 767 caractères.
constexpr DWORD kLongPathChars = 32'768;
// Combien de modules on énumère au plus : un client en charge une centaine.
constexpr std::size_t kModuleCapacity = 1024;

[[nodiscard]] std::wstring ModulePath(HMODULE module) {
  std::wstring path(kLongPathChars, L'\0');
  const DWORD written = GetModuleFileNameW(module, path.data(), kLongPathChars);
  path.resize(written);
  return path;
}

// ⚠ RtlGetVersion ET NON GetVersionEx : celle-ci ment, et répond « Windows 8 »
// à tout programme dont le manifeste ne déclare pas les versions suivantes.
[[nodiscard]] std::string DescribeWindows() {
  using RtlGetVersionCall = LONG(WINAPI*)(OSVERSIONINFOW*);
  const HMODULE ntdll = GetModuleHandleW(L"ntdll.dll");
  const auto get_version =
      ntdll == nullptr
          ? nullptr
          : reinterpret_cast<RtlGetVersionCall>(GetProcAddress(ntdll, "RtlGetVersion"));
  OSVERSIONINFOW version = {};
  version.dwOSVersionInfoSize = sizeof(version);
  if (get_version == nullptr || get_version(&version) != 0) {
    return "Windows (version inconnue)";
  }
  return std::format("Windows {}.{} build {}", version.dwMajorVersion, version.dwMinorVersion,
                     version.dwBuildNumber);
}

[[nodiscard]] bool RunsElevated() {
  HANDLE token = nullptr;
  if (OpenProcessToken(GetCurrentProcess(), TOKEN_QUERY, &token) == 0) {
    return false;
  }
  TOKEN_ELEVATION elevation = {};
  DWORD written = 0;
  const bool read =
      GetTokenInformation(token, TokenElevation, &elevation, sizeof(elevation), &written) != 0;
  CloseHandle(token);
  return read && elevation.TokenIsElevated != 0;
}

[[nodiscard]] std::string DescribeMemory() {
  MEMORYSTATUSEX status = {};
  status.dwLength = sizeof(status);
  if (GlobalMemoryStatusEx(&status) == 0) {
    return "(inconnue)";
  }
  return std::format("{} Mio dont {} libres", status.ullTotalPhys / kBytesPerMebibyte,
                     status.ullAvailPhys / kBytesPerMebibyte);
}

// Vrai si `path` se trouve sous `folder`, sans égard à la casse.
[[nodiscard]] bool IsUnder(const std::wstring& path, const std::wstring& folder) {
  return !folder.empty() && path.size() > folder.size() &&
         _wcsnicmp(path.c_str(), folder.c_str(), folder.size()) == 0 &&
         (path[folder.size()] == L'\\' || folder.back() == L'\\');
}

// 🔴 LE NOM EST TENU PAR LE PROCESSUS JUSQU'À SA MORT : la poignée n'est
// jamais refermée, et c'est voulu — c'est elle qui dit au client suivant que
// celui-ci tourne encore.
void WarnOfOtherClient() {
  const HANDLE named = CreateMutexW(nullptr, FALSE, L"Local\\moonclient");
  if (named != nullptr && GetLastError() == ERROR_ALREADY_EXISTS) {
    LogWarning(
        "machine : un autre moonclient tourne déjà sur cette session Windows — s'il est figé, le "
        "fermer depuis le gestionnaire des tâches");
  }
}

#endif

}  // namespace

void LogMachine() {
#if defined(_WIN32)
  const std::filesystem::path executable(ModulePath(nullptr));
  LogInfo("machine : {}, {}", DescribeWindows(),
          RunsElevated() ? "lancé en administrateur" : "droits ordinaires");
  LogInfo("machine : processeur {} ({} fils), mémoire {}", DescribeProcessor(),
          std::thread::hardware_concurrency(), DescribeMemory());
  LogInfo("machine : exécutable {}", Utf8FromPath(executable));
  std::error_code failed;
  const std::filesystem::path current = std::filesystem::current_path(failed);
  const bool beside_executable =
      !failed && std::filesystem::equivalent(executable.parent_path(), current, failed);
  if (failed || !beside_executable) {
    LogWarning(
        "machine : lancé depuis {}, qui n'est pas le dossier de l'exécutable — le journal et les "
        "rapports de plantage sont écrits là",
        CurrentFolder());
  }
  WarnOfOtherClient();
#else
  LogInfo("machine : processeur {} ({} fils), dossier courant {}", DescribeProcessor(),
          std::thread::hardware_concurrency(), CurrentFolder());
#endif
}

void LogForeignModules(std::string_view moment) {
#if defined(_WIN32)
  std::wstring windows(kLongPathChars, L'\0');
  windows.resize(GetWindowsDirectoryW(windows.data(), kLongPathChars));
  const std::wstring game = std::filesystem::path(ModulePath(nullptr)).parent_path().wstring();

  std::vector<HMODULE> modules(kModuleCapacity);
  DWORD used_bytes = 0;
  if (EnumProcessModules(GetCurrentProcess(), modules.data(),
                         static_cast<DWORD>(modules.size() * sizeof(HMODULE)), &used_bytes) == 0) {
    LogWarning("modules {} : liste indisponible (erreur {})", moment, GetLastError());
    return;
  }
  modules.resize(used_bytes / sizeof(HMODULE));
  std::string beside_game;
  std::string foreign;
  // Le premier module est l'exécutable lui-même.
  for (std::size_t rank = 1; rank < modules.size(); ++rank) {
    const std::wstring path = ModulePath(modules[rank]);
    if (path.empty() || IsUnder(path, windows)) {
      continue;
    }
    const std::filesystem::path shown(path);
    if (IsUnder(path, game)) {
      beside_game += (beside_game.empty() ? "" : ", ") + Utf8FromPath(shown.filename());
    } else {
      foreign += (foreign.empty() ? "" : ", ") + Utf8FromPath(shown);
    }
  }
  LogInfo("modules {} : du dossier du jeu — {}", moment,
          beside_game.empty() ? "aucun" : beside_game);
  // ⚠ EN ALERTE QUAND IL Y EN A : c'est la ligne qu'on cherche quand le client
  // se ferme seul, et elle ne doit pas se perdre parmi les informations.
  if (foreign.empty()) {
    LogInfo("modules {} : aucun hors de Windows et du jeu", moment);
  } else {
    LogWarning("modules {} : hors de Windows et du jeu — {}", moment, foreign);
  }
#else
  static_cast<void>(moment);
#endif
}

Result<void> ProbeWritableFolder(const std::filesystem::path& folder) {
  const std::filesystem::path probe = folder / kProbeFileName;
  const std::string shown = Utf8FromPath(folder);
  std::error_code ignored;
  {
    std::ofstream written(probe, std::ios::binary | std::ios::trunc);
    if (!written) {
      return Fail(ErrorCode::kIoFailure, shown + " refuse d'y créer un fichier");
    }
    written << kProbeText;
    written.flush();
    if (!written) {
      written.close();
      std::filesystem::remove(probe, ignored);
      return Fail(ErrorCode::kIoFailure, shown + " laisse créer un fichier mais refuse d'y écrire");
    }
  }
  std::string read_back;
  {
    std::ifstream read(probe, std::ios::binary);
    read_back.assign(std::istreambuf_iterator<char>(read), std::istreambuf_iterator<char>());
  }
  std::error_code failed;
  std::filesystem::remove(probe, failed);
  if (read_back != kProbeText) {
    return Fail(ErrorCode::kIoFailure, shown + " : ce qu'on y écrit ne s'y relit pas");
  }
  if (failed) {
    return Fail(ErrorCode::kIoFailure,
                std::format("{} refuse d'effacer un fichier qu'on vient d'y écrire (erreur {})",
                            shown, failed.value()));
  }
  return {};
}

}  // namespace ro::core
