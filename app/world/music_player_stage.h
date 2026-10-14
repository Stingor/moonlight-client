#pragma once

#include <string>
#include <string_view>
#include <vector>

#include "ro/audio/audio_engine.h"
#include "ro/ui/music_player_window.h"

#include "world/map_music.h"
#include "world/music_catalogue.h"

namespace moonclient {

// LE LECTEUR DE MUSIQUE, dans la trame de l'interface : la fenêtre, sa vue, et
// ce qu'elle a demandé.
struct MusicPlayerWindows {
  ro::ui::MusicPlayerState window;
  ro::ui::MusicPlayerView view;
  // La vue a été composée pour l'image : sans elle, la fenêtre s'ouvrirait une
  // image vide, et manquerait le défilement jusqu'au morceau qui joue.
  bool composed = false;
  // Ce que la fenêtre a demandé, lu à l'avancée suivante.
  ro::ui::MusicPlayerAsk answer;
};

// CE QUI JOUE CE QUE LE LECTEUR DEMANDE, et lui dit ce qui joue.
//
// ⚠ LE DOSSIER DES MUSIQUES SE LIT À LA PREMIÈRE OUVERTURE, et une seule fois :
// deux cents fichiers ne changent pas pendant une partie, et un joueur qui
// n'ouvre jamais le lecteur ne doit pas payer leur liste.
class MusicPlayerStage {
 public:
  // À CHAQUE IMAGE : joue le morceau demandé ou rend la main à la carte, puis
  // compose la vue de la fenêtre ouverte.
  void Advance(MusicPlayerWindows& windows, MapMusic& music, ro::audio::AudioEngine& audio,
               std::string_view map_name);

 private:
  void List(const MapMusic& music);

  bool listed_ = false;
  std::vector<MusicTrack> tracks_;
  // Les cartes de chaque morceau, jointes : la vue ne fait que les désigner.
  std::vector<std::string> joined_maps_;
  std::vector<ro::ui::MusicPlayerRow> rows_;
};

}  // namespace moonclient
