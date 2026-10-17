// Les libellés des tables de filtre de la base de données, traduits à la
// LECTURE : les tables sont constantes, et les traduire à leur définition les
// figerait en français.

#include <algorithm>

#include "ro/core/ascii.h"
#include "ro/core/i18n.h"
#include "ro/ui/database_window.h"

namespace ro::ui {

const char* DatabaseItemTypeLabel(std::string_view server_type) {
  for (const DatabaseItemType& type : kDatabaseItemTypes) {
    if (core::AsciiEqualsFolded(type.server_type, server_type)) {
      return core::Tr(type.label);
    }
  }
  return nullptr;
}

const char* DatabaseItemPlaceLabel(std::string_view server_place) {
  for (const DatabaseItemPlace& group : kDatabaseItemPlaces) {
    if (std::ranges::any_of(group.server_places, [&](std::string_view named) {
          return core::AsciiEqualsFolded(named, server_place);
        })) {
      return core::Tr(group.label);
    }
  }
  return nullptr;
}

const char* DatabaseFilterChoice(int chosen, std::span<const char* const> values) {
  const int rank = std::clamp(chosen, kDatabaseAnyChoice, static_cast<int>(values.size()));
  return rank == kDatabaseAnyChoice ? core::Tr("Tous")
                                    : core::Tr(values[static_cast<std::size_t>(rank - 1)]);
}

}  // namespace ro::ui
