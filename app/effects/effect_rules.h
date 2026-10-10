#pragma once

#include <algorithm>
#include <cstdint>

#include "ro/render/effect_frame.h"
#include "ro/render/effect_screen_burst_seed.h"
#include "ro/render/effect_table.h"

// LES RÈGLES DU JEU D'EFFETS QUE LA CLASSE NE POSSÈDE PAS : l'attente entre deux
// semis, et le chemin par lequel une compétence demande son effet.
//
// Elles vivent à part d'`effect_view.h` pour deux raisons qui se renforcent. La
// première tient le plafond de huit cents lignes. La seconde : les sortir ici
// les rend atteignables SANS traîner `EffectsInPlay` et tout ce qu'elle inclut,
// jusqu'à GL par la carte.
namespace moonclient {

// L'attente entre deux semis d'un effet qui doit DURER.
//
// C'est la règle du client : il compte les tics et relance quand le compteur
// atteint `durée − retrait`. Le retrait est un RECOUVREMENT — il fait repartir
// l'effet quelques tics avant sa fin pour qu'aucune image ne montre le sol nu.
//
// Au moins un tic, quoi qu'annonce la table : une attente nulle resèmerait à
// chaque image, et cent troupes naîtraient en une seconde.
//
// Les deux familles d'effets s'en servent — celle des identifiants et celle des
// noms —, et c'est pourquoi elle est ici plutôt que dans l'une des deux.
[[nodiscard]] inline std::uint32_t PeriodMs(std::int32_t life_ticks, std::uint8_t withhold_ticks) {
  const std::int32_t ticks = std::max(1, life_ticks - static_cast<std::int32_t>(withhold_ticks));
  return static_cast<std::uint32_t>(ticks * ro::render::kEffectTickMs);
}

// L'EFFET D'INCANTATION SE RELANCE-T-IL tant que la barre court ?
//
// Le client pose UNE instance, dont la vie vaut la durée annoncée ; c'est son
// bâtisseur qui décide de ce que cette vie montre. Les anneaux donnent à leur
// nœud la vie de l'instance, d'autres sèment à chaque tic : ils tiennent toute
// la barre, et c'est la relance qui les imite ici.
//
// 🔴 L'ÉCLAT D'ÉCRAN NE TIENT PAS, lui : il sème ses nœuds une fois, à un âge
// fixe, avec une vie posée, et ne fait plus rien ensuite. L'effet 16 — celui
// d'une compétence qui n'en nomme aucun, Magnum Break compris — gicle donc
// UNE fois au début de l'incantation, même longue. Le relancer le faisait
// gicler en boucle jusqu'à la fin de la barre.
//
// 🔴 ET UN EFFET SANS NŒUD NE SE RELANCE PAS DAVANTAGE. Un `.str` seul joue
// son fichier le nombre de tours que sa fiche lui donne, puis l'instance
// s'éteint : rien ne le resème. La relance, elle, n'a de période que la vie
// des nœuds, et une vie NULLE la ferait repartir à chaque tic — un fichier
// nouveau toutes les vingt-quatre millisecondes tant que la barre court.
// `procedural_life_ticks` est la vie la plus longue des nœuds que l'effet
// sème, zéro s'il n'en sème aucun.
[[nodiscard]] inline bool CastAuraRelaunches(std::uint32_t effect_id,
                                             std::int32_t procedural_life_ticks) {
  return procedural_life_ticks > 0 && ro::render::EffectScreenBurstPatternsFor(effect_id).empty();
}

// DE QUEL CHEMIN UN EFFET DE COMPÉTENCE EST DEMANDÉ.
//
// 🔴 LE CLIENT EN A TROIS, ET CHACUN SA TABLE. Une compétence peut avoir un
// effet à chaque, et ils diffèrent : Magnum Break montre ses anneaux à
// l'annonce et l'anneau de choc au coup, Heal s'annonce d'une façon et frappe
// les morts-vivants d'une autre. Prendre l'un pour l'autre le joue au mauvais
// moment — et, au coup, une fois par victime.
// ⚠ LE TROISIÈME CHEMIN, la compétence posée sur une CASE, ne passe pas par
// ici : son effet se joue à l'endroit visé et non sur quelqu'un.
enum class SkillMoment : std::uint8_t {
  // Le serveur annonce la compétence, sans dégâts : un soin, un renfort.
  kAnnounced,
  // Un coup porte. Un message par frappe, donc un appel par victime.
  kImpact,
};

}  // namespace moonclient
