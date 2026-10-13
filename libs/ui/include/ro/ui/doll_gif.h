#pragma once

#include <cstdint>
#include <string>
#include <string_view>

#include "ro/render/doll_animation.h"

namespace ro::ui {

// ── LE GIF DU PANTIN, VU DE LA FICHE ────────────────────────────────────────
//
// 🔴 `ui` NE FAIT NI L'IMAGE NI LE FICHIER : elle dit ce que le joueur demande
// — enregistrer, ou copier — et à quelle échelle, puis montre le mot que `app`
// lui rend. La composition, l'encodage et le dialogue du système vivent ailleurs.

// L'ÉCHELLE PAR DÉFAUT : un sprite de RO à l'échelle un est minuscule sur un
// écran d'aujourd'hui, et trois pèse lourd dans un salon de discussion.
inline constexpr std::uint8_t kDefaultDollGifScale = 2;

// Ce que le joueur vient de demander.
enum class DollGifVerb : std::uint8_t {
  kNothing,
  // « Générer le GIF » : le dialogue « Enregistrer sous », puis le fichier.
  kSave,
  // « Copier » : le GIF dans le presse-papier, sans fichier à ranger.
  kCopy,
};

// CE QUE LA FICHE MONTRE DE L'EXPORT, posé par `app` à chaque image.
struct DollGifView {
  // Un export en cours — dialogue ouvert ou encodage : les deux boutons sont
  // grisés, et un second clic ne lance rien.
  bool busy = false;
  // LE DERNIER MOT, sous la ligne des poses. ⚠ IL NE S'EFFACE PAS SEUL : il dit
  // où le dernier fichier est parti, et on le relit volontiers après coup.
  std::string_view message;
};

// LE NOM PROPOSÉ : `avatar_<pseudo>_<pose>_d<direction>.gif`.
//
// ⚠ LE PSEUDO ET LA POSE SONT LIBRES : tout ce que le système refuse dans un nom
// de fichier — et l'espace, qu'on retape mal dans une console — devient un
// souligné. Un pseudo vide s'écrit `perso`.
[[nodiscard]] std::string DollGifFileName(std::string_view player_name, std::string_view pose_label,
                                          int direction);

}  // namespace ro::ui
