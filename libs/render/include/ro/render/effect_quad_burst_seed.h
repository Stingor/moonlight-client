#pragma once

#include <array>
#include <cstdint>
#include <span>
#include <string_view>

#include <glm/glm.hpp>

#include "ro/render/effect_node_pool_fwd.h"
#include "ro/render/effect_track_fwd.h"

namespace ro::render {

// LE SEMIS DE LA RAFALE, sous-type 29.
//
// UN SEUL bâtisseur pour les huit identifiants, et tout ce qui les distingue tient
// dans un OCTET de zéro à sept — qui choisit l'image, la marque, la taille, la
// hauteur de départ, le nombre de pistes vives et l'écart initial. Six choses.
//
// ⚠ CHAQUE OCTET A SES PROPRES VALEURS : garder celles d'une seule branche pour
// toutes donnerait un rayon de deux virgule quatre à tous, là où l'octet zéro
// en pose huit dixièmes, et deux images là où le bâtisseur n'en charge qu'une.

// LES TROIS IMAGES, une par branche. Aucune ne vient du site.
inline constexpr std::string_view kQuadBurstFreezingTexture =
    "data\\texture\\effect\\freezing_circle.bmp";
inline constexpr std::string_view kQuadBurstWhiteTexture = "data\\texture\\effect\\whitelight.tga";
inline constexpr std::string_view kQuadBurstGhostTexture = "data\\texture\\effect\\ghost.bmp";

// LES TROIS TAILLES DE CARRÉ, en unités du monde.
inline constexpr float kQuadBurstSmallRadius = 0.8F;
inline constexpr float kQuadBurstRadius = 2.4F;
inline constexpr float kQuadBurstGhostRadius = 3.2F;

// LES DEUX VITESSES DE MONTÉE, rangées dans la clé de la piste.
inline constexpr float kQuadBurstSpeed = 0.15F;
inline constexpr float kQuadBurstGhostSpeed = 0.6F;

// LES QUATRE HAUTEURS DE DÉPART de la marque quatorze.
//
// ⚠ QUARANTE, CINQUANTE, SOIXANTE, SOIXANTE-DIX : les quatre particules
// partent ÉCHELONNÉES, et c'est ce qui fait la colonne. Les poser à la même
// hauteur les ferait monter ensemble, en un seul paquet.
inline constexpr std::array<float, 4> kQuadBurstRisingStarts = {-40.0F, -50.0F, -60.0F, -70.0F};

// L'ÉCHELLE DU NŒUD, que seul un octet pose.
inline constexpr float kQuadBurstScaleForSeven = 7.0F;

// LA VIE D'UN NŒUD DE CETTE FAMILLE.
//
// ⚠ ELLE NE FINIT PAS. Ces effets sont des marques de CARTE — le fantôme, la
// lumière verte, le mur magique —, et le client se renouvelle lui-même à son
// avant-dernier tic. Leur donner une vie ordinaire les ferait disparaître au
// bout de quelques secondes.
inline constexpr std::int32_t kQuadBurstEndlessLifeTicks = 99999999;

// UNE RAFALE qu'un effet pose.
struct EffectQuadBurstPattern {
  std::uint32_t effect_id = 0;
  std::string_view client_function;
  // L'OCTET DU SITE, de zéro à sept.
  std::uint8_t variant = 0;
};

[[nodiscard]] std::span<const EffectQuadBurstPattern> EffectQuadBurstPatternTable();

[[nodiscard]] std::span<const EffectQuadBurstPattern> EffectQuadBurstPatternsFor(
    std::uint32_t effect_id);

// L'IMAGE, LA MARQUE et L'ÉCHELLE qu'un octet choisit.
[[nodiscard]] std::string_view EffectQuadBurstTextureOf(std::uint8_t variant);
[[nodiscard]] std::uint8_t EffectQuadBurstMarkOf(std::uint8_t variant);
[[nodiscard]] float EffectQuadBurstScaleOf(std::uint8_t variant);

// LES QUATRE PISTES d'un nœud — une, deux ou quatre sont vives.
//
// ⚠ TROIS OCTETS N'EN ALLUMENT PAS QUATRE : le trois n'allume que la première,
// le sept et le huit les deux premières. Le client écrit quand même l'état des
// quatre.
[[nodiscard]] std::array<EffectTrack, kEffectTracksPerNode> SeedEffectQuadBurstTracks(
    const EffectQuadBurstPattern& pattern, std::uint32_t& generator);

// LE NŒUD qu'un patron décrit, prêt à rejoindre la troupe.
[[nodiscard]] PooledEffectNode SowEffectQuadBurstNode(const EffectQuadBurstPattern& pattern,
                                                      glm::vec3 anchor, std::uint32_t& generator);

}  // namespace ro::render
