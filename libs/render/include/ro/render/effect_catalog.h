#pragma once

#include <cstddef>
#include <functional>
#include <map>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

#include "ro/formats/image.h"
#include "ro/render/effect_frame.h"
#include "ro/vfs/vfs.h"

namespace ro::render {

// Un effet prêt à jouer : sa description et ses textures décodées.
struct LoadedEffect {
  PreparedEffect prepared;
  // Une image par NUMÉRO de la liste à plat de l'effet. Elle est VIDE pour une
  // texture absente ou illisible : le quad qui la désigne ne se peint pas, et
  // le reste de l'effet paraît quand même.
  std::vector<formats::Image> textures;
  // Leur chemin complet dans l'archive. C'est lui qui les IDENTIFIE : deux
  // effets qui partagent un halo doivent partager son téléversement, et le
  // numéro seul ne dirait rien à qui téléverse.
  std::vector<std::string> texture_paths;
};

// Charge les effets à la demande, une fois chacun.
//
// Un effet porte quelques dizaines de textures ; les relire à chaque lancement
// ferait un à-coup à chaque coup porté, sur une carte où dix créatures se
// battent en même temps.
class EffectCatalog {
 public:
  explicit EffectCatalog(vfs::Vfs& vfs) : vfs_(&vfs) {}

  // Nul si le fichier est absent ou illisible.
  //
  // L'échec est RETENU comme le succès : un chemin qui n'existe pas ne sera pas
  // relu à chaque image, et il n'existera pas davantage la fois suivante.
  [[nodiscard]] const LoadedEffect* Load(std::string_view path);

  [[nodiscard]] std::size_t loaded_count() const noexcept { return effects_.size(); }

 private:
  vfs::Vfs* vfs_ = nullptr;
  std::map<std::string, std::optional<LoadedEffect>, std::less<>> effects_;
};

// Le dossier d'un chemin d'archive, séparateur compris.
//
// Les textures d'un effet sont nommées RELATIVEMENT à lui : c'est ce dossier
// qu'il faut leur préfixer pour retrouver le fichier.
[[nodiscard]] std::string EffectFolderOf(std::string_view path);

}  // namespace ro::render
