#pragma once

#include <array>
#include <cstdint>
#include <optional>
#include <span>
#include <string_view>

#include <glm/glm.hpp>

#include "ro/render/effect_node_pool_fwd.h"
#include "ro/render/effect_track_fwd.h"

namespace ro::render {

// LE SEMIS DU BOUMERANG, sous-type 42.
//
// UN bâtisseur, TROIS sites, et un seul argument — l'image. Tout le reste est
// écrit en dur ou CALCULÉ sur la
// distance qui sépare le lanceur de sa victime.
//
// ⚠ CE N'EST PAS UN EFFET QUI SE POSE, C'EST UN EFFET QUI VA D'UN POINT À UN
// AUTRE, et c'est ce qui le distingue de toute la famille. Son ancre est le
// LANCEUR, son cap vise la CIBLE, et son mouvement le fait revenir. Deux
// positions, donc, là où presque tous les autres n'en demandent qu'une —
// `PooledEffectNode` les porte toutes deux depuis le trait tendu, et c'est ce
// qui rend celui-ci possible aujourd'hui.
//
// ⚠ ET LA MONTÉE SE CALCULE, elle ne se pose pas. Le client la fixe à « vitesse
// sur distance, fois l'écart d'altitude » : le bouclier arrive donc SUR sa
// victime et non à côté, quelle que soit la distance. Une valeur en dur ferait
// passer au-dessus des cibles lointaines et sous les proches.
//
// La figure est celle du carré teinté ; le mouvement est
// `AdvanceHomingTrailTracks`.
//
// ⚠ ET UN SECOND BÂTISSEUR PARTAGE LA FAMILLE : celui de l'effet 265 —
// EF_TANJI, Finger Offensive. Même nœud, même
// ancre, même vitesse, même clé, même montée ; il diffère par CINQ écritures,
// qui sont les cinq dernières colonnes du patron. La plus lourde de sens est le
// GENRE : à un, le projectile ÉCLATE au lieu de revenir, et le dessinateur lit
// le même octet comme une teinte bleutée ajoutée.

// LE MOT DE RENDU que les deux bâtisseurs écrivent en dur : le seau lit le
// mélange de chaque primitive, et l'épreuve de profondeur reste. Voir
// `effect_render_word.h`.
inline constexpr std::uint32_t kShieldBoomerangRenderWord = 5;

// CE QU'UN SITE POSE : son image, et ce que son bâtisseur écrit en dur là où
// les deux frères diffèrent.
//
// ⚠ LES CHAMPS AJOUTÉS VONT EN QUEUE, et la table les donne TOUS : une valeur
// par défaut prise pour une mesure est exactement l'erreur que ce patron évite.
struct EffectShieldBoomerangPattern {
  std::uint32_t effect_id = 0;
  std::string_view client_function;
  std::string_view texture;
  // Les tics de la tête à la naissance. NÉGATIFS, ils la font attendre :
  // le mouvement ne la déplace qu'une fois son compte remonté à un.
  std::int32_t start_ticks = 0;
  // L'alpha de la tête à la naissance.
  std::int16_t head_alpha = 0;
  // LE GENRE, que deux lecteurs lisent. Le mouvement y voit « éclate » (un) ou
  // « revient » (tout le reste) ; le dessinateur du carré teinté y voit sa
  // MARQUE de teinte.
  std::uint8_t kind = 0;
  // Le rayon, en deux termes. Le second multiplie le PARAMÈTRE DE L'INSTANCE —
  // le nombre de coups du coup qui lance l'effet,
  // un pour tout autre chemin.
  float core_radius = 0.0F;
  float radius_per_param = 0.0F;
};

[[nodiscard]] std::span<const EffectShieldBoomerangPattern> EffectShieldBoomerangPatternTable();

[[nodiscard]] std::span<const EffectShieldBoomerangPattern> EffectShieldBoomerangPatternsFor(
    std::uint32_t effect_id);

// LES QUATRE PISTES d'un nœud, toutes allumées.
//
// ⚠ LES TROIS DERNIÈRES NAISSENT À ALPHA NUL, et ce n'est pas un oubli : elles
// ne reçoivent leur contenu que par la CASCADE, cran après cran. Leur donner
// l'alpha de la tête montrerait quatre boucliers au premier tic.
//
// `anchor` est la position du lanceur, `target` celle de sa victime. Une cible
// ABSENTE laisse la distance nulle : le vol s'achèverait au premier tic, et
// c'est pourquoi le semis la REFUSE plutôt que de la deviner — voir
// `SowEffectShieldBoomerangNode`. `effect_param` est le paramètre de l'instance.
[[nodiscard]] std::array<EffectTrack, kEffectTracksPerNode> SeedEffectShieldBoomerangTracks(
    const EffectShieldBoomerangPattern& pattern, glm::vec3 anchor, glm::vec3 target,
    std::int32_t effect_param);

// LE NŒUD qu'un patron décrit, ou RIEN quand il n'y a pas de cible.
//
// ⚠ IL REND UN OPTIONNEL, et c'est la règle du crible portée jusqu'ici : un
// boumerang sans victime n'a ni distance à parcourir ni altitude à rattraper.
// Le semer quand même donnerait un carré immobile là où le client ne montre
// rien, et une division par zéro pour le calculer.
[[nodiscard]] std::optional<PooledEffectNode> SowEffectShieldBoomerangNode(
    const EffectShieldBoomerangPattern& pattern, glm::vec3 anchor,
    const std::optional<glm::vec3>& target, std::int32_t effect_param);

}  // namespace ro::render
