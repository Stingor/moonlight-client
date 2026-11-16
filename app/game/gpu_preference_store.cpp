#include "game/gpu_preference_store.h"

#include <optional>
#include <string>

#if defined(_WIN32)
#include <windows.h>
#endif

#if defined(_WIN32)
// LES DEUX VARIABLES QUE LES PILOTES DES PORTABLES À DEUX CARTES CHERCHENT dans
// l'exécutable. Leur nom est imposé par NVIDIA et par AMD, et c'est l'exécutable
// qui doit les exporter : une DLL ne compte pas.
//
// ⚠ ZÉRO AU DÉPART, et c'est voulu : un pilote qui les lit au démarrage du
// processus ne doit rien voir d'imposé, pour que la préférence de Windows décide.
extern "C" {
__declspec(dllexport) DWORD NvOptimusEnablement = 0;
__declspec(dllexport) int AmdPowerXpressRequestHighPerformance = 0;
}
#endif

namespace moonclient {

#if defined(_WIN32)
namespace {

// Là où Windows range une préférence par exécutable : une valeur par chemin.
constexpr const wchar_t* kPreferencesKey = L"Software\\Microsoft\\DirectX\\UserGpuPreferences";
// Assez pour un chemin long ; Windows en tronque au-delà.
constexpr DWORD kLongestPath = 32768;
// Ce qu'un caractère de la valeur peut valoir pour se lire en octet : la valeur
// n'est faite que de lettres, de chiffres et de ponctuation ASCII.
constexpr wchar_t kLastAscii = 0x7F;

[[nodiscard]] std::wstring ExecutablePath() {
  std::wstring path(kLongestPath, L'\0');
  const DWORD length = GetModuleFileNameW(nullptr, path.data(), kLongestPath);
  path.resize(length);
  return path;
}

// La valeur rangée pour cet exécutable, vide si aucune ; rien si elle ne se lit
// pas comme Windows l'écrit.
[[nodiscard]] std::optional<std::string> ReadValue(const std::wstring& executable) {
  DWORD size_bytes = 0;
  if (RegGetValueW(HKEY_CURRENT_USER, kPreferencesKey, executable.c_str(), RRF_RT_REG_SZ, nullptr,
                   nullptr, &size_bytes) != ERROR_SUCCESS ||
      size_bytes == 0) {
    // ⚠ UNE CHAÎNE VIDE ET NON « RIEN » : aucune valeur rangée est le cas
    // ordinaire, celui d'un jeu que personne n'a réglé dans Windows.
    return std::string{};
  }
  std::wstring wide(size_bytes / sizeof(wchar_t), L'\0');
  if (RegGetValueW(HKEY_CURRENT_USER, kPreferencesKey, executable.c_str(), RRF_RT_REG_SZ, nullptr,
                   wide.data(), &size_bytes) != ERROR_SUCCESS) {
    return std::nullopt;
  }
  // ⚠ LA TAILLE RENDUE COMPTE LE ZÉRO FINAL.
  wide.resize(size_bytes / sizeof(wchar_t));
  while (!wide.empty() && wide.back() == L'\0') {
    wide.pop_back();
  }
  std::string narrow;
  narrow.reserve(wide.size());
  for (const wchar_t sign : wide) {
    if (sign > kLastAscii) {
      // Rien de ce que Windows y range ne sort de l'ASCII : une valeur qui en
      // sort n'est pas la sienne, et ne se réécrit pas.
      return std::nullopt;
    }
    narrow.push_back(static_cast<char>(sign));
  }
  return narrow;
}

}  // namespace

GpuPreference ReadWindowsGpuPreference() {
  return GpuPreferenceIn(ReadValue(ExecutablePath()).value_or(std::string{}));
}

ro::core::Result<void> WriteWindowsGpuPreference(GpuPreference wanted) {
  const std::wstring executable = ExecutablePath();
  if (executable.empty()) {
    return ro::core::Fail(ro::core::ErrorCode::kIoFailure,
                          "le chemin de l'exécutable ne se lit pas");
  }
  const std::optional<std::string> before = ReadValue(executable);
  if (!before.has_value()) {
    return ro::core::Fail(ro::core::ErrorCode::kIoFailure,
                          "la préférence graphique rangée par Windows ne se relit pas : "
                          "elle reste telle quelle");
  }
  const std::string after = WithGpuPreference(*before, wanted);
  if (after == *before) {
    return {};
  }
  HKEY key = nullptr;
  if (RegCreateKeyExW(HKEY_CURRENT_USER, kPreferencesKey, 0, nullptr, 0, KEY_SET_VALUE, nullptr,
                      &key, nullptr) != ERROR_SUCCESS) {
    return ro::core::Fail(ro::core::ErrorCode::kIoFailure,
                          "la clé des préférences graphiques de Windows ne s'ouvre pas");
  }
  LSTATUS written = ERROR_SUCCESS;
  if (after.empty()) {
    // Plus rien à ranger pour cet exécutable : la valeur part, comme quand la
    // page de Windows rend la main à l'automatique.
    written = RegDeleteValueW(key, executable.c_str());
  } else {
    const std::wstring wide(after.begin(), after.end());
    written = RegSetValueExW(key, executable.c_str(), 0, REG_SZ,
                             reinterpret_cast<const BYTE*>(wide.c_str()),
                             static_cast<DWORD>((wide.size() + 1) * sizeof(wchar_t)));
  }
  RegCloseKey(key);
  if (written != ERROR_SUCCESS) {
    return ro::core::Fail(ro::core::ErrorCode::kIoFailure,
                          "la préférence graphique de Windows ne s'écrit pas (erreur " +
                              std::to_string(written) + ")");
  }
  return {};
}

void RequestHighPerformanceGpu(bool wanted) {
  NvOptimusEnablement = wanted ? 1 : 0;
  AmdPowerXpressRequestHighPerformance = wanted ? 1 : 0;
}

#else

GpuPreference ReadWindowsGpuPreference() { return GpuPreference::kAutomatic; }

ro::core::Result<void> WriteWindowsGpuPreference(GpuPreference /*wanted*/) { return {}; }

void RequestHighPerformanceGpu(bool /*wanted*/) {}

#endif

}  // namespace moonclient
