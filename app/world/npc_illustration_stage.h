#pragma once

#include <optional>
#include <string>

#include "ro/render/game_images.h"
#include "ro/sim/npc_illustration.h"
#include "ro/ui/npc_illustration_window.h"

namespace moonclient {

// L'ILLUSTRATION DU PNJ, entre ce que le serveur montre et la fenêtre.
//
// Le slot dit laquelle montrer ; ceci trouve son image dans l'installation et
// retient ce que le joueur a fermé. Rien n'en part vers le serveur : fermer une
// illustration est un geste local, et le script, lui, continue.
class NpcIllustrationStage {
 public:
  // Ce que `ro::net::ApplyNpcIllustrationFrame` remplit.
  [[nodiscard]] ro::sim::NpcIllustrationSlot& slot() noexcept { return slot_; }

  // La vue à dessiner, ou rien : pas d'illustration, celle-ci fermée par le
  // joueur, ou aucune image trouvée — ce que `GameImages` dit au journal.
  [[nodiscard]] std::optional<ro::ui::NpcIllustrationView> Stage(ro::render::GameImages* images);

  // Une illustration fermée ne revient pas ; la SUIVANTE, si — et la même aussi,
  // pourvu que le script l'ait effacée entre-temps. Renvoyée telle quelle sans
  // effacement, elle reste fermée : le client d'origine ignore de même un
  // `cutin` qui ne change ni l'image ni la place.
  void Apply(const ro::ui::NpcIllustrationAction& asked);

  // Le changement de carte l'emporte : elle appartenait au PNJ qu'on a quitté.
  void Forget() noexcept {
    slot_.Clear();
    dismissed_.reset();
  }

 private:
  ro::sim::NpcIllustrationSlot slot_;
  std::optional<ro::sim::NpcIllustration> dismissed_;
  // L'illustration dont on a cherché l'image, et le chemin trouvé — vide quand
  // aucun n'existe. Chercher à chaque image transcoderait deux dossiers par image.
  std::optional<ro::sim::NpcIllustration> searched_;
  std::string found_path_;
};

}  // namespace moonclient
