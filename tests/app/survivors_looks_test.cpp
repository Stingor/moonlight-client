// LA TABLE D'APPARENCE DES ACTEURS DU MODE : chaque espèce, plante et objet
// des règles livrées se résout dans les tables livrées avec le client —
// `class_sprites.yml` pour une classe, `item_info.yml` pour un objet —, sans
// repli muet en Poring ; chaque seuil de gemme a son objet ; le héros garde la
// tête du personnage sur le corps du mode.

#include "survivors/survivors_looks.h"

#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <filesystem>
#include <fstream>
#include <optional>
#include <sstream>
#include <string>
#include <string_view>
#include <utility>

#include <catch2/catch_test_macros.hpp>

#include "ro/render/sprite_catalog.h"
#include "ro/render/sprite_frame.h"
#include "ro/render/sprite_tables.h"
#include "ro/sim/survivors_content.h"
#include "ro/sim/survivors_rules.h"
#include "ro/vfs/item_table.h"

namespace {

// Le dépôt se situe depuis ce fichier : le crible Linux ne pose pas les
// définitions de cible.
[[nodiscard]] std::string ShippedTable(const char* name) {
  const std::filesystem::path path = std::filesystem::path(__FILE__).parent_path() / ".." / ".." /
                                     "dist" / "data" / "yaml_files" / name;
  std::ifstream file(path, std::ios::binary);
  REQUIRE(file.good());
  std::ostringstream text;
  text << file.rdbuf();
  return text.str();
}

// Le catalogue des tables livrées : les classes, les armes et les objets.
[[nodiscard]] const ro::render::SpriteCatalog& ShippedCatalog() {
  static const ro::render::SpriteCatalog kCatalog = [] {
    ro::render::SpriteCatalog catalog;
    REQUIRE(ro::render::ParseSpriteTableText(ro::render::SpriteTableFile::kClasses,
                                             ShippedTable("class_sprites.yml"), catalog.tables)
                .has_value());
    REQUIRE(ro::render::ParseSpriteTableText(ro::render::SpriteTableFile::kWeapons,
                                             ShippedTable("weapon_sprites.yml"), catalog.tables)
                .has_value());
    auto items = ro::vfs::ParseItemTable(ShippedTable("item_info.yml"));
    REQUIRE(items.has_value());
    catalog.items = std::move(*items);
    return catalog;
  }();
  return kCatalog;
}

// Une classe a son sprite propre dans la table : ni absente, ni un modèle.
void RequireClassSprite(std::uint16_t foe_id) {
  INFO("classe " << foe_id);
  const std::string* sprite =
      ro::render::FindSpriteEntry(ShippedCatalog().tables.class_sprites, foe_id);
  REQUIRE(sprite != nullptr);
  CHECK_FALSE(sprite->empty());
  CHECK_FALSE(ro::render::DrawnAsModel(ShippedCatalog(), moonclient::FoeLook(foe_id)));
  CHECK(ro::render::BodySpritePath(ShippedCatalog(), moonclient::FoeLook(foe_id)).has_value());
}

// Un objet a sa ressource dans la table des objets.
void RequireItemSprite(std::uint16_t item_id) {
  INFO("objet " << item_id);
  CHECK(ro::render::BodySpritePath(ShippedCatalog(), moonclient::ItemLook(item_id)).has_value());
}

// Un personnage Lord Knight, coiffé, chapeauté, sa cape au dos et un bouclier au bras.
constexpr std::uint32_t kLordKnightJob = 4008;
constexpr std::uint16_t kHair = 7;
constexpr std::uint16_t kHeadgear = 5;
constexpr std::uint16_t kRobe = 2;
constexpr std::uint16_t kShield = 2101;
constexpr std::uint16_t kCostume = 1;
constexpr std::int16_t kClothes = 3;

}  // namespace

TEST_CASE("Apparences du mode : chaque espèce et chaque plante a son sprite", "[app][survivors]") {
  const ro::sim::SurvivorsRules& rules = ro::sim::ShippedSurvivorsRules();
  for (const ro::sim::FoeRule& foe : rules.foes) RequireClassSprite(foe.foe_id);
  for (const ro::sim::PlantRule& plant : rules.plants) RequireClassSprite(plant.foe_id);
}

TEST_CASE("Apparences du mode : chaque monstre qui tire a un projectile à dessiner",
          "[app][survivors]") {
  const ro::sim::SurvivorsRules& rules = ro::sim::ShippedSurvivorsRules();
  std::size_t shooters = 0;
  for (const ro::sim::FoeRule& foe : rules.foes) {
    if (foe.behaviour.move != ro::sim::FoeMove::kRanged) continue;
    ++shooters;
    INFO("classe " << foe.foe_id);
    const std::string_view sprite = ro::render::ArrowSpriteOf(
        ShippedCatalog(), moonclient::FoeShooterLookOf(foe.foe_id), false);
    CHECK((!sprite.empty() || moonclient::FoeShotEffectOf(foe.foe_id).has_value()));
  }
  CHECK(shooters > 0);
}

TEST_CASE("Apparences du mode : chaque objet au sol, coffre et gemme a son sprite",
          "[app][survivors]") {
  const ro::sim::SurvivorsRules& rules = ro::sim::ShippedSurvivorsRules();
  for (const ro::sim::PickupRule& pickup : rules.pickups) RequireItemSprite(pickup.item_id);
  RequireItemSprite(rules.chests.item_id);
  for (const std::uint16_t gem : {moonclient::kBlueGemstoneItem, moonclient::kYellowGemstoneItem,
                                  moonclient::kRedGemstoneItem, moonclient::kEmperiumItem}) {
    RequireItemSprite(gem);
  }
}

TEST_CASE("Apparences du mode : une carte tombée a le sprite RO d'une carte au sol",
          "[app][survivors]") {
  const auto ground_card = ro::render::BodySpritePath(
      ShippedCatalog(), moonclient::ItemLook(moonclient::kGroundCardItem));
  REQUIRE(ground_card.has_value());
  // Chaque carte des règles livrées se montre par la carte au sol, et non par
  // sa propre ressource : celle de la Mastering Card, propre au serveur, n'est
  // pas le sprite d'une carte.
  for (const ro::sim::FoeRule& foe : ro::sim::ShippedSurvivorsRules().foes) {
    if (foe.card_item_id == 0) continue;
    INFO("carte " << foe.card_item_id);
    const ro::sim::SurvivorsPickup card{.kind = ro::sim::PickupKind::kCard,
                                        .item_id = foe.card_item_id};
    CHECK(moonclient::PickupLookOf(card) == moonclient::kGroundCardItem);
  }
  constexpr std::uint16_t kMasteringCard = 4197;
  CHECK(ro::render::BodySpritePath(ShippedCatalog(), moonclient::ItemLook(kMasteringCard)) !=
        ground_card);
}

TEST_CASE("Apparences du mode : chaque seuil de gemme a son objet", "[app][survivors]") {
  using moonclient::GemItemOf;
  CHECK(GemItemOf(1) == moonclient::kBlueGemstoneItem);
  CHECK(GemItemOf(moonclient::kYellowGemFromExp - 1) == moonclient::kBlueGemstoneItem);
  CHECK(GemItemOf(moonclient::kYellowGemFromExp) == moonclient::kYellowGemstoneItem);
  CHECK(GemItemOf(moonclient::kRedGemFromExp - 1) == moonclient::kYellowGemstoneItem);
  CHECK(GemItemOf(moonclient::kRedGemFromExp) == moonclient::kRedGemstoneItem);
  CHECK(GemItemOf(moonclient::kEmperiumFromExp - 1) == moonclient::kRedGemstoneItem);
  CHECK(GemItemOf(moonclient::kEmperiumFromExp) == moonclient::kEmperiumItem);
}

TEST_CASE("Apparences du mode : le héros garde la tête du personnage sur le corps du mode",
          "[app][survivors]") {
  const ro::sim::ClassRule& mage = ro::sim::ShippedSurvivorsRules().classes.front();
  REQUIRE(mage.evolved_job_id != 0);
  ro::render::ActorLook player{.player = true, .job = kLordKnightJob, .sex = 0, .hair = kHair};
  player.headgears = {kHeadgear, 0, 0};
  player.robe = kRobe;
  player.shield = kShield;
  player.body_style = kCostume;
  player.body_palette = kClothes;
  player.cash_mount = true;

  const ro::render::ActorLook hero = moonclient::HeroLook(player, mage, moonclient::HeroBody{});
  CHECK(hero.player);
  CHECK(hero.job == mage.job_id);
  CHECK(hero.weapon == mage.weapon_item_id);
  CHECK(hero.sex == player.sex);
  CHECK(hero.hair == kHair);
  CHECK(hero.headgears == player.headgears);
  CHECK(hero.robe == kRobe);
  CHECK(hero.body_palette == kClothes);
  CHECK(hero.shield == 0);
  CHECK(hero.body_style == 0);
  CHECK_FALSE(hero.cash_mount);

  CHECK(moonclient::HeroLook(player, mage, {.job = mage.evolved_job_id, .evolved = true}).job ==
        mage.evolved_job_id);
  // Le corps du mode et celui de l'évolution ont leur sprite de joueur.
  CHECK(ro::render::BodySpritePath(ShippedCatalog(), hero).has_value());
}

TEST_CASE("Apparences du mode : une arme par corps, et le geste à son rang", "[app][survivors]") {
  const ro::render::ActorLook player{.player = true, .sex = 0, .hair = kHair};
  const ro::sim::SurvivorsRules& rules = ro::sim::ShippedSurvivorsRules();
  const auto class_of = [&rules](std::string_view key) -> const ro::sim::ClassRule& {
    const auto found = std::ranges::find_if(
        rules.classes, [key](const ro::sim::ClassRule& rule) { return key == rule.key; });
    REQUIRE(found != rules.classes.end());
    return *found;
  };
  // Un corps autre que celui du départ, ou le même une fois évolué.
  const auto rank_of = [&](const ro::sim::ClassRule& hero_class, std::uint16_t job, bool evolved) {
    return moonclient::HeroAttackAnimation(
        ShippedCatalog(), hero_class,
        moonclient::HeroLook(player, hero_class, {.job = job, .evolved = evolved}));
  };
  const auto weapon_of = [&](const ro::sim::ClassRule& hero_class, std::uint16_t job,
                             bool evolved) {
    return moonclient::HeroLook(player, hero_class, {.job = job, .evolved = evolved}).weapon;
  };
  constexpr std::uint32_t kKnife = 1201;
  constexpr std::uint32_t kJur = 1250;
  const ro::sim::ClassRule& thief = class_of("thief");
  // L'ARME SUIT L'ÉVOLUTION, et non le corps : l'Assassin part à la dague.
  CHECK(weapon_of(thief, thief.job_id, false) == kKnife);
  CHECK(weapon_of(thief, thief.evolved_job_id, true) == kJur);
  CHECK(weapon_of(thief, thief.reborn_job_id, true) == kJur);
  // Le katar n'a d'images qu'au rang que le jeu lui donne, dague au rang ordinaire.
  CHECK(rank_of(thief, thief.job_id, false) == ro::render::kAnimationAttack2);
  CHECK(rank_of(thief, thief.evolved_job_id, true) == ro::render::kAnimationAttack3);
  CHECK(rank_of(thief, thief.reborn_job_id, true) == ro::render::kAnimationAttack3);

  // Le Chevalier part à l'épée et prend la lance : la lance a le rang que le
  // jeu lui donne, monté ou non.
  constexpr std::uint32_t kSword = 1101;
  constexpr std::uint32_t kJavelin = 1401;
  const ro::sim::ClassRule& knight = class_of("knight");
  CHECK(weapon_of(knight, knight.job_id, false) == kSword);
  CHECK(rank_of(knight, knight.job_id, false) == ro::render::kAnimationAttack2);
  for (const std::uint16_t job : {knight.evolved_job_id, knight.mounted_job_id,
                                  knight.reborn_job_id, knight.reborn_mounted_job_id}) {
    INFO(job);
    CHECK(weapon_of(knight, job, true) == kJavelin);
    CHECK(rank_of(knight, job, true) == ro::render::kAnimationAttack3);
  }

  // Le Magicien garde son arme et son rang, sur tous ses corps.
  const ro::sim::ClassRule& mage = class_of("mage");
  CHECK(mage.evolved_weapon_item_id == 0);
  for (const std::uint16_t job : {mage.job_id, mage.evolved_job_id, mage.reborn_job_id}) {
    if (job == 0) continue;
    CHECK(weapon_of(mage, job, true) == mage.weapon_item_id);
    CHECK(rank_of(mage, job, true) == ro::render::kAnimationAttack2);
  }
}

TEST_CASE("Apparences du mode : le corps de la rencontre est celui du départ de la classe",
          "[app][survivors]") {
  const auto thief = std::ranges::find_if(
      ro::sim::ShippedSurvivorsRules().classes,
      [](const ro::sim::ClassRule& rule) { return std::string_view(rule.key) == "thief"; });
  REQUIRE(thief != ro::sim::ShippedSurvivorsRules().classes.end());
  const ro::render::ActorLook body = moonclient::MeetingBodyLook(*thief);
  CHECK(body.player);
  CHECK(body.job == thief->job_id);
  CHECK(body.weapon == thief->weapon_item_id);
  CHECK(ro::render::BodySpritePath(ShippedCatalog(), body).has_value());
}

TEST_CASE("Apparences du mode : chaque corps qu'une classe montre a son sprite, monté compris",
          "[app][survivors]") {
  const ro::render::ActorLook player{.player = true, .sex = 0, .hair = kHair};
  for (const ro::sim::ClassRule& hero_class : ro::sim::ShippedSurvivorsRules().classes) {
    INFO(hero_class.key);
    for (const std::uint16_t job :
         {hero_class.job_id, hero_class.evolved_job_id, hero_class.mounted_job_id,
          hero_class.reborn_job_id, hero_class.reborn_mounted_job_id}) {
      if (job == 0) continue;
      INFO(job);
      const ro::render::ActorLook hero = moonclient::HeroLook(player, hero_class, {.job = job});
      CHECK(hero.job == job);
      CHECK(ro::render::BodySpritePath(ShippedCatalog(), hero).has_value());
    }
  }
}
