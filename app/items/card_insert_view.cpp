#include "items/card_insert_view.h"

#include <algorithm>
#include <utility>
#include <vector>

#include "ro/core/log.h"
#include "ro/net/card_composition.h"
#include "ro/sim/card_composition.h"
#include "ro/sim/chat_line.h"
#include "ro/sim/inventory.h"
#include "ro/sim/world.h"
#include "ro/vfs/item_link.h"
#include "ro/vfs/item_name.h"

#include "items/card_insert_line.h"
#include "items/inventory_view.h"

namespace moonclient {
namespace {

// LES COULEURS DES DEUX LIGNES, celles que le client passe à sa conversation :
// cyan pour la réussite, rouge pour le refus. En `0xRRGGBB`.
constexpr std::uint32_t kInsertedColor = 0x00FFFF;
constexpr std::uint32_t kRefusedColor = 0xFF0000;

// Toutes les piles de cet objet dans le sac. ⚠ TOUTES, et pas seulement celle
// qu'on sertit : c'est le compte que la fenêtre annonce.
[[nodiscard]] int StockOf(const ro::sim::Inventory& bag, std::uint32_t item_id) {
  int stock = 0;
  for (const ro::sim::InventoryItem& item : bag.items()) {
    if (item.item_id == item_id) {
      stock += item.amount;
    }
  }
  return stock;
}

}  // namespace

CardInsertView::CardInsertView(const ro::vfs::ItemTable* items, const ro::vfs::ItemWords* words,
                               ro::render::ItemIcons* icons)
    : items_(items), words_(words), icons_(icons) {}

std::string CardInsertView::NameOf(const ro::sim::InventoryItem& item) const {
  if (items_ == nullptr || words_ == nullptr) {
    return {};
  }
  ro::vfs::ItemNameFacts facts = ItemNameFactsOf(item);
  creators_.Fill(facts);
  return ro::vfs::ItemNameText(ro::vfs::ComposeItemName(facts, *items_, *words_));
}

ro::ui::SkinImage CardInsertView::IconOf(std::uint32_t item_id) const {
  if (icons_ == nullptr) {
    return {};
  }
  const ro::render::IconTexture& icon = icons_->Get(item_id);
  return ro::ui::SkinImage{
      .id = icon.usable() ? static_cast<ImTextureID>(icon.texture.name()) : ImTextureID{0},
      .width = static_cast<float>(icon.width_px),
      .height = static_cast<float>(icon.height_px)};
}

int CardInsertView::SlotsOf(std::uint32_t item_id) const {
  if (items_ == nullptr) {
    return 0;
  }
  const ro::vfs::ItemEntry* entry = items_->Find(item_id);
  return entry != nullptr ? entry->card_slots : 0;
}

void CardInsertView::Refresh(ro::sim::World& world) {
  const ro::sim::Inventory& bag = world.inventory();

  // ── LE SOUS-MENU RAPIDE ───────────────────────────────────────────────────
  //
  // ⚠ UNE CARTE PARTIE DEPUIS LA RÉPONSE NE SE PROPOSE PLUS : la sertir
  // enverrait une place vide.
  quick_names_.clear();
  quick_cards_.clear();
  const ro::sim::CompatibleCards& compatible = world.composition().compatible();
  for (const std::uint16_t card_slot : compatible.card_slots) {
    if (const ro::sim::InventoryItem* card = bag.Find(card_slot); card != nullptr) {
      quick_names_.push_back(NameOf(*card));
      quick_cards_.push_back(ro::ui::QuickInsertCardView{.card_slot = card_slot});
    }
  }
  for (std::size_t rank = 0; rank < quick_cards_.size(); ++rank) {
    quick_cards_[rank].name = quick_names_[rank];
  }

  // ── LA FENÊTRE ────────────────────────────────────────────────────────────
  row_names_.clear();
  rows_.clear();
  view_ = ro::ui::CardInsertView{};
  const ro::sim::CardComposition& composition = world.composition();
  if (!composition.open()) {
    return;
  }
  const ro::sim::CardOffer& offer = composition.offer();
  view_.offer_seq = composition.offer_seq();
  view_.listed = static_cast<int>(offer.equip_slots.size());
  if (const ro::sim::InventoryItem* card = bag.Find(offer.card_slot); card != nullptr) {
    view_.card_found = true;
    card_name_ = NameOf(*card);
    view_.card_name = card_name_;
    view_.card_icon = IconOf(card->item_id);
    view_.stock = StockOf(bag, card->item_id);
  }

  // Les pièces qu'on RETROUVE, et combien — pleines comprises.
  int found = 0;
  for (const std::uint16_t equip_slot : offer.equip_slots) {
    const ro::sim::InventoryItem* equip = bag.Find(equip_slot);
    if (equip == nullptr) {
      continue;  // partie depuis la réponse du serveur
    }
    ++found;
    const int total = SlotsOf(equip->item_id);
    const bool forged = ro::sim::ItemIsForged(*equip);
    const int used =
        static_cast<int>(ro::sim::ItemCardsInSlots(*equip, static_cast<std::size_t>(total)));
    // 🔴 UNE PIÈCE PLEINE N'EST PLUS PROPOSÉE. Ce n'est pas un filtre de
    // compatibilité — le serveur en est seul juge —, c'est SA borne : il ne la
    // listerait plus.
    if (!forged && total > 0 && used >= total) {
      continue;
    }
    row_names_.push_back(NameOf(*equip));
    rows_.push_back(ro::ui::CardInsertRowView{.equip_slot = equip_slot,
                                              .item_id = equip->item_id,
                                              .icon = IconOf(equip->item_id),
                                              .used_slots = used,
                                              .total_slots = total});
  }
  for (std::size_t rank = 0; rank < rows_.size(); ++rank) {
    rows_[rank].name = row_names_[rank];
  }
  view_.rows = rows_;

  // 🔴 TOUTES PLEINES : LA SESSION SE REFERME ICI. Le serveur n'enverra pas de
  // liste vide — il se tait —, donc personne d'autre ne le ferait.
  //
  // ⚠ SEULEMENT SI L'ON A RETROUVÉ DES PIÈCES : n'en retrouver aucune est un
  // échec de NOTRE lecture, et la fenêtre reste alors pour que le joueur la
  // ferme lui-même.
  if (found > 0 && rows_.empty()) {
    world.composition().Close();
    view_ = ro::ui::CardInsertView{};
  }
}

void CardInsertView::Apply(const ro::ui::CardInsertWindowAction& asked,
                           ro::ui::CardInsertWindowState& state, ro::net::Connection& link,
                           ro::sim::World& world) {
  switch (asked.verb) {
    case ro::ui::CardInsertVerb::kNothing:
      return;
    case ro::ui::CardInsertVerb::kClose:
      world.composition().Close();
      return;
    case ro::ui::CardInsertVerb::kInsert:
      break;
  }
  const std::uint16_t card_slot = world.composition().offer().card_slot;
  if (card_slot == 0 || asked.equip_slot == 0) {
    return;
  }
  // 🔴 LE STOCK D'AVANT LE LOT. La fenêtre borne `times` par lui ET par les
  // cases libres : chaque demande trouve donc une case et une carte.
  const int stock_before = view_.stock;
  const int times = std::clamp(asked.times, 1, ro::ui::kCardInsertMostTimes);
  for (int sent = 0; sent < times; ++sent) {
    if (auto done = ro::net::RequestInsertCard(link, card_slot, asked.equip_slot); !done) {
      ro::core::LogWarning("sertissage : demande non émise — {}", ro::core::Describe(done.error()));
      continue;
    }
    world.composition().AskInsert(card_slot, asked.equip_slot, world.inventory());
  }
  // IL RESTE DES CARTES : on redemande la liste, et la fenêtre reste ouverte
  // sur une liste à jour. ⚠ LE SERVEUR TRAITE DANS L'ORDRE : la liste qu'il
  // renverra tient compte des sertissages qui la précèdent sur le fil.
  if (stock_before - times > 0) {
    world.composition().Ask(card_slot);
    if (auto done = ro::net::RequestCardCandidates(link, card_slot); !done) {
      ro::core::LogWarning("sertissage : liste non demandée — {}",
                           ro::core::Describe(done.error()));
    }
    state.selected = 0;
    return;
  }
  // Plus une seule : la place de la carte ne désigne plus rien.
  world.composition().Close();
}

void CardInsertView::AskCompatible(std::uint16_t equip_slot, ro::net::Connection& link,
                                   ro::sim::World& world) {
  if (equip_slot == 0 || world.composition().compatible().equip_slot == equip_slot) {
    return;
  }
  world.composition().AskCompatible(equip_slot);
  if (auto done = ro::net::RequestCompatibleCards(link, equip_slot); !done) {
    ro::core::LogWarning("sertissage rapide : demande non émise — {}",
                         ro::core::Describe(done.error()));
  }
}

void CardInsertView::NoteVerdict(ro::sim::World& world, std::uint32_t now_ms) {
  // ⚠ CHAQUE ACCUSÉ A SA LIGNE, même arrivés dans la même image : un lot de
  // trois sertissages en écrit trois.
  const std::vector<ro::sim::CardInsertReport> reports = world.composition().TakeReports();
  // ⚠ SANS LES MOTS, LA LIGNE SE TAIT : une ligne vide n'apprend rien au
  // joueur.
  if (words_ == nullptr) {
    return;
  }
  // ⚠ SANS TABLE, AUCUN LIEN : la balise demande le nombre d'emplacements de
  // l'objet, et la ligne retombe sur la phrase générale.
  ItemTagOf tag_of;
  if (items_ != nullptr) {
    tag_of = [this](const ro::sim::InventoryItem& item) {
      const ro::vfs::ItemEntry* entry = items_->Find(item.item_id);
      return ro::vfs::BuildItemLink(ItemLinkOf(item), entry != nullptr ? entry->card_slots : 0);
    };
  }
  for (const ro::sim::CardInsertReport& report : reports) {
    std::string said = CardInsertLine(report, items_, *words_, tag_of);
    if (said.empty()) {
      continue;
    }
    const bool inserted = report.verdict == ro::sim::CardInsertVerdict::kInserted;
    // ⚠ L'ALLURE DES « OBJETS » — c'est un mouvement du sac, une carte en
    // sort, une pièce change —, mais la case générale : ce n'est ni un
    // ramassage ni un jet.
    world.RecordChat(ro::sim::ChatKind::kItem, ro::sim::ChatType::kPublic, 0, std::move(said),
                     now_ms, inserted ? kInsertedColor : kRefusedColor);
  }
}

}  // namespace moonclient
