// La gerbe de reflets d'un impact. Voir l'en-tête pour le tour et l'alternance.

#include "ro/render/effect_lens_flare_seed.h"

#include <algorithm>
#include <cmath>
#include <cstddef>

#include "ro/render/effect_node.h"
#include "ro/render/map_effect_math.h"

namespace ro::render {
namespace {

// LA TABLE, tenue ici et non engendrée : elle porte UN bâtisseur, champ par
// champ.
//
// ⚠ LES TIRAGES SONT DANS L'ORDRE DU CLIENT, et cet ordre compte : deux
// reflets tirés dans un autre ordre ne se ressemblent plus, le générateur
// n'avançant pas de la même façon.
//
// UN SEUL BÂTISSEUR POUR DEUX EFFETS : seuls l'identifiant et la teinte
// changent de l'un à l'autre, et ils ne changent ni le compte ni l'ordre des
// tirages.
//
// ⚠ 401 N'EN EST PAS, bien qu'il appelle le même bâtisseur : il ne l'appelle
// qu'aux tics 20 à 60, et le bâtisseur ne sème qu'au tic zéro. Il ne montre
// donc aucun reflet — seulement son bruit.
[[nodiscard]] constexpr EffectLensFlarePattern MakeFlarePattern(std::uint32_t effect_id,
                                                                std::array<std::int32_t, 3> color,
                                                                bool follows_bearer) {
  EffectLensFlarePattern spray = {effect_id,
                                  "EffectUpdate_HitLensFlare",
                                  "data\\texture\\effect\\lens1.tga",
                                  "data\\texture\\effect\\lens2.tga",
                                  45.0F,
                                  360.0F,
                                  // vie : rand() % 20 + 10
                                  {20, 10.0F, 1.0F},
                                  // cap : rand() % 30, le biais de moins quinze s'ajoute à part
                                  {30, 0.0F, 1.0F},
                                  // pas : (rand() % 45 + 5) / 10
                                  {45, 5.0F, 10.0F},
                                  // demi-largeur : rand() % 15 + 5
                                  {15, 5.0F, 1.0F},
                                  // demi-hauteur : rand() % 20 + 20
                                  {20, 20.0F, 1.0F},
                                  // rayon du décalage : rand() % 5
                                  {5, 0.0F, 1.0F},
                                  // l'étirement : 1,5 par tic, accéléré de 0,25
                                  1.5F,
                                  0.25F,
                                  -15.0F,
                                  -20.0F,
                                  0.0F,
                                  254.0F,
                                  0.125F,
                                  1U,
                                  color,
                                  follows_bearer};
  return spray;
}

// LA TEINTE BLEUTÉE : rouge et vert rabattus, le bleu garde le blanc.
constexpr std::array<std::int32_t, 3> kWhite = {255, 255, 255};
constexpr std::array<std::int32_t, 3> kBleute = {120, 120, 255};

constexpr std::array<EffectLensFlarePattern, 2> kLensFlares = {{
    MakeFlarePattern(1, kWhite, true),
    MakeFlarePattern(183, kBleute, false),
}};

[[nodiscard]] float DrawInRange(const EffectLensFlareDraw& plage, std::uint32_t& rng_state) {
  const float brut = plage.spread > 0 ? static_cast<float>(NextEffectNodeDraw(rng_state) %
                                                           static_cast<std::uint32_t>(plage.spread))
                                      : 0.0F;
  return (brut + plage.floor) / plage.divisor;
}

}  // namespace

std::span<const EffectLensFlarePattern> EffectLensFlarePatternTable() { return kLensFlares; }

std::span<const EffectLensFlarePattern> EffectLensFlarePatternsFor(std::uint32_t effect_id) {
  const auto first = std::ranges::find_if(
      kLensFlares,
      [effect_id](const EffectLensFlarePattern& p) { return p.effect_id == effect_id; });
  if (first == kLensFlares.end()) {
    return {};
  }
  const auto after = std::find_if(first, kLensFlares.end(),
                                  [effect_id](const auto& p) { return p.effect_id != effect_id; });
  return std::span<const EffectLensFlarePattern>(first, after);
}

std::vector<EffectLensFlareNode> SowEffectLensFlareNodes(const EffectLensFlarePattern& pattern,
                                                         std::uint32_t& generator) {
  std::vector<EffectLensFlareNode> spray;
  if (pattern.turn_step_degrees <= 0.0F || pattern.full_turn_degrees <= 0.0F) {
    return spray;
  }
  // LE COMPTE EST CELUI DU TOUR, et non un nombre écrit : c'est ainsi que le
  // client s'arrête, et changer le pas change le compte sans rien d'autre.
  const auto count =
      static_cast<std::size_t>(pattern.full_turn_degrees / pattern.turn_step_degrees);
  spray.reserve(count);

  for (std::size_t rang = 0; rang < count; ++rang) {
    const float branche = pattern.turn_step_degrees * static_cast<float>(rang);
    EffectScreenNode node;
    node.shape = EffectScreenShape::kQuad;
    node.flags = pattern.flags;

    const float life = DrawInRange(pattern.life, generator);
    node.life_ticks = static_cast<std::int32_t>(life);
    // ⚠ UNE VIE NULLE DIVISERAIT. Le plancher du tirage l'interdit, mais la
    // division qui suit est la raison d'être de cette garde : elle est dans le
    // client aussi, sous la forme d'un plancher.
    const float safe_life = std::max(life, 1.0F);

    // LE CAP ET L'ANGLE DU QUAD SONT LE MÊME NOMBRE : le reflet se couche dans
    // la direction où il part. Les séparer le ferait glisser de travers.
    const float cap =
        DrawInRange(pattern.heading, generator) + branche + pattern.heading_bias_degrees;
    node.heading_degrees = cap;
    node.turn_degrees = cap;

    node.step = DrawInRange(pattern.step, generator);
    // ⚠ LA MOITIÉ, et non le quotient entier : le reflet ralentit sans jamais
    // s'arrêter tout à fait dans sa vie.
    node.step_change = -(node.step / safe_life) * 0.5F;

    node.half_width = DrawInRange(pattern.half_width, generator);
    node.half_height = DrawInRange(pattern.half_height, generator);
    // 🔴 ET LA HAUTEUR GRANDIT. Le reflet s'étire en une aiguille au lieu de
    // garder la taille de sa naissance : c'est là qu'était toute sa puissance.
    node.half_height_step = pattern.half_height_step;
    node.half_height_acceleration = pattern.half_height_acceleration;
    // LA LARGEUR S'ÉTEINT EN UNE VIE, la hauteur ne bouge pas : c'est ce qui
    // amincit le reflet en une aiguille au lieu de le rapetisser.
    node.half_width_step = -(node.half_width / safe_life);

    // LE DÉCALAGE PART DU POINT, sur un rayon tiré, et descend de vingt pixels.
    // ⚠ L'ORDONNÉE D'ÉCRAN DESCEND : le cosinus est retranché, et le
    // soulèvement est donc négatif.
    // 🔴 L'ABSCISSE PREND LE SINUS DE LA BRANCHE ET L'ORDONNÉE SON COSINUS : les
    // deux tables du client ont leurs noms échangés. La branche, tronquée, s'y
    // lit comme un degré entier.
    const float radius = DrawInRange(pattern.offset_radius, generator);
    const float angle = NativeTableRadians(static_cast<std::int32_t>(branche));
    node.screen_offset = {std::sin(angle) * radius,
                          -std::cos(angle) * radius + pattern.offset_lift};

    node.alpha = pattern.alpha_start;
    node.alpha_max = pattern.alpha_max;
    node.alpha_step = pattern.alpha_max * pattern.alpha_rise_share;
    node.color = pattern.color;

    spray.push_back(
        EffectLensFlareNode{node, (rang % 2 == 0) ? pattern.even_texture : pattern.odd_texture});
  }
  return spray;
}

}  // namespace ro::render
