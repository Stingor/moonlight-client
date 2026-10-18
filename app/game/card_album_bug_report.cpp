#include "game/card_album_bug_report.h"

#include <algorithm>
#include <array>
#include <cstddef>
#include <cstdio>
#include <format>
#include <string>
#include <string_view>

#include "ro/core/i18n.h"
#include "ro/core/text.h"

namespace moonclient {
namespace {

using ro::ui::CardAlbumPocketView;
using ro::ui::CardAlbumTone;
// Les pochettes de la double page que le rapport cite. ⚠ UNE BORNE ET NON LA
// PAGE ENTIÈRE : une fenêtre agrandie en montre des dizaines, et le contexte
// tient en un kilo-octet côté serveur.
constexpr std::size_t kSpreadPocketsCited = 5;
// La recherche et le compte rendu, en octets avant échappement.
constexpr std::size_t kSearchBytes = 24;
constexpr std::size_t kNoticeBytes = 80;

[[nodiscard]] const char* ToneWord(CardAlbumTone tone) {
  switch (tone) {
    case CardAlbumTone::kDone:
      return "done";
    case CardAlbumTone::kWarning:
      return "warning";
    case CardAlbumTone::kRefusal:
      return "refusal";
  }
  return "?";
}

// La double page : son rang dans la vue, et ses premières pochettes.
[[nodiscard]] std::string SpreadFields(const ro::ui::CardAlbumWindowState& window,
                                       const ro::ui::CardAlbumView& view) {
  const std::size_t order_size = window.order.size();
  const std::size_t first =
      std::min(static_cast<std::size_t>(std::max(window.first, 0)), order_size);
  const std::size_t shown =
      std::min(static_cast<std::size_t>(std::max(window.per_spread, 0)), order_size - first);
  std::string fields =
      std::format(R"("spread":{{"first":{},"per_spread":{},"pockets":[)", first, window.per_spread);
  const std::size_t cited = std::min(shown, kSpreadPocketsCited);
  bool written = false;
  for (std::size_t index = 0; index < cited; ++index) {
    // ⚠ UN RANG HORS DU CATALOGUE se saute : l'ordre date de la dernière vue.
    const int rank = window.order[first + index];
    if (rank < 0 || static_cast<std::size_t>(rank) >= view.pockets.size()) {
      continue;
    }
    const CardAlbumPocketView& pocket = view.pockets[static_cast<std::size_t>(rank)];
    fields += std::format(R"({}{{"card_id":{},"unlocked":{},"reserve":{}}})", written ? "," : "",
                          pocket.card_id, JsonFlag(pocket.unlocked), pocket.reserve);
    written = true;
  }
  fields += std::format(R"(],"more":{}}})", shown - cited);
  return fields;
}

}  // namespace

BugReportContext CardAlbumBugReport(const ro::ui::CardAlbumWindowState& window,
                                    const ro::ui::CardAlbumView& view) {
  const std::string_view search(window.filter);
  std::string fields = std::format(
      R"("card_album":{{"pockets":{},"unlocked":{},"total_reserve":{},"hand":{},)"
      R"("blocked":{},"loading":{},"staff":{},"dropper_known":{},"auto_sacrifice":{},)"
      R"("search":"{}","sort_mode":{},"show_filter":{},"slot_filter":{},"nature_mask":{},)"
      R"("shown":{},"family":{{"unlocked":{},"total":{}}},)",
      view.pockets.size(), view.unlocked_count, view.total_reserve, view.hand.size(),
      JsonFlag(view.blocked), JsonFlag(view.loading), JsonFlag(view.staff),
      JsonFlag(view.dropper_known), JsonFlag(window.look.auto_sacrifice),
      JsonClipped(search, kSearchBytes), window.sort_mode, window.show_filter, window.slot_filter,
      window.nature_mask, window.order.size(), window.family_unlocked, window.family_total);
  fields += SpreadFields(window, view);
  // Le compte rendu que la fenêtre affichait, et la pochette qu'il désigne.
  if (!view.notice.empty()) {
    fields += std::format(R"(,"notice":{{"text":"{}","tone":"{}","card_id":{}}})",
                          JsonClipped(view.notice, kNoticeBytes), ToneWord(view.notice_tone),
                          view.highlight_card);
  }
  fields += '}';

  // Le libellé : la recherche quand il y en a une, sinon la complétion.
  std::array<char, kBugReportLabelBytes> label{};
  if (!search.empty()) {
    std::snprintf(
        label.data(), label.size(), ro::core::Tr("Album de cartes : recherche « %s »"),
        std::string(ro::core::WithoutDanglingUtf8(search.substr(0, kSearchBytes))).c_str());
  } else {
    std::snprintf(label.data(), label.size(),
                  ro::core::Tr("Album de cartes : %d / %d pochettes ouvertes"), view.unlocked_count,
                  static_cast<int>(view.pockets.size()));
  }
  return FieldsBugReport(label.data(), fields);
}

}  // namespace moonclient
