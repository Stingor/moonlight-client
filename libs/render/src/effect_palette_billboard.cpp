#include "ro/render/effect_palette_billboard.h"

#include <algorithm>
#include <cmath>

#include "ro/render/effect_track.h"

#include "effect_track_angles.h"

namespace ro::render {
namespace {

constexpr std::uint8_t kFullChannel = 255;
// Le mode que la voie A entend comme une ADDITION.
constexpr std::int16_t kAddedBlend = 0;
// La demi-largeur du quad : le client prend la moitié de la taille.
constexpr float kHalfOfSize = 0.5F;

// QUELS CANAUX PRENNENT LA NUANCE, par palette. Le client écrit ces sept
// branches à la main, quatre nuances chacune ; ce tableau en est le résumé
// mesuré, et non une règle qu'on lui prête.
struct PaletteRow {
  std::uint8_t palette;
  bool red;
  bool green;
  bool blue;
};

constexpr std::array<PaletteRow, 7> kPaletteRows = {{
    {2, true, false, true},
    {3, true, true, false},
    {4, false, false, true},
    {5, false, true, true},
    {6, false, true, false},
    {7, true, false, false},
    {8, true, true, true},
}};

// ⚠ LA PALETTE HUIT A UNE EXCEPTION, et elle seule : à la dernière nuance, elle
// rend le BLANC au lieu de teinter ses trois canaux. Le client y écrit trois
// fois deux cent cinquante-cinq, et l'oublier donnerait un gris uniforme là où
// il montre un éclat.
constexpr std::uint8_t kPaletteWithWhiteException = 8;
constexpr std::uint8_t kShadeThatTurnsWhite = 3;

// LE CAS PAR DÉFAUT DU CLIENT — le `else` final de sa cascade. Ce n'est pas une
// absence de palette : il teinte le rouge et le vert, comme la palette trois.
constexpr PaletteRow kDefaultRow = {0, true, true, false};

}  // namespace

std::array<std::uint8_t, 3> EffectPaletteBillboardColor(std::uint8_t palette, std::uint8_t shade) {
  if (palette == kPaletteWithWhiteException && shade == kShadeThatTurnsWhite) {
    return {kFullChannel, kFullChannel, kFullChannel};
  }
  const auto found = std::ranges::find(kPaletteRows, palette, &PaletteRow::palette);
  const PaletteRow& row = found == kPaletteRows.end() ? kDefaultRow : *found;
  const std::uint8_t nuance = shade < kPaletteBillboardShades
                                  ? kPaletteBillboardShadeValues[shade]
                                  : kPaletteBillboardShadeValues.front();
  return {row.red ? nuance : kFullChannel, row.green ? nuance : kFullChannel,
          row.blue ? nuance : kFullChannel};
}

std::optional<EffectWorldQuad> ComposePaletteBillboardQuad(const EffectTrack& track,
                                                           glm::vec3 node_origin,
                                                           const EffectOwnerTint& owner) {
  if (!track.active || track.alpha <= 0 || track.ticks <= 0) {
    return std::nullopt;
  }
  const float size = track.value[kSlotOfBillboardSize];
  if (size <= 0.0F) {
    return std::nullopt;
  }

  const float demie = size * kHalfOfSize;
  const std::int32_t azimuth = track.azimuth_degrees;
  // ⚠ L'ANGLE OPPOSÉ SE PREND EN AJOUTANT OU EN RETRANCHANT un demi-tour, selon
  // que l'azimut est sous ou sur cette borne. Toujours ajouter sortirait de la
  // table de sinus du client pour la moitié des azimuts.
  const std::int32_t oppose = azimuth < kEffectTrackHalfTurnDegrees
                                  ? azimuth + kEffectTrackHalfTurnDegrees
                                  : azimuth - kEffectTrackHalfTurnDegrees;
  const float here = DegreesToRadians(static_cast<float>(azimuth));
  const float over_there = DegreesToRadians(static_cast<float>(oppose));

  // ⚠ LE PANNEAU EST CENTRÉ À LA HAUTEUR DE LA CASE D'ÉLÉVATION, au-dessus du
  // sol. Le client écrit son premier bord à « moitié moins élévation » et le
  // second une taille plus bas, dans son repère qui descend ; dans le nôtre,
  // qui monte, les deux ordonnées changent de signe. Recopiées, elles
  // enterraient le panneau.
  const float top = track.value[kSlotOfBillboardLift] - demie;
  const float bottom = top + size;

  EffectParticleQuadRequest request;
  request.corners = {
      glm::vec3{demie * std::cos(here), top, demie * std::sin(here)},
      glm::vec3{demie * std::cos(over_there), top, demie * std::sin(over_there)},
      glm::vec3{demie * std::cos(over_there), bottom, demie * std::sin(over_there)},
      glm::vec3{demie * std::cos(here), bottom, demie * std::sin(here)},
  };
  request.alpha = static_cast<std::uint8_t>(std::min<std::int16_t>(track.alpha, 255));
  request.color =
      EffectPaletteBillboardColor(static_cast<std::uint8_t>(track.value[kSlotOfBillboardPalette]),
                                  static_cast<std::uint8_t>(track.value[kSlotOfBillboardShade]));
  request.blend_mode = kAddedBlend;
  // ⚠ LES SEPT PALETTES RELEVÉES PASSENT EN PLUS DEVANT TOUT ; le cas par défaut
  // garde sa profondeur. Voir `EffectQuadDepth`.
  const bool relevee =
      std::ranges::find(kPaletteRows,
                        static_cast<std::uint8_t>(track.value[kSlotOfBillboardPalette]),
                        &PaletteRow::palette) != kPaletteRows.end();
  request.depth = relevee ? EffectQuadDepth::kFrontmost : EffectQuadDepth::kProjected;
  return ComposeEffectParticleQuad(request, node_origin, owner);
}

}  // namespace ro::render
