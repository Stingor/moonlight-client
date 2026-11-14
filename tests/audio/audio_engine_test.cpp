// Le mélangeur, SANS carte son.
//
// ⚠ Tout ce qui est vérifié ici l'est sur un mélangeur FERMÉ, et c'est
// délibéré : un test qui exigerait un périphérique ne tournerait ni sur le
// runner d'intégration ni sur une machine dont le serveur de son dort. L'état
// que ces essais couvrent — ce qui est audible, ce qui survit à un
// déplacement — ne dépend d'aucun matériel.

#include "ro/audio/audio_engine.h"

#include <utility>

#include <catch2/catch_test_macros.hpp>

TEST_CASE("un mélangeur neuf est audible, et c'est au client de le couper", "[audio]") {
  // Une bibliothèque de son qui partirait muette surprendrait qui la
  // construit. Le choix de couper appartient au client, en un seul endroit.
  const ro::audio::AudioEngine audio;
  CHECK(audio.music_audible());
  CHECK(audio.sound_audible());
}

TEST_CASE("la musique et les bruits se coupent séparément", "[audio]") {
  // On joue souvent sans musique et avec les bruits ; rarement l'inverse.
  ro::audio::AudioEngine audio;
  audio.SetMusicAudible(false);
  CHECK_FALSE(audio.music_audible());
  CHECK(audio.sound_audible());

  audio.SetSoundAudible(false);
  audio.SetMusicAudible(true);
  CHECK(audio.music_audible());
  CHECK_FALSE(audio.sound_audible());
}

TEST_CASE("le silence SURVIT à un déplacement", "[audio]") {
  // ⚠ LE DÉFAUT QUE CET ESSAI GARDE, et il a été payé. Les deux constructions
  // par déplacement énumèrent l'état À LA MAIN — elles le doivent, la source
  // gardant des pointeurs bruts à annuler —, si bien qu'un champ oublié repart
  // à son défaut EN SILENCE.
  //
  // C'est ce qui est arrivé aux drapeaux d'audibilité : `OpenAudio` coupait le
  // son puis rendait le mélangeur PAR VALEUR, et la coupure mourait avec
  // l'objet abandonné. Le journal écrivait « son : coupé au démarrage » et le
  // son jouait ; rien ne se contredisait.
  ro::audio::AudioEngine muted;
  muted.SetMusicAudible(false);
  muted.SetSoundAudible(false);

  const ro::audio::AudioEngine moved(std::move(muted));
  CHECK_FALSE(moved.music_audible());
  CHECK_FALSE(moved.sound_audible());
}

TEST_CASE("le silence survit aussi à une AFFECTATION par déplacement", "[audio]") {
  // La seconde moitié du même piège : elle énumère l'état de son côté, et
  // s'oublie tout aussi silencieusement.
  ro::audio::AudioEngine muted;
  muted.SetMusicAudible(false);
  muted.SetSoundAudible(false);

  ro::audio::AudioEngine other;
  REQUIRE(other.music_audible());
  other = std::move(muted);
  CHECK_FALSE(other.music_audible());
  CHECK_FALSE(other.sound_audible());
}

TEST_CASE("le volume des familles survit aux deux déplacements", "[audio]") {
  // Le même piège que les drapeaux : un champ oublié dans les constructions par
  // déplacement repartirait à fond, et l'ambiance baissée par le joueur
  // reviendrait à plein volume au démarrage.
  constexpr float kQuarter = 0.25F;
  ro::audio::AudioEngine quiet;
  quiet.SetChannelVolume(ro::audio::SoundChannel::kAmbience, kQuarter);

  ro::audio::AudioEngine moved(std::move(quiet));
  CHECK(moved.channel_volume(ro::audio::SoundChannel::kAmbience) == kQuarter);
  CHECK(moved.channel_volume(ro::audio::SoundChannel::kSprites) == 1.0F);

  ro::audio::AudioEngine other;
  other = std::move(moved);
  CHECK(other.channel_volume(ro::audio::SoundChannel::kAmbience) == kQuarter);
}

TEST_CASE("le volume d'une famille reste entre zéro et un", "[audio]") {
  ro::audio::AudioEngine audio;
  audio.SetChannelVolume(ro::audio::SoundChannel::kEffects, 2.0F);
  CHECK(audio.channel_volume(ro::audio::SoundChannel::kEffects) == 1.0F);
  audio.SetChannelVolume(ro::audio::SoundChannel::kEffects, -1.0F);
  CHECK(audio.channel_volume(ro::audio::SoundChannel::kEffects) == 0.0F);
}

TEST_CASE("un mélangeur fermé ne joue rien et ne se plaint pas", "[audio]") {
  // La règle que l'en-tête annonce : toutes les méthodes ne font rien sur un
  // mélangeur fermé, plutôt que d'obliger chaque site d'appel à vérifier.
  ro::audio::AudioEngine audio;
  REQUIRE_FALSE(audio.is_open());
  CHECK(audio.PlayMusic("essai", {}).has_value());
  audio.PlaySound("inconnu", 1.0F, 0.0F, ro::audio::SoundChannel::kEffects);
  audio.StopSounds(ro::audio::SoundChannel::kAmbience);
  audio.StopMusic();
  CHECK(audio.music_name().empty());
  CHECK_FALSE(audio.SoundLengthMs("inconnu").has_value());
}
