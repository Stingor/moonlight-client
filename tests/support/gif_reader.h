#pragma once

#include <cstdint>
#include <optional>
#include <span>
#include <vector>

#include "ro/formats/image.h"

namespace ro::testing {

// ── UN LECTEUR DE GIF, ÉCRIT POUR JUGER LE NÔTRE ────────────────────────────
//
// 🔴 C'EST UNE SECONDE IMPLÉMENTATION, ET C'EST TOUT L'INTÉRÊT. Vérifier un
// encodeur en relisant les octets qu'on vient d'écrire ne prouve que l'accord
// d'un fichier avec lui-même : les deux moitiés partageraient la même erreur.
// Celui-ci part de la spécification GIF89a et ne connaît rien de l'autre — il
// décompresse vraiment le LZW et recompose vraiment la toile.
//
// ⚠ IL NE LIT QUE CE QUE NOUS ÉCRIVONS : table globale, dispositions « laisser
// en place » et « remettre le fond », pas d'entrelacement. Un GIF venu
// d'ailleurs peut lui être indigeste, et ce n'est pas son travail.

// La disposition « remettre le fond » : le rectangle de l'image redevient
// transparent avant la suivante.
inline constexpr int kGifRestoreBackground = 2;

struct GifFrame {
  // Le rectangle que cette image-ci porte, dans la toile.
  int left = 0;
  int top = 0;
  int width = 0;
  int height = 0;
  int delay_centiseconds = 0;
  // La disposition déclarée par le bloc de contrôle.
  int disposal = 0;
  // L'index transparent déclaré, ou moins un.
  int transparent_index = -1;
  // L'état de la TOILE après composition : c'est ce que le joueur voit. Un
  // pixel jamais peint, ou remis au fond, a un alpha nul.
  formats::Image canvas;
};

struct GifClip {
  int width = 0;
  int height = 0;
  bool loops_forever = false;
  // Les entrées de la table globale qui ne sont pas noires. ⚠ CE N'EST PAS LE
  // COMPTE DES COULEURS : l'entrée réservée à la transparence est noire, et un
  // noir véritable de l'image l'est aussi. Cela borne, cela ne dénombre pas.
  int nonblack_palette_entries = 0;
  std::vector<GifFrame> frames;
};

// Rend le clip, ou rien si les octets ne sont pas un GIF que nous écrivons.
[[nodiscard]] std::optional<GifClip> ReadGif(std::span<const std::uint8_t> bytes);

}  // namespace ro::testing
