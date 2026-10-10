#pragma once

// LA SAISIE DE TALKIE BOX, DANS LA BOUCLE DE LA PARTIE : la pose qui attend son
// texte, ce que la fenêtre en montre, et ce que sa réponse devient sur le fil.

#include <optional>

#include "ro/core/error.h"
#include "ro/net/connection.h"
#include "ro/ui/talkbox_window.h"
#include "ro/vfs/item_words.h"

#include "character/skill_book_view.h"

namespace moonclient {

class TalkboxStage {
 public:
  // La pose que le grimoire vient de retenir : la fenêtre s'ouvre sur elle.
  // Une seconde pose remplace la première, comme le client qui recrée sa
  // fenêtre.
  void Ask(const GroundCast& cast) noexcept { pending_ = cast; }

  // La pose qui attend son texte, ou rien.
  [[nodiscard]] const std::optional<GroundCast>& pending() const noexcept { return pending_; }

  // Recopie la pose en attente dans l'état et la vue de la fenêtre. Le titre
  // et la ligne sont ceux du client, dans la langue posée.
  void Stage(const ro::vfs::ItemWords& words, ro::ui::TalkboxWindowState& state,
             ro::ui::TalkboxView& view) const;

  // « OK » envoie la pose avec son texte ; « Annuler » l'abandonne. Les deux
  // ferment la fenêtre. Rend l'échec d'émission, s'il y en a un.
  [[nodiscard]] ro::core::Result<void> Apply(const ro::ui::TalkboxWindowAction& asked,
                                             ro::net::Connection& link);

  // Le changement de carte emporte la pose, sans un mot au serveur.
  void Forget() noexcept { pending_.reset(); }

 private:
  std::optional<GroundCast> pending_;
};

}  // namespace moonclient
