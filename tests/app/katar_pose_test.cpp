// L'ARME D'UN ASSASSIN PENDANT SES COMPÉTENCES : l'état d'attaque que posent
// l'emploi et la reprise d'un enchaînement se résout par l'arme tenue, et c'est
// le seul rang où le katar a des images.

#include <cstddef>
#include <cstdint>
#include <optional>
#include <string>
#include <utility>
#include <vector>

#include <catch2/catch_test_macros.hpp>

#include "ro/render/actor_sprite.h"
#include "ro/render/actor_wardrobe.h"
#include "ro/render/floating_number.h"
#include "ro/render/skill_hit_extra.h"
#include "ro/render/skill_impl.h"
#include "ro/render/sprite_catalog.h"
#include "ro/render/sprite_frame.h"
#include "ro/render/strike_play.h"
#include "ro/sim/actor.h"
#include "ro/vfs/vfs.h"

#include "support/actor_client.h"
#include "support/grf_builder.h"
#include "support/sprite_builder.h"
#include "support/temp_directory.h"
#include "world/actor_view.h"

using ro::render::ActorPart;
using ro::render::kAnimationAttack2;
using ro::render::kAnimationAttack3;
using ro::render::SpriteLayout;

namespace {

constexpr std::uint32_t kAssassinJob = 12;
constexpr std::uint8_t kMale = 1;
constexpr std::uint16_t kHairStyle = 1;
// Le katar : sa CLASSE, et un OBJET de cette classe — ce que le serveur envoie.
constexpr std::uint32_t kKatarClass = 16;
constexpr std::uint32_t kKatarItem = 1250;
constexpr std::uint16_t kSonicBlow = 136;
constexpr std::uint16_t kMeteorAssault = 406;
constexpr std::uint16_t kSonicBlowHits = 8;
constexpr std::uint32_t kAssassinId = 70;
constexpr std::uint32_t kStartMs = 5'000;

constexpr std::size_t kRankCount = 13;
constexpr std::size_t kFramesPerRank = 2;
// Les rangs où le katar a des images, comme dans le fichier de l'installation :
// la garde et la troisième attaque, rien d'autre.
constexpr std::size_t kReadyRank = 4;

// Un .act de treize rangs, deux images par direction ; `drawn` dit quels rangs
// portent un calque — les autres sont des images vides.
template <typename Drawn>
std::vector<std::uint8_t> RankedAnimationBytes(Drawn drawn) {
  ro::test_support::ActBuildOptions options;
  for (std::size_t rank = 0; rank < kRankCount; ++rank) {
    for (std::size_t direction = 0; direction < ro::render::kDirectionCount; ++direction) {
      ro::test_support::ActActionSpec spec;
      for (std::size_t frame = 0; frame < kFramesPerRank; ++frame) {
        ro::test_support::ActFrameSpec frame_spec;
        if (drawn(rank)) {
          ro::test_support::ActLayerSpec layer;
          layer.frame_index = static_cast<std::int32_t>(frame);
          frame_spec.layers.push_back(layer);
        }
        frame_spec.anchors.push_back(ro::test_support::ActAnchorSpec{});
        spec.frames.push_back(std::move(frame_spec));
      }
      options.actions.push_back(std::move(spec));
      options.speeds.push_back(ro::test_support::kFrameSpeed);
    }
  }
  return ro::test_support::BuildAct(options);
}

ro::render::ActorLook AssassinLook() {
  ro::render::ActorLook look;
  look.player = true;
  look.job = kAssassinJob;
  look.sex = kMale;
  look.hair = kHairStyle;
  look.weapon = kKatarItem;
  return look;
}

void WriteTables(const ro::test_support::TempDirectory& client) {
  client.WriteText("data/yaml_files/class_sprites.yml", "version: 1\nclasses:\n");
  client.WriteText("data/yaml_files/weapon_sprites.yml",
                   "version: 1\nweapons:\n  16: {sprite: \"_katar\"}\n");
  // La table des objets : ce katar est de la classe seize.
  client.WriteText("data/yaml_files/item_info.yml",
                   "version: 1\nitems: {1250: {name: \"Jur\", view: 16}}\n");
}

// UN CLIENT OÙ L'ASSASSIN A SON KATAR : le corps et la tête ont des images à
// tous les rangs, le katar aux seuls rangs de la garde et de la troisième
// attaque. Les chemins sont ceux que le catalogue produit lui-même.
struct KatarClient {
  KatarClient() {
    WriteTables(client);
    client.WriteFile("sprites.grf",
                     ro::test_support::BuildGrf(std::vector<ro::test_support::GrfFileSpec>{}));
    std::vector<ro::render::ActorPieceFiles> pieces;
    {
      auto probe = ro::vfs::Vfs::Mount({.root = client.path(), .archives = {"sprites.grf"}});
      REQUIRE(probe.has_value());
      auto catalog = ro::render::LoadSpriteCatalog(*probe);
      REQUIRE(catalog.has_value());
      pieces = ro::render::ActorFiles(*catalog, AssassinLook());
    }
    std::vector<ro::test_support::GrfFileSpec> files;
    bool weapon_found = false;
    for (const ro::render::ActorPieceFiles& piece : pieces) {
      if (piece.part == ActorPart::kWeaponTrail || piece.candidates.empty()) {
        continue;
      }
      const bool weapon = piece.part == ActorPart::kWeapon;
      weapon_found = weapon_found || weapon;
      const std::string& base = piece.candidates.front();
      files.push_back(ro::test_support::Entry(base + ".spr", ro::test_support::SpriteBytes()));
      files.push_back(ro::test_support::Entry(
          base + ".act", RankedAnimationBytes([weapon](std::size_t rank) {
            return !weapon || rank == kReadyRank || rank == kAnimationAttack3;
          })));
    }
    REQUIRE(weapon_found);
    files.push_back(
        ro::test_support::Entry("data\\sprite\\shadow.spr", ro::test_support::SpriteBytes()));
    files.push_back(
        ro::test_support::Entry("data\\sprite\\shadow.act", ro::test_support::SingleActionBytes()));
    client.WriteFile("sprites.grf", ro::test_support::BuildGrf(files));
    auto mounted = ro::vfs::Vfs::Mount({.root = client.path(), .archives = {"sprites.grf"}});
    REQUIRE(mounted.has_value());
    vfs.emplace(std::move(*mounted));
    auto catalog = ro::render::LoadSpriteCatalog(*vfs);
    REQUIRE(catalog.has_value());
    wardrobe.emplace(*vfs, std::move(*catalog));
  }

  ro::test_support::TempDirectory client;
  std::optional<ro::vfs::Vfs> vfs;
  std::optional<ro::render::ActorWardrobe> wardrobe;
};

ro::sim::Actor Assassin() {
  ro::sim::Actor actor;
  actor.id = kAssassinId;
  actor.kind = ro::sim::ActorKind::kPlayer;
  actor.look.job = kAssassinJob;
  actor.look.sex = kMale;
  actor.look.head = kHairStyle;
  actor.look.weapon = kKatarItem;
  actor.position = ro::sim::CellPosition{20, 20, 0};
  return actor;
}

// Vrai quand l'image de ce rang montre un calque de l'arme.
bool ShowsWeapon(ro::render::ActorOutfit& outfit, std::size_t animation) {
  const ro::render::ComposedActor& composed = outfit.Composed(animation, 0, 0.0F, false);
  for (std::size_t index = 0; index < composed.quads.size(); ++index) {
    if (ro::render::PartOfQuad(composed, index) == ActorPart::kWeapon) {
      return true;
    }
  }
  return false;
}

}  // namespace

TEST_CASE("Katar : l'état d'attaque posé prend le rang que l'arme décide, pas celui de la table",
          "[app][posture][weapon]") {
  // Un catalogue vide : l'arme y est donnée par sa classe.
  const ro::render::SpriteCatalog catalog;
  ro::render::ActorLook look = AssassinLook();
  look.weapon = kKatarClass;
  // La table donne le dix à l'attaque ; l'assassin au katar frappe au onze.
  CHECK(ro::render::ActionAnimationFor(ro::render::kStrikeAttackState, SpriteLayout::kPlayer,
                                       kAssassinJob) == kAnimationAttack2);
  CHECK(ro::render::PosedActionAnimation(catalog, look, ro::render::kStrikeAttackState, 0) ==
        kAnimationAttack3);
  CHECK(ro::render::PosedActionAnimation(catalog, look, ro::render::kStrikeAttackVariantState, 0) ==
        kAnimationAttack3);
  // Sans katar, la dague seule : le dix, là où sa lame a ses images.
  ro::render::ActorLook bare = look;
  bare.weapon = 1;
  CHECK(ro::render::PosedActionAnimation(catalog, bare, ro::render::kStrikeAttackState, 0) ==
        kAnimationAttack2);
  // Tout autre état garde le rang de la table ; une créature n'en a aucun.
  CHECK(ro::render::PosedActionAnimation(catalog, look, ro::render::kStrikeUseState, 0) ==
        ro::render::ActionAnimationFor(ro::render::kStrikeUseState, SpriteLayout::kPlayer,
                                       kAssassinJob));
  ro::render::ActorLook creature = look;
  creature.player = false;
  CHECK_FALSE(ro::render::PosedActionAnimation(catalog, creature, ro::render::kStrikeAttackState, 0)
                  .has_value());
}

TEST_CASE("Katar : Meteor Assault employé garde le calque de l'arme", "[app][posture][weapon]") {
  KatarClient katar;
  ro::render::ActorWardrobe& wardrobe = *katar.wardrobe;
  ro::render::ActorOutfit* outfit = wardrobe.OutfitFor(AssassinLook());
  REQUIRE(outfit != nullptr);
  ro::sim::Actor assassin = Assassin();
  assassin.skill_use = ro::sim::SkillUse{kMeteorAssault, kStartMs};
  const std::uint32_t at_ms = kStartMs + 1;
  const std::optional<moonclient::PlayedMotion> played = moonclient::SkillUsePostureAt(
      &wardrobe.catalog(), assassin, *outfit, SpriteLayout::kPlayer, ro::render::Motion::kIdle,
      moonclient::MotionEvents{std::nullopt, {}, at_ms}, at_ms);
  REQUIRE(played.has_value());
  CHECK(played->animation == kAnimationAttack3);
  CHECK(ShowsWeapon(*outfit, played->animation));
  // Le rang de la table n'en montrerait aucun : c'est ce qu'on voyait.
  CHECK_FALSE(ShowsWeapon(*outfit, kAnimationAttack2));
}

TEST_CASE("Katar : chaque reprise de Sonic Blow garde le calque de l'arme",
          "[app][posture][weapon]") {
  // Le client de référence renvoie Sonic Blow sur le chemin de Chain Crush : à
  // chaque frappe après la première, le lanceur repose l'état d'attaque.
  REQUIRE(ro::render::SwingReplaysOf(kSonicBlow, ro::render::kMultipleBlows, kSonicBlowHits) ==
          kSonicBlowHits - 1);
  KatarClient katar;
  ro::render::ActorWardrobe& wardrobe = *katar.wardrobe;
  ro::render::ActorOutfit* outfit = wardrobe.OutfitFor(AssassinLook());
  REQUIRE(outfit != nullptr);
  // C'est cet état que la reprise pose, par la même règle que l'emploi.
  const std::optional<std::size_t> replayed =
      moonclient::PosedAnimationOf(&wardrobe.catalog(), Assassin(), ro::render::kStrikeAttackState,
                                   SpriteLayout::kPlayer, kStartMs);
  REQUIRE(replayed.has_value());
  CHECK(*replayed == kAnimationAttack3);
  CHECK(ShowsWeapon(*outfit, *replayed));
  // La table seule — ce que la reprise jouait — laissait l'assassin sans katar.
  const std::optional<std::size_t> table_rank = moonclient::ActionAnimationOf(
      Assassin(), ro::render::kStrikeAttackState, SpriteLayout::kPlayer);
  REQUIRE(table_rank.has_value());
  CHECK_FALSE(ShowsWeapon(*outfit, *table_rank));
  // Sans catalogue, rien ne sait quelle arme il tient : la table, faute de mieux.
  CHECK(moonclient::PosedAnimationOf(nullptr, Assassin(), ro::render::kStrikeAttackState,
                                     SpriteLayout::kPlayer, kStartMs) == table_rank);
}
