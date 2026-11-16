#pragma once

#include <array>
#include <cstdint>
#include <optional>
#include <span>
#include <string>

#include "ro/render/actor_layers.h"
#include "ro/render/palette_ramps.h"

#include "character/style_layers.h"

namespace moonclient {

// LES DEUX PEAUX DE L'ÉDITEUR : celle du visage, réglée sous l'onglet Tête, et
// celle du corps, réglée sous l'onglet Corps, rangée sous la clé du corps qu'on
// habille. Un corps sans la sienne prend celle du visage.
//
// ⚠ ICI CE QUI SE DÉCIDE SANS FENÊTRE NI GARDE-ROBE : la ligne qu'un point du
// pantin désigne, la raison qui interdit de régler, ce qui peut partir au
// serveur. `StyleView` s'en sert ; ceci se prouve seul.

enum class SkinLine : std::uint8_t { kFace, kBody };

// LES TEINTS PROPOSÉS, du plus clair au plus foncé.
inline constexpr std::size_t kSkinToneCount = 7;
inline constexpr std::array<std::array<std::uint8_t, 3>, kSkinToneCount> kSkinTones = {{
    {255, 228, 210},
    {244, 204, 176},
    {228, 180, 146},
    {204, 148, 110},
    {168, 116, 80},
    {128, 84, 56},
    {88, 56, 38},
}};

// LE CALQUE D'UNE LIGNE : la peau du visage sous la clé zéro, celle du corps
// sous la clé du corps.
[[nodiscard]] LayerAt SkinLayerAt(SkinLine line, std::uint32_t body_key) noexcept;

// LA LIGNE QUE LA PIPETTE DÉSIGNE : un index de peau sur la tête désigne le
// visage, un index de `body_skin` sur le corps les bras ; rien ailleurs.
//
// 🔴 LA PEAU DU CORPS EST CELLE DE SON SPRITE : chaque corps range la sienne où
// il veut, et la tête garde l'ensemble fixe.
[[nodiscard]] std::optional<SkinLine> SkinLineUnder(ro::render::ActorPart part, std::uint8_t index,
                                                    const ro::render::SkinSet& body_skin) noexcept;

// POURQUOI LES PEAUX NE SE RÈGLENT PAS.
enum class SkinRefusal : std::uint8_t {
  kNone,
  // L'habit du GM garde les couleurs de ses sprites.
  kGmOutfit,
  // Une monture listée : la peau du corps y peint aussi l'animal, et ses index
  // se règlent avec le reste du corps. Le visage, lui, reste réglable.
  kOnMount,
  // Le serveur ne range pas la peau du corps : elle suit celle du visage.
  kNoBodySkinServer,
};

// LA RAISON, pour l'apparence montrée : l'habit du GM, puis les chemins de ses
// corps (montures listées), puis, pour la peau du corps, le dernier calque que
// le serveur accepte.
[[nodiscard]] SkinRefusal SkinRefusalFor(SkinLine line, bool gm_outfit,
                                         std::span<const std::string> body_paths,
                                         int server_layer_last);

// LA PIÈCE QUE RÈGLENT LES LIGNES, comme une rampe, sur la base du corps : la
// peau que ce corps porte, ou à défaut l'ensemble d'une tête humaine.
[[nodiscard]] ro::render::PaletteRamp SkinRamp(const ro::render::SkinSet& body_skin);

}  // namespace moonclient
