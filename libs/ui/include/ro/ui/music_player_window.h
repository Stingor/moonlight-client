#pragma once

#include <array>
#include <cstddef>
#include <span>
#include <string_view>

#include "ro/core/i18n.h"
#include "ro/ui/skin_art.h"

namespace ro::ui {

// LE LECTEUR DE MUSIQUE : jouer le morceau qu'on veut, et non celui que la
// carte réclame.
//
// Un ajout de ce client, sans équivalent dans celui d'origine. Il s'ouvre au
// menu d'Échap.
//
// 🔴 IL NE JOUE RIEN LUI-MÊME. Il montre les morceaux que `app` a trouvés dans
// l'installation, et rend le RANG de celui qu'on choisit ; c'est `app` qui le
// fait jouer, et qui rend la main à la carte.

inline constexpr const char* kMusicPlayerTitle = core::TrText("Lecteur de musique");
inline constexpr const char* kMusicPlayerId = "music_player";

// Aucun morceau : ni sélection, ni lecture, ni musique de carte.
inline constexpr int kMusicPlayerNoRow = -1;

// Ce que le champ de recherche peut porter, zéro final compris : un numéro ou
// un nom de carte n'en demande pas davantage.
inline constexpr std::size_t kMusicFilterSize = 64;

// Un morceau, tel que la liste le montre.
struct MusicPlayerRow {
  // Le fichier sans dossier ni extension, « 08 ».
  std::string_view label;
  // Les cartes qui le jouent, déjà jointes ; vide pour un morceau qu'aucune
  // carte ne déclare.
  std::string_view maps;
};

// CE QUE LA FENÊTRE SAIT, recomposé par `app` à chaque image.
struct MusicPlayerView {
  std::span<const MusicPlayerRow> rows;
  // Le morceau qui joue, et celui que réclame la carte où l'on se trouve.
  int playing = kMusicPlayerNoRow;
  int map_row = kMusicPlayerNoRow;
  // La carte où l'on se trouve.
  std::string_view map_name;
  // Le joueur a choisi un morceau : la carte ne décide plus.
  bool chosen = false;
  // ⚠ MUSIQUE COUPÉE AUX OPTIONS, le choix est retenu mais rien ne s'entend :
  // la fenêtre doit le dire, sans quoi le bouton aurait l'air mort.
  bool audible = true;
};

struct MusicPlayerState {
  bool open = false;
  std::array<char, kMusicFilterSize> filter = {};
  int selected = kMusicPlayerNoRow;
};

// CE QUE LA FENÊTRE DEMANDE. Des intentions : `ui` ne joue aucun son.
struct MusicPlayerAsk {
  // Le rang du morceau à jouer, ou `kMusicPlayerNoRow`.
  int play = kMusicPlayerNoRow;
  // Rendre la main à la carte.
  bool follow_map = false;
};

// UNE LIGNE RÉPOND-ELLE À LA RECHERCHE ? Le texte se cherche, sans égard à la
// casse, dans le numéro comme dans les cartes : « 08 » et « prontera »
// trouvent le même morceau. Une recherche vide les garde toutes.
[[nodiscard]] bool MusicRowMatches(const MusicPlayerRow& row, std::string_view filter);

[[nodiscard]] MusicPlayerAsk DrawMusicPlayerWindow(MusicPlayerState& state,
                                                   const MusicPlayerView& view, const SkinArt& art);

}  // namespace ro::ui
