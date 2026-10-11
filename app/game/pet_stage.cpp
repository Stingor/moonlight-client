#include "game/pet_stage.h"

#include <array>
#include <cstdio>
#include <string>
#include <utility>

#include "ro/core/i18n.h"
#include "ro/core/log.h"
#include "ro/core/text.h"
#include "ro/net/pet_frames.h"
#include "ro/sim/pet_egg.h"
#include "ro/vfs/item_link.h"
#include "ro/vfs/item_name.h"

#include "game/game_pieces.h"
#include "items/inventory_view.h"

namespace moonclient {
namespace {

// Les couleurs du jeu pour ses refus, en 0xRRGGBB : celle du « pas de
// nourriture », et celle des deux refus que la fiche opposait.
constexpr std::uint32_t kNoFoodColor = 0xF51E1E;
constexpr std::uint32_t kRefusalColor = 0xFF0000;
// Celle de l'accusé d'un repas pris.
constexpr std::uint32_t kFedColor = 0x64FFFF;
// Assez pour la fin de la ligne d'alerte de faim, dans toutes les langues.
constexpr std::size_t kHungerLineBytes = 96;
// Assez pour une ligne de repas et le nom ou le lien de sa nourriture.
constexpr std::size_t kFoodLineBytes = 192;
// Les lignes que le jeu écrit sans couleur particulière.
constexpr std::uint32_t kPlainColor = 0;

// Les deux refus que la fiche opposait à « nourrir » : une lettre en rédaction,
// un échange en cours.
[[nodiscard]] const char* MailRefusal() {
  return ro::core::Tr("Échec du nourrissage du pet : fermez la fenêtre RODEX.");
}
[[nodiscard]] const char* TradeRefusal() {
  return ro::core::Tr(
      "Échec du nourrissage : le pet est dans un état où il ne peut pas être nourri.");
}

// LA NOURRITURE QUI MANQUE, en lien d'item : la conversation la montre sous son
// nom, et il se clique comme tout autre lien.
[[nodiscard]] std::string NoFoodLine(const ro::vfs::ItemTable* items, std::uint32_t food_item_id) {
  const ro::vfs::ItemEntry* entry = items != nullptr ? items->Find(food_item_id) : nullptr;
  const std::string food_link =
      ro::vfs::BuildItemLink(ro::vfs::ItemLink{.item_id = food_item_id},
                             entry != nullptr ? entry->card_slots : std::uint8_t{0});
  std::array<char, kFoodLineBytes> line = {};
  std::snprintf(line.data(), line.size(),
                ro::core::Tr("La nourriture du pet [%s] n'est pas disponible."), food_link.c_str());
  return line.data();
}

// L'ACCUSÉ D'UN REPAS. Pris, il nomme la nourriture en clair et compte ce
// qu'il en reste — ⚠ le serveur l'a déjà retirée du sac avant d'accuser. Refusé,
// il dit la même ligne que la garde du client.
[[nodiscard]] std::string FeedingLine(const ro::vfs::ItemTable* items,
                                      const ro::sim::PetFeeding& feeding,
                                      const ro::sim::Inventory& bag) {
  if (!feeding.fed) {
    return NoFoodLine(items, feeding.food_item_id);
  }
  const ro::vfs::ItemEntry* entry = items != nullptr ? items->Find(feeding.food_item_id) : nullptr;
  const std::string food_name =
      entry != nullptr && !entry->name.empty() ? entry->name : std::to_string(feeding.food_item_id);
  std::array<char, kFoodLineBytes> line = {};
  std::snprintf(line.data(), line.size(),
                ro::core::Tr("Votre pet a été nourri. %s (il en reste %d)"), food_name.c_str(),
                static_cast<int>(ro::sim::CountInBag(bag, feeding.food_item_id)));
  return line.data();
}

[[nodiscard]] ro::ui::SkinImage IconOf(GamePieces& pieces, std::uint32_t item_id) {
  if (!pieces.item_icons.has_value() || item_id == 0) {
    return {};
  }
  const ro::render::IconTexture& icon = pieces.item_icons->Get(item_id);
  if (!icon.usable()) {
    return {};
  }
  return ro::ui::SkinImage{.id = static_cast<ImTextureID>(icon.texture.name()),
                           .width = static_cast<float>(icon.width_px),
                           .height = static_cast<float>(icon.height_px)};
}

[[nodiscard]] std::string ItemName(const GamePieces& pieces, std::uint32_t item_id) {
  if (!pieces.wardrobe.has_value()) {
    return {};
  }
  const ro::vfs::ItemEntry* entry = pieces.wardrobe->catalog().items.Find(item_id);
  return entry != nullptr ? entry->name : std::string();
}

// LE NOM QUE L'INVENTORY MONTRE, ornements compris : un œuf renommé y porte
// « Beloved ».
[[nodiscard]] std::string OwnedItemName(const GamePieces& pieces,
                                        const ro::sim::InventoryItem& item) {
  if (!pieces.wardrobe.has_value()) {
    return {};
  }
  return ro::vfs::ItemNameText(ro::vfs::ComposeItemName(
      ItemNameFactsOf(item), pieces.wardrobe->catalog().items, pieces.item_words));
}

[[nodiscard]] ro::ui::PetIntimacyLabel LabelOf(ro::sim::PetIntimacyWord word) {
  switch (word) {
    case ro::sim::PetIntimacyWord::kHateWithPassion:
      return ro::ui::PetIntimacyLabel::kHateWithPassion;
    case ro::sim::PetIntimacyWord::kHate:
      return ro::ui::PetIntimacyLabel::kHate;
    case ro::sim::PetIntimacyWord::kAwkward:
      return ro::ui::PetIntimacyLabel::kAwkward;
    case ro::sim::PetIntimacyWord::kShy:
      return ro::ui::PetIntimacyLabel::kShy;
    case ro::sim::PetIntimacyWord::kNeutral:
      return ro::ui::PetIntimacyLabel::kNeutral;
    case ro::sim::PetIntimacyWord::kCordial:
      return ro::ui::PetIntimacyLabel::kCordial;
    case ro::sim::PetIntimacyWord::kLoyal:
      return ro::ui::PetIntimacyLabel::kLoyal;
    case ro::sim::PetIntimacyWord::kUnknown:
      break;
  }
  return ro::ui::PetIntimacyLabel::kUnknown;
}

// Ce que le jeu écrit pour chaque échec d'évolution. Rien pour la réussite :
// les paquets qui la suivent ont déjà tout redit.
[[nodiscard]] const char* EvolutionResultText(std::uint32_t result) {
  using Outcome = ro::proto::PetEvolutionOutcome;
  switch (static_cast<Outcome>(result)) {
    case Outcome::kFailUnknown:
      return ro::core::Tr("Erreur inconnue.");
    case Outcome::kFailNoPet:
      return ro::core::Tr("Aucun pet ne peut être appelé.");
    case Outcome::kFailNotPetEgg:
      return ro::core::Tr("Ce n'est pas l'œuf demandé.");
    case Outcome::kFailRecipe:
      return ro::core::Tr("Les matériaux d'évolution sont insuffisants.");
    case Outcome::kFailMaterial:
      return ro::core::Tr("Il manque des matériaux requis pour l'évolution.");
    case Outcome::kFailIntimacy:
      return ro::core::Tr("Le pet ne peut évoluer que lorsque son intimité est Loyal.");
    case Outcome::kSuccess:
      break;
  }
  return nullptr;
}

[[nodiscard]] ro::proto::PetCommand CommandOf(ro::ui::PetOrder order) {
  switch (order) {
    case ro::ui::PetOrder::kFeed:
      return ro::proto::PetCommand::kFeed;
    case ro::ui::PetOrder::kPerform:
      return ro::proto::PetCommand::kPerform;
    case ro::ui::PetOrder::kReturnToEgg:
      return ro::proto::PetCommand::kReturnToEgg;
    case ro::ui::PetOrder::kUnequipAccessory:
      break;
  }
  return ro::proto::PetCommand::kUnequipAccessory;
}

}  // namespace

// ── LE SERVEUR ──────────────────────────────────────────────────────────────

ro::net::FrameOutcome PetStage::ApplyFrame(const ro::net::Frame& frame, ro::sim::World& world,
                                           std::uint32_t now_ms) {
  last_now_ms_ = now_ms;
  ro::net::FrameOutcome outcome = ro::net::ApplyPetFrame(frame, pet_);
  // L'APPARENCE VAUT POUR TOUT PET VISIBLE : celle d'un voisin n'est pas un
  // état du nôtre, mais elle reste une trame lue.
  if (ro::net::ApplyPetAccessoryLook(frame, world) &&
      outcome == ro::net::FrameOutcome::kUnhandled) {
    outcome = ro::net::FrameOutcome::kApplied;
  }
  if (pet_.TakeLowIntimacyWarning()) {
    force_open_ = true;
  }
  if (const std::optional<std::uint32_t> result = pet_.TakeEvolutionResult()) {
    if (const char* text = EvolutionResultText(*result); text != nullptr) {
      Say(world, text, kPlainColor);
    }
  }
  if (const std::optional<ro::sim::PetFeeding> feeding = pet_.TakeFeeding()) {
    Say(world, FeedingLine(items_, *feeding, world.inventory()),
        feeding->fed ? kFedColor : kNoFoodColor);
  }
  if (const std::optional<bool> fed = pet_.TakeAutoFeedEcho()) {
    Say(world,
        *fed ? ro::core::Tr("Alimentation automatique activée.")
             : ro::core::Tr("Alimentation automatique désactivée."),
        kPlainColor);
  }
  return outcome;
}

void PetStage::Say(ro::sim::World& world, std::string text, std::uint32_t color) const {
  world.RecordChat(ro::sim::ChatKind::kServer, 0, std::move(text), last_now_ms_, color);
}

void PetStage::LoadTables(ro::vfs::Vfs& vfs) {
  if (tables_tried_) {
    return;
  }
  tables_tried_ = true;
  // Chaque fichier absent ou illisible le dit lui-même au journal.
  tables_ = ro::vfs::ReadPetTables(vfs);
}

// ── LES COMPOSITIONS ────────────────────────────────────────────────────────

void PetStage::Stage(ro::vfs::Vfs& vfs, const ro::sim::World& world, GamePieces& pieces,
                     ro::ui::PetWindowState& window, ro::ui::PetWindowView& view,
                     ro::ui::PetHatchWindowState& hatch, ro::ui::PetHatchWindowView& hatch_view) {
  LoadTables(vfs);
  items_ = pieces.wardrobe.has_value() ? &pieces.wardrobe->catalog().items : nullptr;
  mail_writing_ = pieces.mail.writing();
  trading_ = pieces.trade.deal().deal().phase() != ro::sim::DealPhase::kNone;
  if (pieces.wardrobe.has_value() && !sprite_.has_value()) {
    sprite_.emplace(*pieces.wardrobe);
    source_sprite_.emplace(*pieces.wardrobe);
    target_sprite_.emplace(*pieces.wardrobe);
  }

  // L'OUVERTURE FORCÉE, une fois par familier : elle ne referme jamais.
  if (std::exchange(force_open_, false) && pet_.known() && !window.open) {
    ro::ui::OpenPetWindow(window);
  }
  // PARTI, il ferme sa fiche.
  if (window.open && !pet_.known()) {
    window.open = false;
  }
  if (pet_.TakeEggOfferOpened()) {
    ro::ui::OpenPetHatchWindow(hatch);
  }
  // UN FAMILIER SORTI dit que l'éclosion a abouti : la liste n'a plus rien à
  // proposer.
  if (hatch.open && (pet_.seen() || !pet_.eggs_offered())) {
    hatch.open = false;
  }

  if (window.open) {
    ComposeWindow(world, pieces, window, view);
  } else {
    view.present = false;
  }
  if (hatch.open) {
    ComposeHatch(world, pieces, hatch_view);
  }
}

void PetStage::ComposeWindow(const ro::sim::World& world, GamePieces& pieces,
                             ro::ui::PetWindowState& window, ro::ui::PetWindowView& view) {
  view = ro::ui::PetWindowView{};
  view.present = pet_.seen();
  if (!view.present) {
    return;
  }
  const std::uint32_t pet_class = pet_.pet_class();
  view.name = pet_.name();
  view.renameable = pet_.renameable();
  view.name_max_bytes = ro::sim::kPetNameMaxBytes;
  view.species = tables_.has_value() ? std::string(tables_->SpeciesOf(pet_class)) : std::string();
  view.level = pet_.level();
  view.has_accessory = pet_.wears_accessory();
  view.evolution_offered_above = ro::sim::kPetEvolutionOfferedAbove;
  view.evolution_server_minimum = ro::sim::kPetEvolutionServerMinimum;
  if (view.has_accessory) {
    view.accessory = ItemName(pieces, pet_.accessory_item_id());
  }
  view.hunger = pet_.hunger();
  view.previous_hunger = pet_.previous_hunger();
  view.hunger_most = ro::sim::kPetHungerMax;
  view.starving = ro::sim::PetHungerRank(pet_.hunger()) == 0;
  view.intimacy = pet_.intimacy();
  view.intimacy_most = ro::sim::kPetIntimacyMax;
  view.intimacy_label = LabelOf(ro::sim::PetIntimacyWordOf(pet_.intimacy(), pet_class));
  view.mail_writing = mail_writing_;
  view.trading = trading_;
  if (const std::uint32_t food = tables_.has_value() ? tables_->FoodOf(pet_class) : 0; food != 0) {
    view.food_known = true;
    view.food_item_id = food;
    view.food_icon = IconOf(pieces, food);
    view.food_name = ItemName(pieces, food);
    view.food_in_bag = ro::sim::CountInBag(world.inventory(), food);
  }

  if (sprite_.has_value()) {
    const int box = static_cast<int>(window.sprite_box_px);
    // PARÉ COMME SUR LA CARTE : l'accessoire se lit sur l'acteur, seul porteur
    // de l'apparence.
    const ro::sim::Actor* const shown = world.Find(pet_.actor_id());
    view.sprite =
        sprite_
            ->Compose(MonsterSprite::Framing{.sprite_class = pet_class,
                                             .direction = 0,
                                             .animate = window.animate_sprite,
                                             .box_width = box,
                                             .box_height = box,
                                             .pet_accessory = shown != nullptr
                                                                  ? shown->look.lower_headgear
                                                                  : std::uint16_t{0}},
                      last_now_ms_)
            .image;
    static_cast<void>(sprite_->TakeSounds());
  }

  // ── L'ŒUF PORTÉ, qui décide de l'auto-alimentation et des évolutions ────
  const std::uint32_t egg = ro::sim::HatchedEggOf(world.inventory());
  view.auto_feed_listed = egg != 0 && tables_.has_value() && tables_->AutoFeedListed(egg);
  view.auto_feed = pet_.auto_feed();
  if (pet_.intimacy() <= ro::sim::kPetEvolutionOfferedAbove) {
    view.evolution = ro::ui::PetEvolutionShown::kIntimacyTooLow;
  } else if (egg == 0) {
    view.evolution = ro::ui::PetEvolutionShown::kEggUnknown;
  } else if (!tables_.has_value() || tables_->EvolutionsOf(egg).empty()) {
    view.evolution = ro::ui::PetEvolutionShown::kNoRecipe;
  } else {
    view.evolution = ro::ui::PetEvolutionShown::kTargets;
    for (const ro::vfs::PetEvolutionRecipe& recipe : tables_->EvolutionsOf(egg)) {
      view.evolution_targets.push_back(ro::ui::PetEvolutionTargetView{
          .egg_id = recipe.target_egg_id,
          .species = std::string(tables_->SpeciesOf(tables_->ClassOfEgg(recipe.target_egg_id)))});
    }
  }
  if (view.evolution == ro::ui::PetEvolutionShown::kTargets && window.evolution_target != 0) {
    ComposePanel(world, pieces, egg, window, view);
  } else {
    window.evolution_target = 0;
  }
}

void PetStage::ComposePanel(const ro::sim::World& world, GamePieces& pieces,
                            std::uint32_t source_egg, ro::ui::PetWindowState& window,
                            ro::ui::PetWindowView& view) {
  const ro::vfs::PetEvolutionRecipe* recipe =
      tables_->RecipeOf(source_egg, window.evolution_target);
  if (recipe == nullptr) {
    window.evolution_target = 0;
    return;
  }
  ro::ui::PetEvolutionPanelView& panel = view.panel;
  panel.source_egg_id = source_egg;
  panel.target_egg_id = recipe->target_egg_id;
  const std::uint32_t target_class = tables_->ClassOfEgg(recipe->target_egg_id);
  panel.source_species = view.species;
  panel.target_species = std::string(tables_->SpeciesOf(target_class));
  bool has_materials = true;
  for (const ro::vfs::PetEvolutionMaterial& material : recipe->materials) {
    const std::int64_t owned = ro::sim::CountInBag(world.inventory(), material.item_id);
    has_materials = has_materials && owned >= material.amount;
    panel.materials.push_back(
        ro::ui::PetEvolutionMaterialView{.item_id = material.item_id,
                                         .icon = IconOf(pieces, material.item_id),
                                         .name = ItemName(pieces, material.item_id),
                                         .owned = owned,
                                         .needed = material.amount});
  }
  switch (ro::sim::PetEvolutionRefusalOf(pet_.intimacy(), pet_.wears_accessory(), has_materials)) {
    case ro::sim::PetEvolutionRefusal::kIntimacy:
      panel.block = ro::ui::PetEvolutionBlock::kIntimacy;
      break;
    case ro::sim::PetEvolutionRefusal::kAccessory:
      panel.block = ro::ui::PetEvolutionBlock::kAccessory;
      break;
    case ro::sim::PetEvolutionRefusal::kMaterials:
      panel.block = ro::ui::PetEvolutionBlock::kMaterials;
      break;
    case ro::sim::PetEvolutionRefusal::kNone:
      panel.block = ro::ui::PetEvolutionBlock::kNone;
      break;
  }
  if (source_sprite_.has_value() && target_sprite_.has_value()) {
    const int box = static_cast<int>(window.evolution_box_px);
    const auto framing = [&window, box](std::uint32_t sprite_class) {
      return MonsterSprite::Framing{.sprite_class = sprite_class,
                                    .direction = 0,
                                    .animate = window.animate_sprite,
                                    .box_width = box,
                                    .box_height = box};
    };
    panel.source_sprite = source_sprite_->Compose(framing(pet_.pet_class()), last_now_ms_).image;
    panel.target_sprite = target_sprite_->Compose(framing(target_class), last_now_ms_).image;
    static_cast<void>(source_sprite_->TakeSounds());
    static_cast<void>(target_sprite_->TakeSounds());
  }
}

void PetStage::ComposeHatch(const ro::sim::World& world, GamePieces& pieces,
                            ro::ui::PetHatchWindowView& view) const {
  view.rows.clear();
  for (const std::uint16_t slot : pet_.offered_egg_slots()) {
    ro::ui::PetEggRowView row;
    row.client_slot = slot;
    const ro::sim::InventoryItem* item = world.inventory().Find(slot);
    if (item == nullptr) {
      row.state = ro::ui::PetEggState::kMissing;
      view.rows.push_back(std::move(row));
      continue;
    }
    row.item_id = item->item_id;
    row.icon = IconOf(pieces, item->item_id);
    row.name = OwnedItemName(pieces, *item);
    const ro::sim::PetEggFacts facts = ro::sim::DecodePetEgg(item->cards);
    row.state = facts.has_pet ? ro::ui::PetEggState::kInhabited : ro::ui::PetEggState::kBlank;
    if (const std::optional<ro::sim::PetIntimacyWord> word =
            ro::sim::PetEggRankWord(facts.intimacy_rank)) {
      row.intimacy = LabelOf(*word);
    }
    row.renamed = facts.renamed;
    view.rows.push_back(std::move(row));
  }
}

// ── LES GESTES ──────────────────────────────────────────────────────────────

void PetStage::Command(ro::net::Connection& link, ro::proto::PetCommand command) {
  if (const auto sent = ro::net::RequestPetCommand(link, command); !sent) {
    ro::core::LogWarning("ordre au familier non envoyé : {}", ro::core::Describe(sent.error()));
  }
}

void PetStage::FeedIfFood(ro::net::Connection& link, ro::sim::World& world) {
  // LE JEU REFUSE SANS RIEN ENVOYER quand la nourriture de l'espèce manque au
  // sac. ⚠ SANS TABLE, la nourriture est inconnue : le serveur tranchera.
  const std::uint32_t food = tables_.has_value() ? tables_->FoodOf(pet_.pet_class()) : 0;
  if (food != 0 && ro::sim::CountInBag(world.inventory(), food) <= 0) {
    Say(world, NoFoodLine(items_, food), kNoFoodColor);
    return;
  }
  Command(link, ro::proto::PetCommand::kFeed);
}

void PetStage::Apply(const ro::ui::PetWindowAction& asked,
                     const ro::ui::PetHatchWindowAction& hatched, ro::ui::PetWindowState& window,
                     ro::net::Connection& link, ro::sim::World& world, std::uint32_t now_ms) {
  last_now_ms_ = now_ms;
  const std::uint32_t pet_id = pet_.announced_actor_id();
  pet_.ObservePresence(pet_id != 0 && world.Find(pet_id) != nullptr, world.map_entries(), now_ms);
  if (pet_.TakeSheetWanted()) {
    Command(link, ro::proto::PetCommand::kStatus);
  }
  // LA FAIM QUI DESCEND AU PALIER où le pet perd de l'intimité : le bandeau le
  // dit, avec l'icône de ce qu'il mange.
  if (hunger_alarm_.Observe(pet_.known(), pet_.hunger())) {
    std::array<char, kHungerLineBytes> suffix{};
    std::snprintf(suffix.data(), suffix.size(), ro::core::Tr("a faim : %d / %d."), pet_.hunger(),
                  ro::sim::kPetHungerMax);
    hunger_notice_ =
        ToastNotice{.icon_item_id = tables_.has_value() ? tables_->FoodOf(pet_.pet_class()) : 0,
                    .name = pet_.name(),
                    .suffix = std::string(" ") + suffix.data()};
  }
  // LA BASCULE D'UNE TOUCHE OU DE L'ICÔNE DU MENU. Sans pet sorti, elle le dit :
  // une touche muette ne s'explique pas.
  if (std::exchange(window.toggle_requested, false)) {
    if (pet_.known()) {
      ToggleWindow(window, link);
    } else {
      Say(world, ro::core::Tr("Aucun pet n'est sorti."), kRefusalColor);
    }
  }

  if (asked.feed_refused_mail) {
    Say(world, MailRefusal(), kRefusalColor);
  }
  if (asked.feed_refused_trade) {
    Say(world, TradeRefusal(), kRefusalColor);
  }
  if (asked.order.has_value()) {
    if (*asked.order == ro::ui::PetOrder::kFeed) {
      FeedIfFood(link, world);
    } else {
      Command(link, CommandOf(*asked.order));
    }
  }
  if (asked.rename.has_value() && ro::sim::CheckPetName(ro::core::Cp1252FromUtf8(*asked.rename)) ==
                                      ro::sim::PetNameRefusal::kNone) {
    if (const auto sent = ro::net::RequestPetRename(link, *asked.rename); sent) {
      pet_.Rename(*asked.rename);
    } else {
      ro::core::LogWarning("renommage du familier non envoyé : {}",
                           ro::core::Describe(sent.error()));
    }
  }
  if (asked.auto_feed.has_value()) {
    if (const auto sent = ro::net::RequestPetAutoFeed(link, *asked.auto_feed); !sent) {
      ro::core::LogWarning("auto-alimentation non demandée : {}", ro::core::Describe(sent.error()));
    }
  }
  if (asked.evolve_to != 0) {
    if (const auto sent = ro::net::RequestPetEvolution(link, asked.evolve_to); !sent) {
      ro::core::LogWarning("évolution non demandée : {}", ro::core::Describe(sent.error()));
    }
  }
  if (hatched.hatch.has_value()) {
    if (const auto sent = ro::net::RequestPetEgg(link, *hatched.hatch); !sent) {
      ro::core::LogWarning("éclosion non demandée : {}", ro::core::Describe(sent.error()));
    }
  }
}

// ── LE SAC ──────────────────────────────────────────────────────────────────

bool PetStage::OffersAsAccessory(const ro::sim::InventoryItem& item) const {
  if (!ro::sim::IsPetAccessoryItem(item)) {
    return false;
  }
  // LA TABLE LUE TRANCHE, comme chez le client : il ne propose que ce qu'elle
  // connaît. Absente, le serveur jugera seul.
  if (tables_.has_value() && !tables_->accessory_act_by_item.empty()) {
    return tables_->IsPetAccessory(item.item_id);
  }
  return true;
}

bool PetStage::RefusesAccessory(ro::sim::World& world) const {
  if (pet_.actor_id() != 0) {
    return false;
  }
  Say(world, ro::core::Tr("Aucun pet n'est sorti pour porter cet accessoire."), kRefusalColor);
  return true;
}

// ── LE MENU D'ENTITÉ ────────────────────────────────────────────────────────

void PetStage::ToggleWindow(ro::ui::PetWindowState& window, ro::net::Connection& link) {
  if (!pet_.known()) {
    return;
  }
  Command(link, ro::proto::PetCommand::kStatus);
  if (window.open) {
    window.open = false;
    return;
  }
  ro::ui::OpenPetWindow(window);
}

const char* PetStage::FeedRefusal() const {
  if (mail_writing_) {
    return MailRefusal();
  }
  return trading_ ? TradeRefusal() : nullptr;
}

void PetStage::Feed(ro::net::Connection& link, ro::sim::World& world) {
  if (const char* refusal = FeedRefusal(); refusal != nullptr) {
    Say(world, refusal, kRefusalColor);
    return;
  }
  FeedIfFood(link, world);
}

void PetStage::Perform(ro::net::Connection& link) {
  Command(link, ro::proto::PetCommand::kPerform);
}

void PetStage::UnequipAccessory(ro::net::Connection& link) {
  Command(link, ro::proto::PetCommand::kUnequipAccessory);
}

void PetStage::ReturnToEgg(ro::net::Connection& link) {
  Command(link, ro::proto::PetCommand::kReturnToEgg);
}

}  // namespace moonclient
