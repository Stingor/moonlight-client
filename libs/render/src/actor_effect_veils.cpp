#include <algorithm>
#include <array>
#include <cstdint>
#include <optional>

#include "ro/render/actor_effect_table.h"

#include "actor_effect_writes.h"

namespace ro::render {
namespace {

// LES TROIS BITS QUI POSENT UN VOILE SUR LE CORPS.
//
// ⚠ CE SONT DES BITS PORTÉS, et la mesure les lit ici : chaque constante de ce
// fichier déclare un bit que `libs/render` sait montrer.
//
// `0x4000` et `0x40000` sont le MÊME mécanisme à la couleur près — le quad
// teinté à durée du client, reçu avec (5, 5, 255) pour l'un et (255, 5, 5)
// pour l'autre. Neuf identifiants pour une seule pièce, et leurs
// noms le confirment : six « bleu » d'un côté, trois « rouge » de l'autre.
// `0x100` est le même voile en blanc, sur une autre rampe.
inline constexpr std::uint32_t kBlueVeil = 0x4000;
inline constexpr std::uint32_t kRedVeil = 0x40000;
inline constexpr std::uint32_t kWhiteVeil = 0x100;

inline constexpr std::array<std::uint8_t, 3> kBlueVeilColor = {5, 5, 255};
inline constexpr std::array<std::uint8_t, 3> kRedVeilColor = {255, 5, 5};
inline constexpr std::array<std::uint8_t, 3> kWhiteVeilColor = {255, 255, 255};

// Les champs qui mènent la rampe de chacun.
inline constexpr std::uint16_t kTimedVeilField = 0xF6;
inline constexpr std::uint16_t kWhiteVeilField = 0xF4;

// LA RAMPE DU VOILE TEINTÉ : l'alpha monte de quinze par pas jusqu'au dixième,
// tient à cent soixante jusqu'au vingtième, retombe de cinq par pas jusqu'au
// cinquantième, et au-delà le client ne dessine plus rien.
inline constexpr std::int32_t kRisingStep = 15;
inline constexpr std::int32_t kRisingUntil = 10;
inline constexpr std::int32_t kHoldAlpha = 160;
inline constexpr std::int32_t kHoldUntil = 20;
inline constexpr std::int32_t kFallingFrom = 51;
inline constexpr std::int32_t kFallingStep = 5;
inline constexpr std::int32_t kTimedVeilEnd = 50;

// LA RAMPE DU VOILE BLANC : cinquante par pas jusqu'au cinquième, opaque du
// sixième au huitième, puis huit cent quatre-vingt-quinze moins quatre-vingts
// par pas jusqu'au onzième.
inline constexpr std::int32_t kWhiteRisingStep = 50;
inline constexpr std::int32_t kWhiteRisingUntil = 5;
inline constexpr std::int32_t kWhiteOpaqueUntil = 8;
inline constexpr std::int32_t kWhiteFallingBase = 895;
inline constexpr std::int32_t kWhiteFallingStep = 80;
inline constexpr std::int32_t kWhiteVeilEnd = 11;

inline constexpr std::int32_t kOpaque = 255;

// LES CALQUES QU'UN VOILE REPREND : tous, sauf la cape. Le client pose le voile
// sur chaque calque qu'il peint, et la cape passe par un autre chemin.
inline constexpr ActorPartSet kVeiledParts =
    kAllActorParts & static_cast<ActorPartSet>(~PartBit(ActorPart::kGarment));

// Combien de fois le client soumet le voile teinté. Deux : le même quad, deux
// fois de suite, ce qui l'épaissit.
inline constexpr std::size_t kTimedVeilPasses = 2;

// L'alpha du voile teinté à ce pas, ou rien quand il ne se dessine plus.
std::optional<std::int32_t> TimedVeilAlpha(std::int32_t step) {
  if (step < 0 || step > kTimedVeilEnd) {
    return std::nullopt;
  }
  if (step <= kRisingUntil) {
    return kRisingStep * step;
  }
  if (step <= kHoldUntil) {
    return kHoldAlpha;
  }
  return kFallingStep * (kFallingFrom - step);
}

// L'alpha du voile blanc à ce pas, ou rien.
std::optional<std::int32_t> WhiteVeilAlpha(std::int32_t step) {
  if (step < 0 || step > kWhiteVeilEnd) {
    return std::nullopt;
  }
  if (step <= kWhiteRisingUntil) {
    return kWhiteRisingStep * step;
  }
  if (step <= kWhiteOpaqueUntil) {
    return kOpaque;
  }
  return kWhiteFallingBase - kWhiteFallingStep * step;
}

}  // namespace

std::size_t BodyVeilsAfterEffects(std::span<const ActorEffectPlay> played,
                                  std::span<ActorBodyCopy> out) {
  std::size_t written = 0;
  const auto ajouter = [&](std::array<std::uint8_t, 3> color, std::int32_t alpha,
                           std::size_t passes) {
    const auto tint = static_cast<std::uint8_t>(std::clamp(alpha, 0, kOpaque));
    for (std::size_t passe = 0; passe < passes && written < out.size(); ++passe) {
      out[written++] = ActorBodyCopy{
          1.0F, {color[0], color[1], color[2], tint}, {0.0F, 0.0F}, kVeiledParts, true};
    }
  };
  for (const ActorEffectPlay& play : played) {
    const ActorEffectChange* change = ActorEffectChangeFor(play.effect_id);
    if (change == nullptr) {
      continue;
    }
    // Les bits que l'effet relève à cet âge, et eux seuls : voir `RenderBitsAt`.
    const std::uint32_t raised = RenderBitsAt(*change, play.age_ticks);
    const bool blue = (raised & kBlueVeil) != 0;
    const bool red = (raised & kRedVeil) != 0;
    if (blue || red) {
      if (const ActorFieldWrite* write = WriteOn(*change, kTimedVeilField)) {
        const std::int32_t step = ValueAt(*write, play.age_ticks, change->life_ticks);
        if (const std::optional<std::int32_t> alpha = TimedVeilAlpha(step)) {
          // ⚠ LES DEUX BITS SE RENCONTRENT, ET LES DEUX VOILES AVEC : le client
          // teste l'un puis l'autre, sans que le premier n'écarte le second.
          if (blue) {
            ajouter(kBlueVeilColor, *alpha, kTimedVeilPasses);
          }
          if (red) {
            ajouter(kRedVeilColor, *alpha, kTimedVeilPasses);
          }
        }
      }
    }
    if ((raised & kWhiteVeil) != 0) {
      if (const ActorFieldWrite* write = WriteOn(*change, kWhiteVeilField)) {
        const std::int32_t step = ValueAt(*write, play.age_ticks, change->life_ticks);
        if (const std::optional<std::int32_t> alpha = WhiteVeilAlpha(step)) {
          ajouter(kWhiteVeilColor, *alpha, 1);
        }
      }
    }
  }
  return written;
}

}  // namespace ro::render
