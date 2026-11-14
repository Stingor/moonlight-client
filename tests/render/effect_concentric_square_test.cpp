#include "ro/render/effect_concentric_square.h"

#include <cmath>
#include <set>

#include <catch2/catch_approx.hpp>
#include <catch2/catch_test_macros.hpp>

#include "ro/render/effect_track.h"
#include "ro/render/effect_track_burst.h"

using Catch::Approx;
using ro::render::AdvanceOrbitJitterTracks;
using ro::render::ComposeConcentricSquareQuads;
using ro::render::EffectConcentricSquareRequest;
using ro::render::EffectConcentricSquareRequestOf;
using ro::render::EffectTrack;
using ro::render::EffectWorldQuad;
using ro::render::kEffectTracksPerNode;

namespace {

[[nodiscard]] EffectConcentricSquareRequest Request(std::uint8_t mark) {
  EffectConcentricSquareRequest request;
  request.radius = 10.0F;
  request.alpha = 200;
  request.mark = mark;
  return request;
}

[[nodiscard]] std::array<EffectTrack, kEffectTracksPerNode> Advanced(std::uint8_t mark,
                                                                     std::int32_t ticks,
                                                                     bool plonge = false) {
  std::array<EffectTrack, kEffectTracksPerNode> tracks;
  tracks[0].active = true;
  tracks[0].mark[0] = mark;
  std::uint32_t seed = 17U;
  for (std::int32_t tick = 0; tick < ticks; ++tick) {
    AdvanceOrbitJitterTracks(tracks, 40, 60, plonge, seed);
  }
  return tracks;
}

}  // namespace

TEST_CASE("le carre compte deux a quatre couches selon sa marque", "[render][effect][concentric]") {
  // ⚠ DEUX COUCHES TOUJOURS, ET DEUX DE PLUS POUR DEUX MARQUES. Les compter
  // quatre partout peindrait deux halos bleus autour de chaque carre.
  CHECK(ComposeConcentricSquareQuads(Request(1), {}).size() == 2);
  CHECK(ComposeConcentricSquareQuads(Request(20), {}).size() == 3);
  CHECK(ComposeConcentricSquareQuads(Request(21), {}).size() == 4);
}

TEST_CASE("la couche la plus large est la plus pale", "[render][effect][concentric]") {
  const std::vector<EffectWorldQuad> quads = ComposeConcentricSquareQuads(Request(21), {});
  REQUIRE(quads.size() == 4);
  // ⚠ LA PREMIERE N'A QUE LA MOITIE DE L'ALPHA, et elle seule.
  CHECK(quads[0].tint[3] == 100);
  CHECK(quads[1].tint[3] == 200);
  CHECK(quads[3].tint[3] == 200);
}

TEST_CASE("les couches larges sont bleues, les autres suivent la marque",
          "[render][effect][concentric]") {
  const std::vector<EffectWorldQuad> quads = ComposeConcentricSquareQuads(Request(21), {});
  REQUIRE(quads.size() == 4);
  // Les deux halos, toujours bleu pur.
  CHECK(quads[0].tint[0] == 0);
  CHECK(quads[0].tint[2] == 255);
  CHECK(quads[1].tint[2] == 255);
  // ⚠ ET LES DEUX DERNIERES NE SE SUIVENT PAS : la marque vingt et un donne
  // (40, 40, 255) a la couche pleine et (250, 250, 255) a l'etroite. Les traiter
  // ensemble effacerait ce contraste.
  CHECK(quads[2].tint[0] == 40);
  CHECK(quads[3].tint[0] == 250);
}

TEST_CASE("les quatre coins forment un carre", "[render][effect][concentric]") {
  EffectConcentricSquareRequest request = Request(1);
  const std::vector<EffectWorldQuad> quads = ComposeConcentricSquareQuads(request, {});
  REQUIRE_FALSE(quads.empty());
  // La couche pleine porte le rayon tel quel : quatre points a dix unites du
  // centre, et deux diagonales egales.
  const EffectWorldQuad& full = quads.front();
  const auto length = [](const glm::vec3& a, const glm::vec3& b) { return glm::length(a - b); };
  CHECK(length(full.corners[0], full.corners[2]) == Approx(20.0F));
  CHECK(length(full.corners[1], full.corners[3]) == Approx(20.0F));
  // Et les quatre cotes sont egaux : c'est bien un carre, pas un losange.
  CHECK(length(full.corners[0], full.corners[1]) ==
        Approx(length(full.corners[1], full.corners[2])));
}

TEST_CASE("sans rotation, le carre est couche", "[render][effect][concentric]") {
  // 🔴 L'ORDONNEE DES COINS EST NULLE, le sinus va en profondeur : c'est la
  // rotation de billboard qui redresse le carre. Debout, il se presentait a
  // l'oeil par la tranche et chaque sphere n'etait plus qu'un trait.
  const std::vector<EffectWorldQuad> quads = ComposeConcentricSquareQuads(Request(1), {});
  REQUIRE_FALSE(quads.empty());
  for (const glm::vec3& corner : quads.front().corners) {
    CHECK(corner.y == Approx(0.0F).margin(1e-4));
  }
  CHECK(std::abs(quads.front().corners[1].z) == Approx(10.0F));
}

TEST_CASE("le rayon respire au rythme de sa case", "[render][effect][concentric]") {
  EffectConcentricSquareRequest immobile = Request(1);
  EffectConcentricSquareRequest gonflee = Request(1);
  gonflee.breath_degrees = 90.0F;
  const auto radius = [](const std::vector<EffectWorldQuad>& quads) {
    return glm::length(quads.front().corners[0] - quads.front().corners[2]) / 2.0F;
  };
  // ⚠ UN VINGTIEME DU RAYON, au sinus de la case : dix plus un demi.
  CHECK(radius(ComposeConcentricSquareQuads(gonflee, {})) ==
        Approx(radius(ComposeConcentricSquareQuads(immobile, {})) + 0.5F));
}

TEST_CASE("un carre sans alpha ni rayon ne rend rien", "[render][effect][concentric]") {
  EffectConcentricSquareRequest silent = Request(1);
  silent.alpha = 0;
  CHECK(ComposeConcentricSquareQuads(silent, {}).empty());
  silent = Request(1);
  silent.radius = 0.0F;
  CHECK(ComposeConcentricSquareQuads(silent, {}).empty());
}

TEST_CASE("les deux angles viennent de la camera", "[render][effect][concentric]") {
  // 🔴 TELS QU'ILS ARRIVENT : ils sortent déjà de `EffectBillboard*Degrees`, qui
  // fait le calcul du client. Le refaire tournait la figure à l'envers de la
  // caméra. Voir `StoreEffectBillboardAngles`.
  const std::array<EffectTrack, kEffectTracksPerNode> tracks = Advanced(1, 1);
  CHECK(tracks[0].style == 40);
  CHECK(tracks[1].style == 60);
}

TEST_CASE("le tremblement poursuit trois cibles", "[render][effect][concentric]") {
  // ⚠ LES SIX CASES PARTENT TIREES, et non a zero : c'est ce que fait le
  // batisseur, qui tire SEPT angles par noeud. Les semer a zero ferait marcher
  // les trois paires ENSEMBLE pendant trois cent cinquante-neuf tics — le
  // tremblement naîtrait alors d'un seul coup, tres tard, et le test aurait
  // prouve le contraire de ce qu'il veut montrer.
  std::array<EffectTrack, kEffectTracksPerNode> tracks;
  tracks[0].active = true;
  tracks[0].mark[0] = 1;
  tracks[0].value[0] = 100.0F;
  tracks[0].value[1] = 30.0F;
  tracks[0].value[2] = 50.0F;
  tracks[0].value[3] = 200.0F;
  tracks[0].value[4] = 300.0F;
  tracks[0].value[5] = 290.0F;
  std::uint32_t seed = 17U;
  for (int tick = 0; tick < 20; ++tick) {
    AdvanceOrbitJitterTracks(tracks, 40, 60, false, seed);
  }
  // Chacun a marche de vingt degres VERS sa cible, chacun dans son sens.
  CHECK(tracks[0].value[0] == Approx(80.0F));
  CHECK(tracks[0].value[2] == Approx(70.0F));
  CHECK(tracks[0].value[4] == Approx(280.0F));

  // ⚠ ET CELUI QUI ATTEINT SA CIBLE EN RECOIT UNE NEUVE. Le troisieme partait a
  // trois cents pour deux cent quatre-vingt-dix : il l'a touchee au dixieme tic,
  // et sa cible n'est deja plus celle du semis. C'est la poursuite, et c'est
  // elle qui fait le tremblement — un angle qui marche vers une cible fixe
  // s'arreterait au bout d'un cycle.
  CHECK(tracks[0].value[5] != Approx(290.0F));
  for (const std::size_t pair : {0U, 2U, 4U}) {
    CHECK(tracks[0].value[pair] >= 0.0F);
    CHECK(tracks[0].value[pair] <= 359.0F);
  }
}

TEST_CASE("le champ du noeud fait monter ou tomber le meme effet", "[render][effect][concentric]") {
  // ⚠ POSE, LA HAUTEUR DESCEND ET LE RAYON CROIT ; ABSENT, les deux sont FIXES.
  // Un seul champ, deux effets qui n'ont pas l'air parents.
  const std::array<EffectTrack, kEffectTracksPerNode> au_repos = Advanced(10, 20, false);
  CHECK(au_repos[0].value[7] == Approx(-15.0F));
  CHECK(au_repos[0].radius_at_anchor == Approx(6.0F));

  const std::array<EffectTrack, kEffectTracksPerNode> diving = Advanced(10, 20, true);
  CHECK(diving[0].value[7] == Approx(-2.0F));
  CHECK(diving[0].radius_at_anchor == Approx(0.4F));
  // Et aucun des deux ne depasse sa borne.
  const std::array<EffectTrack, kEffectTracksPerNode> late = Advanced(10, 1000, true);
  CHECK(late[0].value[7] == Approx(-30.0F));
  CHECK(late[0].radius_at_anchor == Approx(10.0F));
}

TEST_CASE("l'alpha monte deux cents tics puis s'arrete", "[render][effect][concentric]") {
  CHECK(Advanced(1, 50)[0].alpha == 50);
  CHECK(Advanced(1, 200)[0].alpha == 200);
  CHECK(Advanced(1, 260)[0].alpha == 200);
}

TEST_CASE("la demande lit DEUX pistes", "[render][effect][concentric]") {
  std::array<EffectTrack, kEffectTracksPerNode> tracks;
  tracks[0].active = true;
  tracks[0].style = 111;
  tracks[1].style = 222;
  tracks[0].mark[0] = 13;
  tracks[0].radius_at_anchor = 4.0F;
  const EffectConcentricSquareRequest request =
      EffectConcentricSquareRequestOf(tracks[0], tracks[1], false);
  // ⚠ LE SECOND ANGLE VIENT DE LA PISTE UN, et d'elle seule.
  CHECK(request.turn_degrees == 111);
  CHECK(request.tilt_degrees == 222);
  CHECK(request.mark == 13);
}
