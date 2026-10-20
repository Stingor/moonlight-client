#pragma once

// L'ILLUSTRATION DE L'ÉCRAN DE CHARGEMENT : laquelle, et quand en changer.
//
// ⚠ LOGIQUE PURE : le tirage arrive d'ailleurs, l'horloge aussi. Ce qui se
// décide ici s'éprouve sans archive et sans fenêtre.

#include <cstddef>
#include <cstdint>
#include <optional>
#include <string>

namespace moonclient {

// Les illustrations que l'installation porte : `loading00.jpg` à `loading35.jpg`.
inline constexpr std::size_t kLoadingArtCount = 36;

// UNE ILLUSTRATION DURE TANT QUE LES ÉCRANS SE SUIVENT : un chargement qui en
// suit un autre de moins que ce délai garde la sienne. Changer d'image entre
// deux étapes d'un même démarrage se lirait comme un saut.
inline constexpr std::uint32_t kLoadingArtKeepMs = 1500;

// Le rang qu'un tirage désigne, toujours dans les bornes.
[[nodiscard]] std::size_t LoadingArtRank(std::uint32_t draw) noexcept;

// Le chemin de l'illustration de ce rang, en UTF-8 ; un rang hors bornes s'y
// ramène.
[[nodiscard]] std::string LoadingArtPath(std::size_t rank);

// VRAI QUAND L'ILLUSTRATION SE RETIRE : aucune n'a encore été montrée, ou la
// dernière l'a été il y a plus de `kLoadingArtKeepMs`.
[[nodiscard]] bool LoadingArtExpired(std::optional<std::uint32_t> last_shown_ms,
                                     std::uint32_t now_ms) noexcept;

}  // namespace moonclient
