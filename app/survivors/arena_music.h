#pragma once

#include <cstdint>
#include <string_view>

#include "survivors/arena_night.h"

// LA MUSIQUE DE L'ARÈNE (U18), décidée sans son : quelle piste doit jouer, et
// à quel volume. `app` la fait jouer par `MapMusic`.
namespace moonclient {

// La musique de la carte de l'arène, puis celle de Glast Heim qui vient avec
// la nuit, puis celle de l'antre de Baphomet à minuit, quand il arrive.
inline constexpr std::string_view kArenaFieldTrack = "bgm\\12.mp3";
inline constexpr std::string_view kArenaGlastHeimTrack = "bgm\\42.mp3";
inline constexpr std::string_view kArenaBossTrack = "bgm\\16.mp3";
inline constexpr std::uint64_t kArenaGlastHeimFromMs = kArenaNightFallsMs;
inline constexpr std::uint64_t kArenaBossFromMs = kArenaMidnightMs;

// En pause, la musique baisse de moitié sans se taire.
inline constexpr float kArenaPausedMusicShare = 0.5F;

// La piste de l'arène à `presentation_ms` : elle se lit sur l'horloge de la
// partie, comme la nuit, et gèle avec elle.
[[nodiscard]] std::string_view ArenaTrackAt(std::uint64_t presentation_ms) noexcept;

struct ArenaMusicFacts {
  std::uint64_t presentation_ms = 0;
  bool paused = false;
  // Le morceau choisi au lecteur de musique du joueur ; vide quand la carte
  // décide.
  std::string_view player_track;
  // Le volume de la musique réglé par le joueur, de zéro à un.
  float music_volume = 1.0F;
};

struct ArenaMusicCue {
  // La piste à lancer à cette image ; vide quand rien ne change.
  std::string_view start_track;
  // Le volume à poser sur la musique.
  float volume = 1.0F;
};

// LA BASCULE DES PISTES, une seule fois par piste : `MapMusic` n'est sollicité
// qu'au changement, et non à chaque image.
//
// Un morceau choisi par le joueur est respecté : aucune bascule ne l'écrase.
// Quand il rend la main, la piste du moment repart aussitôt.
// En sortant, `app` appelle `MapMusic::Forget` et repose le volume des
// options : la musique refait suivre la carte du personnage.
//
// ⚠ UNE PISTE ANNONCÉE SOUS UNE MUSIQUE COUPÉE NE JOUE PAS : `app` appelle
// `Reset` quand la musique se rallume, et la piste du moment repart.
class ArenaMusic {
 public:
  [[nodiscard]] ArenaMusicCue Advance(const ArenaMusicFacts& facts) noexcept;

  // Oublie la piste annoncée : la suivante repart, que la partie soit
  // relancée ou que la musique se rallume.
  void Reset() noexcept { cued_ = {}; }

 private:
  std::string_view cued_;
};

}  // namespace moonclient
