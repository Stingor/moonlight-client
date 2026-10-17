// « Voir dans l'album ». Voir l'en-tête.

#include "ro/ui/card_album_link.h"

#include <algorithm>

#include <imgui.h>

#include "ro/core/i18n.h"
#include "ro/ui/skin_tooltip.h"

namespace ro::ui {

CardAlbumEntry CardAlbumEntryOf(const CardAlbumLinkFacts& facts, std::uint32_t item_id,
                                bool known_card) {
  if (!facts.available || item_id == 0) {
    return CardAlbumEntry::kAbsent;
  }
  // Une pochette du catalogue est une carte, quoi qu'en disent les tables.
  if (std::ranges::binary_search(facts.pocket_ids, item_id)) {
    return CardAlbumEntry::kOffered;
  }
  if (!known_card && !std::ranges::binary_search(facts.card_ids, item_id)) {
    return CardAlbumEntry::kAbsent;
  }
  return facts.pockets_known ? CardAlbumEntry::kNoPocket : CardAlbumEntry::kOffered;
}

bool SeeInAlbumMenuItem(CardAlbumEntry entry) {
  if (entry == CardAlbumEntry::kAbsent) {
    return false;
  }
  // ⚠ GRISÉE PLUTÔT QU'ABSENTE : une carte sans pochette est une réponse, et
  // l'entrée manquante passerait pour un menu qui l'a oubliée.
  const bool offered = entry == CardAlbumEntry::kOffered;
  const bool chosen = ImGui::MenuItem(core::Tr("Voir dans l'album"), nullptr, false, offered);
  if (!offered && ImGui::IsItemHovered(ImGuiHoveredFlags_AllowWhenDisabled)) {
    SkinTooltipFormat("%s", core::Tr("Cette carte n'a pas de pochette dans l'album"));
  }
  return chosen;
}

}  // namespace ro::ui
