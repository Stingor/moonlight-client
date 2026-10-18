#pragma once

#include <cstddef>
#include <filesystem>
#include <string>
#include <string_view>
#include <vector>

#include "ro/core/error.h"

namespace moonclient {

// LES FAVORIS ET LES RÉCENTS DE LA BIBLIOTHÈQUE DE MODÈLES de l'éditeur de
// carte, et le côté de ses vignettes : ce que le staff y retrouve d'une
// session à l'autre, dans `savedata\`.
//
// ⚠ POUR L'INSTALLATION, et non par compte : c'est l'outil de qui édite les
// cartes sur cette machine, pas une préférence de joueur.

inline constexpr std::string_view kModelShelfFileName = "map_editor_models.yml";

struct ModelShelf {
  // Combien de récents se gardent, et de favoris au plus.
  static constexpr std::size_t kRecentLimit = 40;
  static constexpr std::size_t kFavoriteLimit = 500;

  // Les favoris, triés et sans doublon, relatifs à data\model\.
  std::vector<std::string> favorites;
  // Les modèles posés récemment, le plus récent d'abord, sans doublon.
  std::vector<std::string> recents;
  // Le côté des vignettes de la grille, en pixels ; zéro : celui de la fenêtre.
  float cell_px = 0.0F;

  friend bool operator==(const ModelShelf&, const ModelShelf&) = default;
};

// Ajoute `file` aux favoris, ou l'en retire. Rend faux quand le plafond le
// refuse.
bool ToggleFavorite(ModelShelf& shelf, const std::string& file);

// `file` vient d'être posé : il passe en tête des récents, qu'il y fût ou non,
// et le plus ancien sort au-delà du plafond.
void RememberRecent(ModelShelf& shelf, const std::string& file);

[[nodiscard]] std::filesystem::path ModelShelfFilePath(const std::filesystem::path& client_root);

// LE TEXTE DU FICHIER, dans les deux sens, sans rien lire ni écrire. La lecture
// range ce qu'elle lit comme les gestes le rangeraient — favoris triés, récents
// sans doublon, plafonds tenus. Un texte que le YAML refuse rend une erreur, et
// le fichier ne doit alors pas être réécrit.
[[nodiscard]] ro::core::Result<ModelShelf> ReadModelShelf(std::string_view text);
[[nodiscard]] std::string WriteModelShelf(const ModelShelf& shelf);

// Le fichier de l'installation. Absent : une étagère vide. Abîmé : une erreur.
[[nodiscard]] ro::core::Result<ModelShelf> LoadModelShelf(const std::filesystem::path& client_root);
[[nodiscard]] ro::core::Result<void> SaveModelShelf(const std::filesystem::path& client_root,
                                                    const ModelShelf& shelf);

}  // namespace moonclient
