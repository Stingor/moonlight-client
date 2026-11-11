// LES TENUES DANS L'ÉDITEUR DE STYLE : publier le code court, reprendre une
// tenue par son code ou depuis la garde-robe du compte, ranger la sienne.
//
// 🔴 UNE TENUE REPRISE NE FAIT QU'UNE PROPOSITION : elle se verse dans les
// brouillons de l'éditeur, et rien ne part au serveur sans « Valider », avec
// toutes ses gardes ordinaires.

#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <optional>
#include <utility>
#include <vector>

#include "ro/core/i18n.h"
#include "ro/net/style_outfits.h"
#include "ro/sim/style_outfit.h"
#include "ro/sim/world.h"

#include "character/style_recipe.h"
#include "character/style_view.h"

namespace moonclient {
namespace {

// Ce que le serveur a répondu de travers, dit au joueur.
[[nodiscard]] const char* OutfitRefusal(const ro::sim::OutfitReply& reply) {
  switch (static_cast<ro::sim::OutfitResult>(reply.raw_result)) {
    case ro::sim::OutfitResult::kAccepted:
      return nullptr;
    case ro::sim::OutfitResult::kTooSoon:
      return ro::core::Tr("Le serveur demande d'attendre un peu entre deux demandes de tenue.");
    case ro::sim::OutfitResult::kMalformed:
      return ro::core::Tr("Le serveur n'a pas pu lire cette tenue.");
    case ro::sim::OutfitResult::kCap:
      return ro::core::Tr("La garde-robe du compte est pleine.");
    case ro::sim::OutfitResult::kUnavailable:
      return ro::core::Tr("Le serveur ne peut pas ranger les tenues pour l'instant.");
    case ro::sim::OutfitResult::kUnknownCode:
      return ro::core::Tr("Aucune tenue ne porte ce code.");
    case ro::sim::OutfitResult::kEmptySlot:
      return ro::core::Tr("Ce rangement est vide.");
  }
  return ro::core::Tr("Le serveur a refusé la demande de tenue.");
}

// Une demande qui ne part pas pour une raison de forme, dite au joueur.
[[nodiscard]] const char* SendRefusal(const ro::core::Result<void>& sent) {
  return sent ? nullptr : ro::core::Tr("La demande de tenue n'a pas pu partir.");
}

}  // namespace

void StyleView::RefreshOutfits(const ro::sim::World& world, std::uint32_t /*now_ms*/) {
  outfits_.Update(world);
  const ro::sim::OutfitReply& last = world.outfits().last();
  if (last.sequence != 0 && last.sequence != outfit_heard_) {
    outfit_heard_ = last.sequence;
    outfit_status_ = OutfitRefusal(last);
    // UN RANGEMENT A BOUGÉ : la liste connue ne dit plus la vérité.
    if (last.accepted() && (last.action == ro::sim::OutfitAction::kWardrobeStore ||
                            last.action == ro::sim::OutfitAction::kWardrobeClear)) {
      wardrobe_stale_ = true;
    }
  }
  if (std::optional<ro::sim::StyleOutfit> proposal = outfits_.TakeProposal()) {
    ProposeOutfit(*proposal, world);
  }

  const ro::sim::Actor* me = world.player();
  view_.outfits_served = StyleOutfits::Served(world);
  view_.has_outfit =
      me != nullptr && StyleOutfits::Capture(world.player_id(), world.styles()).has_value();
  outfit_code_shown_ = outfits_.published_code().empty()
                           ? std::string()
                           : ro::net::ShowOutfitCode(outfits_.published_code());
  view_.outfit_code = outfit_code_shown_.c_str();
  view_.outfit_status = outfit_status_;
  wardrobe_view_.clear();
  for (const ro::sim::StyleWardrobeEntry& entry : world.outfits().wardrobe()) {
    wardrobe_view_.push_back(
        ro::ui::PaletteWardrobeSlot{.slot = static_cast<int>(entry.slot), .name = entry.name});
  }
  view_.wardrobe = wardrobe_view_;
  view_.wardrobe_slots =
      std::min(world.session().style_wardrobe_slots, static_cast<int>(ro::net::kWardrobeSlotsMost));
  view_.wardrobe_listed = world.outfits().wardrobe_sequence() != 0 && !wardrobe_stale_;
}

void StyleView::ApplyOutfits(const ro::ui::PaletteEditorAction& asked, ro::net::Connection& link,
                             const ro::sim::World& world, std::uint32_t now_ms) {
  const auto captured = [&world] {
    return StyleOutfits::Capture(world.player_id(), world.styles());
  };
  using Verb = ro::ui::PaletteEditorVerb;
  const auto slot = static_cast<std::uint8_t>(std::max(asked.outfit_slot, 0));
  const bool slotted = asked.outfit_slot >= 0;
  if (asked.verb == Verb::kPublishOutfit) {
    if (const std::optional<ro::sim::StyleOutfit> outfit = captured()) {
      outfit_status_ = SendRefusal(outfits_.Publish(link, world, *outfit, now_ms));
    }
  } else if (asked.verb == Verb::kFetchOutfit) {
    const ro::core::Result<void> sent = outfits_.Fetch(link, world, asked.pasted, now_ms);
    outfit_status_ = sent ? nullptr : ro::core::Tr("Ce code court n'est pas bien formé.");
  } else if (asked.verb == Verb::kListWardrobe) {
    outfit_status_ = SendRefusal(outfits_.ListWardrobe(link, world, now_ms));
    wardrobe_stale_ = wardrobe_stale_ && outfit_status_ != nullptr;
  } else if (asked.verb == Verb::kWearOutfit) {
    if (slotted && outfits_.ProposeFromWardrobe(world, slot)) {
      if (std::optional<ro::sim::StyleOutfit> proposal = outfits_.TakeProposal()) {
        ProposeOutfit(*proposal, world);
      }
    }
  } else if (asked.verb == Verb::kStoreOutfit) {
    if (const std::optional<ro::sim::StyleOutfit> outfit = captured();
        outfit.has_value() && slotted) {
      outfit_status_ =
          SendRefusal(outfits_.Store(link, world, slot, asked.preset, *outfit, now_ms));
    }
  } else if (asked.verb == Verb::kClearOutfit) {
    if (slotted) {
      outfit_status_ = SendRefusal(outfits_.Clear(link, world, slot, now_ms));
    }
  } else if (wardrobe_stale_ && outfits_.CanSend(world, now_ms) &&
             outfits_.ListWardrobe(link, world, now_ms)) {
    // ⚠ UNE LISTE PÉRIMÉE SE REDEMANDE D'ELLE-MÊME, dès que la cadence le
    // permet et qu'aucune autre demande de tenue ne part à cette image.
    wardrobe_stale_ = false;
  }
}

void StyleView::ProposeOutfit(const ro::sim::StyleOutfit& outfit, const ro::sim::World& world) {
  ro::render::PaletteRecipe next = session_.recipe();
  if (const ro::sim::StyleRecipe* recipe = OutfitBodyFor(outfit, session_.body_key())) {
    next = FromWire(*recipe);
    session_.Load(next);
  }
  // Les objets portés, sous leur apparence affichée : ce sont les onglets.
  std::vector<WornItem> worn;
  for (const LayerTab& tab : tabs_) {
    if (tab.shown.item) {
      worn.push_back(WornItem{.layer = static_cast<ro::sim::StyleLayerKind>(tab.at.layer),
                              .view = tab.at.key});
    }
  }
  for (const ro::sim::StyleLayerRecipe& layer :
       OutfitLayersNow(outfit.layers, KeysAfter(next, world), worn)) {
    if (const std::optional<ro::render::LayerDye> dye = FromWire(layer)) {
      layers_.Put(*dye);
    }
  }
}

}  // namespace moonclient
