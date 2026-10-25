#pragma once

#include <cstdint>
#include <optional>
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

// LE VOLUME DE LA MUSIQUE PENDANT LA PARTIE : le réglage du joueur, que la
// pause baisse, et celui que `app` repose en sortant.
//
// ⚠ LE RÉGLAGE SE RELIT À CHAQUE CHANGEMENT, et non seulement à l'entrée : un
// volume changé dans les options ouvertes sur la pause est celui de la suite
// de la partie, et celui qu'on rend en sortant.
class ArenaMusicVolume {
 public:
  // Le réglage du joueur, à l'entrée ou changé en cours de partie : la
  // prochaine pose le reprend, pause comprise.
  void Follow(float player_volume) noexcept {
    player_volume_ = player_volume;
    posed_.reset();
  }
  // Oublie le dernier volume posé : la prochaine pose le repose.
  void Forget() noexcept { posed_.reset(); }
  // Le volume à poser à cette image, `cue_volume` de `ArenaMusic::Advance` ;
  // rien quand il est déjà posé.
  [[nodiscard]] std::optional<float> Pose(float cue_volume) noexcept;

  [[nodiscard]] float player_volume() const noexcept { return player_volume_; }

 private:
  float player_volume_ = 1.0F;
  std::optional<float> posed_;
};

}  // namespace moonclient
