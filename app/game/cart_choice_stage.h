#pragma once

// LA LISTE DES CHARRETTES, DANS LA BOUCLE DE LA PARTIE : ce que la fenêtre
// montre de la liste ouverte, et ce que son choix devient sur le fil.

#include <cstdint>
#include <map>
#include <vector>

#include "ro/net/connection.h"
#include "ro/render/gl_texture.h"
#include "ro/sim/cart_choice.h"
#include "ro/ui/cart_choice_window.h"
#include "ro/vfs/item_words.h"
#include "ro/vfs/vfs.h"

namespace moonclient {

class CartChoiceStage {
 public:
  [[nodiscard]] ro::sim::CartChoice& choice() noexcept { return choice_; }

  // Recopie la liste ouverte dans l'état et la vue de la fenêtre.
  //
  // ⚠ LES IMAGES DES BOUTONS SE LISENT À LA PREMIÈRE LISTE qui les montre, une
  // fois par type : une installation sans elles laisse des boutons vides, que
  // l'on clique quand même.
  //
  // Le titre est celui du client, dans la langue posée.
  void Stage(ro::vfs::Vfs* vfs, const ro::vfs::ItemWords& words,
             ro::ui::CartChoiceWindowState& state, ro::ui::CartChoiceView& view);

  // Envoie le type choisi, et ferme la liste. La croix la ferme sans rien
  // envoyer.
  void Apply(const ro::ui::CartChoiceWindowAction& asked, ro::net::Connection& link);

  // Le changement de carte emporte la liste, sans un mot au serveur.
  void Forget() noexcept { choice_.Close(); }

 private:
  const ro::render::GlTexture* ImageFor(ro::vfs::Vfs& vfs, std::uint8_t cart_type);

  ro::sim::CartChoice choice_;
  // ⚠ UN ÉCHEC SE RETIENT AUSSI, par une texture vide : relire à chaque image
  // un fichier absent coûterait une recherche par image.
  std::map<std::uint8_t, ro::render::GlTexture> images_;
  std::vector<ro::ui::SkinImage> shown_;
};

}  // namespace moonclient
