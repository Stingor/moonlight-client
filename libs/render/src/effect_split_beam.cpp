// Le trait fendu et son semis. Voir l'en-tête pour l'arête partagée, le facteur
// d'un demi et les paliers du lancer.

#include "ro/render/effect_split_beam.h"

#include <array>
#include <cmath>

#include "ro/core/sorted_table.h"
#include "ro/render/map_effect_math.h"

#include "effect_draw_below.h"

namespace ro::render {
namespace {

// LA TABLE, tenue ici et non engendrée : un seul bâtisseur, décrit champ par
// champ.
//
// ⚠ LA TABLE ENGENDRÉE N'EN REND QU'UNE MOITIÉ, et la moitié qu'elle rend a
// l'air complète : elle a capté la branche du jeu LONG — cent à cent neuf de
// demi-hauteur, dix de demi-largeur — et laissé l'autre, les neuf exemplaires,
// les angles et les décalages. C'est le piège de l'ADR 0020 dans sa forme
// ordinaire : ce qui est rendu est nécessaire, pas suffisant.
//
// ⚠ ET UN CHAMP DORT ICI. Le bâtisseur écrit vingt dans la borne d'arrêt de
// croissance — mais il ne pose jamais le drapeau qui la fait lire, et elle ne
// commande donc rien. La porter dans le patron ajouterait un nombre sans
// effet ; la taire ferait croire à un oubli le jour où quelqu'un relira le cas.
constexpr std::array<EffectSplitBeamPattern, 1> kSplitBeams = {{
    {51, "Effect_Build_IceSmokeColumn", "data\\texture\\effect\\ice.tga",
     // neuf éclats, tous les quarante degrés : la boucle tourne tant que
     // « base + 40 » reste sous le tour entier.
     9, 40.0F,
     // un sur trois est long : `rang % 3` choisit la branche.
     3,
     // vie : quinze tics, sortie à « vie moins dix »
     15, 5,
     // alpha : deux cents
     200.0F,
     // le cap : base − 15 + rand() % 60, écrit comme cap ET comme angle
     -15.0F, 60,
     // le rayon de naissance : rand() % 5, en pixels
     5,
     // et le décalage vertical : vingt pixels plus haut
     -20.0F,
     // le freinage : `-(pas / vie) / 1.5`
     1.5F,
     // le jeu LONG : demi-hauteur rand() % 10 + 100, demi-largeur 10,
     // pas (rand() % 45) / 10 + 12,8
     EffectSplitBeamSize{10, 100.0F, 10.0F, 12.8F},
     // et le COURT : rand() % 5 + 45, demi-largeur 4, pas … + 4,8
     EffectSplitBeamSize{5, 45.0F, 4.0F, 4.8F}},
}};

// LA FIGURE INDEXE UNE TABLE DE DEGRÉS ENTIERS, comme partout dans le client :
// l'angle se TRONQUE avant d'être employé, il ne s'arrondit pas, et un négatif
// s'y lit un degré de travers — voir `NativeTableRadians`.
[[nodiscard]] float CosOfTruncatedDegrees(float degrees) {
  return std::cos(NativeTableRadians(static_cast<std::int32_t>(std::trunc(degrees))));
}

[[nodiscard]] float SinOfTruncatedDegrees(float degrees) {
  return std::sin(NativeTableRadians(static_cast<std::int32_t>(std::trunc(degrees))));
}

// UN ÉCLAT, à son rang dans l'étoile. L'ordre des tirages est celui du client.
[[nodiscard]] EffectScreenNode SowShard(const EffectSplitBeamPattern& node_pattern,
                                        std::uint16_t rang, std::uint32_t& rng_state) {
  const float base = node_pattern.base_step_degrees * static_cast<float>(rang);
  const EffectSplitBeamSize& size =
      node_pattern.long_every != 0 && rang % node_pattern.long_every == 0
          ? node_pattern.long_shard
          : node_pattern.short_shard;

  EffectScreenNode effect_node;
  effect_node.shape = EffectScreenShape::kSplitBeam;
  effect_node.life_ticks = node_pattern.life_ticks;
  effect_node.fade_out_tick = node_pattern.fade_out_tick;
  // ⚠ L'ALPHA COURANT SEUL : le bâtisseur ne touche pas au PLAFOND, qui garde
  // les 254 du constructeur. Le reposer à deux cents ici inventerait une valeur,
  // et ne changerait rien tant que rien ne fait monter l'alpha — ce qui est le
  // cas, et pourrait cesser de l'être.
  effect_node.alpha = node_pattern.alpha;

  // LE CAP ET L'ANGLE DE LA FIGURE SONT LE MÊME NOMBRE : le bâtisseur l'écrit
  // deux fois, comme cap et comme angle. Un éclat file donc dans la direction
  // où il pointe, et les séparer le ferait glisser de travers.
  const float cap = base + node_pattern.heading_bias_degrees +
                    static_cast<float>(DrawBelowIfPositive(rng_state, node_pattern.heading_spread));
  effect_node.heading_degrees = cap;
  effect_node.turn_degrees = cap;

  effect_node.half_width = size.half_width;
  effect_node.half_height = size.half_height_floor + static_cast<float>(DrawBelowIfPositive(
                                                         rng_state, size.half_height_spread));

  // ⚠ LE PAS SE TIRE PAR PALIERS, division ENTIÈRE comprise.
  effect_node.step =
      size.step_floor + static_cast<float>(DrawBelowIfPositive(rng_state, kSplitBeamStepSpread) /
                                           static_cast<std::int32_t>(kSplitBeamStepDivisor));
  // ⚠ ET LE FREINAGE SE CALCULE, il ne se pose pas : une vie nulle ne freinerait
  // rien, et un diviseur nul non plus.
  effect_node.step_change =
      node_pattern.life_ticks != 0 && node_pattern.step_slowdown_divisor != 0.0F
          ? -(effect_node.step / static_cast<float>(node_pattern.life_ticks)) /
                node_pattern.step_slowdown_divisor
          : 0.0F;

  // LA PLACE DE NAISSANCE : sur un cercle de rayon tiré, autour de l'angle de
  // BASE et non du cap. ⚠ L'ABSCISSE PREND LE SINUS ET L'ORDONNÉE PERD SON
  // COSINUS — les deux tables du client ont leurs noms échangés —, et le
  // décalage vertical s'ajoute par-dessus.
  const auto radius =
      static_cast<float>(DrawBelowIfPositive(rng_state, node_pattern.birth_radius_spread));
  effect_node.screen_offset = {
      SinOfTruncatedDegrees(base) * radius,
      -CosOfTruncatedDegrees(base) * radius + node_pattern.screen_offset_y};
  return effect_node;
}

}  // namespace

std::vector<EffectScreenQuad> ComposeScreenSplitBeamQuads(const EffectScreenNode& node,
                                                          glm::vec2 owner_screen_position) {
  // LE POINT, L'ANGLE, LA TEINTE ET L'IMAGE viennent du quad : le trait fendu
  // partage son mouvement (sous-types 0 et 2 dans le même cas), donc aussi la
  // façon dont sa place se compose — décalage, porteur, et déplacement cumulé.
  const EffectScreenQuadRequest request = EffectScreenQuadRequestOf(node, owner_screen_position);
  const float half_height = request.half_height * kSplitBeamHalfShare;
  const float half_width = request.half_width * kSplitBeamHalfShare;
  if (half_height <= 0.0F || half_width <= 0.0F) {
    return {};
  }
  const float tip = half_height / kSplitBeamApexDivisor;
  const float sinus = SinOfTruncatedDegrees(static_cast<float>(request.turn_degrees));
  const float cosinus = CosOfTruncatedDegrees(static_cast<float>(request.turn_degrees));
  // LA ROTATION USUELLE, comme celle du quad : voir `ComposeScreenQuad`.
  const auto rotated = [&request, sinus, cosinus](float u, float v) {
    return glm::vec2{request.screen_position.x + (u * cosinus) - (v * sinus),
                     request.screen_position.y + (u * sinus) + (v * cosinus)};
  };

  // L'ARÊTE, commune aux deux triangles.
  const glm::vec2 arete_haute = rotated(0.0F, half_height);
  const glm::vec2 arete_basse = rotated(0.0F, -half_height);

  // ⚠ LES DEUX POINTES NE SONT PAS SYMÉTRIQUES DE L'ARÊTE : celle de gauche est
  // décalée vers le bas de la différence entre la demi-longueur et ses cinq
  // sixièmes, celle de droite vers le haut. C'est ce décalage qui donne au trait
  // sa forme de pointe de glace.
  const glm::vec2 left_tip = rotated(-half_width, tip - half_height);
  const glm::vec2 right_tip = rotated(half_width, half_height - tip);

  const auto triangle = [&](glm::vec2 vertex, float abscisse) {
    EffectScreenQuad aile;
    // LE QUATRIÈME COIN REPREND LA POINTE : le second triangle du quad est
    // d'aire nulle, et la figure reste le triangle que le client soumet.
    aile.corners = {vertex, arete_haute, arete_basse, vertex};
    aile.texture_uv = {
        glm::vec2{abscisse, kSplitBeamApexV}, glm::vec2{kSplitBeamSpineU, kSplitBeamSpineHighV},
        glm::vec2{kSplitBeamSpineU, kSplitBeamSpineLowV}, glm::vec2{abscisse, kSplitBeamApexV}};
    aile.tint = request.tint;
    aile.blend_mode = kSplitBeamBlendMode;
    aile.texture = request.texture;
    return aile;
  };

  // ⚠ LE CLIENT DONNE UN BLANC OPAQUE À L'UN DES TROIS SOMMETS, celui du haut de
  // l'arête, et la teinte du nœud aux deux autres : le trait s'éclaircit vers un
  // bout. Notre quad d'écran porte UNE teinte pour ses quatre coins, et le
  // dégradé ne se porte donc pas. C'est un écart NOMMÉ ; le rendre demanderait
  // une couleur PAR SOMMET dans la voie entière, pour une différence d'alpha qui
  // vaut ici cinquante-cinq sur deux cent cinquante-cinq, à un sommet sur trois.
  return {triangle(left_tip, kSplitBeamLeftApexU), triangle(right_tip, kSplitBeamRightApexU)};
}

std::span<const EffectSplitBeamPattern> EffectSplitBeamPatternTable() { return kSplitBeams; }

std::span<const EffectSplitBeamPattern> EffectSplitBeamPatternsFor(std::uint32_t effect_id) {
  return core::RowsWithKey(EffectSplitBeamPatternTable(), effect_id,
                           &EffectSplitBeamPattern::effect_id);
}

std::vector<EffectScreenNode> SowEffectSplitBeamNodes(const EffectSplitBeamPattern& pattern,
                                                      std::uint32_t& generator) {
  std::vector<EffectScreenNode> eclats;
  eclats.reserve(pattern.count);
  for (std::uint16_t rang = 0; rang < pattern.count; ++rang) {
    eclats.push_back(SowShard(pattern, rang, generator));
  }
  return eclats;
}

}  // namespace ro::render
