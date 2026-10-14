#include <algorithm>
#include <cmath>
#include <cstdint>
#include <numbers>

#include "ro/render/actor_effect_table.h"

namespace ro::render {
namespace {

// LES TROIS BITS QUI POSENT DES COPIES DU CORPS.
//
// ⚠ CE SONT DES BITS PORTÉS, et la mesure les lit ici : chaque constante de ce
// fichier déclare un bit que `libs/render` sait montrer. En ajouter un le
// déclare sans qu'on ait à tenir une liste ailleurs.
//
// `0x8` fait RESPIRER deux copies blanches autour du corps — la lueur
// d'`EF_ASSUMPTIO`. `0x800` et `0x80000` posent deux copies légèrement plus
// grandes et teintées, dont l'alpha est le paramètre `kOutlineAlphaField` : le
// halo bleu d'`EF_ASURABODY`, le vert d'`EF_UNDEADBODY`.
inline constexpr std::uint32_t kBreathingGlow = 0x8;
inline constexpr std::uint32_t kBlueOutline = 0x800;
inline constexpr std::uint32_t kGreenOutline = 0x80000;

// LES ANNEAUX QUI S'ÉCARTENT — `EF_REFLECTBODY`.
//
// Le client pose QUATRE copies de plus en plus larges, chacune décalée d'un
// quart de cycle, et dont l'alpha tombe à mesure qu'elles s'élargissent. C'est
// le bouclier qui renvoie les coups. Chaque calque s'élargit autour de son
// centre : les anneaux partent dans toutes les directions.
inline constexpr std::uint32_t kSpreadingRings = 0x200;

// LA CROIX — `EF_4WAYBODY`.
//
// Le client pose QUATRE copies qui s'écartent du corps dans deux axes opposés,
// en s'effaçant.
inline constexpr std::uint32_t kSpreadCross = 0x1000;

// LES BITS QUI RETIENNENT UNE COPIE : le saut et la réception éteignent la
// lueur qui respire, la réception seule les contours.
inline constexpr std::uint32_t kFadeOutRising = 0x10000;
inline constexpr std::uint32_t kFadeInFalling = 0x20000;

// LES CALQUES QUE LA LUEUR QUI RESPIRE REPREND : voir `kBodyAndHeadwearParts`.
inline constexpr ActorPartSet kBreathingGlowParts = kBodyAndHeadwearParts;

// L'alpha des deux copies qui respirent, tiré des couleurs 0x64FFFFFF et
// 0x78FFFFFF que le client leur donne.
inline constexpr std::uint8_t kInnerGlowAlpha = 100;
inline constexpr std::uint8_t kOuterGlowAlpha = 120;

// DE COMBIEN DE PIXELS chacune s'élargit. Le client ajoute `sin(phase) + 3` à
// la première et `2 × sin(phase) + 5` à la seconde, de chaque côté du quad.
inline constexpr float kInnerGlowBase = 3.0F;
inline constexpr float kOuterGlowBase = 5.0F;
inline constexpr float kOuterGlowSwing = 2.0F;

// LA PHASE de la respiration. Le client tient un octet qu'il incrémente à
// chaque image et remet à zéro passé cent quatre-vingts ; l'angle se lit alors
// dans sa table de sinus.
//
// ⚠ NOUS COMPTONS EN TICS D'EFFET, LUI EN IMAGES. La respiration est donc un
// peu plus lente chez nous que chez lui, et c'est dit ici plutôt que caché : le
// nombre d'images par seconde n'est pas un fait du client, c'est un fait de la
// machine, et le rejouer à l'identique demanderait de le porter.
inline constexpr std::uint32_t kPhaseWrap = 181;

// Les couleurs des deux contours, tirées de `0x3737FF` et `0x056405`.
//
// ⚠ LE CLIENT EMPAQUETTE EN BLEU, VERT, ROUGE, et l'ordre ne se devine pas d'un
// nombre. Ce qui le PROUVE est ailleurs : le quad teinté à durée
// du client reçoit ses trois composantes en clair, et le bit `0x4000` — celui de
// `EF_BLUEBODY`, `EF_BLUE_HIT`, `EF_BLUELIGHTBODY`, `EF_MADNESS_BLUE` — lui
// passe `(5, 5, 255)` là où le bit `0x40000` — `EF_REDLIGHTBODY`, `EF_RED_HIT`
// — lui passe `(255, 5, 5)`. Six noms bleus et trois noms rouges tranchent
// l'ordre sans qu'on ait à le supposer, et il est l'INVERSE de celui qu'on lit
// spontanément dans `0x3737FF`.
inline constexpr std::array<std::uint8_t, 3> kBlueOutlineColor = {55, 55, 255};
inline constexpr std::array<std::uint8_t, 3> kGreenOutlineColor = {5, 100, 5};

// LES DEUX ÉCHELLES d'un contour : le client boucle deux fois et grandit d'un
// centième de plus à chaque tour.
inline constexpr float kOutlineScaleStep = 0.01F;
inline constexpr std::size_t kOutlineCopies = 2;

// Le champ qui porte l'alpha d'un contour.
inline constexpr std::uint16_t kOutlineAlphaField = 0xF2;

inline constexpr std::int32_t kOpaque = 255;

// ── Les anneaux qui s'écartent ──────────────────────────────────────────────

// Le client tient son propre compteur d'IMAGES pour ces anneaux — un octet
// qu'il incrémente lui-même et ramène à zéro passé deux cents. Il ne
// vient donc pas de l'effet, et un anneau tourne tant que le bit est levé.
inline constexpr std::uint32_t kRingPhaseWrap = 200;

// DE COMBIEN UN ANNEAU S'ÉLARGIT par pas de compteur, et où il repart.
inline constexpr float kRingPixelsPerStep = 0.1F;
inline constexpr float kRingWidthPixels = 20.0F;
inline constexpr std::size_t kRingCount = 4;

// L'ALPHA D'UN ANNEAU : cent au plus étroit, zéro au plus large.
inline constexpr float kRingAlphaAtCenter = 100.0F;

// ── La croix ────────────────────────────────────────────────────────────────

// Le champ que la croix lit, de combien elle s'écarte par pas, et la pente de
// son effacement. `EF_4WAYBODY` n'écrit son compteur qu'à partir du trentième
// tic : la croix part donc du corps et s'en va sur les trente suivants.
inline constexpr std::uint16_t kCrossField = 0xF4;
inline constexpr float kCrossPixelsPerStep = 4.0F;
inline constexpr std::int32_t kCrossAlphaAtCenter = 150;
inline constexpr std::int32_t kCrossAlphaPerStep = 5;
inline constexpr std::size_t kCrossArms = 4;

float SineOfPhase(std::uint32_t age_ticks) {
  const auto degrees = static_cast<float>(age_ticks % kPhaseWrap);
  return std::sin(degrees * std::numbers::pi_v<float> / 180.0F);
}

const ActorFieldWrite* WriteOn(const ActorEffectChange& change, std::uint16_t field) {
  for (const ActorFieldWrite& write : change.writes) {
    if (write.field == field) {
      return &write;
    }
  }
  return nullptr;
}

}  // namespace

std::size_t BodyCopiesAfterEffects(std::span<const ActorEffectPlay> played,
                                   std::span<ActorBodyCopy> out) {
  std::size_t written = 0;
  const auto ajouter = [&](float scale, std::array<std::uint8_t, 4> tint,
                           std::array<float, 2> offset_pixels = {0.0F, 0.0F},
                           ActorPartSet parts = kAllActorParts, bool additive = false,
                           bool in_front = false, float grow_pixels = 0.0F) {
    if (written < out.size()) {
      out[written++] =
          ActorBodyCopy{scale, tint, offset_pixels, parts, additive, in_front, grow_pixels};
    }
  };
  // ⚠ LES GARDES LISENT LE MOT DE RENDU ENTIER, pas les bits d'un seul effet :
  // un saut porté par un autre effet éteint la lueur tout autant.
  std::uint32_t raised_by_all = 0;
  for (const ActorEffectPlay& play : played) {
    if (const ActorEffectChange* change = ActorEffectChangeFor(play.effect_id)) {
      raised_by_all |= RenderBitsAt(*change, play.age_ticks);
    }
  }
  for (const ActorEffectPlay& play : played) {
    const ActorEffectChange* change = ActorEffectChangeFor(play.effect_id);
    if (change == nullptr) {
      continue;
    }
    // Les bits que l'effet relève à cet âge, et eux seuls : voir `RenderBitsAt`.
    const std::uint32_t raised = RenderBitsAt(*change, play.age_ticks);
    if ((raised & kBreathingGlow) != 0 &&
        (raised_by_all & (kFadeOutRising | kFadeInFalling)) == 0) {
      const float sine = SineOfPhase(play.age_ticks);
      ajouter(1.0F, {255, 255, 255, kInnerGlowAlpha}, {0.0F, 0.0F}, kBreathingGlowParts, true,
              false, sine + kInnerGlowBase);
      ajouter(1.0F, {255, 255, 255, kOuterGlowAlpha}, {0.0F, 0.0F}, kBreathingGlowParts, true,
              false, kOuterGlowSwing * sine + kOuterGlowBase);
    }
    if ((raised & kSpreadingRings) != 0) {
      const auto phase = static_cast<float>(play.age_ticks % kRingPhaseWrap);
      for (std::size_t ring = 1; ring <= kRingCount; ++ring) {
        // Les quatre anneaux sont à un quart de cycle les uns des autres, et
        // celui qui dépasse la largeur repart du corps.
        float width = phase * kRingPixelsPerStep +
                      static_cast<float>(ring) * kRingWidthPixels / static_cast<float>(kRingCount);
        if (width >= kRingWidthPixels) {
          width -= kRingWidthPixels;
        }
        const auto alpha = static_cast<std::uint8_t>(std::clamp<std::int32_t>(
            static_cast<std::int32_t>(kRingAlphaAtCenter * (1.0F - width / kRingWidthPixels)), 0,
            kOpaque));
        // ⚠ DEVANT LE CORPS : le client avance leur profondeur, et ils
        // voilent le corps qu'ils entourent.
        ajouter(1.0F, {255, 255, 255, alpha}, {0.0F, 0.0F}, kAllActorParts, false, true, width);
      }
    }
    if ((raised & kSpreadCross) != 0) {
      if (const ActorFieldWrite* write = WriteOn(*change, kCrossField)) {
        // ⚠ LE COMPTEUR RESTE À ZÉRO TANT QUE LE CLIENT NE L'ÉCRIT PAS.
        // `EF_4WAYBODY` ne l'écrit qu'à partir de son trentième tic — sa rampe
        // vaut tic moins trente, et le cas entier est sous une garde —, si
        // bien que l'octet de l'acteur vaut encore zéro avant. Laisser la
        // valeur négative écarterait la croix À L'ENVERS et la rendrait plus
        // opaque que le corps, trente tics avant qu'elle ne doive paraître.
        const std::int32_t step = std::max(0, ValueAt(*write, play.age_ticks, change->life_ticks));
        const float offset = kCrossPixelsPerStep * static_cast<float>(step);
        const auto alpha = static_cast<std::uint8_t>(
            std::clamp<std::int32_t>(kCrossAlphaAtCenter - kCrossAlphaPerStep * step, 0, kOpaque));
        // ⚠ DEUX BRAS PAR AXE, ET LA CROIX EST SYMÉTRIQUE : le client écarte
        // deux copies le long d'un couple de bords et deux le long de l'autre,
        // du même nombre de pixels. Lequel des deux couples est l'horizontale
        // ne se lit pas dans son code, et ne se voit pas non plus.
        static_assert(kCrossArms == 4);
        ajouter(1.0F, {255, 255, 255, alpha}, {offset, 0.0F});
        ajouter(1.0F, {255, 255, 255, alpha}, {-offset, 0.0F});
        ajouter(1.0F, {255, 255, 255, alpha}, {0.0F, offset});
        ajouter(1.0F, {255, 255, 255, alpha}, {0.0F, -offset});
      }
    }
    const bool blue = (raised & kBlueOutline) != 0;
    const bool green = (raised & kGreenOutline) != 0;
    if ((!blue && !green) || (raised_by_all & kFadeInFalling) != 0) {
      continue;
    }
    // ⚠ LE BLEU PASSE DEVANT LE VERT, comme chez le client : les deux bits se
    // rencontrent, et son `if` teste le bleu en premier.
    const std::array<std::uint8_t, 3> color = blue ? kBlueOutlineColor : kGreenOutlineColor;
    std::uint8_t alpha = kOpaque;
    if (const ActorFieldWrite* write = WriteOn(*change, kOutlineAlphaField)) {
      alpha = static_cast<std::uint8_t>(std::clamp<std::int32_t>(
          ValueAt(*write, play.age_ticks, change->life_ticks), 0, kOpaque));
    }
    // ⚠ DEVANT LE CORPS : le client avance leur profondeur au lieu de la
    // reculer, et leur addition teint le corps entier. Peints derrière lui, on
    // n'en voyait que le pourtour.
    for (std::size_t copie = 1; copie <= kOutlineCopies; ++copie) {
      ajouter(1.0F + static_cast<float>(copie) * kOutlineScaleStep,
              {color[0], color[1], color[2], alpha}, {0.0F, 0.0F}, kAllActorParts, true, true);
    }
  }
  return written;
}

}  // namespace ro::render
