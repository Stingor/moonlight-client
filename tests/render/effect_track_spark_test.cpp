#include <array>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <numbers>

#include <catch2/catch_approx.hpp>
#include <catch2/catch_test_macros.hpp>

#include "ro/render/effect_track.h"
#include "ro/render/effect_track_motion.h"

namespace {

// Une piste vive, semée comme le bâtisseur d'EF_HEAL la sème : elle monte d'un
// demi et ne dérive pas.
[[nodiscard]] ro::render::EffectTrack Spark(float rise = 0.5F, float derive = 0.0F) {
  ro::render::EffectTrack track;
  track.active = true;
  track.ticks = 0;
  track.alpha = 0;
  track.value[2] = rise;
  track.value[1] = derive;
  track.anchor = {10.0F, 5.0F, -3.0F};
  return track;
}

[[nodiscard]] std::array<ro::render::EffectTrack, ro::render::kEffectTracksPerNode> FourTracks() {
  std::array<ro::render::EffectTrack, ro::render::kEffectTracksPerNode> tracks;
  for (ro::render::EffectTrack& track : tracks) {
    track = Spark();
  }
  return tracks;
}

}  // namespace

TEST_CASE("Étincelle montante : les deux angles de caméra vont dans les deux premières pistes",
          "[render][track]") {
  // ⚠ LA FIGURE LES RELIT DANS LES PISTES, pas dans la caméra : le mouvement les
  // y range au dernier tic, et les recalculer au moment de composer donnerait un
  // quad en avance d'un tic une fois sur deux.
  auto tracks = FourTracks();
  std::uint32_t seed = 1U;
  ro::render::AdvanceRisingSparkTracks(tracks, 123, 45, seed);
  CHECK(tracks[0].style == 123);
  CHECK(tracks[1].style == 45);
}

TEST_CASE("Étincelle montante : l'alpha monte NEUF fois, puis descend jusqu'à l'extinction",
          "[render][track]") {
  // Le premier tic est pris par le TIRAGE DU CAP et n'ajoute rien à l'alpha.
  // Compter dix montées donnerait cent cinquante là où le client s'arrête à cent
  // trente-cinq, et l'étincelle vivrait cinq tics de trop.
  auto tracks = FourTracks();
  std::uint32_t seed = 7U;
  for (int tick = 0; tick < 10; ++tick) {
    ro::render::AdvanceRisingSparkTracks(tracks, 0, 0, seed);
  }
  CHECK(tracks[0].ticks == 10);
  CHECK(tracks[0].alpha == 135);

  // Puis trois par tic : quarante-cinq tics pour épuiser cent trente-cinq.
  int ticks = 10;
  while (tracks[0].active && ticks < 200) {
    ro::render::AdvanceRisingSparkTracks(tracks, 0, 0, seed);
    ++ticks;
  }
  CHECK(tracks[0].alpha == 0);
  CHECK(ticks == 56);
}

TEST_CASE("Étincelle montante : elle MONTE, et ne descend pas", "[render][track]") {
  // ⚠ LE REPÈRE DU CLIENT DESCEND, LE NÔTRE MONTE. Le client retranche pour
  // élever ; recopier son signe enterrerait les étincelles sous le sol, et
  // l'erreur ne se voit qu'en jeu.
  auto tracks = FourTracks();
  std::uint32_t seed = 3U;
  const float start = tracks[0].anchor.y;
  for (int tick = 0; tick < 4; ++tick) {
    ro::render::AdvanceRisingSparkTracks(tracks, 0, 0, seed);
  }
  CHECK(tracks[0].anchor.y == Catch::Approx(start + 4.0F * 0.5F));
  // SANS DÉRIVE, ELLE NE BOUGE PAS DE SON POINT : le semis de Heal pose zéro, et
  // le cap tiré au sort ne se voit alors nulle part.
  CHECK(tracks[0].anchor.x == Catch::Approx(10.0F));
  CHECK(tracks[0].anchor.z == Catch::Approx(-3.0F));
}

TEST_CASE("Étincelle montante : elle tourne sur elle-même, dans le sens qui décroît",
          "[render][track]") {
  auto tracks = FourTracks();
  tracks[0].degrees = 12;
  std::uint32_t seed = 5U;
  ro::render::AdvanceRisingSparkTracks(tracks, 0, 0, seed);
  CHECK(tracks[0].degrees == 7);
  ro::render::AdvanceRisingSparkTracks(tracks, 0, 0, seed);
  CHECK(tracks[0].degrees == 2);
  // ET LE PASSAGE SOUS ZÉRO SE RATTRAPE : le client y ajoute un tour.
  ro::render::AdvanceRisingSparkTracks(tracks, 0, 0, seed);
  CHECK(tracks[0].degrees == 357);
}

TEST_CASE("Étincelle montante : chaque piste éclate À SON TOUR", "[render][track]") {
  // La première piste éclate à ses tics trente-cinq et trente-six, la deuxième à
  // vingt-huit et vingt-neuf, et ainsi de suite — sept tics d'écart. Les quatre
  // s'allument l'une après l'autre au lieu de scintiller ensemble.
  auto tracks = FourTracks();
  for (ro::render::EffectTrack& track : tracks) {
    track.mark[1] = 1;
  }
  std::uint32_t seed = 11U;
  std::array<int, 4> first_shard = {0, 0, 0, 0};
  for (int tick = 1; tick <= 40; ++tick) {
    ro::render::AdvanceRisingSparkTracks(tracks, 0, 0, seed);
    for (std::size_t rang = 0; rang < tracks.size(); ++rang) {
      if (tracks[rang].mark[0] == 1 && first_shard[rang] == 0) {
        first_shard[rang] = tick;
      }
    }
  }
  CHECK(first_shard[0] == 35);
  CHECK(first_shard[1] == 28);
  CHECK(first_shard[2] == 21);
  CHECK(first_shard[3] == 14);
}

TEST_CASE("Étincelle montante : sans la seconde marque, aucun éclat", "[render][track]") {
  // ⚠ SEUL LE SEMIS QUI CHARGE UNE DEUXIÈME TEXTURE autorise l'éclat. Les autres
  // gardent la première marque telle que leur bâtisseur l'a posée ; la leur
  // changer quand même ferait scintiller ce qui doit rester égal.
  auto tracks = FourTracks();
  for (ro::render::EffectTrack& track : tracks) {
    track.mark[0] = 9;
    track.mark[1] = 0;
  }
  std::uint32_t seed = 13U;
  for (int tick = 0; tick < 40; ++tick) {
    ro::render::AdvanceRisingSparkTracks(tracks, 0, 0, seed);
  }
  CHECK(tracks[0].mark[0] == 9);
}

TEST_CASE("Étincelle montante : une piste éteinte n'avance pas, et ne décale pas les autres",
          "[render][track]") {
  // ⚠ LE SEUIL DE L'ÉCLAT DESCEND MÊME POUR UNE PISTE ÉTEINTE : le client le
  // décrémente en tête de boucle, hors de toute garde. Ne le faire avancer que
  // pour les pistes vives décalerait l'éclat de toutes les suivantes.
  auto tracks = FourTracks();
  tracks[0].active = false;
  for (ro::render::EffectTrack& track : tracks) {
    track.mark[1] = 1;
  }
  std::uint32_t seed = 17U;
  int shard_of_second = 0;
  for (int tick = 1; tick <= 40; ++tick) {
    ro::render::AdvanceRisingSparkTracks(tracks, 0, 0, seed);
    if (tracks[1].mark[0] == 1 && shard_of_second == 0) {
      shard_of_second = tick;
    }
  }
  CHECK(tracks[0].ticks == 0);
  CHECK(shard_of_second == 28);
}

TEST_CASE("Étincelle montante : une piste RETARDÉE compte en silence", "[render][track]") {
  // Son bâtisseur lui donne des tics négatifs ; tant qu'ils le sont, elle ne
  // bouge pas — ni alpha, ni montée.
  auto tracks = FourTracks();
  tracks[0].ticks = -3;
  std::uint32_t seed = 19U;
  const float start = tracks[0].anchor.y;
  for (int tick = 0; tick < 3; ++tick) {
    ro::render::AdvanceRisingSparkTracks(tracks, 0, 0, seed);
  }
  CHECK(tracks[0].ticks == 0);
  CHECK(tracks[0].alpha == 0);
  CHECK(tracks[0].anchor.y == Catch::Approx(start));
}

TEST_CASE("Étincelle montante : avec une dérive, elle s'écarte selon son cap", "[render][track]") {
  auto tracks = FourTracks();
  for (ro::render::EffectTrack& track : tracks) {
    track = Spark(0.5F, 2.0F);
  }
  std::uint32_t seed = 23U;
  ro::render::AdvanceRisingSparkTracks(tracks, 0, 0, seed);
  // Le cap se tire AU PREMIER TIC, et la dérive s'applique dès ce tic-là.
  const float offset = std::hypot(tracks[0].anchor.x - 10.0F, tracks[0].anchor.z + 3.0F);
  CHECK(offset == Catch::Approx(2.0F).margin(0.001F));
}

TEST_CASE("Étincelle montante : la dérive suit le cap du client, profondeur niée",
          "[render][track]") {
  // ⚠ LE CAP TIRÉ EST CELUI DU CLIENT, l'ancre est dans notre repère : à graine
  // égale, l'étincelle doit partir là où le client l'envoie, donc à la
  // profondeur opposée au sinus de son cap.
  constexpr float kDrift = 2.0F;
  std::array<ro::render::EffectTrack, ro::render::kEffectTracksPerNode> tracks;
  tracks[0] = Spark(0.5F, kDrift);
  std::uint32_t seed = 11U;
  ro::render::AdvanceRisingSparkTracks(tracks, 0, 0, seed);
  const float cap =
      static_cast<float>(tracks[0].azimuth_degrees) * std::numbers::pi_v<float> / 180.0F;
  CHECK(tracks[0].anchor.x - 10.0F == Catch::Approx(kDrift * std::cos(cap)).margin(0.001F));
  CHECK(tracks[0].anchor.z + 3.0F == Catch::Approx(-kDrift * std::sin(cap)).margin(0.001F));
}
