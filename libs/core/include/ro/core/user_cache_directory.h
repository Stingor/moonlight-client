#pragma once

#include <filesystem>
#include <optional>
#include <string_view>

#include "ro/core/error.h"

namespace ro::core {

// ── LE DOSSIER DE CACHE DE L'UTILISATEUR ────────────────────────────────────
//
// Ce que le client recalcule à volonté et garde d'une session à l'autre — des
// centaines de mégaoctets de vignettes, par exemple — ne va PAS dans le dossier
// du jeu : il va dans le dossier que le système réserve aux caches de
// l'utilisateur, sous un dossier propre au client.
//
//   Windows : %LOCALAPPDATA% (le dossier connu, à défaut la variable).
//   Linux   : $XDG_CACHE_HOME, à défaut ~/.cache.
//
// Un chemin relatif ne compte pas : la spécification XDG l'ignore, et il
// désignerait un dossier différent selon le dossier courant.

// Le dossier du client sous celui de l'utilisateur.
inline constexpr std::string_view kClientCacheFolder = "Moonlight-Destiny";

// LE CHOIX SOUS WINDOWS : le dossier connu, sinon la variable LOCALAPPDATA.
// Absents, vides ou relatifs tous deux : `kNotFound`.
[[nodiscard]] Result<std::filesystem::path> PickWindowsCacheDirectory(
    const std::optional<std::filesystem::path>& known_folder,
    const std::optional<std::filesystem::path>& local_app_data);

// LE CHOIX SOUS LINUX : $XDG_CACHE_HOME, sinon « .cache » sous $HOME. Absents,
// vides ou relatifs tous deux : `kNotFound`.
[[nodiscard]] Result<std::filesystem::path> PickXdgCacheDirectory(
    const std::optional<std::filesystem::path>& xdg_cache_home,
    const std::optional<std::filesystem::path>& home);

// Le dossier de cache de l'utilisateur, lu dans le système de cette machine.
[[nodiscard]] Result<std::filesystem::path> UserCacheDirectory();

// Le dossier de cache du client sous `user_cache` : « <user_cache>/Moonlight-Destiny ».
[[nodiscard]] std::filesystem::path ClientCacheDirectoryUnder(
    const std::filesystem::path& user_cache);

// Le dossier de cache du client sur cette machine. Rien n'y est créé.
[[nodiscard]] Result<std::filesystem::path> ClientCacheDirectory();

}  // namespace ro::core
