#pragma once

#include <array>
#include <cstddef>
#include <cstdint>
#include <span>

#include <imgui.h>

namespace ro::ui {

// LES EFFETS D'UNE PARTIE DE « SURVIVANTS DE MIDGARD », tels que le joueur
// les voit :
//
//   - au HUD, une colonne sur le bord droit, sous les compteurs, des effets
//     limités dans le temps : l'icône de l'objet, un voile sur la part
//     écoulée, les secondes restantes dessous ; l'icône clignote les
//     `kSurvivorsEffectBlinkMs` dernières millisecondes ;
//   - au HUD encore, l'avis d'un objet à effet ramassé, au-dessus du héros :
//     son nom et son effet en clair ;
//   - dans la pause, les effets permanents de la partie — cartes, Bénédictions,
//     charges Kafra, vies —, une ligne chacun avec son effet chiffré.
//
// 🔴 `ui` NE CONNAÎT AUCUN TYPE DE `sim` : `app` dit la sorte d'un chiffre par
// `SurvivorsEffectFigure`, et `ui` en écrit le texte traduit.

// CE QU'UN CHIFFRE MESURE : une statistique du héros, l'effet d'un objet au
// sol, une charge ou une vie qui reste.
enum class SurvivorsEffectFigure : std::uint8_t {
  kDamagePct,
  kMaxHp,
  kArmor,
  kRegenPerMinute,
  kWalkPct,
  kCooldownPct,  // une réduction : la valeur est retranchée
  kAmount,
  kAreaPct,
  kDurationPct,
  kLuckPct,
  kMagnetPct,
  kExpPct,
  kHeal,
  kZeny,
  kPurifyCells,  // la portée de la purification, en cases
  kGatherGems,   // sans valeur
  kRerollsLeft,
  kSkipsLeft,
  kBanishesLeft,
  kLivesLeft,
};

// UN EFFET EN COURS, au HUD : l'icône de l'objet qui l'a donné, ce qui en
// reste et sa durée pleine, pour le voile.
struct SurvivorsHudEffect {
  ImTextureID icon = 0;
  std::uint32_t remaining_ms = 0;
  std::uint32_t full_ms = 0;
};

// L'AVIS D'UN OBJET RAMASSÉ : son nom (résolu par `app`), son effet, sa durée
// (0 : immédiat), et où il en est de sa vie, de 0 à 1.
struct SurvivorsHudNotice {
  const char* name = "";
  SurvivorsEffectFigure figure = SurvivorsEffectFigure::kHeal;
  std::int32_t value = 0;
  std::uint32_t duration_ms = 0;
  float fraction = 0.0F;
};

// UN EFFET PERMANENT, dans la pause : l'icône et le nom de ce qui le donne,
// son niveau (0 : sans niveau) et son effet chiffré.
struct SurvivorsPauseEffectRow {
  ImTextureID icon = 0;
  const char* name = "";
  int level = 0;
  SurvivorsEffectFigure figure = SurvivorsEffectFigure::kDamagePct;
  std::int32_t value = 0;
};

// LE CLIGNOTEMENT DE LA FIN : sous ce reste, l'icône s'allume et s'éteint à
// chaque demi-période.
inline constexpr std::uint32_t kSurvivorsEffectBlinkMs = 3000;
inline constexpr std::uint32_t kSurvivorsEffectBlinkHalfPeriodMs = 250;

// Les secondes affichées sous l'icône : arrondies au-dessus, si bien qu'un
// effet qui dure encore ne montre jamais 0.
[[nodiscard]] std::uint32_t SurvivorsEffectSecondsLeft(std::uint32_t remaining_ms) noexcept;

// L'icône se peint-elle à ce reste ? Toujours au-dessus du seuil de
// clignotement, une demi-période sur deux en dessous.
[[nodiscard]] bool SurvivorsEffectIconLit(std::uint32_t remaining_ms) noexcept;

// LA PART VOILÉE DE L'ICÔNE, de 0 (effet neuf) à 1 (effet fini) : la part
// écoulée de la durée pleine. Une durée nulle voile tout.
[[nodiscard]] float SurvivorsEffectVeil(std::uint32_t remaining_ms, std::uint32_t full_ms) noexcept;

// L'opacité d'un avis le long de sa vie : entier, puis il s'efface pendant
// son dernier quart.
[[nodiscard]] float SurvivorsNoticeAlpha(float fraction) noexcept;

// LES TEXTES, traduits : un chiffre dans son unité (« Expérience +100 % »),
// puis l'avis entier (« Battle Manual : Expérience +100 %, 30 s »).
inline constexpr std::size_t kSurvivorsFigureTextSize = 96;
using SurvivorsFigureText = std::array<char, kSurvivorsFigureTextSize>;
[[nodiscard]] SurvivorsFigureText SurvivorsEffectFigureText(SurvivorsEffectFigure figure,
                                                            std::int32_t value);
inline constexpr std::size_t kSurvivorsNoticeTextSize = 160;
using SurvivorsNoticeText = std::array<char, kSurvivorsNoticeTextSize>;
[[nodiscard]] SurvivorsNoticeText SurvivorsNoticeTextOf(const SurvivorsHudNotice& notice);

// LA COLONNE DES EFFETS EN COURS, alignée à droite sur `right` depuis `top`,
// peinte sur `draw` ; rend l'ordonnée sous la dernière case.
float DrawSurvivorsHudEffects(ImDrawList& draw, std::span<const SurvivorsHudEffect> effects,
                              float right, float top);

// LES AVIS, empilés au-dessus de `anchor` (les pieds du héros), le plus récent
// en bas.
void DrawSurvivorsHudNotices(ImDrawList& draw, std::span<const SurvivorsHudNotice> notices,
                             ImVec2 anchor);

// LES EFFETS PERMANENTS, dans la fenêtre ouverte : un titre et une ligne par
// effet ; rien quand la liste est vide.
void DrawSurvivorsPauseEffects(std::span<const SurvivorsPauseEffectRow> rows);

}  // namespace ro::ui
