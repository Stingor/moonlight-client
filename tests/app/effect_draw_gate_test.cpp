// La porte de dessin d'un effet porté par un acteur.
//
// 🔴 UN EFFET NE TRAHIT PAS UN PERSONNAGE JOUEUR CACHÉ : le client saute le
// dessin de ce qu'il porte, sauf pour le joueur lui-même, et pour un membre du
// groupe là où la carte le permet.

#include "effects/effect_draw_gate.h"

#include <cstdint>
#include <utility>

#include <catch2/catch_test_macros.hpp>

#include "ro/render/effect_dye.h"
#include "ro/render/hat_effect_table.h"
#include "ro/sim/map_mode.h"
#include "ro/sim/party_roster.h"
#include "ro/sim/world.h"

#include "effects/effect_bearer_gate.h"

using moonclient::BearerGate;
using moonclient::BearerGateOf;
using moonclient::EffectDrawGate;
using ro::sim::Actor;
using ro::sim::ActorKind;
using ro::sim::World;

namespace {

constexpr std::uint32_t kSelf = 2000001;
constexpr std::uint32_t kRogue = 2000002;
constexpr std::uint32_t kFriend = 2000003;
constexpr std::uint32_t kWalker = 2000004;
constexpr std::uint32_t kGuide = 110000001;
constexpr std::uint32_t kNobody = 2000099;
constexpr std::uint32_t kSomeEffect = 16;

[[nodiscard]] Actor Someone(std::uint32_t id, ActorKind kind, std::uint32_t option) {
  Actor actor;
  actor.id = id;
  actor.kind = kind;
  actor.option = option;
  return actor;
}

}  // namespace

TEST_CASE("Porte de dessin : un porteur caché cache ses effets, sauf aux siens et au PNJ",
          "[app][effects][draw_gate]") {
  World world;
  world.set_player_id(kSelf);
  world.Spawn(Someone(kSelf, ActorKind::kPlayer, ro::sim::kOptionHide));
  world.Spawn(Someone(kRogue, ActorKind::kPlayer, ro::sim::kOptionCloak));
  world.Spawn(Someone(kFriend, ActorKind::kPlayer, ro::sim::kOptionHide));
  world.Spawn(Someone(kWalker, ActorKind::kPlayer, 0));
  // Un PNJ n'est jamais caché à cette porte, quelles que soient ses options.
  world.Spawn(Someone(kGuide, ActorKind::kNonPlayer, ro::sim::kOptionInvisible));
  ro::sim::PartyRoster party;
  party.Upsert(ro::sim::PartyMember{.account_id = kFriend});

  CHECK(BearerGateOf(kWalker, world, &party) == BearerGate::kShown);
  CHECK(BearerGateOf(kGuide, world, &party) == BearerGate::kShown);
  CHECK(BearerGateOf(kSelf, world, &party) == BearerGate::kExcepted);
  CHECK(BearerGateOf(kRogue, world, &party) == BearerGate::kConcealed);
  // Sans porteur, ou un porteur que le monde ne connaît plus : rien ne le cache.
  CHECK(BearerGateOf(0, world, &party) == BearerGate::kShown);
  CHECK(BearerGateOf(kNobody, world, &party) == BearerGate::kShown);

  // ⚠ LE GROUPE NE VOIT SES CACHÉS QUE LÀ OÙ LA CARTE LE PERMET : un champ de
  // bataille, oui ; une carte ordinaire, non.
  CHECK(BearerGateOf(kFriend, world, &party) == BearerGate::kConcealed);
  world.TellMapType(ro::sim::kMapTypeBattlefield);
  CHECK(BearerGateOf(kFriend, world, &party) == BearerGate::kExcepted);
  // Et seulement un membre EN LIGNE.
  party.Upsert(ro::sim::PartyMember{.account_id = kFriend, .offline = true});
  CHECK(BearerGateOf(kFriend, world, &party) == BearerGate::kConcealed);
  CHECK(BearerGateOf(kFriend, world, nullptr) == BearerGate::kConcealed);
}

TEST_CASE("Porte de dessin : seul un personnage joueur caché cache ses effets",
          "[app][effects][draw_gate]") {
  // 🔴 LE CLIENT NE BÂTIT QU'AU TYPE D'OBJET ZÉRO l'acteur dont la porte lit les
  // options. Un monstre sous Cloaking, ou un joueur déguisé — annoncé d'un
  // autre type —, montre ses effets.
  constexpr std::uint32_t kMonster = 3000;
  constexpr std::uint32_t kDisguised = 2000005;
  constexpr std::uint8_t kDisguisedObjectType = 1;
  World world;
  world.set_player_id(kSelf);
  world.Spawn(Someone(kMonster, ActorKind::kMonster, ro::sim::kOptionCloak));
  Actor disguised = Someone(kDisguised, ActorKind::kPlayer, ro::sim::kOptionHide);
  disguised.object_type = kDisguisedObjectType;
  world.Spawn(disguised);
  CHECK(BearerGateOf(kMonster, world, nullptr) == BearerGate::kShown);
  CHECK(BearerGateOf(kDisguised, world, nullptr) == BearerGate::kShown);

  // LE BOSS CACHÉ, que seule la porte du signe du vent écoute.
  constexpr std::uint32_t kBoss = 3001;
  Actor boss = Someone(kBoss, ActorKind::kMonster, ro::sim::kOptionCloak);
  boss.boss = ro::sim::BossRank::kMvp;
  world.Spawn(boss);
  CHECK(moonclient::IsConcealedBoss(*world.Find(kBoss), world));
  CHECK_FALSE(moonclient::IsConcealedBoss(*world.Find(kMonster), world));
}

TEST_CASE("Porte de dessin : le joueur caché voit ses effets, pas ceux de ses chapeaux",
          "[app][effects][draw_gate]") {
  EffectDrawGate gate;
  gate.concealed = {{kSelf, BearerGate::kExcepted}, {kRogue, BearerGate::kConcealed}};
  gate.present = {kSelf, kRogue, kWalker};
  // 🔴 LA MARQUE EST CELLE DE L'INSTANCE, et non du numéro : le même effet né
  // d'une compétence se montre, né d'un chapeau il se cache.
  CHECK(gate.Draws(kSelf, false));
  CHECK_FALSE(gate.Draws(kSelf, true));
  CHECK_FALSE(gate.Draws(kRogue, false));
  CHECK(gate.Draws(kWalker, true));
  CHECK(gate.Draws(0, false));
  // LA CIBLE D'UN TRAIT DE DEVOTION passe la même porte, sans les chapeaux — et
  // une cible introuvable ne laisse rien dessiner.
  CHECK(gate.DrawsTarget(kSelf));
  CHECK_FALSE(gate.DrawsTarget(kRogue));
  CHECK(gate.DrawsTarget(kWalker));
  CHECK_FALSE(gate.DrawsTarget(kNobody));
}

TEST_CASE("Porte de dessin : le signe du vent ne se cache que sur un boss caché",
          "[app][effects][draw_gate]") {
  constexpr std::uint32_t kBoss = 3001;
  EffectDrawGate gate;
  gate.concealed = {{kRogue, BearerGate::kConcealed}};
  gate.present = {kRogue, kBoss, kWalker};
  gate.concealed_bosses = {kBoss};
  // Un joueur caché le montre : la porte commune ne joue pas pour lui.
  CHECK(gate.DrawsEffect(moonclient::kWindSignEffect, kRogue, false));
  CHECK(gate.DrawsEffect(moonclient::kWindSignEffect, kWalker, false));
  CHECK_FALSE(gate.DrawsEffect(moonclient::kWindSignEffect, kBoss, false));
  // Sans porteur, il ne se dessine pas.
  CHECK_FALSE(gate.DrawsEffect(moonclient::kWindSignEffect, kNobody, false));
  // Tout autre effet garde la porte commune.
  CHECK_FALSE(gate.DrawsEffect(kSomeEffect, kRogue, false));
  CHECK(gate.DrawsEffect(kSomeEffect, kBoss, false));
}

TEST_CASE("Porte de dessin : la teinture d'un chapeau ne vaut que pour l'instance qu'il a semée",
          "[app][effects][draw_gate]") {
  // Le halo du Nouvel An (numéro 20), joué sous son rang : le rang plus la base.
  constexpr std::uint32_t kHaloNumber = 20;
  constexpr std::uint32_t kHaloBorn = ro::render::kHatEffectIdBase + kHaloNumber;
  const ro::render::EffectDye cyan = ro::render::MakeEffectDye(120, 0, 0, false);
  EffectDrawGate gate;
  gate.hat_dyes.emplace(std::pair{kWalker, kHaloBorn}, cyan);

  const ro::render::EffectDye* dye = gate.HatDyeOf(kHaloBorn, kWalker, true);
  REQUIRE(dye != nullptr);
  CHECK(*dye == cyan);
  // La même naissance, non marquée « chapeau » : une compétence que le
  // porteur joue sous le même identifiant reste d'origine.
  CHECK(gate.HatDyeOf(kHaloBorn, kWalker, false) == nullptr);
  // Un autre porteur du même costume, sans teinture à lui.
  CHECK(gate.HatDyeOf(kHaloBorn, kFriend, true) == nullptr);
  // Un effet posé, sans porteur.
  CHECK(gate.HatDyeOf(kHaloBorn, 0, true) == nullptr);
}
