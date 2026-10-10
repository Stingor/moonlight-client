#pragma once

#include <cstddef>
#include <cstdint>
#include <optional>
#include <string>
#include <vector>

#include <glm/glm.hpp>

#include "ro/render/camera.h"
#include "ro/render/skill_named_table.h"

namespace ro::render {

// CE QU'UNE ENTRÉE NOMMÉE DEVIENT UNE FOIS SES TIRAGES FAITS : un ou plusieurs
// semis, chacun avec son nom formé, sa place, son heure et son allure.
//
// ⚠ `render` NE TIRE PAS LUI-MÊME : le générateur vient de l'appelant, comme
// pour les nœuds d'effet, ce qui garde ces semis rejouables. Il ne connaît pas
// non plus le monde — l'angle à l'écran, l'allure de la source et le rang de la
// frappe lui sont donnés.

// L'ALLURE D'UN ACTEUR, telle que l'action que son état d'emploi vient de lui
// faire prendre la pose.
struct NamedEffectPaceOf {
  // Ce que dure une image de cette action, en pas de vingt-quatre
  // millisecondes, jamais moins d'un, grandie du facteur du coup.
  float cadence_steps = 0.0F;
  // L'image où le coup porte : celle de l'attaque — un tireur y ajoute les huit
  // pas du vol de sa flèche —, six pour l'emploi ordinaire, moins un pour tout
  // autre état.
  float hit_frame = 0.0F;
};

// CE QUE LE MOMENT SAIT AU SEMIS.
struct NamedSowingContext {
  // Le niveau que le paquet annonce.
  std::int32_t level = 0;
  // L'angle À L'ÉCRAN de la source vers la cible, de zéro inclus à trois cent
  // soixante exclu : zéro vers la droite, quatre-vingt-dix vers le bas. Absent
  // quand le moment n'a pas de cible, ou qu'aucune vue n'est posée.
  std::optional<float> screen_degrees;
  // La direction de SPRITE de la source, de zéro à sept, telle qu'elle se
  // dessine à cet instant.
  std::optional<std::size_t> source_sprite_direction;
  // Le rang de la frappe dans son coup, zéro pour la première.
  std::uint32_t hit_index = 0;
  // L'allure de la source, absente quand le moment n'en a pas.
  std::optional<NamedEffectPaceOf> source_pace;
  // L'élément des sphères de la source, zéro sans sphère ni source.
  std::int16_t source_charm_element = 0;
};

// UN SEMIS PRÊT.
struct NamedSowing {
  // Le nom, trou rempli.
  std::string name;
  // Où il naît autour du point que l'ancrage désigne, DANS LE REPÈRE DU
  // CLIENT : x, la hauteur — négative vers le haut —, z.
  glm::vec3 client_offset{0.0F};
  // Le délai de départ, ou `kFicheStartDelay` pour celui de la fiche.
  std::int32_t start_delay_ms = kRecordStartDelay;
  // L'orientation, absente pour un effet que la méthode ne tourne pas.
  std::optional<float> turn_degrees;
  bool flip_x = false;
  bool flip_y = false;
  float scale = 1.0F;
  bool laid_on_ground = false;
};

// LES TIRAGES QU'UN MÊME APPEL PARTAGE ENTRE SES ENTRÉES : le socle et
// l'éclat d'un même carré tombent sur la même place.
struct NamedSowingDeck {
  std::int32_t grid_side = 0;
  std::vector<std::int32_t> grid_x;
  std::vector<std::int32_t> grid_z;
};

// Les semis qu'une entrée donne dans ce contexte, dans l'ordre où la méthode
// les pose. Vide quand la méthode ne sème rien — une recette qui demande un
// angle sans en avoir, une dispersion au niveau zéro.
//
// ⚠ UNE ENTRÉE PAR IDENTIFIANT (`effect_id`) rend UN semis sans nom : c'est à
// l'appelant de la jouer par son numéro.
[[nodiscard]] std::vector<NamedSowing> ExpandNamedEntry(const SkillNamedEffectEntry& entry,
                                                        const NamedSowingContext& context,
                                                        NamedSowingDeck& deck,
                                                        std::uint32_t& generator);

// LE DÉLAI QUE L'ALLURE DONNE, en millisecondes, ou `kFicheStartDelay` quand
// l'entrée n'en tire aucun. ⚠ SANS ALLURE, la cadence des servants rend cinq
// cents ; les deux autres n'ont pas de sens sans source et rendent
// `kFicheStartDelay`.
[[nodiscard]] std::int32_t NamedPaceDelayMs(NamedEffectPace pace,
                                            const std::optional<NamedEffectPaceOf>& source_pace);

// L'ANGLE À L'ÉCRAN D'UN POINT VERS UN AUTRE, en degrés de zéro inclus à trois
// cent soixante exclu, l'ordonnée de l'écran DESCENDANT. Deux points confondus
// rendent zéro.
//
// ⚠ PAR L'ARC COSINUS, comme le client, et non par un arc tangente : le demi-tour
// se retourne quand le second point est plus HAUT que le premier.
[[nodiscard]] float NamedScreenDegrees(glm::vec2 from, glm::vec2 to);

// OÙ UN POINT DU MONDE TOMBE À L'ÉCRAN, à un facteur près : l'abscisse vers la
// droite, l'ordonnée vers le BAS. Suffit à mesurer un angle, que ni la taille
// de la fenêtre ni le champ ne changent.
[[nodiscard]] glm::vec2 NamedScreenPoint(const Camera& camera, glm::vec3 world);

}  // namespace ro::render
