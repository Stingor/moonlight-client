#include "ro/render/effect_triple_wall.h"

#include <algorithm>
#include <set>

#include <catch2/catch_approx.hpp>
#include <catch2/catch_test_macros.hpp>

#include "ro/render/effect_track.h"
#include "ro/render/effect_track_burst.h"
#include "ro/render/effect_triple_wall_seed.h"

using Catch::Approx;
using ro::render::AdvanceTripleWallTracks;
using ro::render::ComposeTripleWallQuads;
using ro::render::EffectTrack;
using ro::render::EffectWorldQuad;
using ro::render::kEffectTracksPerNode;
using ro::render::kTripleWallCopies;

namespace {

[[nodiscard]] EffectTrack LiveTrack(std::uint8_t mark = 0) {
  EffectTrack track;
  track.active = true;
  track.alpha = 90;
  track.azimuth_degrees = 0;
  track.value[0] = 4.0F;
  track.radius_at_anchor = 2.0F;
  track.lift_key = 10.0F;
  track.mark[0] = mark;
  return track;
}

[[nodiscard]] EffectTrack Advanced(std::uint8_t mark, std::int32_t ticks) {
  std::array<EffectTrack, kEffectTracksPerNode> tracks;
  tracks[0] = LiveTrack(mark);
  tracks[0].value[0] = 0.0F;
  tracks[0].radius_at_anchor = 0.0F;
  tracks[0].lift_key = 0.0F;
  tracks[0].alpha = 0;
  for (std::int32_t tick = 0; tick < ticks; ++tick) {
    AdvanceTripleWallTracks(tracks);
  }
  return tracks[0];
}

}  // namespace

TEST_CASE("le mur se peint TROIS fois, a un degre d'ecart", "[render][effect][wall]") {
  const std::vector<EffectWorldQuad> quads = ComposeTripleWallQuads(LiveTrack(), {});
  REQUIRE(quads.size() == kTripleWallCopies);
  // ⚠ LES DEUX COPIES DES COTES N'ONT QUE LE TIERS DE L'ALPHA : c'est cela le
  // halo, et non une teinte. N'en peindre qu'une donnerait un bord net.
  CHECK(quads[1].tint[3] == 90);
  CHECK(quads[0].tint[3] == 30);
  CHECK(quads[2].tint[3] == 30);
}

TEST_CASE("les trois murs n'ont pas la meme pente", "[render][effect][wall]") {
  const std::vector<EffectWorldQuad> quads = ComposeTripleWallQuads(LiveTrack(), {});
  REQUIRE(quads.size() == 3);
  // Le mur du milieu monte a la cle ENTIERE au bord exterieur ; les cotes aux
  // deux cinquiemes. Leur donner la meme hauteur les superposerait, et le halo
  // disparaitrait derriere le mur franc.
  CHECK(quads[1].corners[2].y == Approx(10.0F));
  CHECK(quads[0].corners[2].y == Approx(4.0F));
  // Et au bord interieur : un cinquieme contre un dixieme.
  CHECK(quads[1].corners[3].y == Approx(2.0F));
  CHECK(quads[0].corners[3].y == Approx(1.0F));
}

TEST_CASE("le mur est un rectangle debout entre deux rayons", "[render][effect][wall]") {
  const std::vector<EffectWorldQuad> quads = ComposeTripleWallQuads(LiveTrack(), {});
  const EffectWorldQuad& middle = quads[1];
  // Les deux bords bas sont AU SOL, les deux hauts ne le sont pas.
  CHECK(middle.corners[0].y == Approx(0.0F));
  CHECK(middle.corners[1].y == Approx(0.0F));
  CHECK(middle.corners[2].y > 0.0F);
  // Le rayon interieur est la case zero ; l'exterieur y AJOUTE le rayon a
  // l'ancre. Les confondre collerait le mur sur lui-meme.
  CHECK(middle.corners[0].x == Approx(4.0F));
  CHECK(middle.corners[1].x == Approx(6.0F));
}

TEST_CASE("une piste eteinte ou muette ne rend rien", "[render][effect][wall]") {
  EffectTrack track = LiveTrack();
  track.active = false;
  CHECK(ComposeTripleWallQuads(track, {}).empty());
  track = LiveTrack();
  track.alpha = 0;
  CHECK(ComposeTripleWallQuads(track, {}).empty());
}

TEST_CASE("le mur s'eloigne et grandit", "[render][effect][wall]") {
  // Le rayon interieur gagne une unite par tic ; l'epaisseur DEUX pour une
  // marque ordinaire, UNE pour la marque un.
  CHECK(Advanced(0, 3).value[0] == Approx(3.0F));
  CHECK(Advanced(0, 3).radius_at_anchor == Approx(6.0F));
  CHECK(Advanced(1, 3).radius_at_anchor == Approx(3.0F));
}

TEST_CASE("la marque change QUATRE nombres a la fois", "[render][effect][wall]") {
  // ⚠ DEUX PAS, UN PLAFOND, UN SEUIL. La lire comme un interrupteur ferait un
  // mur qui s'ouvre deux fois trop vite ou deux fois trop lentement.
  //
  // La hauteur plafonne a DEUX pour la marque un, a QUATRE pour les autres — et
  // elle tombe PILE dessus, parce que chaque pas divise son plafond : deux
  // dixiemes dix fois font deux, quatre dixiemes dix fois font quatre.
  CHECK(Advanced(1, 40).lift_key == Approx(2.0F));
  CHECK(Advanced(0, 40).lift_key == Approx(4.0F));
  // Et l'alpha monte cinq tics pour l'une, sept pour les autres.
  CHECK(Advanced(1, 5).alpha == 25);
  CHECK(Advanced(0, 7).alpha == 35);
  CHECK(Advanced(1, 9).alpha == 25);
}

TEST_CASE("le mur meurt par son alpha, pas par son age", "[render][effect][wall]") {
  // Le rayon passe dix au onzieme tic ; l'alpha tombe alors de deux par tic
  // depuis trente-cinq, et la piste s'eteint quand il passe sous zero.
  CHECK(Advanced(0, 12).active);
  CHECK(Advanced(0, 12).alpha < 35);
  const EffectTrack morte = Advanced(0, 60);
  CHECK_FALSE(morte.active);
  CHECK(morte.alpha == 0);
}

TEST_CASE("chaque effet pose DEUX noeuds, a deux azimuts", "[render][effect][wall]") {
  // ⚠ LE SITE APPELLE SON BATISSEUR DEUX FOIS, a zero et a quarante-cinq. N'en
  // poser qu'un donnerait la moitie des murs, entrelaces autrement.
  CHECK(ro::render::kTripleWallNodeAzimuths[0] == 0);
  CHECK(ro::render::kTripleWallNodeAzimuths[1] == 45);

  const ro::render::EffectTripleWallPattern& pattern =
      ro::render::EffectTripleWallPatternsFor(590).front();
  const std::array<EffectTrack, kEffectTracksPerNode> second =
      ro::render::SeedEffectTripleWallTracks(pattern, 45);
  // Les quatre pistes d'un noeud se partagent le tour, a partir de son azimut.
  CHECK(second[0].azimuth_degrees == 45);
  CHECK(second[1].azimuth_degrees == 135);
  CHECK(second[3].azimuth_degrees == 315);
}

TEST_CASE("le semis naît invisible et deja epais", "[render][effect][wall]") {
  const ro::render::EffectTripleWallPattern& pattern =
      ro::render::EffectTripleWallPatternsFor(590).front();
  const EffectTrack track = ro::render::SeedEffectTripleWallTracks(pattern, 0)[0];
  CHECK(track.active);
  // ⚠ ALPHA ZERO ET RAYON INTERIEUR UN : c'est le mouvement qui allume et
  // eloigne. Le semer visible le ferait paraitre d'un coup, colle au centre.
  CHECK(track.alpha == 0);
  CHECK(track.value[0] == Approx(1.0F));
  CHECK(track.radius_at_anchor == Approx(2.0F));
  CHECK(ComposeTripleWallQuads(track, {}).empty());
}

TEST_CASE("l'ange pose ses deux murs a deux tics de suite", "[render][effect][wall]") {
  for (const std::uint32_t angel : {338U, 582U}) {
    REQUIRE(ro::render::EffectTripleWallPatternsFor(angel).size() == 1);
    const ro::render::EffectTripleWallPattern& pattern =
        ro::render::EffectTripleWallPatternsFor(angel).front();
    CHECK(pattern.texture.ends_with("ring_blue.tga"));
    CHECK(pattern.lift_key == Approx(2.0F));
    CHECK_FALSE(ro::render::EffectNodeScheduleSowsAt(pattern.schedule, 64));
    CHECK(ro::render::EffectNodeScheduleSowsAt(pattern.schedule, 65));
    CHECK(ro::render::EffectNodeScheduleSowsAt(pattern.schedule, 66));
    CHECK_FALSE(ro::render::EffectNodeScheduleSowsAt(pattern.schedule, 67));
    // ⚠ UN AZIMUT PAR TIC : en poser deux a chacun doublerait les murs.
    const auto first = ro::render::EffectTripleWallAzimuthsAt(pattern, 65);
    const auto second = ro::render::EffectTripleWallAzimuthsAt(pattern, 66);
    REQUIRE(first.size() == 1);
    REQUIRE(second.size() == 1);
    CHECK(first[0] == 0);
    CHECK(second[0] == 45);
  }
  // Les autres effets posent les deux au meme tic.
  const ro::render::EffectTripleWallPattern& plain =
      ro::render::EffectTripleWallPatternsFor(590).front();
  CHECK(ro::render::EffectTripleWallAzimuthsAt(plain, plain.schedule.first_tick).size() == 2);
}

TEST_CASE("la table des murs est triee", "[render][effect][wall]") {
  const std::span<const ro::render::EffectTripleWallPattern> table =
      ro::render::EffectTripleWallPatternTable();
  REQUIRE(table.size() == 6);
  CHECK(std::ranges::is_sorted(table, {}, &ro::render::EffectTripleWallPattern::effect_id));

  // ⚠ ET UN SEUL DES QUATRE SEME UNE HAUTEUR NULLE, celui dont la marque vaut
  // UN : son mur part de rien et monte au plus lent des deux rythmes.
  const ro::render::EffectTripleWallPattern& lent =
      ro::render::EffectTripleWallPatternsFor(444).front();
  CHECK(lent.mark == 1);
  CHECK(lent.lift_key == Approx(0.0F));
  CHECK(ro::render::EffectTripleWallPatternsFor(337).front().lift_key == Approx(2.0F));
}

TEST_CASE("le mur vert attend le cinquieme tic", "[render][effect][wall]") {
  // ⚠ UN BATISSEUR SUR TROIS COMPARE LE COMPTEUR DE L'INSTANCE A CINQ, et il
  // sert deux identifiants sur quatre. Les semer au premier tic montrerait le
  // mur vert un quart de seconde avant le reste de son effet.
  for (const std::uint32_t vert : {337U, 834U}) {
    const ro::render::EffectNodeSchedule& horaire =
        ro::render::EffectTripleWallPatternsFor(vert).front().schedule;
    CHECK_FALSE(ro::render::EffectNodeScheduleSowsAt(horaire, 0));
    CHECK(ro::render::EffectNodeScheduleSowsAt(horaire, 5));
    CHECK(ro::render::EffectNodeScheduleLastSowTick(horaire) == 5);
  }
  // Le bleu de 444 seme au premier tic ; celui de 590 au QUINZIEME, parce que
  // c'est son SITE qui attend.
  const ro::render::EffectNodeSchedule& blue =
      ro::render::EffectTripleWallPatternsFor(444U).front().schedule;
  CHECK(ro::render::EffectNodeScheduleSowsAt(blue, 0));
  CHECK(ro::render::EffectNodeScheduleLastSowTick(blue) == 0);
  const ro::render::EffectNodeSchedule& tardif =
      ro::render::EffectTripleWallPatternsFor(590U).front().schedule;
  CHECK_FALSE(ro::render::EffectNodeScheduleSowsAt(tardif, 0));
  CHECK(ro::render::EffectNodeScheduleSowsAt(tardif, 15));
  CHECK(ro::render::EffectNodeScheduleLastSowTick(tardif) == 15);
}
