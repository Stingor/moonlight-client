#include "world/item_obtain_toast.h"

#include <algorithm>
#include <cstdint>
#include <span>
#include <utility>

#include "ro/sim/world.h"
#include "ro/vfs/item_words.h"

#include "game/skin_image_of.h"
#include "items/inventory_view.h"
#include "world/item_obtain_rules.h"

namespace moonclient {
namespace {

// L'image d'un objet, telle que le dessin la prend : une texture inutilisable
// rend une image vide, et le bandeau recule alors son texte comme le client le
// fait quand il n'a rien à poser devant.
[[nodiscard]] ro::ui::SkinImage IconOf(ro::render::ItemIcons* icons, std::uint32_t item_id) {
  if (icons == nullptr) {
    return ro::ui::SkinImage{};
  }
  return SkinImageOf(icons->Get(item_id));
}

}  // namespace

ObtainedLine ItemObtainToast::LineOf(const ro::sim::InventoryItem& picked,
                                     std::uint32_t obtained_ms,
                                     const CreatorNames& creators) const {
  ObtainedLine line;
  line.item_id = picked.item_id;
  // Le client affiche la quantité telle qu'elle vient, et une quantité nulle
  // n'aurait pas de sens dans un bandeau qui annonce un gain.
  line.amount = std::max<std::int32_t>(picked.amount, 1);
  line.refine_level = picked.refine_level;
  line.identified = picked.identified;
  line.damaged = picked.damaged;
  line.born_ms = obtained_ms;
  line.name = ReportedItemName(picked, creators, *items_, *words_);
  RefreshSuffix(line);
  return line;
}

void ItemObtainToast::RefreshSuffix(ObtainedLine& line) const {
  line.suffix = ro::vfs::ObtainedText(*words_, line.amount);
}

void ItemObtainToast::Notice(const ro::sim::World& world, const ro::ui::ItemObtainToastLook& look,
                             const CreatorNames& creators, std::uint32_t now_ms) {
  const ro::sim::Inventory& bag = world.inventory();
  const std::uint64_t last_seq = bag.last_obtained_seq();
  // Le rang a reculé : c'est une autre session, et le témoin d'avant ne
  // désigne plus rien. Le remettre à zéro vaut mieux que de taire tout ce qui
  // suivra.
  if (last_seq < seen_seq_) {
    seen_seq_ = 0;
  }
  if (AwaitsCreator(bag, seen_seq_, creators, now_ms) ||
      ro::sim::AwaitsTradeVerdict(bag, seen_seq_, now_ms)) {
    return;
  }
  for (const ro::sim::ObtainedItem& entry : bag.obtained()) {
    // UN DÉPÔT D'ÉCHANGE RENDU N'EST PAS UN GAIN : l'échange le dit, une fois.
    if (entry.seq <= seen_seq_ ||
        ro::sim::VerdictOf(entry, now_ms) == ro::sim::ObtainedVerdict::kTradeReturn) {
      continue;
    }
    const std::size_t at = MergeOrPushObtained(
        lines_, LineOf(entry.picked, entry.obtained_ms, creators), look.merge_same);
    // Une ligne fusionnée porte maintenant la somme : son suffixe ne dit plus
    // la bonne quantité.
    RefreshSuffix(lines_[at]);
  }
  seen_seq_ = last_seq;
}

void ItemObtainToast::ComposePreview() {
  if (preview_.item_id == ro::ui::kToastPreviewItemId) {
    return;
  }
  ro::sim::InventoryItem shown;
  shown.item_id = ro::ui::kToastPreviewItemId;
  shown.amount = 1;
  shown.identified = true;
  preview_ = LineOf(shown, 0, CreatorNames());
}

std::span<const ro::ui::ItemObtainToastEntry> ItemObtainToast::Shown(
    std::uint32_t now_ms, const ro::ui::ItemObtainToastLook& look) {
  ExpireObtainedLines(lines_, now_ms, ObtainedLifeMs(look.duration_ms));
  CapObtainedLines(lines_, look.most_lines);
  shown_.clear();

  // ⚠ LA PILE NE BOUGE PLUS À PARTIR D'ICI : c'est ce qui rend les vues sûres.
  if (lines_.empty()) {
    // Déverrouillé et rien à montrer : la ligne d'exemple. Sans elle, placer
    // l'ancre supposerait d'aller ramasser un objet toutes les cinq secondes
    // pour voir ce qu'on règle.
    if (look.locked) {
      return {};
    }
    ComposePreview();
    shown_.push_back(ro::ui::ItemObtainToastEntry{
        .name = preview_.name,
        .suffix = preview_.suffix,
        .icon = look.show_icon ? IconOf(icons_, preview_.item_id) : ro::ui::SkinImage{},
        .damaged = false});
    return shown_;
  }

  shown_.reserve(lines_.size());
  const std::size_t count = lines_.size();
  for (std::size_t rank = 0; rank < count; ++rank) {
    // La pile est tenue dans l'ordre d'arrivée ; c'est ici qu'elle se renverse
    // quand le réglage veut la plus récente en tête.
    const ObtainedLine& line = look.newest_on_top ? lines_[count - 1 - rank] : lines_[rank];
    shown_.push_back(ro::ui::ItemObtainToastEntry{
        .name = line.name,
        .suffix = line.suffix,
        .icon = look.show_icon ? IconOf(icons_, line.item_id) : ro::ui::SkinImage{},
        .damaged = line.damaged});
  }
  return shown_;
}

void ItemObtainToast::Announce(ToastNotice notice, std::uint32_t now_ms) {
  static_cast<void>(MergeOrPushObtained(lines_,
                                        ObtainedLine{.item_id = notice.icon_item_id,
                                                     .born_ms = now_ms,
                                                     .name = std::move(notice.name),
                                                     .suffix = std::move(notice.suffix)},
                                        /*merge_same=*/false));
}

void ItemObtainToast::Forget() {
  lines_.clear();
  shown_.clear();
  // 🔴 LE TÉMOIN DE RANG NE RECULE PAS, ET C'EST TOUT L'ENJEU : le journal du
  // sac SURVIT au changement de carte, si bien qu'un témoin remis à zéro ferait
  // paraître d'un coup, au premier pas sur la carte suivante, les vingt derniers
  // ramassages de la précédente. Ce qu'on oublie, c'est ce qui est à l'écran ;
  // ce qu'on a déjà vu reste vu.
  //
  // Une VRAIE nouvelle session se reconnaît autrement : le rang du sac y repart
  // de zéro, donc en dessous du témoin, et `Notice` le remet alors lui-même.
}

}  // namespace moonclient
