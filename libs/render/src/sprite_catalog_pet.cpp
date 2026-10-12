// LE CORPS D'UN PET PARÉ : l'accessoire ne s'ajoute pas comme une coiffe, il
// change l'animation du corps entier.

#include <algorithm>
#include <cctype>
#include <optional>
#include <string>
#include <string_view>

#include "ro/render/sprite_catalog.h"

#include "sprite_catalog_internal.h"

namespace ro::render {
namespace {

// La table nomme le fichier AVEC son extension ; les chemins d'une pièce s'en
// passent.
constexpr std::string_view kAnimationSuffix = ".act";

[[nodiscard]] bool EndsWithNoCase(std::string_view text, std::string_view suffix) {
  if (text.size() < suffix.size()) {
    return false;
  }
  return std::ranges::equal(text.substr(text.size() - suffix.size()), suffix,
                            [](char left, char right) {
                              return std::tolower(static_cast<unsigned char>(left)) ==
                                     std::tolower(static_cast<unsigned char>(right));
                            });
}

}  // namespace

std::string PetAccessoryAnimationPath(const SpriteCatalog& catalog, const ActorLook& look,
                                      std::string_view body_path) {
  if (look.pet_accessory == 0 || look.player || look.item) {
    return {};
  }
  std::optional<std::string> name =
      catalog_internal::TableName(catalog.tables.pet_accessory_acts, look.pet_accessory);
  if (!name.has_value() || name->empty()) {
    return {};
  }
  std::string_view stem = *name;
  if (EndsWithNoCase(stem, kAnimationSuffix)) {
    stem.remove_suffix(kAnimationSuffix.size());
  }
  // LE DOSSIER EST CELUI DU CORPS : la table ne nomme que le fichier.
  const std::size_t folder_end = body_path.rfind(catalog_internal::kSeparator);
  const std::string_view folder = folder_end == std::string_view::npos
                                      ? std::string_view()
                                      : body_path.substr(0, folder_end + 1);
  return catalog_internal::Join({folder, stem});
}

}  // namespace ro::render
