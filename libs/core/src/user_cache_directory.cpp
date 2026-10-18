#include "ro/core/user_cache_directory.h"

#include <cstdlib>
#include <string>

#include "ro/core/utf8_path.h"

#ifdef _WIN32
#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
// clang-format off
// L'ordre est SÉMANTIQUE : shlobj.h et knownfolders.h ne compilent pas sans les
// types de windows.h.
#include <windows.h>
#include <knownfolders.h>
#include <shlobj.h>
// clang-format on
#endif

namespace ro::core {
namespace {

bool Usable(const std::optional<std::filesystem::path>& folder) {
  return folder.has_value() && !folder->empty() && folder->is_absolute();
}

#ifdef _WIN32
std::optional<std::filesystem::path> KnownLocalAppData() {
  PWSTR wide = nullptr;
  std::optional<std::filesystem::path> folder;
  if (SUCCEEDED(SHGetKnownFolderPath(FOLDERID_LocalAppData, 0, nullptr, &wide)) &&
      wide != nullptr) {
    folder = std::filesystem::path(wide);
  }
  // Libéré même en échec : le contrat de la fonction l'exige.
  CoTaskMemFree(wide);
  return folder;
}

std::optional<std::filesystem::path> WideVariable(const wchar_t* name) {
  // Le premier appel donne la place qu'il faut, zéro final compris ; zéro dit
  // que la variable n'existe pas.
  const DWORD needed = GetEnvironmentVariableW(name, nullptr, 0);
  if (needed == 0) {
    return std::nullopt;
  }
  std::wstring value(needed, L'\0');
  const DWORD written = GetEnvironmentVariableW(name, value.data(), needed);
  if (written == 0 || written >= needed) {
    return std::nullopt;
  }
  value.resize(written);
  return std::filesystem::path(value);
}
#else
std::optional<std::filesystem::path> Variable(const char* name) {
  const char* value = std::getenv(name);
  if (value == nullptr) {
    return std::nullopt;
  }
  // Les variables d'environnement d'un Linux sont lues en UTF-8.
  return PathFromUtf8(value);
}
#endif

}  // namespace

Result<std::filesystem::path> PickWindowsCacheDirectory(
    const std::optional<std::filesystem::path>& known_folder,
    const std::optional<std::filesystem::path>& local_app_data) {
  if (Usable(known_folder)) {
    return *known_folder;
  }
  if (Usable(local_app_data)) {
    return *local_app_data;
  }
  return Fail(ErrorCode::kNotFound,
              "ni le dossier connu LocalAppData ni la variable LOCALAPPDATA ne donnent un chemin "
              "absolu");
}

Result<std::filesystem::path> PickXdgCacheDirectory(
    const std::optional<std::filesystem::path>& xdg_cache_home,
    const std::optional<std::filesystem::path>& home) {
  if (Usable(xdg_cache_home)) {
    return *xdg_cache_home;
  }
  if (Usable(home)) {
    return *home / ".cache";
  }
  return Fail(ErrorCode::kNotFound, "ni XDG_CACHE_HOME ni HOME ne donnent un chemin absolu");
}

Result<std::filesystem::path> UserCacheDirectory() {
#ifdef _WIN32
  return PickWindowsCacheDirectory(KnownLocalAppData(), WideVariable(L"LOCALAPPDATA"));
#else
  return PickXdgCacheDirectory(Variable("XDG_CACHE_HOME"), Variable("HOME"));
#endif
}

std::filesystem::path ClientCacheDirectoryUnder(const std::filesystem::path& user_cache) {
  return user_cache / PathFromUtf8(kClientCacheFolder);
}

Result<std::filesystem::path> ClientCacheDirectory() {
  Result<std::filesystem::path> user_cache = UserCacheDirectory();
  if (!user_cache) {
    return std::unexpected(user_cache.error());
  }
  return ClientCacheDirectoryUnder(*user_cache);
}

}  // namespace ro::core
