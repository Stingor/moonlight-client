#pragma once

#include <cstdint>
#include <filesystem>
#include <optional>
#include <string>
#include <string_view>

#include "ro/core/error.h"

namespace moonclient::patcher {

// CE QUE L'INSTALLATION A DÉJÀ REÇU : l'index du dernier patch appliqué.
//
// 🔴 UN FICHIER À CE CLIENT, et non le `patcher.dat` du patcher d'origine. Les
// deux suivent des chaînes DIFFÉRENTES, publiées dans deux dossiers du
// serveur : leurs index ne se comparent pas, et reprendre celui de l'autre
// ferait sauter autant de patchs de la nôtre. La forme reste la même,
// `{"last_patch_index":12}`.
inline constexpr std::string_view kPatcherStateFileName = "moonclient-patch.dat";

// ⚠ UN FICHIER ILLISIBLE EST UNE ERREUR, pas un index nul : l'appelant décide
// alors de tout réappliquer, et le journal dit pourquoi.
[[nodiscard]] ro::core::Result<std::uint64_t> ParsePatcherState(std::string_view text);

[[nodiscard]] std::string FormatPatcherState(std::uint64_t last_applied_index);

// L'index du dernier patch appliqué à l'installation `install_root`, ou rien
// quand le patcheur n'y a jamais rien posé. Un fichier illisible vaut rien, et
// le journal le dit.
[[nodiscard]] std::optional<std::uint64_t> ReadLastAppliedPatch(
    const std::filesystem::path& install_root);

}  // namespace moonclient::patcher
