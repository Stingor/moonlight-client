#include "game/party_booking_bug_report.h"

#include <algorithm>
#include <array>
#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <format>
#include <string>
#include <string_view>

#include "ro/core/i18n.h"
#include "ro/core/text.h"

namespace moonclient {
namespace {

using ro::ui::PartyBookingFilter;
using ro::ui::PartyBookingRowView;
using ro::ui::PartyBookingSlots;
using ro::ui::PartyBookingView;
using ro::ui::PartyBookingWindowState;
// Les premières annonces de la liste : assez pour reconnaître une ligne mal
// composée, trop peu pour peser.
constexpr std::size_t kMostRowsReported = 3;
// Le plus long texte repris d'une annonce : un lieu, une liste de métiers. Le
// début suffit à reconnaître la ligne ; le contexte entier doit tenir loin
// sous la borne du serveur.
constexpr std::size_t kMostTextBytes = 32;

// Un texte borné, coupé sur une frontière de caractère, prêt pour le JSON.
[[nodiscard]] std::string Bounded(std::string_view text) {
  return JsonEscaped(ro::core::WithoutDanglingUtf8(text.substr(0, kMostTextBytes)));
}

[[nodiscard]] const char* FilterWord(PartyBookingFilter filter) {
  switch (filter) {
    case PartyBookingFilter::kNone:
      return "none";
    case PartyBookingFilter::kPlace:
      return "place";
    case PartyBookingFilter::kJob:
      return "job";
  }
  return "?";
}

// Les codes des cases remplies, dans leur ordre.
[[nodiscard]] std::string SlotCodes(const PartyBookingSlots& slots) {
  std::string codes = "[";
  bool first = true;
  for (const std::int16_t code : slots) {
    if (code == ro::ui::kPartyBookingEmptySlot) {
      continue;
    }
    codes += std::format("{}{}", first ? "" : ",", code);
    first = false;
  }
  codes += ']';
  return codes;
}

// Une annonce telle que sa rangée la montre, SANS SON AUTEUR : un nom de
// joueur n'aide pas à reproduire une ligne mal composée.
[[nodiscard]] std::string RowFields(const PartyBookingRowView& row) {
  return std::format(R"({{"levels":"{}","place":"{}","jobs":"{}"}})", Bounded(row.levels),
                     Bounded(row.place), Bounded(row.jobs));
}

// LA RECHERCHE : les critères tels que « Rechercher » les enverrait, et la
// liste reçue.
[[nodiscard]] std::string SearchFields(const PartyBookingWindowState& window,
                                       const PartyBookingView& view) {
  const ro::ui::PartyBookingAction asked = ro::ui::PartyBookingSearchAsked(window, view);
  std::string fields = std::format(
      R"("search":{{"level":{},"filter":"{}","map_code":{},"job":{},"searching":{},)"
      R"("searched":{},"more_on_server":{},"rows":{},"first_rows":[)",
      asked.level, FilterWord(window.filter), asked.map_code, asked.job, JsonFlag(view.searching),
      JsonFlag(view.searched), JsonFlag(view.more_on_server), view.rows.size());
  const std::size_t shown = std::min(view.rows.size(), kMostRowsReported);
  for (std::size_t rank = 0; rank < shown; ++rank) {
    if (rank > 0) {
      fields += ',';
    }
    fields += RowFields(view.rows[rank]);
  }
  fields += "]}";
  return fields;
}

// L'ANNONCE DU JOUEUR et les métiers qu'il y coche, sinon celle qu'il rédige —
// telle que « Publier » l'enverrait.
[[nodiscard]] std::string MineFields(const PartyBookingWindowState& window,
                                     const PartyBookingView& view) {
  if (view.has_mine) {
    return std::format(
        R"("mine":{{"index":{},"levels":"{}","place":"{}","jobs":{},"edited_jobs":{}}})",
        view.mine_index, Bounded(view.mine.levels), Bounded(view.mine.place),
        SlotCodes(view.mine_jobs), SlotCodes(window.edited_jobs));
  }
  const ro::ui::PartyBookingAction draft = ro::ui::PartyBookingRegisterAsked(window, view);
  return std::format(R"("draft":{{"level":{},"map_code":{},"jobs":{},"registering":{}}})",
                     draft.level, draft.map_code, SlotCodes(draft.jobs),
                     JsonFlag(view.registering));
}

}  // namespace

BugReportContext PartyBookingBugReport(const PartyBookingWindowState& window,
                                       const PartyBookingView& view) {
  const std::string fields =
      std::format(R"("party_booking":{{"levels":[{},{}],{},{}}})", view.least_level,
                  view.most_level, SearchFields(window, view), MineFields(window, view));

  // Le libellé : l'annonce du joueur, sinon la liste reçue, sinon la fenêtre.
  std::array<char, kBugReportLabelBytes> label{};
  if (view.has_mine) {
    std::snprintf(label.data(), label.size(),
                  ro::core::Tr("Recherche de groupe : mon annonce (%s)"),
                  std::string(view.mine.place).c_str());
  } else if (view.searched) {
    std::snprintf(label.data(), label.size(), ro::core::Tr("Recherche de groupe : %u annonces"),
                  static_cast<unsigned>(view.rows.size()));
  } else {
    std::snprintf(label.data(), label.size(), "%s", ro::core::Tr("Recherche de groupe"));
  }
  return FieldsBugReport(std::string(ro::core::WithoutDanglingUtf8(label.data())), fields);
}

}  // namespace moonclient
