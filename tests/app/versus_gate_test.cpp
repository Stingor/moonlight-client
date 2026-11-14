// CE QUI OUVRE LE CIBLAGE D'UN AUTRE JOUEUR.
//
// 🔴 LA RÈGLE DE LA FICHE 0055 SE TIENT ICI, MÉCANIQUEMENT : le client DÉSIGNE,
// le serveur DISPOSE. On ouvre sur la CARTE et jamais sur l'individu, parce que
// le serveur ne dit pas qui est vraiment frappable — `pvp_noguild` n'a aucun
// bit, et six états individuels ne voyagent dans aucun paquet.
//
// ⚠ CE QUI N'EST PAS COUVERT ICI, ET POURQUOI. `CursorShapeOver` et
// `ActorTakesClick` — la forme du curseur, et le fait qu'un acteur prenne le
// clic — vivent dans l'EXÉCUTABLE et non dans une bibliothèque : aucun essai ne
// peut les lier. Leur invariant tient par construction, `ActorTakesClick` étant
// DÉFINIE comme « le curseur change de forme » plutôt qu'écrite en parallèle ;
// mais il n'est pas vérifié. Les sortir demande de découper `overlay_view`, ce
// qui touchera deux chantiers en cours — à faire quand l'arbre sera calme.

#include "world/versus_gate.h"

#include <cstdint>

#include <catch2/catch_test_macros.hpp>

#include "ro/sim/map_mode.h"
#include "ro/sim/pending_action.h"
#include "ro/sim/world.h"

namespace {

using moonclient::VersusGate;
using moonclient::VersusGateOf;

// La carte PVP telle que CE serveur l'annonce : le mapflag `pvp` lève PARTY,
// DISABLE_LOCKON et COUNT_PK, et rien d'autre. Relevé dans `clif_map_property`.
[[nodiscard]] ro::sim::MapMode PvpMap() {
  ro::sim::MapMode mode;
  mode.kind = ro::sim::MapKind::kFreePvpZone;
  mode.attackable_beyond_party = true;
  mode.attackable_without_shift = true;
  mode.pvp_counter_shown = true;
  return mode;
}

// La carte de siège : ni PARTY ni COUNT_PK, mais GUILD et l'emblème.
[[nodiscard]] ro::sim::MapMode SiegeMap() {
  ro::sim::MapMode mode;
  mode.kind = ro::sim::MapKind::kAgitZone;
  mode.attackable_beyond_guild = true;
  mode.guild_emblems_shown = true;
  return mode;
}

}  // namespace

TEST_CASE("Porte : une carte ordinaire la laisse fermée", "[app][pvp]") {
  const ro::sim::World world;

  const VersusGate gate = VersusGateOf(world, /*shift_held=*/false, /*no_shift=*/false);
  CHECK_FALSE(gate.map_opens);
  CHECK_FALSE(gate.forced);
  CHECK_FALSE(gate.opens());
}

TEST_CASE("Porte : la carte l'ouvre sans aucune touche", "[app][pvp]") {
  ro::sim::World world;
  world.TellMapMode(PvpMap());

  const VersusGate gate = VersusGateOf(world, false, false);
  CHECK(gate.map_opens);
  CHECK(gate.opens());
}

TEST_CASE("Porte : la zone de siège l'ouvre aussi, par un AUTRE bit", "[app][pvp]") {
  // ⚠ NI PARTY NI COUNT_PK EN ZONE GvG : n'écouter que le premier laisserait le
  // curseur en flèche sur une carte de siège, où l'on se frappe pourtant.
  ro::sim::World world;
  world.TellMapMode(SiegeMap());

  CHECK(VersusGateOf(world, false, false).opens());
}

TEST_CASE("Porte : le geste la force hors de toute zone versus", "[app][pvp]") {
  // C'est le MAJ + clic du client d'origine : il rend hostile là où la carte ne
  // le fait pas. Le serveur refusera, et son refus n'est pas notre affaire.
  const ro::sim::World world;

  CHECK(VersusGateOf(world, /*shift_held=*/true, /*no_shift=*/false).forced);
  CHECK(VersusGateOf(world, true, false).opens());
  CHECK_FALSE(VersusGateOf(world, true, false).map_opens);
}

TEST_CASE("Porte : le réglage REMPLACE la touche, il ne l'inverse pas", "[app][pvp]") {
  // 🔴 COMME CHEZ LE CLIENT D'ORIGINE : le drapeau entre en OU PUR avec l'état de la
  // touche. Tenir MAJ alors que `/noshift` est allumé ne referme donc rien —
  // c'est ce qu'un OU EXCLUSIF ferait, et ce serait le geste rendu impossible
  // exactement pour qui a allumé le réglage.
  const ro::sim::World world;

  CHECK_FALSE(VersusGateOf(world, false, false).forced);
  CHECK(VersusGateOf(world, true, false).forced);
  CHECK(VersusGateOf(world, false, true).forced);
  CHECK(VersusGateOf(world, true, true).forced);
}

TEST_CASE("Porte : elle se referme en quittant la zone", "[app][pvp]") {
  // Le serveur redit « rien » en sortant, et `EnterMap` remet le mode à neuf
  // pour le cas où c'est nous qui partons — il ne dit rien alors.
  ro::sim::World world;
  world.TellMapMode(PvpMap());
  REQUIRE(VersusGateOf(world, false, false).opens());

  world.EnterMap("prontera", 150, 150);
  CHECK_FALSE(VersusGateOf(world, false, false).opens());
}

TEST_CASE("Cadence : Ctrl ou /noctrl, et le réglage remplace la touche", "[app][pvp]") {
  // 🔴 MÊME FORME QUE LA PORTE, MAIS DÉFAUT INVERSE : `/noctrl` est ALLUMÉ
  // d'usine chez le client d'origine, là où `/noshift` est éteint. Sans relâche
  // est donc l'ordinaire, et ce que la commande ÉTEINTE rend possible est le
  // coup unique.
  CHECK_FALSE(moonclient::AttackRepeats(/*ctrl_held=*/false, /*no_ctrl=*/false));
  CHECK(moonclient::AttackRepeats(/*ctrl_held=*/true, /*no_ctrl=*/false));
  CHECK(moonclient::AttackRepeats(/*ctrl_held=*/false, /*no_ctrl=*/true));
  // ⚠ ET PAS D'INVERSION ICI NON PLUS : tenir Ctrl alors que `/noctrl` est
  // allumé ne fait pas retomber sur le coup unique.
  CHECK(moonclient::AttackRepeats(/*ctrl_held=*/true, /*no_ctrl=*/true));
}

TEST_CASE("Cadence : une action en attente poursuit par défaut", "[app][pvp]") {
  // ⚠ LE DÉFAUT EST CELUI DU RÉGLAGE ALLUMÉ, et c'est ce qui fait qu'ajouter
  // cette cadence n'a rien changé au jeu : une action bâtie sans rien dire
  // frappe sans relâche, comme avant.
  const ro::sim::PendingAction fresh;
  CHECK(fresh.repeat);
}

TEST_CASE("Porte : les deux moitiés sont indépendantes", "[app][pvp]") {
  // ⚠ DEUX CHAMPS ET NON UN BOOLÉEN : `map_opens` sert à autre chose que
  // `forced` — le compteur de rang et les marqueurs au sol ne regardent que la
  // CARTE, et se moquent de ce que le joueur tient enfoncé.
  ro::sim::World world;
  world.TellMapMode(PvpMap());

  const VersusGate forced_on_pvp = VersusGateOf(world, true, false);
  CHECK(forced_on_pvp.map_opens);
  CHECK(forced_on_pvp.forced);

  const VersusGate quiet_on_pvp = VersusGateOf(world, false, false);
  CHECK(quiet_on_pvp.map_opens);
  CHECK_FALSE(quiet_on_pvp.forced);
}

namespace {

constexpr std::uint32_t kSelf = 150001;
constexpr std::uint32_t kMate = 150002;
constexpr std::uint32_t kFoe = 150003;
constexpr std::uint32_t kStranger = 150004;
constexpr std::uint16_t kOurCamp = 7;
constexpr std::uint16_t kTheirCamp = 8;

// Un champ de bataille où nous avons un coéquipier et un ennemi ; un troisième
// joueur n'a pas de camp dit.
[[nodiscard]] ro::sim::World Battlefield() {
  ro::sim::World world;
  world.set_player_id(kSelf);
  ro::sim::MapMode mode;
  mode.battlefield = true;
  world.TellMapMode(mode);
  world.battlefield().TellCamp(kSelf, kOurCamp, kSelf);
  world.battlefield().TellCamp(kMate, kOurCamp, kSelf);
  world.battlefield().TellCamp(kFoe, kTheirCamp, kSelf);
  return world;
}

}  // namespace

TEST_CASE("Porte : sur un champ de bataille, la carte n'ouvre que sur l'autre camp",
          "[app][pvp][battleground]") {
  const ro::sim::World world = Battlefield();
  const VersusGate gate = VersusGateOf(world, false, false);
  CHECK(gate.map_opens);
  CHECK(gate.opens_on(kFoe));
  CHECK_FALSE(gate.opens_on(kMate));
  // Un camp que le serveur n'a pas dit ouvre, comme le reste de la carte.
  CHECK(gate.opens_on(kStranger));
}

TEST_CASE("Porte : le geste ouvre aussi sur un coéquipier", "[app][pvp][battleground]") {
  // Le serveur refusera ; viser reste le choix du joueur.
  const ro::sim::World world = Battlefield();
  CHECK(VersusGateOf(world, true, false).opens_on(kMate));
  CHECK(VersusGateOf(world, false, true).opens_on(kMate));
}

TEST_CASE("Porte : hors du champ de bataille, les camps ne comptent pas",
          "[app][pvp][battleground]") {
  // Un camp resté connu ne doit pas fermer une carte PVP ordinaire.
  ro::sim::World world;
  world.set_player_id(kSelf);
  world.TellMapMode(PvpMap());
  world.battlefield().TellCamp(kSelf, kOurCamp, kSelf);
  world.battlefield().TellCamp(kMate, kOurCamp, kSelf);
  const VersusGate gate = VersusGateOf(world, false, false);
  CHECK(gate.battlefield == nullptr);
  CHECK(gate.opens_on(kMate));
  // Et une carte ordinaire reste fermée pour tous.
  CHECK_FALSE(VersusGateOf(ro::sim::World(), false, false).opens_on(kFoe));
}
