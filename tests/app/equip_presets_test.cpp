// Les presets d'équipement : ce qu'un preset retient de ce qui est porté, et ce
// qu'il demande au serveur pour le remettre. Ces défauts-là se prennent en jeu
// pour des ratés du serveur.

#include "character/equip_presets.h"

#include <cstdint>
#include <optional>
#include <string>
#include <vector>

#include <SDL3/SDL_keycode.h>

#include <catch2/catch_test_macros.hpp>

#include "ro/sim/equip_location.h"
#include "ro/sim/inventory.h"

namespace {

using moonclient::Binding;
using moonclient::EquipPreset;
using moonclient::EquipPresetBook;
using moonclient::Hotkeys;
using moonclient::PlanPreset;
using moonclient::PlanUndress;
using moonclient::PresetFamily;
using moonclient::PresetKeyCapturable;
using moonclient::PresetKeyHolder;
using moonclient::PresetPiece;
using moonclient::PresetPlan;
using moonclient::PresetWear;
using moonclient::WornPresetPieces;
using ro::sim::Inventory;
using ro::sim::InventoryItem;

constexpr std::uint32_t kSword = 1101;
constexpr std::uint32_t kDagger = 1201;
constexpr std::uint32_t kBow = 1701;
constexpr std::uint32_t kOtherBow = 1702;
constexpr std::uint32_t kArmor = 2301;
constexpr std::uint32_t kShield = 2101;
constexpr std::uint32_t kClip = 2607;
constexpr std::uint32_t kRing = 2621;
constexpr std::uint32_t kHat = 2220;
constexpr std::uint32_t kMask = 2286;
constexpr std::uint32_t kCostumeHat = 19500;
constexpr std::uint32_t kCostumeCape = 20500;
constexpr std::uint32_t kArrow = 1750;
constexpr std::uint32_t kSilverArrow = 1751;
constexpr std::uint32_t kShadowArmor = 24001;
constexpr std::uint32_t kCard = 4001;
constexpr std::uint32_t kAbsent = 9999;

// Un accessoire se déclare des deux côtés, et le serveur choisit.
constexpr std::uint32_t kEitherAccessory =
    ro::sim::kEquipAccessoryLeft | ro::sim::kEquipAccessoryRight;
// Chez qui peut porter deux armes — le seul qui en porte deux —, le serveur
// annonce une dague sur les deux mains, comme une arme à deux mains.
constexpr std::uint32_t kDualWieldDagger = ro::sim::kEquipBothHands;
// Une coiffe d'apparat qui va sur l'un des trois étages, au choix du serveur.
constexpr std::uint32_t kAnyCostumeHead =
    ro::sim::kEquipCostumeHeadTop | ro::sim::kEquipCostumeHeadMid | ro::sim::kEquipCostumeHeadLow;
constexpr std::uint32_t kHydraCard = 4035;
constexpr std::uint32_t kMinorousCard = 4126;
constexpr std::uint32_t kShadowArmorPlace = 0x010000;

[[nodiscard]] InventoryItem Piece(std::uint16_t client_slot, std::uint32_t item_id,
                                  std::uint32_t wearable_at, std::uint32_t worn_at = 0,
                                  std::uint8_t refine_level = 0) {
  return InventoryItem{.client_slot = client_slot,
                       .item_id = item_id,
                       .wearable_at = wearable_at,
                       .worn_at = worn_at,
                       .refine_level = refine_level};
}

[[nodiscard]] PresetPiece Entry(std::uint32_t item_id, int refine_level = 0,
                                PresetFamily family = PresetFamily::kEquipment) {
  return PresetPiece{.item_id = item_id, .refine_level = refine_level, .family = family};
}

[[nodiscard]] EquipPreset PresetOf(std::vector<PresetPiece> pieces, bool with_costumes = false) {
  return EquipPreset{.name = "PvM", .pieces = std::move(pieces), .with_costumes = with_costumes};
}

[[nodiscard]] bool Asks(const PresetPlan& plan, std::uint16_t client_slot, std::uint32_t position) {
  for (const PresetWear& wear : plan.wear) {
    if (wear.client_slot == client_slot && wear.position == position) {
      return true;
    }
  }
  return false;
}

}  // namespace

// ── CE QUI SE RETIENT ────────────────────────────────────────────────────────

TEST_CASE("Presets : ce qui est porté se retient pièce par pièce, dans l'ordre des places",
          "[app][presets]") {
  Inventory bag;
  InventoryItem armor = Piece(5, kArmor, ro::sim::kEquipArmor, ro::sim::kEquipArmor, 4);
  armor.cards = {kCard, 0, 0, 0};
  armor.grade = 2;
  bag.Put(armor);
  bag.Put(Piece(3, kSword, ro::sim::kEquipWeapon, ro::sim::kEquipWeapon, 7));
  bag.Put(Piece(7, kHat, ro::sim::kEquipHeadTop, ro::sim::kEquipHeadTop));
  bag.Put(Piece(9, kCostumeHat, ro::sim::kEquipCostumeHeadTop, ro::sim::kEquipCostumeHeadTop));
  InventoryItem arrows = Piece(11, kArrow, ro::sim::kEquipAmmo, ro::sim::kEquipAmmo, 3);
  arrows.amount = 500;
  bag.Put(arrows);
  bag.Put(Piece(2, kDagger, ro::sim::kEquipWeapon));  // dans le sac, pas porté

  const std::vector<PresetPiece> kept = WornPresetPieces(bag);
  REQUIRE(kept.size() == 5);
  // L'arme (0x02) avant l'armure (0x10) avant la coiffe (0x100), puis le
  // costume, puis la munition.
  CHECK(kept[0].item_id == kSword);
  CHECK(kept[0].refine_level == 7);
  CHECK(kept[1].item_id == kArmor);
  CHECK(kept[1].grade == 2);
  CHECK(kept[1].cards[0] == kCard);
  CHECK(kept[2].item_id == kHat);
  CHECK(kept[3].item_id == kCostumeHat);
  CHECK(kept[3].family == PresetFamily::kCostume);
  CHECK(kept[4].item_id == kArrow);
  CHECK(kept[4].family == PresetFamily::kAmmo);
  // La munition ne se retient que par son objet.
  CHECK(kept[4].refine_level == 0);
  for (const PresetPiece& piece : kept) {
    CHECK_FALSE(piece.left_hand);
  }
}

TEST_CASE("Presets : une pièce à deux places ne se retient qu'une fois", "[app][presets]") {
  // Retenue deux fois, elle serait demandée deux fois, et le serveur refuse de
  // poser une pièce déjà posée.
  Inventory bag;
  bag.Put(Piece(3, kBow, ro::sim::kEquipBothHands, ro::sim::kEquipBothHands));
  const std::uint32_t top_and_mid = ro::sim::kEquipHeadTop | ro::sim::kEquipHeadMid;
  bag.Put(Piece(4, kMask, top_and_mid, top_and_mid));

  const std::vector<PresetPiece> kept = WornPresetPieces(bag);
  REQUIRE(kept.size() == 2);
  CHECK(kept[0].item_id == kBow);
  // Une arme à deux mains n'est pas une arme de la main gauche.
  CHECK_FALSE(kept[0].left_hand);
  CHECK(kept[1].item_id == kMask);
}

TEST_CASE("Presets : une arme de la main gauche se retient comme telle", "[app][presets]") {
  Inventory bag;
  bag.Put(Piece(3, kDagger, kDualWieldDagger, ro::sim::kEquipWeapon));
  bag.Put(Piece(4, kDagger, kDualWieldDagger, ro::sim::kEquipShield));
  const std::vector<PresetPiece> kept = WornPresetPieces(bag);
  REQUIRE(kept.size() == 2);
  CHECK_FALSE(kept[0].left_hand);
  CHECK(kept[1].left_hand);

  SECTION("un bouclier, lui, n'en est pas une") {
    Inventory guarded;
    guarded.Put(Piece(4, kShield, ro::sim::kEquipShield, ro::sim::kEquipShield));
    const std::vector<PresetPiece> shield = WornPresetPieces(guarded);
    REQUIRE(shield.size() == 1);
    CHECK_FALSE(shield[0].left_hand);
  }
}

TEST_CASE("Presets : l'équipement d'ombre ne se retient pas", "[app][presets]") {
  Inventory bag;
  bag.Put(Piece(3, kShadowArmor, kShadowArmorPlace, kShadowArmorPlace));
  CHECK(WornPresetPieces(bag).empty());
}

// ── CE QUI SE DEMANDE ────────────────────────────────────────────────────────

TEST_CASE("Presets : remettre ce qu'on porte déjà ne demande rien", "[app][presets]") {
  // Deux accessoires identiques : désignés tous deux par le premier du sac, le
  // second serait retiré au moment même où l'on remet la tenue qu'on porte.
  Inventory bag;
  bag.Put(Piece(3, kSword, ro::sim::kEquipWeapon, ro::sim::kEquipWeapon, 7));
  bag.Put(Piece(5, kClip, kEitherAccessory, ro::sim::kEquipAccessoryRight));
  bag.Put(Piece(6, kClip, kEitherAccessory, ro::sim::kEquipAccessoryLeft));
  bag.Put(Piece(8, kCostumeHat, ro::sim::kEquipCostumeHeadTop, ro::sim::kEquipCostumeHeadTop));
  bag.Put(Piece(9, kArrow, ro::sim::kEquipAmmo, ro::sim::kEquipAmmo));

  const PresetPlan plan = PlanPreset(PresetOf(WornPresetPieces(bag), true), bag);
  CHECK(plan.take_off.empty());
  CHECK(plan.wear.empty());
  CHECK(plan.missing == 0);
}

TEST_CASE("Presets : deux pièces identiques désignent deux objets", "[app][presets]") {
  Inventory bag;
  bag.Put(Piece(5, kClip, kEitherAccessory, ro::sim::kEquipAccessoryRight));
  bag.Put(Piece(6, kClip, kEitherAccessory));  // la jumelle, dans le sac

  const PresetPlan plan = PlanPreset(PresetOf({Entry(kClip), Entry(kClip)}), bag);
  CHECK(plan.take_off.empty());
  REQUIRE(plan.wear.size() == 1);
  CHECK(Asks(plan, 6, kEitherAccessory));
  CHECK(plan.missing == 0);
}

TEST_CASE("Presets : la pièce portée passe avant sa jumelle du sac", "[app][presets]") {
  // Les échanger ne changerait rien au personnage, et coûterait deux demandes.
  Inventory bag;
  bag.Put(Piece(3, kSword, ro::sim::kEquipWeapon, 0, 7));
  bag.Put(Piece(10, kSword, ro::sim::kEquipWeapon, ro::sim::kEquipWeapon, 7));

  const PresetPlan plan = PlanPreset(PresetOf({Entry(kSword, 7)}), bag);
  CHECK(plan.take_off.empty());
  CHECK(plan.wear.empty());
}

TEST_CASE("Presets : la pièce exacte passe avant une autre du même objet", "[app][presets]") {
  Inventory bag;
  bag.Put(Piece(3, kSword, ro::sim::kEquipWeapon, ro::sim::kEquipWeapon, 5));
  bag.Put(Piece(4, kSword, ro::sim::kEquipWeapon, 0, 7));

  const PresetPlan plan = PlanPreset(PresetOf({Entry(kSword, 7)}), bag);
  REQUIRE(plan.wear.size() == 1);
  CHECK(Asks(plan, 4, ro::sim::kEquipWeapon));
  CHECK(plan.missing == 0);

  SECTION("et faute d'exacte, celle qu'on porte déjà") {
    const PresetPlan fallback = PlanPreset(PresetOf({Entry(kSword, 9)}), bag);
    CHECK(fallback.wear.empty());
    CHECK(fallback.take_off.empty());
    CHECK(fallback.missing == 0);
  }
}

TEST_CASE("Presets : ce que le preset ne porte pas se retire", "[app][presets]") {
  Inventory bag;
  bag.Put(Piece(3, kArmor, ro::sim::kEquipArmor, ro::sim::kEquipArmor));
  bag.Put(Piece(4, kSword, ro::sim::kEquipWeapon, ro::sim::kEquipWeapon));

  const PresetPlan plan = PlanPreset(PresetOf({Entry(kSword)}), bag);
  REQUIRE(plan.take_off.size() == 1);
  CHECK(plan.take_off[0] == 3);
  CHECK(plan.wear.empty());
}

TEST_CASE("Presets : une pièce que le preset remplace ne se retire pas à part", "[app][presets]") {
  // Retiré à part, l'arc ferait tomber la munition, qui ne reviendrait pas : elle
  // était déjà en place.
  Inventory bag;
  bag.Put(Piece(3, kBow, ro::sim::kEquipBothHands, ro::sim::kEquipBothHands));
  bag.Put(Piece(4, kOtherBow, ro::sim::kEquipBothHands));
  bag.Put(Piece(5, kArrow, ro::sim::kEquipAmmo, ro::sim::kEquipAmmo));

  const PresetPlan plan =
      PlanPreset(PresetOf({Entry(kOtherBow), Entry(kArrow, 0, PresetFamily::kAmmo)}), bag);
  CHECK(plan.take_off.empty());
  REQUIRE(plan.wear.size() == 1);
  // Demandé dans la main droite : le serveur le pose quand même sur les deux.
  CHECK(Asks(plan, 4, ro::sim::kEquipWeapon));

  SECTION("mais un accessoire dont le serveur choisit le côté ne remplace rien d'avance") {
    // Il se pose sur la place libre : l'ancien laissé en place l'enverrait de
    // l'autre côté.
    Inventory hands;
    hands.Put(Piece(3, kClip, kEitherAccessory, ro::sim::kEquipAccessoryRight));
    hands.Put(Piece(4, kRing, kEitherAccessory));
    const PresetPlan ring = PlanPreset(PresetOf({Entry(kRing)}), hands);
    REQUIRE(ring.take_off.size() == 1);
    CHECK(ring.take_off[0] == 3);
    CHECK(Asks(ring, 4, kEitherAccessory));
  }
}

TEST_CASE("Presets : les costumes ne bougent que si le preset les pilote", "[app][presets]") {
  Inventory bag;
  bag.Put(Piece(3, kCostumeCape, ro::sim::kEquipCostumeGarment, ro::sim::kEquipCostumeGarment));
  bag.Put(Piece(4, kCostumeHat, ro::sim::kEquipCostumeHeadTop));
  const std::vector<PresetPiece> pieces = {Entry(kCostumeHat, 0, PresetFamily::kCostume),
                                           Entry(kAbsent, 0, PresetFamily::kCostume)};

  SECTION("décochée : ni retirés, ni remis, ni comptés manquants") {
    const PresetPlan plan = PlanPreset(PresetOf(pieces, false), bag);
    CHECK(plan.take_off.empty());
    CHECK(plan.wear.empty());
    CHECK(plan.missing == 0);
  }
  SECTION("cochée : l'apparence enregistrée, exactement") {
    const PresetPlan plan = PlanPreset(PresetOf(pieces, true), bag);
    REQUIRE(plan.take_off.size() == 1);
    CHECK(plan.take_off[0] == 3);
    CHECK(Asks(plan, 4, ro::sim::kEquipCostumeHeadTop));
    CHECK(plan.missing == 1);
  }
}

TEST_CASE("Presets : la munition ne se retire jamais, elle se remplace", "[app][presets]") {
  Inventory bag;
  bag.Put(Piece(3, kArrow, ro::sim::kEquipAmmo, ro::sim::kEquipAmmo));
  bag.Put(Piece(4, kSilverArrow, ro::sim::kEquipAmmo));

  CHECK(PlanPreset(PresetOf({}), bag).take_off.empty());
  const PresetPlan replaced =
      PlanPreset(PresetOf({Entry(kSilverArrow, 0, PresetFamily::kAmmo)}), bag);
  CHECK(replaced.take_off.empty());
  CHECK(Asks(replaced, 4, ro::sim::kEquipAmmo));
}

TEST_CASE("Presets : une arme de la main gauche se redemande à gauche", "[app][presets]") {
  Inventory bag;
  bag.Put(Piece(4, kDagger, kDualWieldDagger));
  PresetPiece left = Entry(kDagger);
  left.left_hand = true;
  CHECK(Asks(PlanPreset(PresetOf({left}), bag), 4, ro::sim::kEquipShield));
}

TEST_CASE("Presets : une dague se demande dans la main droite, pas dans l'une des deux",
          "[app][presets]") {
  // Demandée « dans l'une des deux », elle irait dans la main libre — la gauche,
  // la droite tenant encore la dague qu'elle remplace —, et il en resterait deux.
  Inventory bag;
  bag.Put(Piece(3, kDagger, kDualWieldDagger, ro::sim::kEquipWeapon));
  bag.Put(Piece(4, kSword, kDualWieldDagger));

  const PresetPlan plan = PlanPreset(PresetOf({Entry(kSword)}), bag);
  CHECK(plan.take_off.empty());
  REQUIRE(plan.wear.size() == 1);
  CHECK(Asks(plan, 4, ro::sim::kEquipWeapon));

  SECTION("et ce que la main gauche tient se retire, puisqu'elle n'est pas remplacée") {
    bag.Put(Piece(5, kShield, ro::sim::kEquipShield, ro::sim::kEquipShield));
    const PresetPlan shielded = PlanPreset(PresetOf({Entry(kSword)}), bag);
    CHECK(shielded.take_off == std::vector<std::uint16_t>{5});
    CHECK(Asks(shielded, 4, ro::sim::kEquipWeapon));
  }
}

TEST_CASE("Presets : une dague portée dans l'autre main se retire d'abord", "[app][presets]") {
  // Le preset veut A à droite et B à gauche ; B est à droite. Le serveur refuse
  // de poser une pièce déjà portée : B quitte la droite avant d'aller à gauche.
  Inventory bag;
  InventoryItem first = Piece(3, kDagger, kDualWieldDagger);
  first.cards = {kHydraCard, 0, 0, 0};
  bag.Put(first);
  bag.Put(Piece(4, kDagger, kDualWieldDagger, ro::sim::kEquipWeapon));
  PresetPiece right = Entry(kDagger);
  right.cards = {kHydraCard, 0, 0, 0};
  PresetPiece left = Entry(kDagger);
  left.left_hand = true;

  const PresetPlan plan = PlanPreset(PresetOf({right, left}), bag);
  CHECK(plan.take_off == std::vector<std::uint16_t>{4});
  REQUIRE(plan.wear.size() == 2);
  CHECK(plan.wear[0].client_slot == 3);
  CHECK(plan.wear[0].position == ro::sim::kEquipWeapon);
  CHECK(plan.wear[1].client_slot == 4);
  CHECK(plan.wear[1].position == ro::sim::kEquipShield);
}

TEST_CASE("Presets : deux dagues identiques rangées vont chacune dans sa main", "[app][presets]") {
  Inventory bag;
  bag.Put(Piece(3, kDagger, kDualWieldDagger));
  bag.Put(Piece(4, kDagger, kDualWieldDagger));
  PresetPiece left = Entry(kDagger);
  left.left_hand = true;

  const PresetPlan plan = PlanPreset(PresetOf({Entry(kDagger), left}), bag);
  CHECK(plan.missing == 0);
  CHECK(Asks(plan, 3, ro::sim::kEquipWeapon));
  CHECK(Asks(plan, 4, ro::sim::kEquipShield));
}

TEST_CASE("Presets : une pièce absente du sac se compte", "[app][presets]") {
  Inventory bag;
  const PresetPlan plan = PlanPreset(PresetOf({Entry(kAbsent), Entry(kSword)}), bag);
  CHECK(plan.missing == 2);
  CHECK(plan.wear.empty());
}

TEST_CASE("Presets : un preset relevé place par place ne se croit pas incomplet",
          "[app][presets]") {
  // Un preset peut porter une arme à deux mains deux fois, une par main.
  Inventory bag;
  bag.Put(Piece(3, kBow, ro::sim::kEquipBothHands, ro::sim::kEquipBothHands));
  PresetPiece second = Entry(kBow);
  second.left_hand = true;
  const PresetPlan plan = PlanPreset(PresetOf({Entry(kBow), second}), bag);
  CHECK(plan.missing == 0);
  CHECK(plan.wear.empty());
  CHECK(plan.take_off.empty());

  SECTION("et la jumelle du sac ne remplace pas l'arme qu'on porte") {
    // Sinon les arcs s'échangeraient à chaque appui : la seconde entrée prendrait
    // l'autre arc du sac, qui délogerait le premier.
    InventoryItem hydra = Piece(3, kBow, ro::sim::kEquipBothHands, ro::sim::kEquipBothHands);
    hydra.cards = {kHydraCard, 0, 0, 0};
    Inventory twins;
    twins.Put(hydra);
    InventoryItem minorous = Piece(4, kBow, ro::sim::kEquipBothHands);
    minorous.cards = {kMinorousCard, 0, 0, 0};
    twins.Put(minorous);
    InventoryItem copy = Piece(5, kBow, ro::sim::kEquipBothHands);
    copy.cards = {kHydraCard, 0, 0, 0};
    twins.Put(copy);
    PresetPiece first = Entry(kBow);
    first.cards = {kHydraCard, 0, 0, 0};
    PresetPiece doubled = first;
    doubled.left_hand = true;

    const PresetPlan kept = PlanPreset(PresetOf({first, doubled}), twins);
    CHECK(kept.wear.empty());
    CHECK(kept.take_off.empty());
    CHECK(kept.missing == 0);
  }
  SECTION("mais deux dagues identiques restent deux pièces") {
    Inventory one;
    one.Put(Piece(3, kDagger, kDualWieldDagger, ro::sim::kEquipWeapon));
    PresetPiece other_hand = Entry(kDagger);
    other_hand.left_hand = true;
    CHECK(PlanPreset(PresetOf({Entry(kDagger), other_hand}), one).missing == 1);
  }
}

TEST_CASE("Presets : une coiffe d'apparat délogée se redemande", "[app][presets]") {
  // Elle va sur l'un des trois étages, et le preset pose en haut, là où elle se
  // trouve, une coiffe qui ne va qu'en haut : elle revient ensuite, ailleurs.
  Inventory bag;
  bag.Put(Piece(3, kCostumeCape, kAnyCostumeHead, ro::sim::kEquipCostumeHeadTop));
  bag.Put(Piece(4, kCostumeHat, ro::sim::kEquipCostumeHeadTop));
  const PresetPlan plan = PlanPreset(PresetOf({Entry(kCostumeCape, 0, PresetFamily::kCostume),
                                               Entry(kCostumeHat, 0, PresetFamily::kCostume)},
                                              true),
                                     bag);
  CHECK(plan.take_off.empty());
  REQUIRE(plan.wear.size() == 2);
  CHECK(plan.wear[0].client_slot == 4);
  CHECK(plan.wear[0].position == ro::sim::kEquipCostumeHeadTop);
  CHECK(plan.wear[1].client_slot == 3);
  CHECK(plan.wear[1].position == kAnyCostumeHead);
}

TEST_CASE("Presets : les poses dont le serveur choisit la place partent en dernier",
          "[app][presets]") {
  // Posée avant, la coiffe « n'importe quel étage » prendrait le milieu — le
  // premier libre après le haut —, et la coiffe du milieu l'en délogerait.
  Inventory bag;
  bag.Put(Piece(3, kCostumeHat, ro::sim::kEquipCostumeHeadTop));
  bag.Put(Piece(4, kCostumeCape, kAnyCostumeHead));
  bag.Put(Piece(5, kMask, ro::sim::kEquipCostumeHeadMid));
  const PresetPlan plan = PlanPreset(PresetOf({Entry(kCostumeHat, 0, PresetFamily::kCostume),
                                               Entry(kCostumeCape, 0, PresetFamily::kCostume),
                                               Entry(kMask, 0, PresetFamily::kCostume)},
                                              true),
                                     bag);
  REQUIRE(plan.wear.size() == 3);
  CHECK(plan.wear[0].client_slot == 3);
  CHECK(plan.wear[1].client_slot == 5);
  CHECK(plan.wear[2].client_slot == 4);
}

TEST_CASE("Presets : tout déséquiper retire une fois chaque pièce, sans demander la munition",
          "[app][presets]") {
  Inventory bag;
  bag.Put(Piece(3, kBow, ro::sim::kEquipBothHands, ro::sim::kEquipBothHands));
  bag.Put(Piece(4, kArmor, ro::sim::kEquipArmor, ro::sim::kEquipArmor));
  bag.Put(Piece(5, kCostumeHat, ro::sim::kEquipCostumeHeadTop, ro::sim::kEquipCostumeHeadTop));
  bag.Put(Piece(6, kArrow, ro::sim::kEquipAmmo, ro::sim::kEquipAmmo));
  bag.Put(Piece(7, kShadowArmor, kShadowArmorPlace, kShadowArmorPlace));
  bag.Put(Piece(8, kSword, ro::sim::kEquipWeapon));

  CHECK(PlanUndress(bag, false) == std::vector<std::uint16_t>{3, 4});
  CHECK(PlanUndress(bag, true) == std::vector<std::uint16_t>{3, 4, 5});
}

// ── LE CARNET ET SES TOUCHES ─────────────────────────────────────────────────

TEST_CASE("Presets : cinq par personnage, et un nom pris se remplace", "[app][presets]") {
  EquipPresetBook book;
  for (const char* name : {"A", "B", "C", "D", "E"}) {
    CHECK(book.Save(name, {Entry(kSword)}) == EquipPresetBook::Saved::kAdded);
  }
  CHECK(book.Save("F", {}) == EquipPresetBook::Saved::kFull);
  CHECK(book.presets().size() == 5);

  // Au plafond, un nom déjà pris s'enregistre encore, et garde ses costumes et
  // sa touche.
  const Binding hotkey{.key = SDLK_F5, .ctrl = true};
  REQUIRE(book.SetWithCostumes(2, true));
  REQUIRE(book.SetHotkey(2, hotkey));
  const std::uint32_t before = book.revision();
  CHECK(book.Save("C", {Entry(kBow), Entry(kArrow)}) == EquipPresetBook::Saved::kReplaced);
  CHECK(book.revision() != before);
  const EquipPreset* replaced = book.At(2);
  REQUIRE(replaced != nullptr);
  CHECK(replaced->pieces.size() == 2);
  CHECK(replaced->with_costumes);
  CHECK(replaced->hotkey == hotkey);
}

TEST_CASE("Presets : la fenêtre des raccourcis reprend une touche au preset qui la tient",
          "[app][presets]") {
  EquipPresetBook book(
      {EquipPreset{.name = "A"}, EquipPreset{.name = "B"}, EquipPreset{.name = "C"}});
  const Binding hotkey{.key = SDLK_Q, .ctrl = true};
  REQUIRE(book.SetHotkey(1, hotkey));
  CHECK(book.Holding(hotkey) == std::optional<std::size_t>(1));
  CHECK_FALSE(book.Holding(hotkey, 1).has_value());
  // Une touche sans modificateur n'est pas la même.
  CHECK_FALSE(book.Holding(Binding{.key = SDLK_Q}).has_value());

  CHECK(book.Release(hotkey) == std::optional<std::size_t>(1));
  CHECK_FALSE(book.At(1)->hotkey.bound());
  CHECK_FALSE(book.Release(hotkey).has_value());
}

TEST_CASE("Presets : une touche prise est refusée, et le refus nomme qui la tient",
          "[app][presets]") {
  const Hotkeys table;
  EquipPresetBook book({EquipPreset{.name = "PvM"}, EquipPreset{.name = "MvP"}});
  const Binding held{.key = SDLK_J, .ctrl = true};
  REQUIRE(book.SetHotkey(0, held));

  SECTION("par un autre preset du personnage") {
    const std::optional<std::string> holder = PresetKeyHolder(table, book, 1, held);
    REQUIRE(holder.has_value());
    CHECK(holder->find("PvM") != std::string::npos);
    // Redéfinir sa propre touche n'est pas un conflit.
    CHECK_FALSE(PresetKeyHolder(table, book, 0, held).has_value());
  }
  SECTION("par une action de la table") {
    // Alt+E ouvre le sac, d'usine.
    CHECK(PresetKeyHolder(table, book, 1, Binding{.key = SDLK_E, .alt = true}).has_value());
  }
  SECTION("et une touche libre passe") {
    CHECK_FALSE(PresetKeyHolder(table, book, 1, Binding{.key = SDLK_J, .shift = true}).has_value());
  }
}

TEST_CASE("Presets : une touche de preset est une lettre, un chiffre, F1 à F12 ou l'espace",
          "[app][presets]") {
  for (const std::uint32_t key : {SDLK_A, SDLK_Z, SDLK_0, SDLK_9, SDLK_F1, SDLK_F12, SDLK_SPACE}) {
    CHECK(PresetKeyCapturable(key));
  }
  for (const std::uint32_t key :
       {SDLK_ESCAPE, SDLK_TAB, SDLK_RETURN, SDLK_KP_1, SDLK_F13, SDLK_LCTRL, SDLK_DELETE}) {
    CHECK_FALSE(PresetKeyCapturable(key));
  }
}
