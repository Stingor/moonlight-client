#pragma once

#include <array>
#include <cstdint>
#include <span>
#include <string>
#include <string_view>
#include <vector>

#include <glm/glm.hpp>

#include "ro/render/effect_node_pool_fwd.h"
#include "ro/render/effect_track_fwd.h"
#include "ro/render/effect_typed_ribbon.h"

namespace ro::render {

// LE SEMIS DU RUBAN TYPÉ, et les dix effets qui l'emploient par le sous-type 149.
//
// Les bulles et le feu d'incantation. Dix identifiants, un seul bâtisseur, UN
// SEUL octet pour les
// séparer tous — et cet octet est le TYPE que la figure relit dans la piste.
//
// ⚠ LES QUATRE PISTES NE DIFFÈRENT QUE PAR DEUX ANGLES, et c'est toute la
// figure : leurs azimuts sont à quatre-vingt-dix degrés d'écart et leurs
// inclinaisons décroissent. Comme le rayon de base porte le sinus de l'azimut,
// les quatre rubans s'échelonnent au lieu de se superposer.
//
// ⚠ ET ILS NAISSENT SANS LONGUEUR NI ALPHA. Les vingt et une cases de chaque
// piste sont à zéro, et l'alpha aussi : c'est le mouvement qui y met l'onde et
// fait monter la lumière. Les semer garnis montrerait un anneau figé au premier
// tic, là où le client fait naître une vague.

// UN RUBAN QU'UN EFFET POSE : ce que son site d'appel passe au bâtisseur.
struct EffectTypedRibbonPattern {
  std::uint32_t effect_id = 0;
  // LE NOM DU BÂTISSEUR qui pose ce ruban chez le client.
  std::string_view client_function;
  // LE MODÈLE DE CHEMIN et le nombre d'images qu'il numérote. Le client les
  // charge par `sprintf`, de UN à ce nombre inclus : `W_bubble01` à
  // `W_bubble27`, `YBfire_01` à `YBfire_04`.
  std::string_view path_template;
  std::uint8_t image_count = 0;
  // L'octet unique : le type, posé tel quel dans les quatre pistes.
  std::uint8_t type = 0;
};

// Tous les rubans typés connus, par identifiant d'effet.
[[nodiscard]] std::span<const EffectTypedRibbonPattern> EffectTypedRibbonPatternTable();

// TOUS les rubans typés d'un effet, ou un span VIDE.
[[nodiscard]] std::span<const EffectTypedRibbonPattern> EffectTypedRibbonPatternsFor(
    std::uint32_t effect_id);

// LES CHEMINS QUE LE MODÈLE NUMÉROTE, dans l'ordre où le client les charge.
[[nodiscard]] std::vector<std::string> EffectTypedRibbonImagePaths(
    const EffectTypedRibbonPattern& pattern);

// LES QUATRE PISTES D'UN RUBAN TYPÉ, toutes vives et toutes nues.
//
// ⚠ AUCUN TIRAGE : le bâtisseur ne consulte aucun générateur.
[[nodiscard]] std::array<EffectTrack, kEffectTracksPerNode> SeedEffectTypedRibbonTracks(
    const EffectTypedRibbonPattern& pattern);

// LE NŒUD qu'un patron décrit, pistes semées, prêt à rejoindre la troupe.
//
// ⚠ LA VIE NE DESCEND PAS SOUS CENT VINGT TICS : le bâtisseur relève celle de
// l'instance quand elle est plus courte. Le mouvement s'en sert pour savoir quand
// commencer à éteindre, si bien qu'une vie plus brève éteindrait avant d'allumer.
[[nodiscard]] PooledEffectNode SowEffectTypedRibbonNode(const EffectTypedRibbonPattern& pattern,
                                                        glm::vec3 anchor,
                                                        std::int32_t effect_life_ticks);

// LA VIE LA PLUS COURTE qu'un ruban typé accepte.
inline constexpr std::int32_t kTypedRibbonShortestLifeTicks = 120;

}  // namespace ro::render
