#pragma once

#include <array>
#include <cstdint>
#include <optional>

#include "ro/sim/movement.h"
#include "ro/sim/walk_map.h"
#include "ro/sim/walk_path.h"
#include "ro/sim/world_fwd.h"

namespace ro::sim {

// L'ACTION EN ATTENTE : ce que le client fait AVANT d'émettre.
//
// 🔴 UN CLIC SUR UNE ENTITÉ N'ENVOIE RIEN AU SERVEUR. Le client l'INSCRIT sur
// son propre acteur, et une horloge la relance : tant que la cible est HORS DE
// PORTÉE, elle repousse le personnage vers elle, et la demande ne part qu'une
// fois la portée atteinte.
//
// 🔴 L'ATTAQUE ORDINAIRE EN A BESOIN AUTANT QUE LA COMPÉTENCE. Le serveur ne
// rapproche PERSONNE d'une cible trop loin quand le demandeur est un JOUEUR :
// `unit_attack_timer_sub` (src/map/unit.cpp) n'appelle `unit_walktobl` que
// pour un monstre, et pour un joueur il envoie `clif_movetoattack` — le paquet
// `ZC_ATTACK_FAILURE_FOR_DISTANCE` — puis s'arrête sans armer aucune horloge.
// La poursuite est donc entièrement l'affaire du client, ici comme chez lui.
//
// Cette action n'est PAS un fait du monde, comme `PickupIntent` : le serveur
// n'en sait rien, et elle vit à part de lui.

// ⚠ LA COMPÉTENCE CIBLÉE N'A AUCUNE CADENCE. Le tic garde bien une horloge de
// relance — 1200 ms pour le genre 5, 450 pour les autres —, mais elle vit dans
// le cas de l'ATTAQUE. Le cas de la compétence ne compare rien : seul le
// changement de case visée relance la marche, et c'est ce qui suffit à la
// borner, puisque cette case ne bouge que lorsque la cible change de case.

// ── LA CADENCE DE LA MARCHE D'APPROCHE, en millisecondes ────────────────────
//
// Elle ne vaut QUE pour l'attaque, et son chiffre dépend d'un genre que le
// client fige à l'armement : 1200 ms pour le genre 5, 450 pour le genre 1.
//
// ⚠ ET LA CADENCE NE SUFFIT PAS À ELLE SEULE : le client exige AUSSI que la
// case d'approche ait changé. Les deux gardes sont conjointes, et n'en garder
// qu'une redemanderait la même marche à chaque échéance.
inline constexpr std::uint32_t kChaseRepeatQuickMs = 450;
inline constexpr std::uint32_t kChaseRepeatSlowMs = 1200;

// LE PLANCHER DE TOUTE MARCHE DU PERSONNAGE, en millisecondes : une marche
// demandée cent millisecondes ou moins après la précédente ne part pas.
//
// 🔴 C'EST LA MÊME PORTE POUR TOUTES : le clic au sol, sa reprise au bouton
// tenu, la marche qu'un geste avait retenue, la marche vers un objet, et celle
// d'approche d'une attaque ou d'une compétence. Le client n'a qu'un seul chemin
// vers le paquet de marche, et le plancher est posé dessus.
//
// ⚠ UNE APPROCHE QUI TOMBE N'EST PAS REDEMANDÉE POUR AUTANT : la case visée est
// déjà retenue, et seul un changement de case relancera la marche. La marche
// vers un objet, elle, se redemande au tic suivant. Un clic au sol qui tombe
// est PERDU : rien ne le rejoue, sinon le maintien du bouton.
//
// C'est ce que fait le client du joueur — cent là où l'exécutable d'origine en
// attend trois cent cinquante.
inline constexpr std::uint32_t kApproachWalkFloorMs = 100;

// L'HORLOGE DE CE PLANCHER, qui appartient au PERSONNAGE et non à l'action.
//
// 🔴 ELLE SURVIT D'UNE CIBLE À L'AUTRE, d'une attaque à une compétence, et d'un
// clic au sol à une poursuite : une seule pour toutes les marches, que leurs
// propriétaires se passent. Elle ne repart qu'avec le personnage lui-même,
// c'est-à-dire à l'entrée sur une carte, où la première marche part toujours.
struct ApproachWalkClock {
  // L'instant de la dernière marche PARTIE. Vide tant qu'aucune n'est partie
  // sur cette carte.
  std::optional<std::uint32_t> sent_ms;
  // L'entrée sur carte à laquelle `sent_ms` appartient — voir
  // `World::map_entries`.
  std::uint32_t map_entry = 0;
};

// La marche demandée maintenant peut-elle partir ? Si oui, l'horloge la NOTE :
// ne l'appeler qu'au moment d'émettre.
//
// ⚠ ET C'EST CE QUI REND UNE CONFUSION LISIBLE. Le serveur dévie CHAQUE marche
// d'un personnage confus vers une diagonale tirée au sort ; plus les demandes
// sont serrées, plus le trajet se brise souvent.
//
// `now_ms` vient de l'appelant, jamais d'une horloge.
[[nodiscard]] bool AdmitWalkRequest(const World& world, ApproachWalkClock& clock,
                                    std::uint32_t now_ms);

// LE GENRE SE DÉCIDE SUR LES DEUX VITESSES, et le client le calcule à
// l'armement : `vitesse_du_joueur × 4 / 3 >= vitesse_de_la_cible` donne le
// genre 1, donc la cadence rapide ; sinon le genre 5, donc la lente.
//
// ⚠ UNE VITESSE EST UN TEMPS PAR CASE : plus le nombre est GRAND, plus l'acteur
// est LENT. La comparaison ci-dessus est donc bien « la cible est au moins
// aussi rapide que moi » — et c'est ce cas-là qui relance le plus souvent,
// parce que c'est celui où la case d'approche fuit.
//
// Une vitesse nulle veut dire « pas encore dite » : on prend alors la cadence
// lente, celle qui parle le moins.
[[nodiscard]] std::uint32_t ChaseRepeatMs(std::uint16_t self_ms_per_cell,
                                          std::uint16_t target_ms_per_cell) noexcept;

// LA TOLÉRANCE DU TEST DE PORTÉE, en cases.
//
// Le client compare la distance à `5 × portée + 5,5`, en unités du monde. Une
// case en valant CINQ, la tolérance est de 1,1 case : elle couvre le fait qu'un
// acteur n'est jamais au centre exact de sa case.
inline constexpr float kRangeToleranceCells = 1.1F;

// Un pas d'une case, en deltas entiers.
struct CellStep {
  std::int32_t dx = 0;
  std::int32_t dy = 0;
};

// L'ORDRE DANS LEQUEL LE CLIENT ESSAIE LES VOISINS d'une case d'approche
// bloquée.
//
// ⚠ CE N'EST PAS UN TOUR DE CADRAN, et l'ordre n'a rien d'évident : est,
// nord-est, sud-est, ouest, nord-ouest, sud-ouest, nord, sud. Le « ranger »
// changerait la case où le personnage s'arrête devant un mur.
inline constexpr std::array<CellStep, 8> kApproachFallbacks = {
    CellStep{1, 0},  CellStep{1, 1},   CellStep{1, -1}, CellStep{-1, 0},
    CellStep{-1, 1}, CellStep{-1, -1}, CellStep{0, 1},  CellStep{0, -1}};

// Ce qu'une action en attente garde entre deux tics.
struct PendingAction {
  std::uint32_t target_id = 0;
  // ZÉRO VEUT DIRE « UN COUP ORDINAIRE », et c'est ce qui sépare les deux cas
  // du tic : la compétence n'a pas de cadence d'approche, l'attaque en a une.
  std::uint16_t skill_id = 0;
  std::uint16_t skill_level = 0;
  // La portée, en cases, telle que le SERVEUR l'annonce avec la compétence
  // apprise (`KnownSkill::range_cells`).
  //
  // ⚠ LE CLIENT NATIF LA REDEMANDE AU LUA à chaque tic — `GetSkillAttackRange`,
  // qui tient compte d'un objet équipé. Nous la COPIONS à l'armement : c'est le
  // serveur qui accepte ou refuse la demande, et c'est donc sa portée qui
  // décide. Elle ne change pas pendant une approche.
  std::uint16_t range_cells = 0;
  // La case vers laquelle la dernière marche d'approche a été demandée — le
  // client la garde sur son acteur.
  //
  // 🔴 ELLE SERT DEUX FOIS. Elle évite de redemander la même marche, et elle
  // est le second critère de tir : arrivé DESSUS, le personnage lance même si
  // le calcul de portée dit encore non. Sans elle, une cible qu'on ne peut pas
  // approcher de plus près ferait piétiner sans fin.
  std::optional<CellPosition> walking_to;
  // De combien de temps la marche d'approche se REDEMANDE — voir
  // `ChaseRepeatMs`. Zéro pour une compétence, qui n'a pas de cadence.
  std::uint32_t chase_repeat_ms = 0;
  // L'instant de la dernière marche demandée, dans l'horloge de l'APPELANT.
  // Il n'a de sens que lorsque `walking_to` porte une case.
  std::uint32_t approached_ms = 0;
  // SANS RELÂCHE, ou un seul coup.
  //
  // 🔴 RETENU À L'INSTANT DU CLIC, ET NON RELU AU MOMENT DU COUP. Le coup ne
  // part qu'une fois la cible atteinte, parfois plusieurs secondes après le
  // geste : relire CTRL à ce moment-là ferait dépendre la nature de l'attaque
  // de ce que le doigt fait au terme d'une course. C'est la même règle que le
  // maintien du clic au sol — le MOMENT DE L'APPUI tranche.
  //
  // ⚠ VRAI PAR DÉFAUT, parce que le réglage `/noctrl` du client d'origine est
  // ALLUMÉ par défaut (`GameSettings.lub`, OptionTbl[15]). Un défaut à faux
  // ferait qu'un clic ne porte qu'un coup, ce que personne n'attend.
  //
  // ⚠ SANS EFFET POUR UNE COMPÉTENCE : elle se lance une fois, et le champ ne
  // se lit que sur le chemin de l'attaque ordinaire.
  bool repeat = true;

  // LA CASE VISÉE PAR UNE COMPÉTENCE AU SOL, à la place d'une entité.
  //
  // 🔴 ELLE S'APPROCHE COMME UNE CIBLE. Le serveur ignore EN SILENCE une
  // demande au sol hors de portée — ni refus, ni réponse : Volcano ou
  // Hammerfall, à une ou deux cases de portée, ne partiraient presque jamais.
  // On marche donc jusqu'à la portée, puis la demande part. Une case ne
  // disparaît pas : seule la mort du lanceur fait renoncer.
  std::optional<CellPosition> ground_cell;
};

// Ce que l'action demande à cet instant.
enum class PendingStep : std::uint8_t {
  // Rien à faire : la case visée n'a pas changé depuis la dernière marche.
  kWait,
  // Marcher vers `toward`, puis reprendre l'attente.
  kApproach,
  // À portée : LÂCHER L'ACTION — lancer la compétence, ou porter le coup.
  // L'action est alors CONSOMMÉE — le client l'efface au même endroit.
  kRelease,
  // La cible a disparu ou est morte : abandonner.
  kGiveUp,
};

struct PendingMove {
  PendingStep step = PendingStep::kWait;
  // Où marcher. N'a de sens que pour `kApproach`.
  CellPosition toward;
};

// LA CASE OÙ S'APPROCHER : celle qui est à `range_cells` de la cible, sur la
// ligne droite qui va de la CIBLE vers SOI.
//
// ⚠ ON PART DE LA CIBLE, PAS DE SOI. Le client pose le point à `portée` cases
// de la cible, dans la direction du demandeur : le personnage s'arrête donc au
// bord de sa portée, et n'entre pas dans le monstre.
//
// Rend cette case si elle se franchit ; sinon le premier de ses huit voisins
// qui se franchit, dans l'ordre du client ; sinon LE DERNIER ESSAYÉ — c'est ce
// que le client laisse dans ses deux sorties quand aucun ne convient, et le
// corriger ferait viser une autre case que lui devant un mur.
[[nodiscard]] CellPosition ApproachCell(FractionalCell self, FractionalCell target,
                                        std::uint16_t range_cells, const WalkMap& walk);

// Vrai si la cible est à portée : `portée + 1,1 >= distance`, en cases.
//
// ⚠ LA DISTANCE DU CLIENT EST À TROIS DIMENSIONS — elle compte le RELIEF. Le
// monde ne porte pas la hauteur du terrain, si bien que la nôtre est à plat :
// sur une pente, elle dit « à portée » un peu plus tôt que lui.
//
// ⚠ ET LE CLIENT RETRANCHE LE RAYON DE LA CIBLE : un gros monstre se frappe de
// plus loin, de quatre unités par cran de taille. Nous ne connaissons pas ce
// rayon ; à rayon nul, le cas de loin le plus courant, les deux calculs
// coïncident.
[[nodiscard]] bool WithinSkillRange(FractionalCell self, FractionalCell target,
                                    std::uint16_t range_cells) noexcept;

// Ce que l'action en attente demande, et ce qu'elle inscrit au passage.
//
// ⚠ ELLE MUTE `action`, comme le client mute son acteur : la case demandée est
// ce qui empêche la marche de repartir à chaque image. La lire au-dehors
// obligerait l'appelant à refaire le même test. Elle mute aussi `clock`, celle
// du personnage : voir `ApproachWalkClock`.
//
// `now_ms` vient de l'appelant, jamais d'une horloge.
[[nodiscard]] PendingMove AdvancePendingAction(const World& world, WalkPlanner& planner,
                                               const WalkMap& walk, PendingAction& action,
                                               ApproachWalkClock& clock, std::uint32_t now_ms);

}  // namespace ro::sim
