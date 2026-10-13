// L'emblème du grade d'un résultat. Voir l'en-tête.

#include "game/store_search_grade.h"

#include <string_view>

namespace moonclient {
namespace {

// Le dossier des emblèmes et leur nom, de part et d'autre du numéro du grade.
constexpr std::string_view kGradeBadgeStem = "grade_enchant\\grade_icon";
constexpr std::string_view kGradeBadgeExtension = ".bmp";

}  // namespace

std::string StoreSearchGradeBadgePath(std::uint8_t grade) {
  if (grade == 0) {
    return {};
  }
  std::string path(kGradeBadgeStem);
  path += std::to_string(grade);
  path += kGradeBadgeExtension;
  return path;
}

}  // namespace moonclient
