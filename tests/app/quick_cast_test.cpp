// Quick cast : le sort qui part dès qu'il s'arme, sa relance tant que la touche
// est tenue, le cercle retiré au relâchement, et la répétition d'une case
// d'item — avec le plancher qui sépare le staff d'un joueur. Rien de tout cela
// ne se voit hors du jeu autrement qu'ici.

#include "world/quick_cast.h"

#include <cstddef>
#include <cstdint>
#include <optional>
#include <set>
#include <vector>

#include <catch2/catch_test_macros.hpp>

#include "ro/sim/skill_book.h"
#include "ro/ui/quick_cast_look.h"

using moonclient::QuickCast;
using moonclient::QuickCastAim;
using moonclient::QuickCastHands;
using ro::sim::CellPosition;
using ro::sim::KnownSkill;
using ro::ui::QuickCastLook;

namespace {

constexpr std::uint32_t kKey = 30;
constexpr std::uint32_t kOtherKey = 31;
constexpr std::uint32_t kWolf = 2000123;
constexpr std::uint32_t kHudWolf = 2000456;
constexpr std::uint32_t kStart = 10000;
constexpr std::size_t kSlot = 3;
constexpr std::uint32_t kBlueBox = 603;

[[nodiscard]] KnownSkill Bolt() {
  KnownSkill skill;
  skill.id = 14;
  skill.level = 10;
  skill.targeting = ro::sim::kSkillOnEnemy;
  return skill;
}

[[nodiscard]] KnownSkill StormGust() {
  KnownSkill skill;
  skill.id = 89;
  skill.level = 10;
  skill.targeting = ro::sim::kSkillOnGround;
  return skill;
}

[[nodiscard]] KnownSkill Heal() {
  KnownSkill skill;
  skill.id = 28;
  skill.level = 10;
  skill.targeting = ro::sim::kSkillOnFriend;
  return skill;
}

struct Shot {
  std::uint16_t skill_id = 0;
  std::uint32_t target_id = 0;
  std::optional<CellPosition> cell;
};

// Le jeu, réduit à ce que Quick cast lui demande.
class FakeHands final : public QuickCastHands {
 public:
  [[nodiscard]] bool InPlay() const override { return in_play; }
  [[nodiscard]] bool HasFocus() const override { return focus; }
  [[nodiscard]] bool Typing() const override { return typing; }
  [[nodiscard]] bool Staff() const override { return staff; }
  [[nodiscard]] bool KeyHeld(std::uint32_t key) const override { return held.contains(key); }

  [[nodiscard]] std::optional<KnownSkill> Armed() const override { return armed; }
  void Disarm() override { armed.reset(); }
  [[nodiscard]] bool CoolingDown(const KnownSkill& /*skill*/) const override {
    return cooling_down;
  }
  [[nodiscard]] bool Casting(const KnownSkill& skill) const override {
    return casting && skill.id != free_of_casting;
  }

  [[nodiscard]] bool MouseOnWorld() const override { return mouse_on_world; }
  [[nodiscard]] std::optional<CellPosition> GroundUnderMouse() const override { return ground; }
  [[nodiscard]] std::uint32_t ActorUnderMouse(QuickCastAim /*aim*/) const override {
    return under_mouse;
  }
  [[nodiscard]] bool HudCastArmed() const override { return hud_armed; }
  [[nodiscard]] std::uint32_t HudTarget(QuickCastAim aim) const override {
    return hud_armed && aim != QuickCastAim::kGround ? hud_target : 0;
  }
  [[nodiscard]] bool TileCastArmed() const override { return tile_armed; }
  [[nodiscard]] std::uint32_t TileTarget() const override { return tile_armed ? tile_target : 0; }

  bool CastOnGround(const KnownSkill& skill, CellPosition where) override {
    shots.push_back(Shot{.skill_id = skill.id, .cell = where});
    return true;
  }
  bool CastOnActor(const KnownSkill& skill, std::uint32_t id) override {
    shots.push_back(Shot{.skill_id = skill.id, .target_id = id});
    return true;
  }
  bool RepeatItem(std::size_t slot, std::uint32_t item_id) override {
    if (slot != kSlot || item_id != kBlueBox || items_left == 0) {
      return false;
    }
    --items_left;
    ++items_used;
    return true;
  }

  // Une touche enfoncée arme `skill` : la frappe, puis l'armement.
  void PressAndArm(QuickCast& quick, std::uint32_t key, const KnownSkill& skill,
                   std::uint32_t now_ms) {
    held.insert(key);
    quick.NotePress(key, now_ms);
    armed = skill;
    quick.OnEnterTargeting(*this, now_ms);
  }

  bool in_play = true;
  bool focus = true;
  bool typing = false;
  bool staff = false;
  std::set<std::uint32_t> held;
  std::optional<KnownSkill> armed;
  bool cooling_down = false;
  bool casting = false;
  // Le skill de celui qui lance sans dépendre de l'incantation du personnage.
  std::uint16_t free_of_casting = 0;
  bool mouse_on_world = true;
  std::optional<CellPosition> ground = CellPosition{.x = 100, .y = 120};
  std::uint32_t under_mouse = 0;
  bool hud_armed = false;
  std::uint32_t hud_target = 0;
  bool tile_armed = false;
  std::uint32_t tile_target = 0;
  std::vector<Shot> shots;
  int items_left = 0;
  int items_used = 0;
};

}  // namespace

TEST_CASE("Quick cast : le genre de visée d'un sort", "[app][quickcast]") {
  CHECK(moonclient::QuickCastAimOf(StormGust()) == QuickCastAim::kGround);
  CHECK(moonclient::QuickCastAimOf(Bolt()) == QuickCastAim::kOffensive);
  CHECK(moonclient::QuickCastAimOf(Heal()) == QuickCastAim::kSupport);
}

TEST_CASE("Quick cast : ce qui vise un piège posé se désigne au clic, comme dans le jeu",
          "[app][quickcast]") {
  KnownSkill remove_trap;
  remove_trap.id = 124;
  remove_trap.level = 1;
  remove_trap.targeting = ro::sim::kSkillTrap;
  CHECK(moonclient::QuickCastAimOf(remove_trap) == QuickCastAim::kNative);

  QuickCastLook look;
  look.ground = true;
  look.target = true;
  QuickCast quick(look);
  FakeHands hands;
  hands.PressAndArm(quick, kKey, remove_trap, kStart);
  CHECK(hands.shots.empty());
  CHECK(hands.armed.has_value());
  // Ce n'est pas un cercle de Quick cast : le relâchement ne le retire pas.
  CHECK(quick.arm_key() == 0);
  CHECK(quick.repeat_key() == 0);
}

TEST_CASE("Quick cast : éteint, un sort armé attend le clic", "[app][quickcast]") {
  const QuickCastLook look;
  QuickCast quick(look);
  FakeHands hands;
  hands.under_mouse = kWolf;
  hands.PressAndArm(quick, kKey, Bolt(), kStart);
  CHECK(hands.shots.empty());
  CHECK(hands.armed.has_value());
  CHECK(quick.arm_key() == 0);
}

TEST_CASE("Quick cast : un sort de zone part sur la case sous la souris", "[app][quickcast]") {
  QuickCastLook look;
  look.ground = true;
  QuickCast quick(look);
  FakeHands hands;
  hands.PressAndArm(quick, kKey, StormGust(), kStart);
  REQUIRE(hands.shots.size() == 1);
  REQUIRE(hands.shots[0].cell.has_value());
  CHECK(hands.shots[0].cell->x == 100);
  CHECK_FALSE(hands.armed.has_value());
}

TEST_CASE("Quick cast : sans le monde sous la souris, le sort de zone reste armé",
          "[app][quickcast]") {
  QuickCastLook look;
  look.ground = true;
  QuickCast quick(look);
  FakeHands hands;
  hands.mouse_on_world = false;
  hands.PressAndArm(quick, kKey, StormGust(), kStart);
  CHECK(hands.shots.empty());
  CHECK(hands.armed.has_value());
  // Et la touche en répond : relâchée, le cercle part avec elle.
  CHECK(quick.arm_key() == kKey);
  hands.held.clear();
  quick.UpdateDisarm(hands);
  CHECK_FALSE(hands.armed.has_value());
}

TEST_CASE("Quick cast : la souris passe avant la cible du mode Ciblage", "[app][quickcast]") {
  QuickCastLook look;
  look.target = true;
  QuickCast quick(look);
  FakeHands hands;
  hands.hud_armed = true;
  hands.hud_target = kHudWolf;
  hands.under_mouse = kWolf;
  hands.PressAndArm(quick, kKey, Bolt(), kStart);
  REQUIRE(hands.shots.size() == 1);
  CHECK(hands.shots[0].target_id == kWolf);
}

TEST_CASE("Quick cast : la cible du mode Ciblage répond là où la souris ne dit rien",
          "[app][quickcast]") {
  QuickCastLook look;
  look.target = true;
  QuickCast quick(look);
  FakeHands hands;
  hands.hud_armed = true;
  hands.hud_target = kHudWolf;

  SECTION("rien sous le curseur") { hands.PressAndArm(quick, kKey, Bolt(), kStart); }
  SECTION("le curseur sur une fenêtre") {
    hands.under_mouse = kWolf;
    hands.mouse_on_world = false;
    hands.PressAndArm(quick, kKey, Bolt(), kStart);
  }
  SECTION("la visée à la souris éteinte") {
    look.target = false;
    hands.under_mouse = kWolf;
    hands.PressAndArm(quick, kKey, Bolt(), kStart);
  }
  REQUIRE(hands.shots.size() == 1);
  CHECK(hands.shots[0].target_id == kHudWolf);
}

TEST_CASE("Quick cast : la touche tenue relance à la cadence", "[app][quickcast]") {
  QuickCastLook look;
  look.target = true;
  look.repeat_ms = 200;
  QuickCast quick(look);
  FakeHands hands;
  hands.under_mouse = kWolf;
  hands.PressAndArm(quick, kKey, Bolt(), kStart);
  REQUIRE(hands.shots.size() == 1);
  CHECK(quick.repeat_key() == kKey);

  quick.Update(hands, kStart + 199);
  CHECK(hands.shots.size() == 1);
  quick.Update(hands, kStart + 200);
  CHECK(hands.shots.size() == 2);

  // Une incantation en cours retient la relance, sans l'éteindre.
  hands.casting = true;
  quick.Update(hands, kStart + 400);
  CHECK(hands.shots.size() == 2);
  hands.casting = false;
  // La visée se réévalue : le curseur est passé sur un autre monstre.
  hands.under_mouse = kHudWolf;
  quick.Update(hands, kStart + 400);
  REQUIRE(hands.shots.size() == 3);
  CHECK(hands.shots[2].target_id == kHudWolf);

  // Relâchée, la relance s'éteint.
  hands.held.clear();
  quick.Update(hands, kStart + 1000);
  CHECK(quick.repeat_key() == 0);
  CHECK(hands.shots.size() == 3);
}

TEST_CASE("Quick cast : l'incantation du personnage ne retient pas le skill d'un compagnon",
          "[app][quickcast]") {
  QuickCastLook look;
  look.target = true;
  QuickCast quick(look);
  FakeHands hands;
  hands.under_mouse = kWolf;
  hands.PressAndArm(quick, kKey, Bolt(), kStart);
  REQUIRE(hands.shots.size() == 1);
  hands.casting = true;
  hands.free_of_casting = Bolt().id;
  quick.Update(hands, kStart + 200);
  CHECK(hands.shots.size() == 2);
}

TEST_CASE("Quick cast : un lancement refusé sur l'instant aboutit sans relâcher",
          "[app][quickcast]") {
  QuickCastLook look;
  look.target = true;
  QuickCast quick(look);
  FakeHands hands;
  hands.under_mouse = kWolf;
  hands.cooling_down = true;
  hands.PressAndArm(quick, kKey, Bolt(), kStart);
  CHECK(hands.shots.empty());
  CHECK(quick.repeat_key() == kKey);
  hands.cooling_down = false;
  quick.Update(hands, kStart + 16);
  CHECK(hands.shots.size() == 1);
}

TEST_CASE("Quick cast : un armement au clic ne se relance pas et garde son cercle",
          "[app][quickcast]") {
  QuickCastLook look;
  look.target = true;
  QuickCast quick(look);
  FakeHands hands;
  // Pas de frappe : c'est une case cliquée. Rien sous le curseur.
  hands.armed = Bolt();
  quick.OnEnterTargeting(hands, kStart);
  CHECK(hands.shots.empty());
  CHECK(quick.repeat_key() == 0);
  CHECK(quick.arm_key() == 0);
  quick.UpdateDisarm(hands);
  CHECK(hands.armed.has_value());
}

TEST_CASE("Quick cast : une frappe trop ancienne n'appartient plus à l'armement",
          "[app][quickcast]") {
  QuickCastLook look;
  look.target = true;
  QuickCast quick(look);
  FakeHands hands;
  hands.held.insert(kKey);
  quick.NotePress(kKey, kStart);
  hands.armed = Bolt();
  quick.OnEnterTargeting(hands, kStart + moonclient::kQuickCastPendingKeyLifetimeMs + 1);
  CHECK(quick.arm_key() == 0);
  CHECK(quick.repeat_key() == 0);
}

TEST_CASE("Quick cast : le réarmement d'une touche tenue reste sous sa garde", "[app][quickcast]") {
  QuickCastLook look;
  look.target = true;
  look.repeat_ms = 200;
  QuickCast quick(look);
  FakeHands hands;
  hands.under_mouse = kWolf;
  hands.PressAndArm(quick, kKey, Bolt(), kStart);
  REQUIRE(hands.shots.size() == 1);
  // La répétition du clavier réarme, sans frappe fraîche, trop tôt pour partir.
  hands.armed = Bolt();
  quick.OnEnterTargeting(hands, kStart + 50);
  CHECK(hands.shots.size() == 1);
  CHECK(quick.arm_key() == kKey);
  CHECK(quick.repeat_key() == kKey);
  // Relâchée : le cercle réarmé part avec elle.
  hands.held.clear();
  quick.UpdateDisarm(hands);
  CHECK_FALSE(hands.armed.has_value());
}

TEST_CASE("Quick cast : le relâchement ne retire pas un cercle d'un autre genre",
          "[app][quickcast]") {
  QuickCastLook look;
  look.ground = true;
  look.target = true;
  QuickCast quick(look);
  FakeHands hands;
  hands.mouse_on_world = false;
  hands.PressAndArm(quick, kKey, StormGust(), kStart);
  REQUIRE(quick.arm_key() == kKey);
  // Un sort ciblé est armé depuis, à la souris : ce cercle-là n'est pas le sien.
  hands.armed = Bolt();
  hands.held.clear();
  quick.UpdateDisarm(hands);
  CHECK(hands.armed.has_value());
}

TEST_CASE("Quick cast : un item se répète à la cadence, au-dessus du plancher du droit",
          "[app][quickcast]") {
  QuickCastLook look;
  look.item = true;
  look.item_repeat_ms = ro::ui::kQuickCastItemFloorStaffMs;
  QuickCast quick(look);
  FakeHands hands;
  hands.items_left = 10;
  hands.held.insert(kKey);
  quick.NotePress(kKey, kStart);
  quick.OnUseItemSlot(hands, kSlot, kBlueBox, kStart);
  REQUIRE(quick.item_key() == kKey);

  SECTION("un joueur ne descend pas sous cent millisecondes") {
    quick.UpdateItemRepeat(hands, kStart + ro::ui::kQuickCastItemFloorStaffMs);
    CHECK(hands.items_used == 0);
    quick.UpdateItemRepeat(hands, kStart + ro::ui::kQuickCastItemFloorPlayerMs);
    CHECK(hands.items_used == 1);
  }
  SECTION("le staff descend à vingt") {
    hands.staff = true;
    quick.UpdateItemRepeat(hands, kStart + ro::ui::kQuickCastItemFloorStaffMs);
    CHECK(hands.items_used == 1);
  }
}

TEST_CASE("Quick cast : la répétition d'item s'arrête d'elle-même", "[app][quickcast]") {
  QuickCastLook look;
  look.item = true;
  look.item_repeat_ms = ro::ui::kQuickCastItemFloorPlayerMs;
  QuickCast quick(look);
  FakeHands hands;
  hands.items_left = 1;
  hands.held.insert(kKey);
  quick.NotePress(kKey, kStart);
  quick.OnUseItemSlot(hands, kSlot, kBlueBox, kStart);
  std::uint32_t now_ms = kStart;

  SECTION("le dernier exemplaire parti") {
    now_ms += ro::ui::kQuickCastItemFloorPlayerMs;
    quick.UpdateItemRepeat(hands, now_ms);
    CHECK(hands.items_used == 1);
    now_ms += ro::ui::kQuickCastItemFloorPlayerMs;
    quick.UpdateItemRepeat(hands, now_ms);
    CHECK(quick.item_key() == 0);
  }
  SECTION("une frappe partie dans un champ de texte") {
    hands.typing = true;
    quick.UpdateItemRepeat(hands, now_ms + ro::ui::kQuickCastItemFloorPlayerMs);
    CHECK(quick.item_key() == 0);
    CHECK(hands.items_used == 0);
  }
  SECTION("le jeu passé au second plan") {
    hands.focus = false;
    quick.UpdateItemRepeat(hands, now_ms + ro::ui::kQuickCastItemFloorPlayerMs);
    CHECK(quick.item_key() == 0);
  }
}

TEST_CASE("Quick cast : l'item tenu survit au téléport sur la même carte", "[app][quickcast]") {
  QuickCastLook look;
  look.item = true;
  look.item_repeat_ms = ro::ui::kQuickCastItemFloorPlayerMs;
  QuickCast quick(look);
  FakeHands hands;
  hands.items_left = 10;
  hands.held.insert(kKey);
  quick.NotePress(kKey, kStart);
  quick.OnUseItemSlot(hands, kSlot, kBlueBox, kStart);
  REQUIRE(quick.item_key() == kKey);

  SECTION("la même carte : l'item repart") {
    quick.ForgetAim();
    CHECK(quick.item_key() == kKey);
    quick.UpdateItemRepeat(hands, kStart + ro::ui::kQuickCastItemFloorPlayerMs);
    CHECK(hands.items_used == 1);
  }
  SECTION("une autre carte : tout s'arrête") {
    quick.Forget();
    CHECK(quick.item_key() == 0);
    quick.UpdateItemRepeat(hands, kStart + ro::ui::kQuickCastItemFloorPlayerMs);
    CHECK(hands.items_used == 0);
  }
}

TEST_CASE("Quick cast : le téléport sur la même carte coupe la relance de sort",
          "[app][quickcast]") {
  QuickCastLook look;
  look.target = true;
  QuickCast quick(look);
  FakeHands hands;
  hands.under_mouse = kWolf;
  hands.PressAndArm(quick, kKey, Bolt(), kStart);
  REQUIRE(quick.repeat_key() == kKey);
  quick.ForgetAim();
  CHECK(quick.repeat_key() == 0);
  CHECK(quick.arm_key() == 0);
}

TEST_CASE("Quick cast : un item utilisé au clic ne se répète pas", "[app][quickcast]") {
  QuickCastLook look;
  look.item = true;
  QuickCast quick(look);
  FakeHands hands;
  hands.items_left = 10;
  quick.OnUseItemSlot(hands, kSlot, kBlueBox, kStart);
  CHECK(quick.item_key() == 0);
  // Et la répétition éteinte ne répète rien, même au clavier.
  look.item = false;
  hands.held.insert(kOtherKey);
  quick.NotePress(kOtherKey, kStart);
  quick.OnUseItemSlot(hands, kSlot, kBlueBox, kStart);
  CHECK(quick.item_key() == 0);
}

TEST_CASE("Quick cast : une image ne rejoue que ce que les comptes disent nouveau",
          "[app][quickcast]") {
  QuickCastLook look;
  look.target = true;
  look.item = true;
  QuickCast quick(look);
  FakeHands hands;
  hands.under_mouse = kWolf;
  hands.items_left = 10;

  // Un sort s'arme au clavier : le compte du grimoire a bougé.
  hands.held.insert(kKey);
  quick.NotePress(kKey, kStart);
  hands.armed = Bolt();
  quick.Advance(hands, moonclient::QuickCastSignals{.arms = 1}, kStart);
  CHECK(hands.shots.size() == 1);

  // Le même compte à l'image suivante : rien ne s'est armé depuis. Un sort armé
  // entre-temps sans que le grimoire l'ait compté ne part pas de lui-même.
  hands.armed = Bolt();
  hands.under_mouse = 0;
  quick.Advance(hands, moonclient::QuickCastSignals{.arms = 1}, kStart + 1);
  CHECK(hands.shots.size() == 1);

  // Une case d'item sert au clavier : la répétition s'arme.
  hands.held.insert(kOtherKey);
  quick.NotePress(kOtherKey, kStart + 2);
  quick.Advance(hands,
                moonclient::QuickCastSignals{
                    .arms = 1, .item_uses = 1, .item_slot = kSlot, .item_id = kBlueBox},
                kStart + 2);
  CHECK(quick.item_key() == kOtherKey);
}

TEST_CASE("Quick cast : la période d'item suit le droit", "[app][quickcast]") {
  CHECK(ro::ui::QuickCastItemPeriodMs(ro::ui::kQuickCastItemFloorStaffMs, /*staff=*/true) ==
        ro::ui::kQuickCastItemFloorStaffMs);
  CHECK(ro::ui::QuickCastItemPeriodMs(ro::ui::kQuickCastItemFloorStaffMs, /*staff=*/false) ==
        ro::ui::kQuickCastItemFloorPlayerMs);
  constexpr int kSlow = 400;
  CHECK(ro::ui::QuickCastItemPeriodMs(kSlow, /*staff=*/false) == kSlow);
}

TEST_CASE("Quick cast : la tuile survolée passe avant la souris pour un soutien",
          "[app][quickcast]") {
  constexpr std::uint32_t kMate = 2000789;
  QuickCastLook look;
  look.target = true;
  QuickCast quick(look);
  FakeHands hands;
  hands.under_mouse = kWolf;
  hands.tile_armed = true;
  hands.tile_target = kMate;
  hands.PressAndArm(quick, kKey, Heal(), kStart);
  REQUIRE(hands.shots.size() == 1);
  CHECK(hands.shots[0].target_id == kMate);
}

TEST_CASE("Quick cast : la tuile ne vise jamais pour un sort offensif", "[app][quickcast]") {
  constexpr std::uint32_t kMate = 2000789;
  QuickCastLook look;
  look.target = true;
  QuickCast quick(look);
  FakeHands hands;
  hands.tile_armed = true;
  hands.tile_target = kMate;
  hands.PressAndArm(quick, kKey, Bolt(), kStart);
  CHECK(hands.shots.empty());
  hands.armed.reset();
  hands.under_mouse = kWolf;
  hands.PressAndArm(quick, kOtherKey, Bolt(), kStart + 1000);
  REQUIRE(hands.shots.size() == 1);
  CHECK(hands.shots[0].target_id == kWolf);
}

TEST_CASE("Quick cast : la grille seule lance le soutien et laisse l'offensif au clic",
          "[app][quickcast]") {
  constexpr std::uint32_t kMate = 2000789;
  const QuickCastLook look;
  QuickCast quick(look);
  FakeHands hands;
  hands.tile_armed = true;
  hands.tile_target = kMate;
  hands.PressAndArm(quick, kKey, Heal(), kStart);
  REQUIRE(hands.shots.size() == 1);
  CHECK(hands.shots[0].target_id == kMate);

  hands.held.clear();
  quick.UpdateDisarm(hands);
  hands.PressAndArm(quick, kOtherKey, Bolt(), kStart + 1000);
  CHECK(hands.shots.size() == 1);
  CHECK(hands.armed.has_value());
  // Ce cercle n'est pas à Quick cast : le relâchement ne le retire pas.
  CHECK(quick.arm_key() == 0);
}
