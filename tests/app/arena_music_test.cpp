// LA MUSIQUE DE L'ARÈNE (U18) : la carte jusqu'à 6:59, Glast Heim à 7:00, la
// piste de Baphomet à 15:00, moitié du volume en pause, et le morceau choisi
// par le joueur respecté.

#include "survivors/arena_music.h"

#include <cstdint>
#include <optional>
#include <string_view>

#include <catch2/catch_approx.hpp>
#include <catch2/catch_test_macros.hpp>

using moonclient::ArenaMusic;
using moonclient::ArenaMusicCue;
using moonclient::ArenaMusicFacts;
using moonclient::ArenaMusicVolume;
using moonclient::ArenaTrackAt;
using moonclient::kArenaBossTrack;
using moonclient::kArenaFieldTrack;
using moonclient::kArenaGlastHeimTrack;

namespace {

constexpr std::uint64_t kMsPerSecond = 1000;
constexpr std::uint64_t kSecondsPerMinute = 60;
constexpr std::uint64_t kLastSecond = 59;
constexpr float kPlayerVolume = 0.8F;
constexpr float kLowerPlayerVolume = 0.3F;

[[nodiscard]] constexpr std::uint64_t ClockMs(std::uint64_t minutes, std::uint64_t seconds) {
  return (minutes * kSecondsPerMinute + seconds) * kMsPerSecond;
}

}  // namespace

TEST_CASE("Musique de l'arène : la piste aux bornes de 6:59, 7:00, 14:59 et 15:00",
          "[app][survivors]") {
  CHECK(ArenaTrackAt(0) == kArenaFieldTrack);
  CHECK(ArenaTrackAt(ClockMs(6, kLastSecond)) == kArenaFieldTrack);
  CHECK(ArenaTrackAt(ClockMs(7, 0)) == kArenaGlastHeimTrack);
  CHECK(ArenaTrackAt(ClockMs(14, kLastSecond)) == kArenaGlastHeimTrack);
  CHECK(ArenaTrackAt(ClockMs(15, 0)) == kArenaBossTrack);
  CHECK(kArenaBossTrack == std::string_view("bgm\\16.mp3"));
}

TEST_CASE("Musique de l'arène : les champs sont ceux de l'arène, la nuit est la même",
          "[app][survivors]") {
  constexpr std::string_view kBiomeTrack = "bgm\\99.mp3";
  CHECK(ArenaTrackAt(ClockMs(6, kLastSecond), kBiomeTrack) == kBiomeTrack);
  CHECK(ArenaTrackAt(ClockMs(7, 0), kBiomeTrack) == kArenaGlastHeimTrack);
  CHECK(ArenaTrackAt(ClockMs(15, 0), kBiomeTrack) == kArenaBossTrack);
  ArenaMusic music;
  CHECK(music.Advance(ArenaMusicFacts{.field_track = kBiomeTrack}).start_track == kBiomeTrack);
}

TEST_CASE("Musique de l'arène : une bascule par piste, pas une par image", "[app][survivors]") {
  ArenaMusic music;
  ArenaMusicFacts facts;
  CHECK(music.Advance(facts).start_track == kArenaFieldTrack);
  facts.presentation_ms = ClockMs(6, kLastSecond);
  CHECK(music.Advance(facts).start_track.empty());
  facts.presentation_ms = ClockMs(7, 0);
  CHECK(music.Advance(facts).start_track == kArenaGlastHeimTrack);
  CHECK(music.Advance(facts).start_track.empty());
  facts.presentation_ms = ClockMs(15, 0);
  CHECK(music.Advance(facts).start_track == kArenaBossTrack);

  // Une partie relancée repart de la carte.
  music.Reset();
  facts.presentation_ms = 0;
  CHECK(music.Advance(facts).start_track == kArenaFieldTrack);
}

TEST_CASE("Musique de l'arène : le morceau choisi par le joueur n'est jamais écrasé",
          "[app][survivors]") {
  ArenaMusic music;
  ArenaMusicFacts facts{.player_track = "bgm\\01.mp3"};
  for (const std::uint64_t at_ms : {std::uint64_t{0}, ClockMs(7, 0), ClockMs(15, 0)}) {
    facts.presentation_ms = at_ms;
    CHECK(music.Advance(facts).start_track.empty());
  }
}

TEST_CASE("Musique de l'arène : le joueur rend la main, la piste du moment repart",
          "[app][survivors]") {
  ArenaMusic music;
  ArenaMusicFacts facts{.presentation_ms = ClockMs(3, 0)};
  REQUIRE(music.Advance(facts).start_track == kArenaFieldTrack);
  // Le joueur choisit un morceau, puis le relâche dans le même segment.
  facts.player_track = "bgm\\01.mp3";
  CHECK(music.Advance(facts).start_track.empty());
  facts.presentation_ms = ClockMs(4, 0);
  facts.player_track = {};
  CHECK(music.Advance(facts).start_track == kArenaFieldTrack);
  CHECK(music.Advance(facts).start_track.empty());
}

TEST_CASE("Musique de l'arène : moitié du volume en pause, sans se taire", "[app][survivors]") {
  ArenaMusic music;
  ArenaMusicFacts facts{.music_volume = kPlayerVolume};
  CHECK(music.Advance(facts).volume == Catch::Approx(kPlayerVolume));
  facts.paused = true;
  const ArenaMusicCue paused = music.Advance(facts);
  CHECK(paused.volume == Catch::Approx(kPlayerVolume / 2.0F));
  // La pause ne relance pas la piste.
  CHECK(paused.start_track.empty());
  facts.paused = false;
  CHECK(music.Advance(facts).volume == Catch::Approx(kPlayerVolume));
}

TEST_CASE("Musique de l'arène : un volume changé en pause tient à la reprise et à la sortie",
          "[app][survivors]") {
  ArenaMusic music;
  ArenaMusicVolume volume;
  volume.Follow(kPlayerVolume);
  ArenaMusicFacts facts{.paused = true, .music_volume = volume.player_volume()};
  std::optional<float> posed = volume.Pose(music.Advance(facts).volume);
  REQUIRE(posed.has_value());
  CHECK(*posed == Catch::Approx(kPlayerVolume / 2.0F));
  // Le même volume ne se repose pas à chaque image.
  CHECK_FALSE(volume.Pose(music.Advance(facts).volume).has_value());

  // Les options de la pause baissent la musique : la pause s'applique au
  // nouveau réglage, et non à celui de l'entrée.
  volume.Follow(kLowerPlayerVolume);
  facts.music_volume = volume.player_volume();
  posed = volume.Pose(music.Advance(facts).volume);
  REQUIRE(posed.has_value());
  CHECK(*posed == Catch::Approx(kLowerPlayerVolume / 2.0F));

  // La reprise rend le nouveau réglage entier, et la sortie le rend aussi.
  facts.paused = false;
  posed = volume.Pose(music.Advance(facts).volume);
  REQUIRE(posed.has_value());
  CHECK(*posed == Catch::Approx(kLowerPlayerVolume));
  CHECK(volume.player_volume() == Catch::Approx(kLowerPlayerVolume));

  // Une partie relancée repose le volume, même inchangé.
  volume.Forget();
  CHECK(volume.Pose(music.Advance(facts).volume).has_value());
}
