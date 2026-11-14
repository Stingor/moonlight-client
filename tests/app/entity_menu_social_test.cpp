// LE MENU D'ENTITÉ, SES LIGNES QUI AGISSENT POUR AUTRUI : le courrier,
// l'adoption et le suivi sur un joueur, le ramassage automatique sur un objet
// au sol. Chacune paraît dans ses cas, et seulement dans ceux-là.

#include <algorithm>
#include <cstdint>
#include <string>
#include <vector>

#include <catch2/catch_test_macros.hpp>

#include "ro/ui/entity_menu.h"

#include "world/entity_menu.h"

using moonclient::BuildEntityMenu;
using moonclient::EntityFamily;
using moonclient::EntityMenuFacts;
using moonclient::EntityVerb;
using ro::ui::EntityMenuItem;
using ro::ui::EntityMenuView;

namespace {

constexpr std::uint32_t kSelf = 2000001;
constexpr std::uint32_t kPlayer = 2000002;
constexpr std::uint32_t kJellopy = 909;

[[nodiscard]] const EntityMenuItem* Find(const EntityMenuView& view, EntityVerb verb) {
  const auto found = std::ranges::find_if(view.items, [verb](const EntityMenuItem& item) {
    return item.verb == static_cast<int>(verb);
  });
  return found != view.items.end() ? &*found : nullptr;
}

[[nodiscard]] std::size_t RankOf(const EntityMenuView& view, EntityVerb verb) {
  const auto found = std::ranges::find_if(view.items, [verb](const EntityMenuItem& item) {
    return item.verb == static_cast<int>(verb);
  });
  return static_cast<std::size_t>(found - view.items.begin());
}

[[nodiscard]] EntityMenuFacts Player() {
  EntityMenuFacts facts;
  facts.family = EntityFamily::kPlayer;
  facts.id = kPlayer;
  facts.name = "Quelqu'un";
  return facts;
}

[[nodiscard]] EntityMenuFacts GroundItem(bool identified) {
  EntityMenuFacts facts;
  facts.family = EntityFamily::kGroundItem;
  facts.id = kJellopy;
  facts.item_id = kJellopy;
  facts.item_identified = identified;
  return facts;
}

}  // namespace

TEST_CASE("Menu d'entité : le courrier s'écrit à tout autre joueur, jamais à soi",
          "[app][entity_menu][mail]") {
  const EntityMenuView view = BuildEntityMenu(Player(), {});
  const EntityMenuItem* mail = Find(view, EntityVerb::kMailSend);
  REQUIRE(mail != nullptr);
  CHECK(mail->label == "Envoyer un courrier");
  CHECK_FALSE(mail->disabled);
  // À SA PLACE : après l'amitié, avant le blocage des chuchotements.
  CHECK(RankOf(view, EntityVerb::kFriendAdd) + 1 == RankOf(view, EntityVerb::kMailSend));
  CHECK(RankOf(view, EntityVerb::kMailSend) + 1 == RankOf(view, EntityVerb::kWhisperBlock));

  EntityMenuFacts self;
  self.family = EntityFamily::kSelf;
  self.id = kSelf;
  self.name = "Moi";
  CHECK(Find(BuildEntityMenu(self, {}), EntityVerb::kMailSend) == nullptr);
}

TEST_CASE("Menu d'entité : le courrier se grise sans nom, ou refusé d'avance",
          "[app][entity_menu][mail]") {
  EntityMenuFacts facts = Player();
  facts.name.clear();
  const EntityMenuView unnamed = BuildEntityMenu(facts, {});
  REQUIRE(Find(unnamed, EntityVerb::kMailSend) != nullptr);
  CHECK(Find(unnamed, EntityVerb::kMailSend)->disabled);
  CHECK_FALSE(Find(unnamed, EntityVerb::kMailSend)->tip.empty());

  facts = Player();
  facts.mail_refusal = "Un courrier est déjà en cours d'écriture.";
  const EntityMenuView writing = BuildEntityMenu(facts, {});
  REQUIRE(Find(writing, EntityVerb::kMailSend) != nullptr);
  CHECK(Find(writing, EntityVerb::kMailSend)->disabled);
  CHECK(Find(writing, EntityVerb::kMailSend)->tip == facts.mail_refusal);
}

TEST_CASE("Menu d'entité : « Adopter » n'existe que si le client la juge possible",
          "[app][entity_menu][adoption]") {
  EntityMenuFacts facts = Player();
  CHECK(Find(BuildEntityMenu(facts, {}), EntityVerb::kAdopt) == nullptr);
  facts.adoption_possible = true;
  const EntityMenuView view = BuildEntityMenu(facts, {});
  const EntityMenuItem* adopt = Find(view, EntityVerb::kAdopt);
  REQUIRE(adopt != nullptr);
  CHECK(adopt->label == "Adopter");
  CHECK_FALSE(adopt->disabled);
  CHECK_FALSE(adopt->tip.empty());
  CHECK(RankOf(view, EntityVerb::kMailSend) + 1 == RankOf(view, EntityVerb::kAdopt));
}

TEST_CASE("Menu d'entité : « Suivre » un joueur, ou « Ne plus le suivre » celui qu'on suit",
          "[app][entity_menu][suivi]") {
  EntityMenuFacts facts = Player();
  const EntityMenuView idle = BuildEntityMenu(facts, {});
  const EntityMenuItem* follow = Find(idle, EntityVerb::kFollow);
  REQUIRE(follow != nullptr);
  CHECK(follow->label == "Suivre");
  CHECK_FALSE(follow->disabled);
  CHECK(Find(idle, EntityVerb::kUnfollow) == nullptr);
  // À SA PLACE : après l'échange, avant l'amitié.
  CHECK(RankOf(idle, EntityVerb::kDealPropose) + 1 == RankOf(idle, EntityVerb::kFollow));
  CHECK(RankOf(idle, EntityVerb::kFollow) + 1 == RankOf(idle, EntityVerb::kFriendAdd));

  facts.followed = true;
  const EntityMenuView following = BuildEntityMenu(facts, {});
  const EntityMenuItem* unfollow = Find(following, EntityVerb::kUnfollow);
  REQUIRE(unfollow != nullptr);
  CHECK(unfollow->label == "Ne plus le suivre");
  CHECK(Find(following, EntityVerb::kFollow) == nullptr);

  // Ni sur soi, ni sur un monstre.
  EntityMenuFacts self;
  self.family = EntityFamily::kSelf;
  self.id = kSelf;
  self.name = "Moi";
  CHECK(Find(BuildEntityMenu(self, {}), EntityVerb::kFollow) == nullptr);
  EntityMenuFacts monster;
  monster.family = EntityFamily::kMonster;
  monster.id = kJellopy;
  CHECK(Find(BuildEntityMenu(monster, {}), EntityVerb::kFollow) == nullptr);
}

TEST_CASE("Menu d'entité : le ramassage automatique suit la liste du serveur",
          "[app][entity_menu][autoloot]") {
  // AVANT QUE LE SERVEUR AIT DIT LA LISTE : aucune des deux lignes.
  EntityMenuFacts facts = GroundItem(true);
  const EntityMenuView untold = BuildEntityMenu(facts, {});
  CHECK(Find(untold, EntityVerb::kAutolootAdd) == nullptr);
  CHECK(Find(untold, EntityVerb::kAutolootRemove) == nullptr);

  facts.item_autolooted = false;
  const EntityMenuView out = BuildEntityMenu(facts, {});
  const EntityMenuItem* add = Find(out, EntityVerb::kAutolootAdd);
  REQUIRE(add != nullptr);
  CHECK(add->label == "Ajouter au ramassage automatique");
  CHECK_FALSE(add->disabled);
  CHECK(add->separator);
  CHECK(Find(out, EntityVerb::kAutolootRemove) == nullptr);
  // Entre le lien et les commandes @, dans son propre bloc.
  CHECK(RankOf(out, EntityVerb::kItemLink) + 1 == RankOf(out, EntityVerb::kAutolootAdd));
  CHECK(RankOf(out, EntityVerb::kAutolootAdd) + 1 == RankOf(out, EntityVerb::kItemInfo));
  CHECK(Find(out, EntityVerb::kItemInfo)->separator);

  facts.item_autolooted = true;
  const EntityMenuView in = BuildEntityMenu(facts, {});
  const EntityMenuItem* remove = Find(in, EntityVerb::kAutolootRemove);
  REQUIRE(remove != nullptr);
  CHECK(remove->label == "Retirer du ramassage automatique");
  CHECK(Find(in, EntityVerb::kAutolootAdd) == nullptr);
}

TEST_CASE("Menu d'entité : le ramassage automatique ne trahit pas un objet inconnu",
          "[app][entity_menu][autoloot]") {
  EntityMenuFacts facts = GroundItem(false);
  facts.item_autolooted = true;
  const EntityMenuView view = BuildEntityMenu(facts, {});
  CHECK(Find(view, EntityVerb::kAutolootRemove) == nullptr);
  const EntityMenuItem* neutral = Find(view, EntityVerb::kAutolootAdd);
  REQUIRE(neutral != nullptr);
  CHECK(neutral->label == "Ramassage automatique");
  CHECK(neutral->disabled);
  CHECK_FALSE(neutral->tip.empty());
}

TEST_CASE("Menu d'entité : une échoppe se compare au Marché, et seulement elle",
          "[app][entity_menu][market]") {
  EntityMenuFacts facts = Player();
  CHECK(Find(BuildEntityMenu(facts, {}), EntityVerb::kMarketCompare) == nullptr);
  facts.store_open = true;
  const EntityMenuView view = BuildEntityMenu(facts, {});
  const EntityMenuItem* compare = Find(view, EntityVerb::kMarketCompare);
  REQUIRE(compare != nullptr);
  CHECK(compare->label == "Comparer sur le Marché");
  CHECK_FALSE(compare->disabled);
  CHECK_FALSE(compare->tip.empty());
  // Juste après la proposition d'échange : les deux lignes du commerce.
  CHECK(RankOf(view, EntityVerb::kMarketCompare) == RankOf(view, EntityVerb::kDealPropose) + 1);
}

TEST_CASE("Menu d'entité : un PNJ qui parle se coupe comme un joueur",
          "[app][entity_menu][voice]") {
  EntityMenuFacts npc;
  npc.family = EntityFamily::kNpc;
  npc.id = 110000123;
  npc.name = "Sting-Bot";
  // Muet, il n'a rien à couper.
  CHECK(Find(BuildEntityMenu(npc, {}), EntityVerb::kVoiceBlock) == nullptr);

  npc.voice_heard = true;
  const EntityMenuView heard = BuildEntityMenu(npc, {});
  const EntityMenuItem* block = Find(heard, EntityVerb::kVoiceBlock);
  REQUIRE(block != nullptr);
  CHECK(block->label == "Couper sa voix");

  npc.voice_blocked = true;
  const EntityMenuView blocked = BuildEntityMenu(npc, {});
  CHECK(Find(blocked, EntityVerb::kVoiceBlock) == nullptr);
  REQUIRE(Find(blocked, EntityVerb::kVoiceAllow) != nullptr);
  CHECK(Find(blocked, EntityVerb::kVoiceAllow)->label == "Rétablir sa voix");
}
