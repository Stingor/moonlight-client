// LE CLASSEMENT PVP, ET LES TROIS PIÈGES QU'IL TEND.
//
// 🔴 LE PAQUET ARRIVE EN ZONE. `clif_pvpset` diffuse en AREA : on reçoit le rang
// des voisins autant que le sien. Prendre le dernier reçu pour le nôtre ferait
// sauter le compteur chaque fois qu'un autre tue quelqu'un — et ce serait
// invisible en solo, où l'on ne reçoit que le sien.
//
// 🔴 LE RANG D'UN JOUEUR CACHÉ VAUT `UINT32_MAX`, et le serveur annonce dans son
// propre commentaire « On client displays as -- ». C'est faux pour ce client :
// lu en SIGNÉ, c'est moins un, et sa garde de dessin — signée aussi — ne montre
// alors rien du tout. Sa fonte de chiffres ne porte d'ailleurs aucun tiret.
//
// 🔴 LE CHRONOMÈTRE PART QUAND LE RANG DEVIENT UN. Le serveur reparle dès que le
// rang OU LE TOTAL bouge : sur une carte fréquentée, un joueur qui entre fait
// repartir le paquet avec le même rang. Le remettre à zéro à chaque annonce
// ferait un compteur qui ne dépasse jamais une seconde.

#include "ro/net/pvp_rank.h"

#include <algorithm>
#include <cstdint>
#include <vector>

#include <catch2/catch_test_macros.hpp>

#include "ro/net/world_update.h"
#include "ro/sim/actor.h"
#include "ro/sim/pvp_rank.h"
#include "ro/sim/world.h"

#include "support/net_frames.h"

using ro::net::ApplyFrame;
using ro::net::FrameOutcome;
using ro::sim::World;
using ro::test_support::FrameOf;

namespace {

constexpr std::uint32_t kMyId = 2'000'001;
constexpr std::uint32_t kOtherId = 2'000'002;

// La trame telle que le serveur l'écrit : quatorze octets, tout en petit-boutiste.
[[nodiscard]] std::vector<std::uint8_t> Ranking(std::uint32_t id, std::uint32_t rank,
                                                std::uint32_t total) {
  std::vector<std::uint8_t> bytes(ro::net::kPvpRankWireSize, 0);
  const auto put16 = [&bytes](std::size_t at, std::uint16_t value) {
    bytes[at] = static_cast<std::uint8_t>(value & 0xFFU);
    bytes[at + 1] = static_cast<std::uint8_t>(value >> 8U & 0xFFU);
  };
  const auto put32 = [&bytes](std::size_t at, std::uint32_t value) {
    for (std::size_t step = 0; step < 4; ++step) {
      bytes[at + step] = static_cast<std::uint8_t>(value >> (step * 8U) & 0xFFU);
    }
  };
  put16(0, 0x019a);
  put32(ro::net::kPvpRankIdOffset, id);
  put32(ro::net::kPvpRankRankOffset, rank);
  put32(ro::net::kPvpRankTotalOffset, total);
  return bytes;
}

// Un monde dont le joueur est nommé : sans cela, aucun paquet n'est « le nôtre ».
[[nodiscard]] World MineNamed() {
  World world;
  world.set_player_id(kMyId);
  return world;
}

}  // namespace

TEST_CASE("Classement PVP : le rang et le total arrivent", "[net][pvp]") {
  World world = MineNamed();
  REQUIRE(ApplyFrame(FrameOf(Ranking(kMyId, 3, 7)), world, 1'000) == FrameOutcome::kApplied);

  CHECK(world.pvp_rank().rank == 3);
  CHECK(world.pvp_rank().total == 7);
  CHECK(ro::sim::PvpRankShown(world.pvp_rank()));
  // Troisième, donc pas de chronomètre.
  CHECK(world.pvp_rank().leader_since_ms == 0);
}

TEST_CASE("Classement PVP : le rang d'un VOISIN ne touche pas le compteur", "[net][pvp]") {
  World world = MineNamed();
  REQUIRE(ApplyFrame(FrameOf(Ranking(kMyId, 4, 9)), world, 1'000) == FrameOutcome::kApplied);

  // ⚠ IGNORÉ, ET NON APPLIQUÉ : le paquet est valable, il ne nous concerne pas.
  // Le dire « non traité » le ferait compter comme du travail restant.
  REQUIRE(ApplyFrame(FrameOf(Ranking(kOtherId, 1, 9)), world, 2'000) == FrameOutcome::kIgnored);

  CHECK(world.pvp_rank().rank == 4);
  CHECK(world.pvp_rank().total == 9);
  CHECK(world.pvp_rank().leader_since_ms == 0);
}

TEST_CASE("Classement PVP : un rang caché ne montre rien, et surtout pas quatre milliards",
          "[net][pvp]") {
  World world = MineNamed();
  REQUIRE(ApplyFrame(FrameOf(Ranking(kMyId, 0xFFFFFFFFU, 5)), world, 1'000) ==
          FrameOutcome::kApplied);

  CHECK(world.pvp_rank().rank == -1);
  CHECK_FALSE(ro::sim::PvpRankShown(world.pvp_rank()));
}

TEST_CASE("Classement PVP : le chronomètre part quand le rang DEVIENT un", "[net][pvp]") {
  World world = MineNamed();
  REQUIRE(ApplyFrame(FrameOf(Ranking(kMyId, 2, 4)), world, 1'000) == FrameOutcome::kApplied);
  CHECK(world.pvp_rank().leader_since_ms == 0);

  REQUIRE(ApplyFrame(FrameOf(Ranking(kMyId, 1, 4)), world, 5'000) == FrameOutcome::kApplied);
  CHECK(world.pvp_rank().leader_since_ms == 5'000);
}

TEST_CASE("Classement PVP : le chronomètre NE REPART PAS tant qu'on tient la tête", "[net][pvp]") {
  World world = MineNamed();
  REQUIRE(ApplyFrame(FrameOf(Ranking(kMyId, 1, 2)), world, 5'000) == FrameOutcome::kApplied);
  REQUIRE(world.pvp_rank().leader_since_ms == 5'000);

  // Un joueur entre : le TOTAL change, le rang non. Le serveur reparle.
  REQUIRE(ApplyFrame(FrameOf(Ranking(kMyId, 1, 3)), world, 9'000) == FrameOutcome::kApplied);

  CHECK(world.pvp_rank().total == 3);
  // ⚠ L'INSTANT DE DÉPART TIENT : c'est tout l'objet de cette épreuve.
  CHECK(world.pvp_rank().leader_since_ms == 5'000);
}

TEST_CASE("Classement PVP : perdre la tête éteint le chronomètre", "[net][pvp]") {
  World world = MineNamed();
  REQUIRE(ApplyFrame(FrameOf(Ranking(kMyId, 1, 2)), world, 5'000) == FrameOutcome::kApplied);
  REQUIRE(world.pvp_rank().leader_since_ms == 5'000);

  REQUIRE(ApplyFrame(FrameOf(Ranking(kMyId, 2, 2)), world, 8'000) == FrameOutcome::kApplied);
  CHECK(world.pvp_rank().leader_since_ms == 0);
}

TEST_CASE("Classement PVP : le serveur éteint la zone par un rang et un total nuls", "[net][pvp]") {
  World world = MineNamed();
  REQUIRE(ApplyFrame(FrameOf(Ranking(kMyId, 1, 2)), world, 5'000) == FrameOutcome::kApplied);
  REQUIRE(ro::sim::PvpRankShown(world.pvp_rank()));

  // `map_mapflag_pvp_stop_sub` envoie exactement cela en SELF.
  REQUIRE(ApplyFrame(FrameOf(Ranking(kMyId, 0, 0)), world, 6'000) == FrameOutcome::kApplied);

  CHECK_FALSE(ro::sim::PvpRankShown(world.pvp_rank()));
  CHECK(world.pvp_rank().leader_since_ms == 0);
}

TEST_CASE("Classement PVP : changer de carte l'oublie", "[net][pvp]") {
  // 🔴 LE SERVEUR NE LE DIT PAS quand c'est NOUS qui partons :
  // `unit_remove_map_` remet son rang à zéro sans rien annoncer.
  World world = MineNamed();
  REQUIRE(ApplyFrame(FrameOf(Ranking(kMyId, 1, 6)), world, 5'000) == FrameOutcome::kApplied);
  REQUIRE(ro::sim::PvpRankShown(world.pvp_rank()));

  world.EnterMap("prontera", 150, 150);

  CHECK_FALSE(ro::sim::PvpRankShown(world.pvp_rank()));
  CHECK(world.pvp_rank().rank == 0);
  CHECK(world.pvp_rank().leader_since_ms == 0);
}

namespace {

// Un joueur en vue, pour que l'anneau ait quelqu'un à marquer.
void SpawnPlayer(World& world, std::uint32_t id) {
  ro::sim::Actor actor;
  actor.id = id;
  actor.kind = ro::sim::ActorKind::kPlayer;
  world.Spawn(std::move(actor));
}

// Vrai si cet acteur porte l'anneau des dix premiers.
[[nodiscard]] bool Marked(const World& world, std::uint32_t id) {
  const ro::sim::Actor* actor = world.Find(id);
  if (actor == nullptr) {
    return false;
  }
  return std::ranges::any_of(actor->effects, [](const ro::sim::PlayedEffect& played) {
    return played.id == ro::sim::kPvpMarkEffectId;
  });
}

}  // namespace

TEST_CASE("Anneau : les dix premiers le portent, et personne d'autre", "[net][pvp]") {
  // 🔴 C'EST LE CLASSEMENT QUI LE POSE, ET NON L'HOSTILITÉ. Le client d'origine
  // ne marque au sol que le top dix ; il n'existe aucun anneau « ce joueur est
  // frappable ». Voir la fiche 0055.
  World world = MineNamed();
  SpawnPlayer(world, kOtherId);

  REQUIRE(ApplyFrame(FrameOf(Ranking(kOtherId, 11, 20)), world, 1'000) == FrameOutcome::kApplied);
  CHECK_FALSE(Marked(world, kOtherId));

  REQUIRE(ApplyFrame(FrameOf(Ranking(kOtherId, 10, 20)), world, 2'000) == FrameOutcome::kApplied);
  CHECK(Marked(world, kOtherId));
}

TEST_CASE("Anneau : il se RETIRE quand on sort du top dix", "[net][pvp]") {
  // ⚠ NE POSER QUE LA POSE LAISSERAIT UNE CARTE ENTIÈRE MARQUÉE au bout d'une
  // heure : chacun y passe, et rien ne le reprendrait.
  World world = MineNamed();
  SpawnPlayer(world, kOtherId);
  REQUIRE(ApplyFrame(FrameOf(Ranking(kOtherId, 2, 20)), world, 1'000) == FrameOutcome::kApplied);
  REQUIRE(Marked(world, kOtherId));

  REQUIRE(ApplyFrame(FrameOf(Ranking(kOtherId, 12, 20)), world, 2'000) == FrameOutcome::kApplied);
  CHECK_FALSE(Marked(world, kOtherId));
}

TEST_CASE("Anneau : un rang qui bouge REPOSE, un total qui bouge non", "[net][pvp]") {
  // 🔴 CE QUI SE VÉRIFIE ICI EST L'INSTANT, ET NON LE COMPTE. `RecordEffect` ne
  // double PAS un effet déjà posé — il en remet l'instant de départ. Compter les
  // exemplaires ne mordrait donc sur rien : une telle épreuve passe au vert la
  // garde ôtée.
  //
  // 🔴 ET LA POSE SE REJOUE QUAND LE RANG BOUGE, parce que c'est lui qui décide
  // de la COULEUR de l'anneau. Une première version de cette épreuve demandait
  // le contraire — que rien ne bouge tant qu'on reste dans les dix —, sur la
  // supposition qu'un anneau reposé clignoterait. La supposition était fausse :
  // le récepteur du client RETIRE puis REPOSE l'effet à chaque annonce, et il le
  // faut bien, sans quoi le premier garderait la teinte du dixième.
  World world = MineNamed();
  SpawnPlayer(world, kOtherId);

  REQUIRE(ApplyFrame(FrameOf(Ranking(kOtherId, 9, 20)), world, 1'000) == FrameOutcome::kApplied);
  const auto ring = [&world]() {
    const ro::sim::Actor* actor = world.Find(kOtherId);
    REQUIRE(actor != nullptr);
    const auto mark =
        std::ranges::find(actor->effects, ro::sim::kPvpMarkEffectId, &ro::sim::PlayedEffect::id);
    REQUIRE(mark != actor->effects.end());
    return *mark;
  };
  CHECK(ring().since_ms == 1'000);
  // ⚠ ET LE RANG VOYAGE AVEC LUI : c'est par là que la teinte le reçoit.
  CHECK(ring().value == 9);

  // Le rang monte : l'anneau se repose, avec son nouveau rang.
  REQUIRE(ApplyFrame(FrameOf(Ranking(kOtherId, 2, 20)), world, 5'000) == FrameOutcome::kApplied);
  CHECK(ring().since_ms == 5'000);
  CHECK(ring().value == 2);

  // ⚠ SEUL LE TOTAL BOUGE, et là rien ne se repose : le serveur reparle à chaque
  // mort de la carte, et la couleur, elle, n'a pas changé.
  REQUIRE(ApplyFrame(FrameOf(Ranking(kOtherId, 2, 21)), world, 9'000) == FrameOutcome::kApplied);
  CHECK(ring().since_ms == 5'000);

  const ro::sim::Actor* after = world.Find(kOtherId);
  REQUIRE(after != nullptr);
  CHECK(after->pvp_rank == 2);
}

TEST_CASE("Anneau : un rang reçu HORS DE VUE attend son porteur", "[net][pvp]") {
  // 🔴 LE SERVEUR NE REDIT UN RANG QUE LORSQU'IL CHANGE, jamais à l'entrée en
  // vue : `pc_calc_pvprank` compare le rang ET le nombre de joueurs, et aucun
  // appel de `clif_pvpset` ne vit ailleurs. Un rang jeté parce que son porteur
  // n'était pas encore là ne revenait donc JAMAIS — l'anneau n'apparaissait qu'à
  // une connexion fraîche. Vu en jeu.
  World world = MineNamed();
  REQUIRE(ApplyFrame(FrameOf(Ranking(kOtherId, 3, 9)), world, 1'000) == FrameOutcome::kIgnored);

  SpawnPlayer(world, kOtherId);
  world.RefreshPvpRings(2'000);
  CHECK(Marked(world, kOtherId));
}

TEST_CASE("Anneau : il REVIENT après une mort ou une sortie de vue", "[net][pvp]") {
  // 🔴 LES TROIS SYMPTÔMES N'EN FONT QU'UN. Mourir, sortir de la vue, revenir :
  // dans les trois cas le serveur REMPLACE l'acteur, et le nouveau ne reçoit
  // aucun paquet de classement. Sans le carnet, il ne porterait plus rien.
  World world = MineNamed();
  SpawnPlayer(world, kOtherId);
  REQUIRE(ApplyFrame(FrameOf(Ranking(kOtherId, 2, 9)), world, 1'000) == FrameOutcome::kApplied);
  REQUIRE(Marked(world, kOtherId));

  REQUIRE(world.Remove(kOtherId));
  SpawnPlayer(world, kOtherId);
  const ro::sim::Actor* revenu = world.Find(kOtherId);
  REQUIRE(revenu != nullptr);
  CHECK(revenu->pvp_rank == 2);

  world.RefreshPvpRings(5'000);
  CHECK(Marked(world, kOtherId));
}

TEST_CASE("Anneau : la passe est MUETTE tant que le rang ne bouge pas", "[net][pvp]") {
  // ⚠ ELLE TOURNE À CHAQUE BATTEMENT : sans la comparaison du rang PORTÉ à celui
  // de l'acteur, l'anneau rejouerait son animation soixante fois par seconde.
  World world = MineNamed();
  SpawnPlayer(world, kOtherId);
  REQUIRE(ApplyFrame(FrameOf(Ranking(kOtherId, 4, 9)), world, 1'000) == FrameOutcome::kApplied);
  const auto pose_at = [&world]() {
    const ro::sim::Actor* actor = world.Find(kOtherId);
    REQUIRE(actor != nullptr);
    const auto mark =
        std::ranges::find(actor->effects, ro::sim::kPvpMarkEffectId, &ro::sim::PlayedEffect::id);
    REQUIRE(mark != actor->effects.end());
    return mark->since_ms;
  };
  REQUIRE(pose_at() == 1'000);

  for (std::uint32_t battement = 2'000; battement <= 9'000; battement += 1'000) {
    world.RefreshPvpRings(battement);
  }
  CHECK(pose_at() == 1'000);
}

TEST_CASE("Anneau : le carnet s'oublie en changeant de carte", "[net][pvp]") {
  // ⚠ LES RANGS D'UNE CARTE NE DISENT RIEN DE LA SUIVANTE : les garder poserait
  // des anneaux sur des inconnus qui se trouveraient porter le même identifiant.
  World world = MineNamed();
  REQUIRE(ApplyFrame(FrameOf(Ranking(kOtherId, 2, 9)), world, 1'000) == FrameOutcome::kIgnored);

  world.EnterMap("prontera", 100, 100);
  SpawnPlayer(world, kOtherId);
  world.RefreshPvpRings(2'000);
  CHECK_FALSE(Marked(world, kOtherId));
}

TEST_CASE("Anneau : le nôtre tombe en changeant de carte", "[net][pvp]") {
  // 🔴 LE JOUEUR EST LE SEUL ACTEUR QUE LE VOYAGE TRANSPORTE. Le serveur
  // n'éteint pas son rang quand c'est lui qui part : garder ce rang ferait
  // reposer l'anneau au premier battement de la carte suivante.
  World world = MineNamed();
  SpawnPlayer(world, kMyId);
  REQUIRE(ApplyFrame(FrameOf(Ranking(kMyId, 1, 2)), world, 1'000) == FrameOutcome::kApplied);
  REQUIRE(Marked(world, kMyId));

  world.EnterMap("prontera", 100, 100);
  world.RefreshPvpRings(2'000);
  CHECK_FALSE(Marked(world, kMyId));
}

TEST_CASE("Anneau : un voisin hors de vue ne fait rien, et le dit", "[net][pvp]") {
  // Le serveur diffuse à toute la CARTE en éteignant la zone : on reçoit alors
  // le rang de joueurs qu'on n'a jamais vus.
  World world = MineNamed();

  CHECK(ApplyFrame(FrameOf(Ranking(kOtherId, 3, 20)), world, 1'000) == FrameOutcome::kIgnored);
}

TEST_CASE("Anneau : le nôtre se pose comme celui des autres", "[net][pvp]") {
  // ⚠ SOI N'EST PAS UN CAS À PART : le client d'origine marque quiconque entre
  // dans les dix premiers, et c'est ce que montre la capture du joueur — son
  // propre personnage cerclé, premier sur deux.
  World world = MineNamed();
  SpawnPlayer(world, kMyId);

  REQUIRE(ApplyFrame(FrameOf(Ranking(kMyId, 1, 2)), world, 5'000) == FrameOutcome::kApplied);
  CHECK(Marked(world, kMyId));
  CHECK(world.pvp_rank().rank == 1);
  CHECK(world.pvp_rank().leader_since_ms == 5'000);
}

TEST_CASE("Anneau : il survit à une réannonce d'unité", "[net][pvp]") {
  // 🔴 LE SERVEUR RÉANNONCE UNE UNITÉ À CHAQUE MISE EN MARCHE, et son paquet ne
  // porte pas le rang. Sans la traversée, l'anneau tomberait au premier pas de
  // celui qui le porte.
  World world = MineNamed();
  SpawnPlayer(world, kOtherId);
  REQUIRE(ApplyFrame(FrameOf(Ranking(kOtherId, 2, 20)), world, 1'000) == FrameOutcome::kApplied);
  REQUIRE(Marked(world, kOtherId));

  SpawnPlayer(world, kOtherId);

  const ro::sim::Actor* actor = world.Find(kOtherId);
  REQUIRE(actor != nullptr);
  CHECK(actor->pvp_rank == 2);
}

TEST_CASE("Anneau : une pose RETIRE d'abord celui du rang précédent", "[net][pvp]") {
  // 🔴 L'ANNEAU NE MEURT PLUS DE LUI-MÊME : son bâtisseur le rebâtit sans fin.
  // Une pose qui ne retirerait rien laisserait l'anneau du rang d'avant tourner
  // sous le nouveau, dans son ancienne teinte. Le client retire puis repose, à
  // chaque annonce ; le retrait se lit dans le journal que l'image consulte.
  World world = MineNamed();
  SpawnPlayer(world, kOtherId);
  REQUIRE(ApplyFrame(FrameOf(Ranking(kOtherId, 9, 20)), world, 1'000) == FrameOutcome::kApplied);
  const ro::sim::Actor* before = world.Find(kOtherId);
  REQUIRE(before != nullptr);
  const std::uint32_t removals_before = before->removed_effects_total;

  REQUIRE(ApplyFrame(FrameOf(Ranking(kOtherId, 2, 20)), world, 5'000) == FrameOutcome::kApplied);
  const ro::sim::Actor* after = world.Find(kOtherId);
  REQUIRE(after != nullptr);
  CHECK(after->removed_effects_total == removals_before + 1);
  REQUIRE_FALSE(after->removed_effects.empty());
  CHECK(after->removed_effects.back() == ro::sim::kPvpMarkEffectId);
  CHECK(Marked(world, kOtherId));
}

TEST_CASE("Anneau : poussé hors de la liste, il se repose sans s'empiler", "[net][pvp]") {
  // ⚠ LA LISTE D'EFFETS D'UN ACTEUR EST BORNÉE, et un porteur chargé d'effets en
  // pousse les plus anciens dehors — l'anneau d'abord, posé le premier. L'image,
  // elle, le montre encore : la repose doit donc le retirer avant de le poser,
  // sans quoi deux anneaux sans fin tourneraient l'un sur l'autre.
  World world = MineNamed();
  SpawnPlayer(world, kOtherId);
  REQUIRE(ApplyFrame(FrameOf(Ranking(kOtherId, 3, 9)), world, 1'000) == FrameOutcome::kApplied);
  constexpr std::uint32_t kFirstOtherEffect = 1'000;
  for (std::uint32_t rank = 0; rank < ro::sim::kEffectsKeptPerActor; ++rank) {
    REQUIRE(world.RecordEffect(kOtherId, kFirstOtherEffect + rank, 2'000));
  }
  REQUIRE_FALSE(Marked(world, kOtherId));
  const std::uint32_t removals_before = world.Find(kOtherId)->removed_effects_total;

  world.RefreshPvpRings(3'000);

  const ro::sim::Actor* actor = world.Find(kOtherId);
  REQUIRE(actor != nullptr);
  CHECK(Marked(world, kOtherId));
  CHECK(actor->removed_effects_total == removals_before + 1);
  CHECK(actor->removed_effects.back() == ro::sim::kPvpMarkEffectId);
}

TEST_CASE("Classement PVP : une trame trop courte se refuse", "[net][pvp]") {
  World world = MineNamed();
  std::vector<std::uint8_t> tronquee = Ranking(kMyId, 1, 2);
  tronquee.pop_back();

  CHECK(ApplyFrame(FrameOf(tronquee), world, 1'000) == FrameOutcome::kUndecodable);
}
