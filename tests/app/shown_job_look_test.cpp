// CE QU'UN ACTEUR MONTRE, LU SUR CE QUE LE SERVEUR EN A DIT : ses options et
// deux de ses états choisissent son corps, et le métier exercé ne bouge pas.

#include <cstdint>

#include <catch2/catch_test_macros.hpp>

#include "ro/render/shown_job.h"
#include "ro/render/sprite_catalog.h"
#include "ro/sim/actor.h"

#include "world/actor_view.h"

namespace {

constexpr std::uint32_t kKnight = 7;
constexpr std::uint32_t kPecoKnight = 13;
constexpr std::uint32_t kMechanic = 4058;
constexpr std::uint32_t kMadogearBody = 4086;
constexpr std::uint32_t kPoringJob = 1002;

ro::sim::Actor Someone(ro::sim::ActorKind kind, std::uint32_t job, std::uint32_t option) {
  ro::sim::Actor actor;
  actor.id = 150'001;
  actor.kind = kind;
  actor.look.job = static_cast<std::uint16_t>(job);
  actor.option = option;
  return actor;
}

}  // namespace

TEST_CASE("Apparence : un chevalier sur son Peco montre le chevalier monté", "[app][shown_job]") {
  const ro::sim::Actor knight =
      Someone(ro::sim::ActorKind::kPlayer, kKnight, ro::render::kOptionPeco);
  CHECK(moonclient::ShownJobOf(knight) == kPecoKnight);
  CHECK(moonclient::LookOf(knight).job == kPecoKnight);
  // Le métier exercé reste celui que le serveur a dit.
  CHECK(knight.look.job == kKnight);
}

TEST_CASE("Apparence : le Madogear se lit sur son état, et sa valeur choisit la combinaison",
          "[app][shown_job]") {
  ro::sim::Actor mechanic = Someone(ro::sim::ActorKind::kPlayer, kMechanic, 0);
  CHECK(moonclient::LookOf(mechanic).job == kMechanic);
  mechanic.status_effects.insert(ro::sim::kStatusMadogear);
  CHECK(moonclient::LookOf(mechanic).job == kMadogearBody);
  CHECK_FALSE(moonclient::LookOf(mechanic).madogear_suit);
  mechanic.status_values[ro::sim::kStatusMadogear] = ro::sim::kMadogearSuitValue;
  CHECK(moonclient::LookOf(mechanic).madogear_suit);
}

TEST_CASE("Apparence : le second costume d'été est un état", "[app][shown_job]") {
  ro::sim::Actor knight = Someone(ro::sim::ActorKind::kPlayer, kKnight, 0);
  knight.status_effects.insert(ro::sim::kStatusDressUp);
  CHECK(moonclient::LookOf(knight).job == ro::render::kDressUpJob);
}

TEST_CASE("Apparence : seul ce qui se compose comme un joueur change de corps",
          "[app][shown_job]") {
  // Un objet au sol porte un identifiant d'OBJET, qui peut valoir celui d'un
  // métier : il ne se monte pas.
  ro::sim::Actor item = Someone(ro::sim::ActorKind::kItem, kKnight, ro::render::kOptionPeco);
  item.ground_item = ro::sim::GroundItem{};
  item.ground_item->item_id = kKnight;
  CHECK(moonclient::LookOf(item).job == kKnight);
  const ro::sim::Actor poring =
      Someone(ro::sim::ActorKind::kMonster, kPoringJob, ro::render::kOptionWedding);
  CHECK(moonclient::ShownJobOf(poring) == kPoringJob);
}

TEST_CASE("Apparence : un joueur déguisé se dessine d'après la classe qu'il emprunte",
          "[app][shown_job]") {
  // Le type d'objet 1 : un joueur déguisé, ou l'interlocuteur fictif d'une
  // commande qui ouvre un dialogue, en PNJ caché.
  constexpr std::uint8_t kDisguisedPlayerObject = 0x1;
  constexpr std::uint32_t kHiddenNpcJob = 111;
  ro::sim::Actor fake_npc = Someone(ro::sim::ActorKind::kPlayer, kHiddenNpcJob, 0);
  fake_npc.object_type = kDisguisedPlayerObject;
  CHECK_FALSE(moonclient::ComposedAsPlayer(fake_npc));
  CHECK_FALSE(moonclient::LookOf(fake_npc).player);
  CHECK(moonclient::LayoutOf(fake_npc) == ro::render::SpriteLayout::kCreature);
  CHECK(moonclient::LookOf(fake_npc).job == kHiddenNpcJob);

  ro::sim::Actor disguised = Someone(ro::sim::ActorKind::kPlayer, kPoringJob, 0);
  disguised.object_type = kDisguisedPlayerObject;
  CHECK(moonclient::LookOf(disguised).job == kPoringJob);
  CHECK_FALSE(moonclient::LookOf(disguised).player);

  // Annoncé en type 0, un joueur se compose toujours — même d'une classe que la
  // table des corps ne connaît pas, qui prend alors celui du novice.
  const ro::sim::Actor knight = Someone(ro::sim::ActorKind::kPlayer, kKnight, 0);
  CHECK(moonclient::ComposedAsPlayer(knight));
  // Et un PNJ vêtu d'un métier aussi.
  const ro::sim::Actor dressed_npc = Someone(ro::sim::ActorKind::kNonPlayer, kKnight, 0);
  CHECK(moonclient::ComposedAsPlayer(dressed_npc));
}

TEST_CASE("Apparence : 104, le PNJ caché et le portail caché ne se montrent pas",
          "[app][shown_job]") {
  constexpr std::uint32_t kUnseenJob = 104;
  constexpr std::uint32_t kHiddenNpcJob = 111;
  constexpr std::uint32_t kHiddenWarpJob = 139;
  for (const std::uint32_t job : {kUnseenJob, kHiddenNpcJob, kHiddenWarpJob}) {
    CHECK(moonclient::IsUnseenClass(Someone(ro::sim::ActorKind::kNonPlayer, job, 0)));
    CHECK(moonclient::IsUnseenClass(Someone(ro::sim::ActorKind::kMonster, job, 0)));
  }
  // L'interlocuteur fictif d'une commande, annoncé en joueur déguisé.
  ro::sim::Actor fake_npc = Someone(ro::sim::ActorKind::kPlayer, kHiddenNpcJob, 0);
  constexpr std::uint8_t kDisguisedPlayerObject = 0x1;
  fake_npc.object_type = kDisguisedPlayerObject;
  CHECK(moonclient::IsUnseenClass(fake_npc));

  // Leurs voisins se montrent, et un objet au sol se lit par son identifiant
  // d'objet, qui peut valoir l'une de ces classes.
  CHECK_FALSE(
      moonclient::IsUnseenClass(Someone(ro::sim::ActorKind::kNonPlayer, kHiddenWarpJob + 1, 0)));
  CHECK_FALSE(moonclient::IsUnseenClass(Someone(ro::sim::ActorKind::kItem, kHiddenWarpJob, 0)));
  CHECK_FALSE(moonclient::IsUnseenClass(Someone(ro::sim::ActorKind::kNonPlayer, kPoringJob, 0)));
}

TEST_CASE("Apparence : le PNJ caché se clique sans se montrer, 104 et le portail caché non",
          "[app][shown_job]") {
  constexpr std::uint32_t kUnseenJob = 104;
  constexpr std::uint32_t kHiddenNpcJob = 111;
  constexpr std::uint32_t kHiddenWarpJob = 139;
  // Le drapeau du château qui mène au concierge et aux coffres est un PNJ caché.
  CHECK(moonclient::IsPickedWithoutBody(Someone(ro::sim::ActorKind::kNonPlayer, kHiddenNpcJob, 0)));
  CHECK_FALSE(
      moonclient::IsPickedWithoutBody(Someone(ro::sim::ActorKind::kNonPlayer, kUnseenJob, 0)));
  CHECK_FALSE(
      moonclient::IsPickedWithoutBody(Someone(ro::sim::ActorKind::kNonPlayer, kHiddenWarpJob, 0)));
  // Un PNJ qui se montre se désigne par son corps, et un objet au sol par le sien.
  CHECK_FALSE(
      moonclient::IsPickedWithoutBody(Someone(ro::sim::ActorKind::kNonPlayer, kPoringJob, 0)));
  CHECK_FALSE(
      moonclient::IsPickedWithoutBody(Someone(ro::sim::ActorKind::kItem, kHiddenNpcJob, 0)));
  // L'interlocuteur fictif d'une commande, sur la case du joueur, ne prend pas
  // le clic porté à ses pieds.
  ro::sim::Actor fake_npc = Someone(ro::sim::ActorKind::kPlayer, kHiddenNpcJob, 0);
  constexpr std::uint8_t kDisguisedPlayerObject = 0x1;
  fake_npc.object_type = kDisguisedPlayerObject;
  CHECK_FALSE(moonclient::IsPickedWithoutBody(fake_npc));
}
