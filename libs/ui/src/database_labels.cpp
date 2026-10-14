// Les libellés des tables de filtre de la base de données, traduits à la
// LECTURE : les tables sont constantes, et les traduire à leur définition les
// figerait en français.

#include <algorithm>
#include <cctype>

#include "ro/core/i18n.h"
#include "ro/ui/database_window.h"

namespace ro::ui {
namespace {

[[nodiscard]] bool SameText(std::string_view left, std::string_view right) {
  return left.size() == right.size() &&
         std::ranges::equal(left, right, [](unsigned char a, unsigned char b) {
           return std::tolower(a) == std::tolower(b);
         });
}

}  // namespace

const char* DatabaseItemTypeLabel(std::string_view server_type) {
  for (const DatabaseItemType& type : kDatabaseItemTypes) {
    if (SameText(type.server_type, server_type)) {
      return core::Tr(type.label);
    }
  }
  return nullptr;
}

const char* DatabaseItemPlaceLabel(std::string_view server_place) {
  for (const DatabaseItemPlace& group : kDatabaseItemPlaces) {
    if (std::ranges::any_of(group.server_places, [&](std::string_view named) {
          return SameText(named, server_place);
        })) {
      return core::Tr(group.label);
    }
  }
  return nullptr;
}

}  // namespace ro::ui
