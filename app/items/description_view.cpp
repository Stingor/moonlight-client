#include "items/description_view.h"

#include <algorithm>
#include <utility>

#include "ro/net/item_facts.h"
#include "ro/sim/card_composition.h"
#include "ro/sim/equip_compare.h"
#include "ro/sim/pet_egg.h"
#include "ro/sim/world.h"
#include "ro/vfs/item_name.h"

#include "items/inventory_view.h"

namespace moonclient {

DescriptionView::DescriptionView(const ro::vfs::ItemTable* items, const ro::vfs::ItemWords* words,
                                 ro::render::ItemIcons* icons)
    : items_(items), words_(words), icons_(icons) {}

namespace {

// LE CONTENANT DÉSIGNÉ, tel que le monde le porte. ⚠ L'ÉQUIPEMENT N'EN EST PAS
// UN : une pièce portée vit dans le sac, avec un drapeau.
[[nodiscard]] const ro::sim::Inventory& HolderOf(const ro::sim::World& world,
                                                 ro::ui::ItemHolder from) {
  switch (from) {
    case ro::ui::ItemHolder::kCart:
      return world.cart();
    case ro::ui::ItemHolder::kStorage:
      return world.storage();
    case ro::ui::ItemHolder::kInventory:
    case ro::ui::ItemHolder::kEquipment:
    case ro::ui::ItemHolder::kViewedEquipment:
    case ro::ui::ItemHolder::kCardAlbum:
    case ro::ui::ItemHolder::kChatLink:
      break;
  }
  return world.inventory();
}

// L'OBJET D'UN LIEN, sous la forme d'un objet tenu : ce que la balise porte, et
// rien d'autre.
[[nodiscard]] ro::sim::InventoryItem ItemOfLink(const ro::vfs::ItemLink& link) {
  ro::sim::InventoryItem item;
  item.item_id = link.item_id;
  item.amount = 1;
  item.wearable_at = link.wearable_at;
  item.refine_level = static_cast<std::uint8_t>(link.refine_level);
  item.grade = static_cast<std::uint8_t>(link.grade);
  item.view_id = static_cast<std::uint16_t>(link.view_id);
  item.cards = link.cards;
  const std::size_t options = std::min(link.option_count, item.options.size());
  for (std::size_t rank = 0; rank < options; ++rank) {
    const ro::vfs::ItemLinkOption& option = link.options[rank];
    // ⚠ LA BALISE ÉCRIT LA VALEUR SANS SIGNE : un malus y revient par le
    // complément à deux.
    item.options[rank] = ro::sim::ItemOption{
        .id = option.id, .value = static_cast<std::int16_t>(option.value), .param = option.param};
  }
  item.identified = true;
  item.damaged = link.damaged;
  return item;
}

}  // namespace

void DescriptionView::Refresh(const ro::sim::World& world, std::uint32_t item_id,
                              ro::ui::ItemHolder from, std::uint16_t decorate_from) {
  if (item_id == 0) {
    view_ = ro::ui::DescriptionView{};
    shown_ = 0;
    return;
  }
  // ⚠ ON NE REFAIT QUE CE QUI CHANGE. Les lignes et l'illustration sont figées
  // pour un identifiant ; seul le nom décoré suit l'objet, qui peut se faire
  // sertir ou raffiner pendant qu'on le lit.
  if (shown_ != item_id) {
    shown_ = item_id;
    lines_.clear();
    if (const ro::vfs::ItemEntry* entry = items_ != nullptr ? items_->Find(item_id) : nullptr;
        entry != nullptr) {
      lines_ = entry->description;
    }
    RefreshOdds(item_id);
  }

  // LE NOM. ⚠ DÉCORÉ QUAND L'OBJET EST ENCORE LÀ, de base sinon : on décrit
  // aussi bien ce qu'on possède que ce qu'on a seulement croisé.
  name_.clear();
  bool damaged = false;
  const bool from_link = from == ro::ui::ItemHolder::kChatLink;
  const ro::sim::InventoryItem* held =
      decorate_from != 0 && from != ro::ui::ItemHolder::kViewedEquipment && !from_link
          ? HolderOf(world, from).Find(decorate_from)
          : nullptr;
  if (from_link && linked_.item_id == item_id) {
    linked_item_ = ItemOfLink(linked_);
    held = &linked_item_;
  }
  // UNE PIÈCE VUE SUR UN AUTRE JOUEUR se décore de ce que son porteur a montré :
  // raffinement, cartes, options — ce que la fenêtre d'équipement liste.
  if (from == ro::ui::ItemHolder::kViewedEquipment && viewed_ != nullptr &&
      viewed_->shown().has_value()) {
    for (const ro::sim::InventoryItem& piece : viewed_->shown()->pieces) {
      if (piece.client_slot == decorate_from) {
        held = &piece;
        break;
      }
    }
  }
  name_pieces_.clear();
  if (items_ != nullptr && words_ != nullptr) {
    if (held != nullptr && held->item_id == item_id) {
      ComposeName(*held, from_link);
      damaged = held->damaged;
    } else {
      // ⚠ LES FAITS D'UN OBJET QU'ON N'A PAS : son identifiant, et rien d'autre.
      // Le composeur rend alors le nom de base, ce qu'on veut.
      name_ = ro::vfs::ItemNameText(
          ro::vfs::ComposeItemName(ro::vfs::ItemNameFacts{.item_id = item_id}, *items_, *words_));
    }
  }

  view_ = ro::ui::DescriptionView{
      .item_id = item_id, .name = name_, .lines = lines_, .damaged = damaged};
  view_.odds = odds_;
  view_.odds_many_groups = odds_many_groups_;
  view_.back_name = back_name_;
  view_.name_pieces = name_pieces_;
  if (const ro::sim::ItemFacts& facts = asker_.facts(); facts.autoloot_told()) {
    view_.autoloot = facts.Autoloots(item_id) ? ro::ui::DescriptionAutoloot::kIn
                                              : ro::ui::DescriptionAutoloot::kOut;
  }
  cards_.clear();
  options_.clear();
  option_texts_.clear();
  view_.pet_egg = ro::sim::IsPetEgg(item_id);
  held_refine_ = -1;
  held_cards_ = {};
  held_view_ = 0;
  held_wearable_ = 0;
  if (held != nullptr && held->item_id == item_id) {
    ComposeInstance(*held);
    held_refine_ = held->refine_level;
    held_cards_ = held->cards;
    held_view_ = held->view_id;
    held_wearable_ = held->wearable_at;
  }
  view_.readable_book = books_.Readable(world, item_id);
  view_.view_id = held_view_;
  view_.doll_possible = held_view_ != 0 && doll_.has_value() && doll_->Shows(held_wearable_);
  // LA PIÈCE PORTÉE AU MÊME EMPLACEMENT, quand le client en compare une. ⚠ PAS
  // POUR L'ÉQUIPEMENT D'UN AUTRE : ses pièces sont portées, et le client ne
  // compare jamais une pièce portée.
  if (compare_ != nullptr && held != nullptr && held->item_id == item_id &&
      from != ro::ui::ItemHolder::kViewedEquipment) {
    if (const ro::sim::InventoryItem* worn = ro::sim::EquippedToCompare(world.inventory(), *held);
        worn != nullptr) {
      compare_->Refresh(world, worn->item_id, ro::ui::ItemHolder::kEquipment, worn->client_slot);
      view_.compared = &compare_->view();
    }
  }
  if (icons_ != nullptr) {
    // 🔴 TROIS IMAGES POUR UN OBJET, ET L'ORDRE EST CELUI DU CLIENT : le dessin
    // de la carte s'il en est une, sinon la grande image de collection — onze
    // mille objets l'ont —, sinon l'icône de 24 pixels. Sauter la première
    // montrerait le dos de carte générique pour les mille cinq cents cartes,
    // puisqu'elles partagent toutes cette ressource ; sauter la seconde
    // donnerait à une description la vignette d'une case de sac.
    const ro::render::IconTexture& drawn = icons_->GetCardIllustration(item_id);
    const ro::render::IconTexture& art = drawn.usable() ? drawn : icons_->GetCollection(item_id);
    const ro::render::IconTexture& shown = art.usable() ? art : icons_->Get(item_id);
    if (shown.usable()) {
      view_.picture = ro::ui::SkinImage{.id = static_cast<ImTextureID>(shown.texture.name()),
                                        .width = static_cast<float>(shown.width_px),
                                        .height = static_cast<float>(shown.height_px)};
      // ⚠ C'EST ICI QU'ON SAIT SI C'EN EST UNE, et nulle part ailleurs : la
      // fenêtre en fait un survol à part, et `ui` n'a pas la table qui le dit.
      view_.picture_is_card = drawn.usable();
      view_.picture_whole = from == ro::ui::ItemHolder::kCardAlbum;
    }
  }
}

BugReportContext DescriptionView::BugReport(std::uint32_t item_id) const {
  return ItemBugReport(item_id, name_, held_refine_, held_cards_);
}

void DescriptionView::NoteHatEffect(bool worn) noexcept {
  view_.doll_effect = worn && doll_.has_value();
}

ro::render::VignetteFraming DescriptionView::doll_framing() const noexcept {
  return doll_.has_value() ? doll_->framing() : ro::render::VignetteFraming{};
}

void DescriptionView::RefreshDoll(const ro::sim::World& world, ro::ui::DescriptionDollState& state,
                                  bool with_effect, ActorStyles& dressing, std::uint32_t now_ms) {
  if (!doll_.has_value()) {
    state.image = ro::ui::SkinImage{};
    return;
  }
  doll_->Refresh(world, held_view_, held_wearable_, with_effect, state, dressing, now_ms);
}

void DescriptionView::EnableComparison() {
  compare_ = std::make_unique<DescriptionView>(items_, words_, icons_);
  compare_->SeeRandomOptions(option_names_);
  compare_->ShareCreatorsWith(*this);
}

void DescriptionView::ToggleAutoloot(ro::net::Connection& link, std::uint32_t item_id) {
  SetAutoloot(link, item_id, !asker_.facts().Autoloots(item_id));
}

void DescriptionView::SetAutoloot(ro::net::Connection& link, std::uint32_t item_id, bool listed) {
  ro::sim::ItemFacts& facts = asker_.facts();
  if (item_id == 0 || !facts.autoloot_told() || !link.is_open() ||
      facts.Autoloots(item_id) == listed) {
    return;
  }
  ro::net::RequestAutoloot(link, item_id, listed);
  facts.NoteAutoloot(item_id, listed);
}

void DescriptionView::ComposeName(const ro::sim::InventoryItem& held, bool from_link) {
  // ⚠ UN LIEN SE NOMME PAR SES PROPRES FAITS : la balise dit seule si l'objet se
  // décore, faute d'en porter la famille.
  ro::vfs::ItemNameFacts facts =
      from_link ? ro::vfs::ItemLinkNameFacts(linked_) : ItemNameFactsOf(held);
  // LE FORGERON : le fil n'en porte que l'identifiant. Tant que le serveur ne
  // l'a pas nommé, le nom dit « sans nom », en rouge — comme au client.
  if (const std::uint32_t creator = ro::sim::ItemCreatorId(held); creator != 0) {
    if (const std::string* known = creators_->facts().CreatorOf(creator); known != nullptr) {
      facts.creator_name = *known;
    } else {
      creators_->WantCreator(creator);
    }
  }
  segments_ = ro::vfs::ComposeItemName(facts, *items_, *words_);
  name_ = ro::vfs::ItemNameText(segments_);
  const bool colored = std::ranges::any_of(segments_, [](const ro::vfs::ItemNameSegment& segment) {
    return segment.role != ro::vfs::ItemNameRole::kOrdinary;
  });
  if (!colored) {
    return;
  }
  // ⚠ LES MORCEAUX POINTENT DANS `segments_`, qui ne bouge plus jusqu'au
  // prochain rafraîchissement.
  name_pieces_.reserve(segments_.size());
  for (const ro::vfs::ItemNameSegment& segment : segments_) {
    const ro::ui::DescriptionNameTone tone =
        segment.role == ro::vfs::ItemNameRole::kCreator ? ro::ui::DescriptionNameTone::kCreator
        : segment.role == ro::vfs::ItemNameRole::kUnknownCreator
            ? ro::ui::DescriptionNameTone::kUnknownCreator
            : ro::ui::DescriptionNameTone::kOrdinary;
    name_pieces_.push_back(ro::ui::DescriptionNamePiece{.text = segment.text, .tone = tone});
  }
}

void DescriptionView::ComposeInstance(const ro::sim::InventoryItem& held) {
  // ── LE FAMILIER D'UN ŒUF : ses cases sont sa fiche, pas des cartes ─────────
  if (view_.pet_egg) {
    const ro::sim::PetEggFacts pet = ro::sim::DecodePetEgg(held.cards);
    view_.pet_inside = pet.has_pet;
    view_.pet_renamed = pet.renamed;
    if (pet.intimacy_rank > 0 && words_ != nullptr) {
      view_.pet_intimacy = words_->pet_intimacy[static_cast<std::size_t>(pet.intimacy_rank - 1)];
    }
  } else if (!ro::sim::ItemIsForged(held)) {
    // ── LES CARTES : jusqu'à la dernière occupée, ou jusqu'au nombre
    // d'emplacements. ⚠ PAS LEUR COMPTE : une case vide au milieu décalerait
    // les suivantes.
    const ro::vfs::ItemEntry* entry = items_ != nullptr ? items_->Find(held.item_id) : nullptr;
    const std::size_t slots = entry != nullptr ? entry->card_slots : 0;
    std::size_t rows = std::min(slots, held.cards.size());
    for (std::size_t rank = 0; rank < held.cards.size(); ++rank) {
      if (held.cards[rank] != 0) {
        rows = std::max(rows, rank + 1);
      }
    }
    view_.card_slots = static_cast<int>(slots);
    cards_.reserve(rows);
    for (std::size_t rank = 0; rank < rows; ++rank) {
      ro::ui::DescriptionCardRow row{.id = held.cards[rank]};
      if (row.id != 0 && items_ != nullptr) {
        if (const ro::vfs::ItemEntry* card = items_->Find(row.id); card != nullptr) {
          row.name = card->name;
        }
      }
      if (row.id != 0 && icons_ != nullptr) {
        if (const ro::render::IconTexture& icon = icons_->Get(row.id); icon.usable()) {
          row.icon = ro::ui::SkinImage{.id = static_cast<ImTextureID>(icon.texture.name()),
                                       .width = static_cast<float>(icon.width_px),
                                       .height = static_cast<float>(icon.height_px)};
        }
      }
      cards_.push_back(row);
    }
  }
  view_.cards = cards_;

  // ── LES OPTIONS ALÉATOIRES ────────────────────────────────────────────────
  //
  // ⚠ LA RÉSERVE D'ABORD : les lignes désignent les libellés, et un vecteur qui
  // grandirait en route les déplacerait sous elles.
  option_texts_.reserve(held.options.size());
  options_.reserve(held.options.size());
  for (const ro::sim::ItemOption& option : held.options) {
    if (!option.filled()) {
      continue;
    }
    option_texts_.push_back(
        option_names_ != nullptr ? option_names_->Format(option.id, option.value) : std::string());
    options_.push_back(ro::ui::DescriptionOptionRow{.text = option_texts_.back(),
                                                    .id = option.id,
                                                    .value = option.value,
                                                    .param = option.param});
  }
  view_.options = options_;
}

void DescriptionView::RefreshOdds(std::uint32_t item_id) {
  odds_.clear();
  odds_names_.clear();
  odds_many_groups_ = false;
  const ro::vfs::PackageTable* table = packages_ != nullptr ? packages_->Find(item_id) : nullptr;
  if (table == nullptr) {
    return;
  }
  odds_many_groups_ = table->groups.size() > 1;
  std::size_t count = 0;
  for (const ro::vfs::PackageGroup& group : table->groups) {
    count += group.entries.size();
  }
  // ⚠ LA RÉSERVE D'ABORD : les lignes désignent les libellés, et un vecteur qui
  // grandirait en route les déplacerait sous elles.
  odds_names_.reserve(count);
  odds_.reserve(count);
  for (const ro::vfs::PackageGroup& group : table->groups) {
    for (const ro::vfs::PackageEntry& entry : group.entries) {
      // Le libellé de la table, ou le nom de l'objet quand elle n'en porte pas.
      std::string name = entry.name;
      if (name.empty() && !entry.is_monster && items_ != nullptr) {
        if (const ro::vfs::ItemEntry* known = items_->Find(entry.id); known != nullptr) {
          name = known->name;
        }
      }
      odds_names_.push_back(std::move(name));
      ro::ui::DescriptionOddsRow row{
          .id = entry.id,
          .monster = entry.is_monster,
          .name = odds_names_.back(),
          .percent = ro::vfs::PackagePercent(group.id, entry.weight, group.total),
          .weight = entry.weight,
          .total = group.total,
          .group = group.id};
      if (!entry.is_monster && icons_ != nullptr) {
        const ro::render::IconTexture& icon = icons_->Get(entry.id);
        if (icon.usable()) {
          row.icon = ro::ui::SkinImage{.id = static_cast<ImTextureID>(icon.texture.name()),
                                       .width = static_cast<float>(icon.width_px),
                                       .height = static_cast<float>(icon.height_px)};
        }
      }
      odds_.push_back(row);
    }
  }
}

void DescriptionView::RefreshSources(std::uint32_t item_id,
                                     const ro::ui::DescriptionWindowAction& asked) {
  if (asked.refresh_ordinary) {
    asker_.Forget(item_id, ItemFactsAsker::Kind::kOrdinary);
  }
  if (asked.refresh_boss) {
    asker_.Forget(item_id, ItemFactsAsker::Kind::kBoss);
  }
}

void DescriptionView::AskFacts(ro::net::Connection& link, bool window_open, std::uint32_t item_id,
                               std::uint32_t now_ms) {
  asker_.Ask(link, window_open, item_id, now_ms);
  ComposeFacts(item_id, link.is_open());
}

void DescriptionView::ComposeFacts(std::uint32_t item_id, bool online) {
  view_.server_tabs = online && item_id != 0;
  const auto drops = [&](bool boss, std::vector<ro::ui::DescriptionDropRow>& rows) {
    ro::ui::DescriptionDropsView shown;
    rows.clear();
    const ro::sim::ItemDropSources* told = asker_.facts().SourcesOf(
        item_id, boss ? ro::sim::ItemSourceScope::kBoss : ro::sim::ItemSourceScope::kOrdinary);
    if (told == nullptr) {
      shown.fetch = asker_.Asked(item_id, boss ? ItemFactsAsker::Kind::kBoss
                                               : ItemFactsAsker::Kind::kOrdinary)
                        ? ro::ui::DescriptionFetch::kWaiting
                        : ro::ui::DescriptionFetch::kUnasked;
      return shown;
    }
    shown.fetch = ro::ui::DescriptionFetch::kReady;
    rows.reserve(told->sources.size());
    for (const ro::sim::ItemDropSource& source : told->sources) {
      rows.push_back(ro::ui::DescriptionDropRow{.monster_id = source.monster_id,
                                                .name = source.name,
                                                .boss_rank = source.boss_rank,
                                                .mvp_reward = source.mvp_reward,
                                                .rate_hundredths = source.rate_hundredths});
    }
    shown.rows = rows;
    shown.truncated = told->truncated;
    shown.treasure_excluded = told->treasure_excluded;
    return shown;
  };
  view_.drops_ordinary = drops(false, ordinary_rows_);
  view_.drops_boss = drops(true, boss_rows_);

  ro::ui::DescriptionScriptsView scripts;
  combo_members_.clear();
  combos_.clear();
  if (const ro::sim::ItemScripts* told = asker_.facts().ScriptsOf(item_id); told != nullptr) {
    scripts.fetch = ro::ui::DescriptionFetch::kReady;
    scripts.unknown = told->status != 0;
    scripts.main = told->main;
    scripts.equip = told->equip;
    scripts.unequip = told->unequip;
    // ⚠ LES MEMBRES D'ABORD, TOUS : les combos désignent leurs listes, et une
    // liste de listes qui grandirait en route les déplacerait sous eux.
    combo_members_.reserve(told->combos.size());
    for (const ro::sim::ItemCombo& combo : told->combos) {
      std::vector<ro::ui::DescriptionComboMember>& members = combo_members_.emplace_back();
      for (const ro::sim::ItemComboMember& part : combo.members) {
        members.push_back(
            ro::ui::DescriptionComboMember{.item_id = part.item_id, .name = part.name});
      }
    }
    for (std::size_t rank = 0; rank < told->combos.size(); ++rank) {
      combos_.push_back(ro::ui::DescriptionComboView{.members = combo_members_[rank],
                                                     .script = told->combos[rank].script});
    }
    scripts.combos = combos_;
  } else if (asker_.Asked(item_id, ItemFactsAsker::Kind::kScripts)) {
    scripts.fetch = ro::ui::DescriptionFetch::kWaiting;
  }
  view_.scripts = scripts;
}

std::string_view DescriptionView::SourceNameOf(std::uint32_t monster_id) const {
  for (const auto* rows : {&ordinary_rows_, &boss_rows_}) {
    for (const ro::ui::DescriptionDropRow& row : *rows) {
      if (row.monster_id == monster_id) {
        return row.name;
      }
    }
  }
  return {};
}

void DescriptionView::NoteBack(std::uint32_t item_id) {
  back_name_.clear();
  if (item_id != 0 && items_ != nullptr) {
    if (const ro::vfs::ItemEntry* entry = items_->Find(item_id); entry != nullptr) {
      back_name_ = entry->name;
    }
  }
  view_.back_name = back_name_;
}

std::string_view DescriptionView::OddsNameOf(std::uint32_t id) const {
  for (const ro::ui::DescriptionOddsRow& row : odds_) {
    if (row.id == id) {
      return row.name;
    }
  }
  return {};
}

void DescriptionView::NoteCraftTag(std::string label, std::string tip) {
  craft_label_ = std::move(label);
  craft_tip_ = std::move(tip);
  // ⚠ LES POINTEURS APRÈS LES AFFECTATIONS : ils désignent ces chaînes-ci.
  view_.craft_label = craft_label_.empty() ? nullptr : craft_label_.c_str();
  view_.craft_tip = craft_tip_.empty() ? nullptr : craft_tip_.c_str();
}

void DescriptionView::NoteVoteShopTag(std::string label) {
  vote_shop_label_ = std::move(label);
  view_.vote_shop_label = vote_shop_label_.empty() ? nullptr : vote_shop_label_.c_str();
}

}  // namespace moonclient
