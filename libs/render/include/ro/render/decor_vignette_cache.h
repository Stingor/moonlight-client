#pragma once

#include <cstdint>
#include <filesystem>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

#include "ro/core/error.h"
#include "ro/formats/image.h"
#include "ro/formats/vignette_file.h"
#include "ro/vfs/vfs.h"

namespace ro::render {

// ── LES VIGNETTES DES MODÈLES, GARDÉES SUR LE DISQUE ────────────────────────
//
// `LoadDecorVignette` coûte des dizaines de millisecondes par modèle ; une
// bibliothèque de milliers de modèles se montre pourtant d'un coup d'œil. Une
// vignette rendue s'écrit donc dans le dossier de cache du client — celui de
// l'utilisateur, jamais le dossier du jeu (voir `core::ClientCacheDirectory`)
// —, un fichier par modèle et par côté, sous une arborescence qui reprend celle de data\model\ ;
// elle se relit à l'ouverture suivante si sa CLÉ est encore celle du modèle —
// nom, côté, révision du rendu, état du fichier dans l'installation, comparés
// champ à champ (voir `formats::VignetteKey`).
//
// ⚠ L'ÉTAT DES TEXTURES N'ENTRE PAS DANS LA CLÉ : un patch qui ne change
// qu'une texture laisse l'ancienne vignette. Vider le dossier les refait toutes.

// La révision du rendu des vignettes. 🔴 À MONTER quand `RasterDecorVignette`
// change ce qu'il dessine : toutes les vignettes gardées se refont.
inline constexpr std::uint32_t kDecorVignetteRenderRevision = 1;

// Le dossier des vignettes, sous le dossier de cache du client.
inline constexpr std::string_view kDecorVignetteCacheFolder = "model_vignettes";

// LES MODÈLES DE L'INSTALLATION, .rsm et .rsm2, relatifs à data\model\, triés :
// ceux du dossier `folder` (relatif aussi, « » pour tous) et de ses
// sous-dossiers. C'est ce que la bibliothèque de l'éditeur propose.
[[nodiscard]] std::vector<std::string> ListDecorModels(const vfs::Vfs& vfs,
                                                       std::string_view folder = {});

// Le dossier des vignettes sous `client_cache`, le dossier de cache du client :
// « <client_cache>/model_vignettes ».
[[nodiscard]] std::filesystem::path DecorVignetteCacheRoot(
    const std::filesystem::path& client_cache);

// Le dossier des vignettes, ou un chemin VIDE — rien ne se garde sur le disque,
// la vignette se rend en mémoire — quand le dossier de cache du client ne s'est
// pas trouvé.
[[nodiscard]] std::filesystem::path DecorVignetteCacheRootOrNone(
    const core::Result<std::filesystem::path>& client_cache);

// Le fichier de la vignette de `rsm_file` (relatif à data\model\, en UTF-8) au
// côté `side_px`, sous `cache_root` : « <côté>/<dossiers>/<nom>.vgn ».
[[nodiscard]] std::filesystem::path DecorVignetteCachePath(const std::filesystem::path& cache_root,
                                                           std::string_view rsm_file,
                                                           std::int32_t side_px);

// La clé qu'une vignette de `rsm_file` doit porter aujourd'hui ; vide quand le
// modèle n'est pas dans l'installation.
[[nodiscard]] std::optional<formats::VignetteKey> DecorVignetteKey(const vfs::Vfs& vfs,
                                                                   std::string_view rsm_file,
                                                                   std::int32_t side_px);

// La vignette gardée à `path`, si elle porte exactement `key`. Une image vide
// rendue dit que le modèle n'a rien à montrer — un échec gardé. Rien : absente,
// abîmée ou périmée.
[[nodiscard]] std::optional<formats::Image> ReadCachedDecorVignette(
    const std::filesystem::path& path, const formats::VignetteKey& key);

// Écrit la vignette `image` — vide pour un échec — sous `key`, d'un seul geste.
[[nodiscard]] core::Result<void> WriteCachedDecorVignette(const std::filesystem::path& path,
                                                          const formats::VignetteKey& key,
                                                          const formats::Image& image);

// D'où vient une vignette.
enum class VignetteOrigin : std::uint8_t { kDisk, kRendered };

struct CachedDecorVignette {
  formats::Image image;
  VignetteOrigin origin = VignetteOrigin::kRendered;
};

// La vignette de `rsm_file`, relue sous `cache_root` si elle y est à jour,
// sinon rendue par `LoadDecorVignette` puis écrite — son échec aussi, pour
// qu'un modèle illisible ne se relise pas à chaque ouverture. Un `cache_root`
// vide ne garde rien. Une écriture qui échoue n'empêche pas de montrer.
[[nodiscard]] core::Result<CachedDecorVignette> LoadCachedDecorVignette(
    vfs::Vfs& vfs, std::string_view rsm_file, std::int32_t side_px,
    const std::filesystem::path& cache_root);

}  // namespace ro::render
