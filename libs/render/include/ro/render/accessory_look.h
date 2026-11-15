#pragma once

#include <cstdint>
#include <optional>
#include <string_view>

// UN ACCESSOIRE DU JOUEUR MONTRÉ SEUL : l'oiseau du chasseur ou le chariot du
// marchand, dessiné comme le corps d'un acteur. C'est ce que l'éditeur de
// style met à la place du joueur pour régler sa teinture.
//
// Sur la carte, ni l'un ni l'autre n'est un acteur : ce sont des sprites-enfants
// du joueur (`companion_falcon.h`, `companion_cart.h`).
namespace ro::render {

enum class AccessoryKind : std::uint8_t { kNone = 0, kFalcon = 1, kCart = 2 };

// Le fichier d'image de l'accessoire sous cette clé de teinture (`FalconArt::
// dye_key`, `CartArt::dye_key`), celui de l'adulte ; rien pour `kNone` ou une
// clé inconnue.
[[nodiscard]] std::optional<std::string_view> AccessorySpritePath(AccessoryKind kind,
                                                                  std::uint32_t dye_key) noexcept;

}  // namespace ro::render
