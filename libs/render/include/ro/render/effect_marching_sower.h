#pragma once

#include <cstdint>

#include <glm/glm.hpp>

namespace ro::render {

// ── LE SEMEUR QUI MARCHE ────────────────────────────────────────────────────
//
// Un effet ordinaire sème autour d'un point fixe. Celui-ci fait AVANCER son
// point de semis : il part à quelques unités de la source, s'en écarte d'un pas
// constant à chaque tic droit vers la cible, pose une figure à chaque pas, et
// s'arrête quand il touche au but. C'est ainsi que le trait de Frost Diver gèle
// le sol de la source à sa victime, pointe après pointe.
//
// 🔴 IL PORTE SON PROPRE HORAIRE, ET IL PASSE DEVANT CELUI DU PATRON. Le client
// ne garde son bâtisseur derrière AUCUNE condition sur le compteur de tics : il
// le rappelle à chaque tic tant que le semeur n'est pas arrivé, et seule la
// MISE EN ROUTE est gardée par « au tic zéro ». La table d'horaires, elle, ne se
// déduit que des conditions, et « aucune condition » lui fait rendre « une
// seule fois ».
//
// ⚠ C'EST POURQUOI CETTE TABLE EST TENUE À LA MAIN. Elle dit ce qu'une absence
// de condition ne peut pas dire ; la déduire comme les autres demanderait de
// prouver, pour chaque bâtisseur, que sa fonction est bien rappelée à chaque
// tic ET que le site n'est gardé par rien — et une garde manquée s'y lirait
// « à chaque tic », ce qui est l'erreur la plus coûteuse des deux.
struct EffectMarchingSower {
  std::uint32_t effect_id = 0;
  // De combien le point du semis est DÉJÀ écarté de la source au premier tic.
  float first_distance = 0.0F;
  // De combien il s'en écarte DE PLUS à chaque tic suivant.
  //
  // ⚠ LE CLIENT L'ÉCRIT EN DEUX TEMPS — un pas de départ de cinq unités, puis
  // une « accélération » de deux retranchée à chaque tic — et la somme est
  // pourtant LINÉAIRE : cinq, sept, neuf, onze. Recopier les deux étages tels
  // quels donnerait une marche qui s'emballe.
  float distance_per_tick = 0.0F;
  // À quelle distance de la cible le semeur s'arrête. ⚠ L'ARRIVÉE SÈME ENCORE :
  // le client lève son drapeau AVANT de bâtir, et bâtit quand même ce tic-là.
  float arrival_distance = 0.0F;
  // Combien de tics au plus, quand il n'arrive jamais — la durée propre de
  // l'effet. Une cible plus proche que `first_distance − arrival_distance` met
  // le semeur hors d'atteinte dès son premier pas, et il marche alors jusqu'au
  // bout de sa vie : c'est le client qui fait cela, pas un garde-fou.
  std::int32_t last_tick = 0;
  // TOUS LES COMBIEN DE TICS il sème ; il marche, lui, à chaque tic. Un : à
  // chaque pas.
  std::int32_t sowing_period_ticks = 1;
  // 🔴 L'ARRIVÉE SE MESURE PARFOIS DEPUIS LA SOURCE, et non depuis le point
  // atteint : le bâtisseur mesure alors la distance de l'instance, qu'il ne
  // déplace pas, à la cible. Elle ne change jamais — le semeur s'arrête au
  // premier semis si la source touche déjà au but, et marche jusqu'au bout de
  // sa vie sinon, bien au-delà de la cible.
  bool arrival_measured_at_source = false;
};

// VRAI QUAND CE SEMEUR SÈME À CE TIC : sa cadence, et son arrivée.
[[nodiscard]] bool EffectMarchingSowerSowsAt(const EffectMarchingSower& sower, std::int32_t tick,
                                             float distance_to_target) noexcept;

// Ce que nous savons de cet effet, ou rien.
[[nodiscard]] const EffectMarchingSower* EffectMarchingSowerFor(std::uint32_t effect_id) noexcept;

// LA DISTANCE DANS LE PLAN DU SOL, comme le client la mesure :
// l'ordonnée n'entre pas dans l'arrivée. Deux étages d'un escalier comptent pour
// voisins, et c'est voulu — la marche, elle aussi, est horizontale.
[[nodiscard]] float EffectGroundDistance(glm::vec3 from, glm::vec3 to) noexcept;

// LE DERNIER TIC OÙ CE SEMEUR SÈME, son arrivée comprise.
[[nodiscard]] std::int32_t EffectMarchingSowerLastSowTick(const EffectMarchingSower& sower,
                                                          float distance_to_target) noexcept;

// LE POINT DU SEMIS À CE TIC : la source, écartée vers la cible de ce que la
// marche a couvert. Reste la source quand les deux points se confondent.
//
// ⚠ LE CAP PASSE PAR LES TABLES DU CLIENT : tronqué au degré, un degré de
// travers quand il est négatif — voir `EffectClientAimedHeading`.
//
// ⚠ HORIZONTAL : le pas du client naît d'une rotation en LACET seule, si bien
// que son ordonnée est nulle. Une pointe semée à mi-chemin garde donc la hauteur
// de la SOURCE, et non celle du terrain sous elle — un trait qui gravit une
// pente s'y enfonce, chez lui comme chez nous.
[[nodiscard]] glm::vec3 EffectMarchingSowerPoint(const EffectMarchingSower& sower, glm::vec3 source,
                                                 glm::vec3 target, std::int32_t tick) noexcept;

}  // namespace ro::render
