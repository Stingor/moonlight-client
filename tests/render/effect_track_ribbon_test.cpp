#include <catch2/catch_approx.hpp>
#include <catch2/catch_test_macros.hpp>

#include "ro/render/effect_track.h"
#include "ro/render/effect_track_figures.h"

using ro::render::ComposeEffectTrackRibbon;
using ro::render::ComposeEffectTrackSkirt;
using ro::render::EffectRibbonTurnOf;
using ro::render::EffectTrack;
using ro::render::EffectWorldQuad;
using ro::render::kEffectTrackSlots;
using ro::render::kEffectTracksPerNode;

namespace {

// La valeur EXACTE à laquelle l'ordonnée de l'ancre cesse d'être une hauteur.
constexpr float kAnchorHeightThatMeansTurn = 1234.5F;
constexpr std::size_t kTrackThatCarriesTheTurn = 2;
// Une teinte que la palette porte et qui ne force pas l'alpha.
constexpr std::uint8_t kPlainVariant = 0;

EffectTrack OneLive(float length = 4.0F) {
  EffectTrack track;
  track.active = true;
  track.alpha = 200;
  track.degrees = 360;
  track.tilt_degrees = 60;
  track.radius_at_anchor = 10.0F;
  for (std::size_t segment = 0; segment < kEffectTrackSlots; ++segment) {
    track.value[segment] = length;
  }
  return track;
}

}  // namespace

TEST_CASE("Ruban : DIX quads, là où la jupe en fait vingt", "[render][figure]") {
  // ⚠ C'EST LA PREMIÈRE DES TROIS DIFFÉRENCES. Le pas angulaire vaut le balayage
  // divisé par DIX ; lui en donner vingt doublerait les quads et rétrécirait
  // chaque facette de moitié.
  const EffectTrack track = OneLive();
  CHECK(ComposeEffectTrackRibbon(track, kPlainVariant, {}, 0).size() == 10);
  CHECK(ComposeEffectTrackSkirt(track, kPlainVariant, {}).size() == 20);
}

TEST_CASE("Ruban : une piste éteinte ou sans balayage ne rend RIEN", "[render][figure]") {
  EffectTrack faded = OneLive();
  faded.active = false;
  CHECK(ComposeEffectTrackRibbon(faded, kPlainVariant, {}, 0).empty());

  EffectTrack a_rebours = OneLive();
  a_rebours.degrees = -1;
  CHECK(ComposeEffectTrackRibbon(a_rebours, kPlainVariant, {}, 0).empty());

  EffectTrack transparente = OneLive();
  transparente.alpha = 0;
  CHECK(ComposeEffectTrackRibbon(transparente, kPlainVariant, {}, 0).empty());
}

TEST_CASE("Ruban : le bord extérieur S'ÉCARTE et MONTE", "[render][figure]") {
  // La longueur du segment, inclinée : elle lève d'autant plus qu'elle éloigne
  // peu. C'est ce qui fait le ruban plutôt qu'un anneau plat.
  const std::vector<EffectWorldQuad> quads =
      ComposeEffectTrackRibbon(OneLive(), kPlainVariant, {}, 0);
  REQUIRE(!quads.empty());
  const EffectWorldQuad& first = quads.front();
  // Les coins deux et trois sont les extérieurs ; les zéro et un les intérieurs.
  CHECK(first.corners[2].y > first.corners[1].y);
  CHECK(glm::length(glm::vec2{first.corners[2].x, first.corners[2].z}) >
        glm::length(glm::vec2{first.corners[1].x, first.corners[1].z}));
}

TEST_CASE("Ruban : il NE SE SOULÈVE PAS, même à la clé qui soulève la jupe", "[render][figure]") {
  // ⚠ MESURÉ DANS SES OCTETS : ce dessinateur ne porte aucune des deux
  // constantes de clé. Lui prêter l'élévation de sa sœur le décollerait du sol.
  EffectTrack with_key = OneLive();
  with_key.lift_key = 6.001F;
  const std::vector<EffectWorldQuad> ribbon =
      ComposeEffectTrackRibbon(with_key, kPlainVariant, {}, 0);
  const std::vector<EffectWorldQuad> skirt = ComposeEffectTrackSkirt(with_key, kPlainVariant, {});
  REQUIRE(!ribbon.empty());
  REQUIRE(!skirt.empty());
  CHECK(ribbon.front().corners[0].y == Catch::Approx(0.0F));
  CHECK(skirt.front().corners[0].y > 0.0F);
}

TEST_CASE("Ruban : l'ordonnée de l'ancre est une SENTINELLE, pas une hauteur", "[render][figure]") {
  // ⚠ LA LIRE COMME UNE HAUTEUR poserait le ruban à mille deux cent trente-quatre
  // unités au-dessus du sol, c'est-à-dire hors de l'écran. À cette valeur exacte,
  // le client tourne l'abscisse et la profondeur de l'ancre et les ajoute.
  EffectTrack sentinelle = OneLive();
  sentinelle.anchor = {3.0F, kAnchorHeightThatMeansTurn, 4.0F};
  const std::vector<EffectWorldQuad> quads =
      ComposeEffectTrackRibbon(sentinelle, kPlainVariant, {}, 0);
  REQUIRE(!quads.empty());
  // Cap nul : le décalage vaut l'ancre elle-même, et RIEN en y. ⚠ SA PROFONDEUR
  // EST CELLE DU CLIENT, et elle se retourne en revenant dans notre repère.
  const std::vector<EffectWorldQuad> sans =
      ComposeEffectTrackRibbon(OneLive(), kPlainVariant, {}, 0);
  CHECK(quads.front().corners[0].x - sans.front().corners[0].x == Catch::Approx(3.0F));
  CHECK(quads.front().corners[0].z - sans.front().corners[0].z == Catch::Approx(-4.0F));
  CHECK(quads.front().corners[0].y == Catch::Approx(sans.front().corners[0].y));
}

TEST_CASE("Ruban : une ordonnée VOISINE de la sentinelle n'en est pas une", "[render][figure]") {
  // L'égalité est EXACTE chez le client. Une comparaison approchée ferait basculer
  // des ancres que rien ne désigne.
  EffectTrack voisine = OneLive();
  voisine.anchor = {3.0F, 1234.4F, 4.0F};
  const std::vector<EffectWorldQuad> quads =
      ComposeEffectTrackRibbon(voisine, kPlainVariant, {}, 0);
  const std::vector<EffectWorldQuad> sans =
      ComposeEffectTrackRibbon(OneLive(), kPlainVariant, {}, 0);
  REQUIRE(!quads.empty());
  CHECK(quads.front().corners[0].x == Catch::Approx(sans.front().corners[0].x));
}

TEST_CASE("Ruban : le CAP fait tourner le décalage de la sentinelle", "[render][figure]") {
  EffectTrack sentinelle = OneLive();
  sentinelle.anchor = {10.0F, kAnchorHeightThatMeansTurn, 0.0F};
  const std::vector<EffectWorldQuad> straight =
      ComposeEffectTrackRibbon(sentinelle, kPlainVariant, {}, 0);
  const std::vector<EffectWorldQuad> rotated =
      ComposeEffectTrackRibbon(sentinelle, kPlainVariant, {}, 90);
  REQUIRE(!straight.empty());
  REQUIRE(!rotated.empty());
  // Un quart de tour porte l'abscisse sur la profondeur du client, qui est
  // l'opposée de la nôtre.
  CHECK(rotated.front().corners[0].z - straight.front().corners[0].z == Catch::Approx(-10.0F));
}

TEST_CASE("Ruban : le cap se prend dans la TROISIÈME piste", "[render][figure]") {
  // ⚠ PAS DANS CELLE QU'ON DESSINE : le mouvement des groupes radiaux l'écrit
  // là, et le dessinateur l'y relit à un déplacement absolu. Le chercher dans la
  // piste courante rendrait un cap différent pour chacune des quatre.
  std::array<EffectTrack, kEffectTracksPerNode> tracks;
  tracks[kTrackThatCarriesTheTurn].style = 137;
  tracks[0].style = 11;
  CHECK(EffectRibbonTurnOf(tracks) == 137);
}
