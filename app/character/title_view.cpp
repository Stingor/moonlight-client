#include "character/title_view.h"

#include <algorithm>
#include <array>
#include <cstdio>
#include <optional>
#include <string_view>
#include <utility>

#include "ro/core/i18n.h"
#include "ro/core/log.h"
#include "ro/net/actor_name.h"
#include "ro/net/title.h"
#include "ro/sim/world.h"

namespace moonclient {
namespace {

// La place d'un libellé numéroté, terminateur compris.
constexpr std::size_t kNumberedTitleBytes = 96;

// Le libellé que la table donne, ou rien.
[[nodiscard]] std::string_view TableLabel(std::int32_t title_id,
                                          const ro::render::TitleTable* labels) {
  return labels != nullptr ? labels->Find(title_id) : std::string_view();
}

}  // namespace

std::string ShownTitle(std::int32_t title_id, const ro::render::TitleTable* labels) {
  const std::string_view named = TableLabel(title_id, labels);
  if (!named.empty()) {
    return std::string(named);
  }
  std::array<char, kNumberedTitleBytes> numbered = {};
  std::snprintf(numbered.data(), numbered.size(), ro::core::Tr("Titre #%d"), title_id);
  return numbered.data();
}

std::vector<NamedTitle> OwnedTitles(std::span<const std::uint32_t> rewarded,
                                    const ro::vfs::AchievementTitles* rewards,
                                    const ro::render::TitleTable* labels) {
  std::vector<std::int32_t> title_ids;
  if (rewards != nullptr) {
    for (const std::uint32_t achievement_id : rewarded) {
      const std::optional<std::int32_t> title_id = rewards->TitleOf(achievement_id);
      if (title_id.has_value() && std::ranges::find(title_ids, *title_id) == title_ids.end()) {
        title_ids.push_back(*title_id);
      }
    }
  }
  // ⚠ LE NUMÉRO DÉPARTAGE deux libellés égaux — deux titres que la table ne
  // nomme pas —, pour qu'une image ne les échange pas avec la suivante.
  std::ranges::sort(title_ids, [labels](std::int32_t left, std::int32_t right) {
    const std::string_view left_label = TableLabel(left, labels);
    const std::string_view right_label = TableLabel(right, labels);
    if (left_label != right_label) {
      return left_label < right_label;
    }
    return left < right;
  });
  std::vector<NamedTitle> named;
  named.reserve(title_ids.size());
  for (const std::int32_t title_id : title_ids) {
    named.push_back(NamedTitle{title_id, ShownTitle(title_id, labels)});
  }
  return named;
}

TitleView::TitleView(const ro::vfs::AchievementTitles* rewards,
                     const ro::render::TitleTable* labels)
    : rewards_(rewards), labels_(labels) {}

void TitleView::Refresh(const ro::sim::TitleBook& book) {
  std::vector<NamedTitle> named = OwnedTitles(book.rewarded_achievements(), rewards_, labels_);
  owned_labels_.clear();
  owned_labels_.reserve(named.size());
  owned_.clear();
  owned_.reserve(named.size());
  // ⚠ LES LIBELLÉS D'ABORD, LES VUES ENSUITE : une vue prise pendant que le
  // vecteur grandit pointerait dans un tampon déjà rendu.
  for (NamedTitle& title : named) {
    owned_labels_.push_back(std::move(title.label));
  }
  for (std::size_t rank = 0; rank < named.size(); ++rank) {
    owned_.push_back(ro::ui::CharacterTitleView{named[rank].title_id, owned_labels_[rank]});
  }
  equipped_label_ = book.equipped() != 0 ? ShownTitle(book.equipped(), labels_) : std::string();
  view_ = ro::ui::CharacterTitlesView{
      .equipped = book.equipped(), .equipped_label = equipped_label_, .owned = owned_};
}

void TitleView::Apply(const ro::ui::CharacterWindowAction& asked, bool tab_shown,
                      ro::net::Connection& link, const ro::sim::World& world) {
  if (tab_shown && !own_card_asked_ && link.is_open()) {
    const ro::sim::Actor* player = world.player();
    if (player != nullptr && !player->card.has_value()) {
      own_card_asked_ = true;
      if (const ro::core::Result<void> sent = ro::net::RequestActorName(link, player->id); !sent) {
        ro::core::LogWarning("titres : carte de nom non demandée — {}",
                             ro::core::Describe(sent.error()));
      }
    }
  }
  if (!asked.equip_title.has_value()) {
    return;
  }
  if (const ro::core::Result<void> sent = ro::net::RequestChangeTitle(link, *asked.equip_title);
      !sent) {
    ro::core::LogWarning("titres : changement non demandé — {}", ro::core::Describe(sent.error()));
  }
}

}  // namespace moonclient
