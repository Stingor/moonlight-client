#include "game/cash_shop_stage.h"

#include <algorithm>
#include <array>
#include <cstdio>
#include <span>
#include <string_view>
#include <utility>

#include <imgui.h>

#include "ro/core/log.h"
#include "ro/net/cash_shop.h"
#include "ro/vfs/cash_shop_words.h"

namespace moonclient {
namespace {

// Les deux couches comptent les mêmes onglets et le même plafond : `ui` ne voit
// pas `sim`, et c'est ici qu'elles se rencontrent.
static_assert(ro::ui::kCashShopTabCount == ro::sim::kCashShopTabCount);
static_assert(static_cast<std::uint32_t>(ro::ui::kCashShopMostPerLine) ==
              ro::sim::kCashShopMostPerItem);

// L'onglet que le bouton du menu demande : le premier, celui des nouveautés —
// comme le client d'origine, dont le bouton, l'icône flottante et la touche
// demandent tous celui-là.
constexpr std::uint32_t kMenuTab = 0;

// Le préfixe que la boutique retire du nom : l'onglet et la vignette disent
// déjà qu'on est dans les costumes.
constexpr std::string_view kCostumePrefix = "Costume ";

constexpr std::size_t kNumberBytes = 16;

// ── LES RÉSULTATS D'ACHAT DU SERVEUR, et le message que le client en tire ───
//
// ⚠ DE UN À SEPT, TOUS TOMBENT SUR « L'ÉCHANGE A ÉCHOUÉ », et l'imprécision est
// celle du client : son traitement n'a pas d'autre message pour eux — pas
// assez de points, item inconnu, surpoids, sac plein compris. La table n'a rien
// de plus précis pour ces codes ; notre fenêtre dirait sinon autre chose que la
// ligne que le client écrit.
constexpr std::uint16_t kResultSomeFailed = 8;
constexpr std::uint16_t kResultRuneOvercount = 9;
constexpr std::uint16_t kResultEachItemOvercount = 10;
constexpr std::uint16_t kResultUnknown = 11;
constexpr std::uint16_t kResultBusy = 12;
constexpr std::uint16_t kResultBillingExpired = 303;
constexpr std::uint16_t kResultBillingError = 304;

[[nodiscard]] ro::vfs::CashShopRefusal RefusalOf(std::uint16_t result) {
  switch (result) {
    case kResultSomeFailed:
      return ro::vfs::CashShopRefusal::kSomeFailed;
    case kResultRuneOvercount:
      return ro::vfs::CashShopRefusal::kRuneOvercount;
    case kResultEachItemOvercount:
      return ro::vfs::CashShopRefusal::kEachItemOvercount;
    case kResultUnknown:
      return ro::vfs::CashShopRefusal::kUnknown;
    case kResultBusy:
      return ro::vfs::CashShopRefusal::kBusy;
    case kResultBillingExpired:
      return ro::vfs::CashShopRefusal::kBillingExpired;
    case kResultBillingError:
      return ro::vfs::CashShopRefusal::kBillingError;
    default:
      return ro::vfs::CashShopRefusal::kDealFailed;
  }
}

void Report(const char* what, const ro::core::Result<void>& sent) {
  if (!sent) {
    ro::core::LogWarning("boutique de vote : {} non émis — {}", what,
                         ro::core::Describe(sent.error()));
  }
}

}  // namespace

void CashShopStage::See(const ro::vfs::ItemTable* items, ro::render::ItemIcons* icons,
                        ro::render::ActorWardrobe* wardrobe) {
  items_ = items;
  icons_ = icons;
  doll_.emplace(wardrobe);
  names_.clear();
  images_.clear();
  cards_composed_ = false;
}

void CashShopStage::StartSession() {
  shop_ = ro::sim::CashShop();
  hat_effects_.Clear();
  catalogue_requested_ = false;
  pending_item_ = 0;
  purchase_handled_ = 0;
  cards_composed_ = false;
  offers_.clear();
  offers_catalogue_seq_ = 0;
}

void CashShopStage::RequestCatalogueOnce(ro::net::Connection& link) {
  if (catalogue_requested_ || !link.is_open()) {
    return;
  }
  catalogue_requested_ = true;
  shop_.ExpectCatalogue();
  Report("demande du catalogue", ro::net::RequestCashShopCatalogue(link));
}

void CashShopStage::Toggle(ro::net::Connection& link) {
  if (link.is_open()) {
    Report("demande d'ouverture", ro::net::RequestCashShopOpen(link, kMenuTab));
  }
}

std::optional<CashShopStage::Offer> CashShopStage::Find(std::uint32_t item_id) const {
  if (item_id == 0) {
    return std::nullopt;
  }
  for (std::size_t tab = 0; tab < ro::sim::kCashShopTabCount; ++tab) {
    if (!ro::ui::CashShopTabShown(tab)) {
      continue;
    }
    for (const ro::sim::CashShopItem& item : shop_.tab(tab)) {
      if (item.item_id == item_id) {
        return Offer{.tab = tab, .price = item.price};
      }
    }
  }
  return std::nullopt;
}

std::span<const ro::ui::VoteShopOffer> CashShopStage::offers() {
  if (offers_catalogue_seq_ != shop_.catalogue_seq()) {
    offers_catalogue_seq_ = shop_.catalogue_seq();
    offers_.clear();
    for (std::size_t tab = 0; tab < ro::sim::kCashShopTabCount; ++tab) {
      if (!ro::ui::CashShopTabShown(tab)) {
        continue;
      }
      for (const ro::sim::CashShopItem& item : shop_.tab(tab)) {
        offers_.push_back(ro::ui::VoteShopOffer{.item_id = item.item_id, .price = item.price});
      }
    }
    // ⚠ LE PREMIER ONGLET L'EMPORTE pour un item vendu dans deux : c'est celui
    // que `Find` rend, et donc celui où l'ouverture emmène le joueur.
    std::ranges::stable_sort(offers_, {}, &ro::ui::VoteShopOffer::item_id);
    const auto repeated = std::ranges::unique(offers_, {}, &ro::ui::VoteShopOffer::item_id);
    offers_.erase(repeated.begin(), repeated.end());
  }
  return offers_;
}

bool CashShopStage::OpenWithItem(std::uint32_t item_id, ro::ui::CashShopWindowState& state,
                                 ro::net::Connection& link) {
  const std::optional<Offer> offer = Find(item_id);
  if (!offer.has_value()) {
    return false;
  }
  if (shop_.open()) {
    ro::ui::RevealCashShopItem(state, item_id, offer->tab, offer->price, NameOf(item_id));
    return true;
  }
  // C'EST LE SERVEUR QUI OUVRE, et c'est chez lui que la session d'achat du
  // personnage commence : l'item attend sa réponse.
  pending_item_ = item_id;
  if (link.is_open()) {
    Report("demande d'ouverture",
           ro::net::RequestCashShopOpen(link, static_cast<std::uint32_t>(offer->tab)));
  }
  return true;
}

std::string_view CashShopStage::NameOf(std::uint32_t item_id) {
  auto [found, fresh] = names_.try_emplace(item_id);
  if (fresh) {
    const ro::vfs::ItemEntry* entry = items_ != nullptr ? items_->Find(item_id) : nullptr;
    if (entry != nullptr && !entry->name.empty()) {
      std::string_view name = entry->name;
      if (name.starts_with(kCostumePrefix)) {
        name.remove_prefix(kCostumePrefix.size());
      }
      found->second = std::string(name);
    } else {
      std::array<char, kNumberBytes> text{};
      std::snprintf(text.data(), text.size(), "#%u", static_cast<unsigned>(item_id));
      found->second = text.data();
    }
  }
  return found->second;
}

void CashShopStage::NoteDrawn(const std::vector<std::uint32_t>& item_ids) {
  if (icons_ == nullptr) {
    return;
  }
  if (icons_->generation() != images_generation_) {
    images_generation_ = icons_->generation();
    images_.clear();
    cards_composed_ = false;
  }
  for (const std::uint32_t item_id : item_ids) {
    if (images_.contains(item_id)) {
      continue;
    }
    // L'IMAGE DE COLLECTION, bien plus grande que l'icône du sac : c'est ce que
    // montre la boutique, et la raison d'être de sa vignette. L'icône ne sert
    // que quand l'art manque.
    const ro::render::IconTexture* icon = &icons_->GetCollection(item_id);
    if (!icon->usable()) {
      icon = &icons_->Get(item_id);
    }
    images_.emplace(
        item_id,
        ro::ui::SkinImage{
            .id = icon->usable() ? static_cast<ImTextureID>(icon->texture.name()) : ImTextureID{0},
            .width = static_cast<float>(icon->width_px),
            .height = static_cast<float>(icon->height_px)});
  }
}

ro::vfs::ItemLink CashShopStage::DescribedLinkOf(std::uint32_t item_id) const {
  ro::vfs::ItemLink link{.item_id = item_id};
  for (std::size_t tab = 0; tab < ro::sim::kCashShopTabCount; ++tab) {
    for (const ro::sim::CashShopItem& item : shop_.tab(tab)) {
      if (item.item_id == item_id) {
        link.view_id = item.view_sprite;
        link.wearable_at = item.equip_location;
        return link;
      }
    }
  }
  return link;
}

void CashShopStage::Close(ro::ui::CashShopWindowState& state) {
  shop_.Close();
  state.open = false;
  // 🔴 L'ATTENTE MEURT AVEC LA FERMETURE. L'ouverture BASCULE : si le joueur
  // ferme entre la demande et la réponse, cette réponse-là referme — et l'item
  // en attente serait montré à la PROCHAINE ouverture, voulue pour autre chose.
  pending_item_ = 0;
}

void CashShopStage::ComposeCards() {
  for (std::size_t tab = 0; tab < ro::sim::kCashShopTabCount; ++tab) {
    std::vector<ro::ui::CashShopCard>& cards = cards_[tab];
    cards.clear();
    for (const ro::sim::CashShopItem& item : shop_.tab(tab)) {
      ro::ui::CashShopCard card{
          .item_id = item.item_id,
          .price = item.price,
          .view_sprite = item.view_sprite,
          .equip_location = item.equip_location,
          .name = NameOf(item.item_id),
          .hat_effect = hat_effects_.OrdinalOf(item.item_id) != 0,
          // Une pièce qui se porte, ou un effet de chapeau :
          // le pantin montre l'un ou l'autre.
          .wearable_preview =
              doll_.has_value() && ((item.view_sprite != 0 && doll_->Shows(item.equip_location)) ||
                                    hat_effects_.OrdinalOf(item.item_id) != 0)};
      if (const auto image = images_.find(item.item_id); image != images_.end()) {
        card.image = image->second;
      }
      cards.push_back(card);
    }
  }
  cards_catalogue_seq_ = shop_.catalogue_seq();
  cards_hat_revision_ = hat_effects_.revision();
  cards_images_ = images_.size();
  cards_composed_ = true;
}

void CashShopStage::ComposeView(const ro::ui::CashShopWindowState& state,
                                const ro::vfs::ItemWords& words) {
  if (!cards_composed_ || cards_catalogue_seq_ != shop_.catalogue_seq() ||
      cards_hat_revision_ != hat_effects_.revision() || cards_images_ != images_.size()) {
    ComposeCards();
  }
  view_.title = ro::vfs::CashShopName(words);
  view_.currency = ro::vfs::CashShopCurrency(words);
  view_.event_points_label = ro::vfs::CashShopEventPoints(words);
  view_.use_event_points_label = ro::vfs::CashShopUseEventPoints(words);
  view_.vote_points = shop_.vote_points();
  view_.event_points = shop_.event_points();
  for (std::size_t tab = 0; tab < ro::ui::kCashShopTabCount; ++tab) {
    view_.tabs[tab] = cards_[tab];
  }
  view_.purchase = ro::ui::CashShopPurchaseNote::kNone;
  view_.refusal = {};
  if (shop_.purchase_seq() != state.purchase_seen) {
    if (shop_.purchase_result() == ro::sim::kCashShopBuySuccess) {
      view_.purchase = ro::ui::CashShopPurchaseNote::kBought;
    } else {
      view_.purchase = ro::ui::CashShopPurchaseNote::kRefused;
      view_.refusal = ro::vfs::CashShopRefusalText(words, RefusalOf(shop_.purchase_result()));
    }
  }
}

void CashShopStage::Advance(ro::ui::CashShopWindowState& state,
                            const ro::ui::CashShopWindowAction& answer, ro::net::Connection& link,
                            const ro::sim::World& world, const ro::vfs::ItemWords& words,
                            ActorStyles& dressing, std::uint32_t now_ms) {
  NoteDrawn(answer.drawn);
  // ── LE CLIC DROIT D'UNE CARTE : sa description ──────────────────────────
  if (answer.item.gesture == ro::ui::ChatLinkGesture::kDescribe && answer.item.item_id != 0) {
    described_ = DescribedLinkOf(answer.item.item_id);
  }

  // ── L'ACHAT ── ⚠ AVANT LA FERMETURE : l'ordre du fil garantit que le
  // serveur reçoit l'achat, puis seulement la fin de la session.
  if (!answer.purchase.empty() && link.is_open() && shop_.open()) {
    std::vector<ro::net::CashShopPurchaseLine> lines;
    lines.reserve(answer.purchase.size());
    for (const ro::ui::CashShopCartLine& line : answer.purchase) {
      lines.push_back(
          ro::net::CashShopPurchaseLine{.item_id = line.item_id,
                                        .amount = static_cast<std::uint32_t>(line.amount),
                                        .tab = static_cast<std::uint16_t>(line.tab)});
    }
    Report("achat", ro::net::RequestCashShopPurchase(link, answer.event_points_spent, lines));
    if (answer.close_after_purchase) {
      Close(state);
    }
  }

  // ── LA FERMETURE PAR LA FENÊTRE : la croix, ou Échap ────────────────────
  //
  // ⚠ SEULEMENT POUR L'OUVERTURE QUE LA FENÊTRE MONTRE : à l'image où le
  // serveur ouvre, la fenêtre est encore fermée, et ce n'est pas un refus.
  if (shop_.open() && state.opening_seen == shop_.opening_seq() && (answer.close || !state.open)) {
    Close(state);
  }
  // 🔴 LA FERMETURE SE DIT AU SERVEUR, quelle qu'en soit la cause — la fenêtre,
  // l'achat en un clic, la réponse qui bascule, le changement de carte. Et
  // l'item en attente meurt avec elle.
  if (shop_.TakeFarewell()) {
    pending_item_ = 0;
    if (link.is_open()) {
      Report("fermeture", ro::net::SayCashShopClosed(link));
    }
  }

  // ── L'OUVERTURE QUI ARRIVE ────────────────────────────────────────────────
  if (shop_.open() && state.opening_seen != shop_.opening_seq()) {
    ro::ui::OpenCashShopOn(state, shop_.opening_seq(), shop_.purchase_seq(), shop_.opening_tab());
    // 🔴 LA DEMANDE DE CATALOGUE PART AUSSI À L'OUVERTURE : sans elle, les
    // onglets resteraient vides si la première demande s'est perdue. Le
    // serveur n'y répond qu'une fois par session, la redire ne coûte rien.
    shop_.ExpectCatalogue();
    Report("demande du catalogue", ro::net::RequestCashShopCatalogue(link));
    catalogue_requested_ = true;
    // Ouverte depuis un lien d'item : il passe devant l'onglet que le serveur a
    // nommé — le joueur a désigné un ITEM, pas une catégorie.
    if (const std::uint32_t item_id = std::exchange(pending_item_, 0); item_id != 0) {
      if (const std::optional<Offer> offer = Find(item_id); offer.has_value()) {
        ro::ui::RevealCashShopItem(state, item_id, offer->tab, offer->price, NameOf(item_id));
      }
    }
  }
  state.open = shop_.open();

  // UN ACHAT RÉUSSI VIDE LE PANIER.
  if (shop_.purchase_seq() != purchase_handled_) {
    purchase_handled_ = shop_.purchase_seq();
    if (shop_.purchase_result() == ro::sim::kCashShopBuySuccess) {
      state.cart.clear();
    }
  }

  // LE PANTIN DE L'APERÇU, composé pour la pièce que la fenêtre survolait.
  if (doll_.has_value()) {
    doll_->Refresh(world, state.doll_view, state.doll_location,
                   hat_effects_.OrdinalOf(state.doll_item) != 0, state.doll, dressing, now_ms);
  }

  if (shop_.open()) {
    ComposeView(state, words);
  }
}

ro::render::VignetteFraming CashShopStage::doll_framing() const noexcept {
  return doll_.has_value() ? doll_->framing() : ro::render::VignetteFraming{};
}

}  // namespace moonclient
