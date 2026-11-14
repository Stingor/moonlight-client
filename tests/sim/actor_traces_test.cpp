// CE QU'UN CHANGEMENT DE CARTE EFFACE, ET CE QU'IL GARDE.
//
// 🔴 LE JOUEUR EST LE SEUL ACTEUR QUE LE MONDE TRANSPORTE. Tous les autres sont
// détruits et réannoncés ; lui reste, parce que le serveur ne redit jamais son
// identité. Il emportait donc ses TRACES — sa dernière incantation, le dernier
// sort qu'il a posé au sol — pendant que la vue, elle, oubliait les avoir déjà
// montrées. À chaque `@load`, le joueur revoyait son dernier sort : l'anneau
// d'incantation sur la case visée, le bruit de l'incantation, et le bruit du
// sort. Vu en jeu.

#include "ro/sim/actor_traces.h"

#include <array>
#include <cstdint>

#include <catch2/catch_test_macros.hpp>

#include "ro/sim/actor.h"
#include "ro/sim/world.h"

namespace {

constexpr std::uint32_t kPlayer = 0x0044'5566;
constexpr std::uint32_t kNeighbour = 0x0077'8899;

// Un joueur qui vient de lancer un sort sur une case, et qui porte de quoi être
// reconnu à l'arrivée.
[[nodiscard]] ro::sim::Actor PlayerWhoJustCast() {
  ro::sim::Actor traced_player;
  traced_player.id = kPlayer;
  traced_player.position = ro::sim::CellPosition{10, 10};
  traced_player.health = 42;
  traced_player.max_health = 99;

  traced_player.cast = ro::sim::Cast{};
  traced_player.ground_skill = ro::sim::GroundSkillUse{};
  traced_player.skill_scale = ro::sim::SkillScale{};
  traced_player.attack = ro::sim::Strike{};
  traced_player.hit = ro::sim::Strike{};
  traced_player.support = ro::sim::SupportHit{};
  traced_player.effects.push_back(ro::sim::PlayedEffect{.id = 315, .since_ms = 1000});
  traced_player.chat_text = "quelque chose";
  traced_player.chat_since_ms = 1000;
  traced_player.announced_skill_id = 89;
  traced_player.announced_skill_since_ms = 1000;
  traced_player.emotion_since_ms = 1000;
  traced_player.pickup_since_ms = 1000;
  return traced_player;
}

}  // namespace

TEST_CASE("Traces : un changement de carte efface la dernière incantation", "[sim][world]") {
  ro::sim::Actor traced_player = PlayerWhoJustCast();
  REQUIRE(traced_player.cast.has_value());
  ro::sim::ForgetLastMapTraces(traced_player);

  // 🔴 LES DEUX QUE LE JOUEUR A VUS REVENIR À CHAQUE `@load`.
  CHECK_FALSE(traced_player.cast.has_value());
  CHECK_FALSE(traced_player.ground_skill.has_value());
  CHECK_FALSE(traced_player.skill_scale.has_value());

  // Et les autres événements passés, qui se rejoueraient de la même façon.
  CHECK_FALSE(traced_player.attack.has_value());
  CHECK_FALSE(traced_player.hit.has_value());
  CHECK_FALSE(traced_player.support.has_value());
  CHECK_FALSE(traced_player.movement.has_value());
  CHECK_FALSE(traced_player.pickup_since_ms.has_value());
  CHECK(traced_player.effects.empty());
  CHECK(traced_player.chat_text.empty());
  CHECK(traced_player.announced_skill_id == 0);

  // ⚠ ET CE QUI RESTE EST UN ÉTAT, PAS UN ÉVÉNEMENT : la vie et la place
  // survivent, sans quoi un personnage qui retraverse un portail clignoterait.
  CHECK(traced_player.health == 42);
  CHECK(traced_player.max_health == 99);
}

TEST_CASE("Traces : le monde les efface en entrant sur une carte", "[sim][world]") {
  // ⚠ LE CONTRÔLE PASSE PAR `EnterMap`, et non par la fonction seule : c'est le
  // BRANCHEMENT qui manquait, pas la liste. Une liste juste qu'on n'appelle pas
  // ne se voit nulle part.
  ro::sim::World world;
  world.set_player_id(kPlayer);
  world.Spawn(PlayerWhoJustCast());

  ro::sim::Actor neighbour;
  neighbour.id = kNeighbour;
  world.Spawn(neighbour);
  REQUIRE(world.actors().size() == 2);
  // Croisé, il protégeait ce voisin.
  std::array<std::uint32_t, ro::sim::kDevotionSlots> devotees{};
  devotees[0] = kNeighbour;
  REQUIRE(world.TellDevotion(kPlayer, devotees));

  world.EnterMap("prontera", 50, 60);

  // Le voisin est détruit — le serveur le réannoncera.
  CHECK_FALSE(world.actors().contains(kNeighbour));
  // Le joueur reste, à sa nouvelle place, mais sans une seule de ses traces.
  const ro::sim::Actor* traced_player = world.player();
  REQUIRE(traced_player != nullptr);
  CHECK(traced_player->position.x == 50);
  CHECK(traced_player->position.y == 60);
  CHECK_FALSE(traced_player->cast.has_value());
  CHECK_FALSE(traced_player->ground_skill.has_value());
  CHECK(traced_player->effects.empty());
  // Ses traits de Devotion sont restés là-bas : rien ne se retend tout seul.
  CHECK(traced_player->devotion.devotee_ids[0] == 0);
  CHECK(traced_player->devotion.told_serial == 0);
}
