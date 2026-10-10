// Les mots de la fenêtre des destinations. Voir l'en-tête.

#include "ro/vfs/warp_list_words.h"

#include <optional>

#include "ro/core/i18n.h"

namespace ro::vfs {
namespace {

// Les deux noms que le client reconnaît avant de consulter la table des cartes.
constexpr std::string_view kRandomName = "Random";
constexpr std::string_view kCancelName = "cancel";

}  // namespace

std::string_view WarpListTitle(const ItemWords& words) { return words.warp_list.title; }

std::string_view WarpListCancelWord(const ItemWords& words) { return words.warp_list.cancel; }

std::string WarpDestinationLabel(const ItemWords& words, const MapDisplayNames* map_names,
                                 std::string_view destination) {
  if (destination == kRandomName) {
    return words.warp_list.random_position;
  }
  if (destination == kCancelName) {
    return std::string(WarpListCancelWord(words));
  }
  if (map_names != nullptr) {
    if (const std::optional<std::string_view> shown = map_names->DisplayNameOf(destination);
        shown.has_value()) {
      return std::string(*shown);
    }
  }
  return words.warp_list.nowhere;
}

void FillWarpListWords(WarpListWords& words) {
  words.title = core::Tr("Choisissez une zone de Warp");
  words.random_position = core::Tr("Zone aléatoire");
  words.cancel = core::Tr("Annuler");
  // LE CLIENT DIT « RANDOM AREA » AUSSI D'UNE DESTINATION INCONNUE : voir
  // `WarpDestinationLabel`.
  words.nowhere = core::Tr("Zone aléatoire");
}

}  // namespace ro::vfs
