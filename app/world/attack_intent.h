#pragma once

#include <cstdint>
#include <optional>

#include "ro/net/connection.h"
#include "ro/sim/actor.h"
#include "ro/sim/pending_action.h"
#include "ro/sim/pickup_intent.h"
#include "ro/sim/walk_map.h"
#include "ro/sim/walk_path.h"
#include "ro/sim/world_fwd.h"

#include "world/mouse_engagement.h"
#include "world/targeting.h"

namespace moonclient {

// FRAPPER UNE CIBLE QU'ON N'ATTEINT PAS ENCORE : y marcher, puis frapper.
//
// 🔴 LE SERVEUR NE RAPPROCHE PAS UN JOUEUR, et c'est le contraire de ce que
// nous avions écrit. `unit_attack_timer_sub` (src/map/unit.cpp) n'appelle
// `unit_walktobl` — la poursuite — que pour un MONSTRE ; pour un joueur trop
// loin, il envoie `ZC_ATTACK_FAILURE_FOR_DISTANCE` et s'arrête là, sans armer
// la moindre horloge. Un clic sur un monstre hors de portée ne faisait donc
// RIEN : le paquet partait, le serveur répondait « trop loin », et personne
// n'écoutait cette réponse.
//
// Le client natif ne demande le coup qu'une fois À PORTÉE : le clic INSCRIT
// l'action sur son propre acteur et c'est son tic qui, selon la distance, émet
// une marche ou le coup. C'est ce chemin-là qu'on suit — la géométrie est celle
// de `ro/sim/pending_action.h`, partagée avec la compétence ciblée.

// LA PORTÉE QU'ON PREND TANT QUE LE SERVEUR N'A PAS DIT LA SIENNE, en cases.
//
// Il la dit à la connexion (`clif_initialstatus`) puis à chaque changement
// d'arme, si bien que ce défaut ne sert qu'aux toutes premières images. Une
// case est la portée du corps à corps : c'est le pire des deux défauts
// possibles — trop court fait s'approcher un peu plus que nécessaire, trop
// long ferait frapper dans le vide.
inline constexpr std::uint16_t kDefaultAttackRangeCells = 1;

// La portée d'attaque du personnage, telle que le serveur l'a annoncée.
[[nodiscard]] std::uint16_t AttackRangeCells(const ro::sim::World& world);

// Ce qu'un clic sur un monstre INSCRIT. Rien ne part encore : le tic mesurera
// la distance à la prochaine image.
[[nodiscard]] ro::sim::PendingAction ArmAttack(const ro::sim::World& world,
                                               const ro::sim::Actor& target, bool repeat);

// UN PNJ OU UN OBJET HORS DE PORTÉE de ce que le serveur accepte : on s'en
// approche comme d'un monstre, puis on lui parle ou on le ramasse. Le PNJ
// s'ouvre à la portée du JEU (`SessionSettings::GameAreaCells`), plus courte
// que la vue de moonclient.
[[nodiscard]] ro::sim::PendingAction ArmNpcContact(const ro::sim::World& world,
                                                   const ro::sim::Actor& npc);
[[nodiscard]] ro::sim::PendingAction ArmFarPickup(const ro::sim::Actor& item);

// L'ÉCHOPPE D'UN MARCHAND TROP LOIN, vente ou achat : l'approche s'inscrit à la
// place de l'action et du ramassage d'avant, et la demande partira à l'arrivée
// — voir `ro/sim/store_approach.h`. Rend faux, sans rien toucher, quand
// l'échoppe se demande d'ici.
[[nodiscard]] bool ArmStoreApproach(const ro::sim::World& world, std::uint32_t owner_id,
                                    std::optional<ro::sim::PendingAction>& attack,
                                    std::optional<ro::sim::PickupIntent>& pickup);

// LA PORTÉE D'APPROCHE D'UNE UNITÉ AU SOL qu'on frappe, en cases : le client ne
// s'approche pas plus près, quelle que soit l'arme, et ne va plus loin que si
// la portée d'attaque le dépasse.
inline constexpr std::uint16_t kGroundUnitAttackReachCells = 5;

// Ce qu'un clic sur une unité au sol frappable INSCRIT : un coup ordinaire sur
// son identifiant, sans cadence de poursuite — elle ne bouge pas.
[[nodiscard]] ro::sim::PendingAction ArmAttackOnGroundUnit(const ro::sim::World& world,
                                                           const ro::sim::SkillUnit& unit,
                                                           bool repeat);

// L'ÉCHOPPE ATTEINTE AU BOUT D'UNE APPROCHE : la visite à demander, que la
// partie remet aux commerces — ils seuls savent si elle peut partir.
struct StoreVisitDue {
  std::uint32_t owner_id = 0;
  ro::sim::SignboardKind kind = ro::sim::SignboardKind::kVendingShop;
};

// ARRÊTE LE PERSONNAGE en chemin : une marche vers la case où il s'arrêterait,
// que le serveur prend comme un changement de destination. Rien s'il ne marche
// pas.
//
// ⚠ L'ARRÊT NE TOMBE PAS SOUS LE PLANCHER des marches : tombé, il laisserait
// courir jusqu'au bout l'étape qu'il défait. Il compte pourtant comme une marche
// partie, pour celles qui suivent.
void HaltPlayerWalk(ro::net::Connection& link, const ro::sim::World& world,
                    ro::sim::WalkPlanner& planner, ro::sim::ApproachWalkClock& clock,
                    std::uint32_t now_ms);

// Fait avancer la poursuite d'une image : marcher, frapper, ou renoncer.
//
// ⚠ `refused_target` EST LA CIBLE D'UN REFUS DE PORTÉE reçu à cette image —
// `ro::net::TargetTooFar` la tire de la trame. Elle RÉARME la poursuite, comme
// le message 60 du client, ce qui reprend la course derrière une cible qui
// s'est éloignée pendant qu'on la frappait. C'est pour elle que la fonction est
// appelée même sans poursuite en cours.
//
// Sans grille de franchissement — le temps qu'une carte se charge — la case
// d'approche ne se calcule pas, et marcher au hasard vaut moins qu'attendre.
//
// 🔴 REND VRAI QUAND LE REFUS A RÉARMÉ, et l'appelant doit alors ÉCARTER toute
// autre poursuite — celle d'une compétence ciblée. Le client natif n'a qu'UNE
// action en attente, que le message 60 réécrit : chez nous elles sont deux, et
// deux poursuites vivantes demanderaient deux marches par image, vers des
// cases différentes. La fenêtre est étroite — un aller-retour réseau — mais
// elle s'ouvre dès qu'on vise une compétence en fuyant un monstre.
//
// `targeting` peut jeter la demande de coup, ou la réduire à un coup : c'est la
// seconde coupe du « clic qui cible sans attaquer ». Nul hors partie.
//
// `engagement` apprend le coup qui part : une attaque qui ne se poursuit pas,
// relâchée en chemin, s'achève avec lui. Nul hors partie.
//
// `store_visit` reçoit l'échoppe atteinte (`ro::sim::PendingDeed::kVisitStore`).
// Une échoppe fermée, ou un marchand sorti de la vue, ARRÊTE le personnage et
// le dit dans le chat.
[[nodiscard]] bool AdvanceAttack(ro::net::Connection& link, ro::sim::World& world,
                                 ro::sim::WalkPlanner& planner, const ro::sim::WalkMap* walk,
                                 std::optional<ro::sim::PendingAction>& attack,
                                 ro::sim::ApproachWalkClock& approach,
                                 std::optional<std::uint32_t> refused_target, bool repeat_on_rearm,
                                 std::uint32_t now_ms, Targeting* targeting = nullptr,
                                 MouseEngagement* engagement = nullptr,
                                 std::optional<StoreVisitDue>* store_visit = nullptr);

}  // namespace moonclient
