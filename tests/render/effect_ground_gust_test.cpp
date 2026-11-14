#include "ro/render/effect_ground_gust.h"

#include <array>
#include <cmath>
#include <cstdint>
#include <optional>
#include <span>
#include <string>

#include <catch2/catch_approx.hpp>
#include <catch2/catch_test_macros.hpp>

#include "ro/render/effect_ground_gust_seed.h"
#include "ro/render/effect_node.h"
#include "ro/render/effect_node_pool.h"
#include "ro/render/effect_pooled_node_shape.h"
#include "ro/render/effect_track.h"

using Catch::Approx;
using ro::render::AdvanceGroundGustTracks;
using ro::render::ComposeGroundGustQuad;
using ro::render::EffectGroundGustBlendMode;
using ro::render::EffectGroundGustColor;
using ro::render::EffectGroundGustPattern;
using ro::render::EffectGroundGustPatternsFor;
using ro::render::EffectGroundGustPatternTable;
using ro::render::EffectTrack;
using ro::render::EffectWorldQuad;
using ro::render::kEffectTracksPerNode;
using ro::render::kGroundGustAddedBlend;
using ro::render::kGroundGustColors;
using ro::render::kGroundGustCorners;
using ro::render::kGroundGustDelayTicks;
using ro::render::kGroundGustLiftAboveNode;
using ro::render::kGroundGustLiftSpread;
using ro::render::kGroundGustMarkOfColor;
using ro::render::kGroundGustOrdinaryBlend;
using ro::render::kGroundGustPalette;
using ro::render::kGroundGustRadius;
using ro::render::kGroundGustSlotOfSpeed;
using ro::render::kGroundGustStartSpeed;
using ro::render::SeedEffectGroundGustTracks;
using ro::render::SowEffectGroundGustNode;

namespace {

// L'IDENTIFIANT UNIQUE du sous-type 119.
constexpr std::uint32_t kGustEffectId = 547;

// Les deux facteurs de mélange que la voie A pose, dans la numérotation de
// Direct3D : l'addition pour le mode zéro, le mélange ordinaire pour l'autre.
constexpr std::int32_t kBlendFactorAdd = 2;
constexpr std::int32_t kBlendFactorOrdinary = 6;

[[nodiscard]] EffectTrack LiveTrack() {
  EffectTrack track;
  track.active = true;
  track.alpha = 200;
  track.radius_at_anchor = kGroundGustRadius;
  track.anchor = {10.0F, 3.0F, -4.0F};
  track.value[kGroundGustSlotOfSpeed] = kGroundGustStartSpeed;
  return track;
}

[[nodiscard]] const EffectGroundGustPattern& SinglePattern() {
  return EffectGroundGustPatternTable().front();
}

}  // namespace

TEST_CASE("le carre est plat et sa demi-diagonale est le rayon", "[render][ground_gust]") {
  const EffectTrack track = LiveTrack();
  const std::optional<EffectWorldQuad> quad = ComposeGroundGustQuad(track);
  REQUIRE(quad.has_value());
  REQUIRE(quad->corners.size() == kGroundGustCorners);
  for (const glm::vec3& corner : quad->corners) {
    // LA FIGURE EST COUCHÉE : les quatre coins sont à la hauteur de l'ancre.
    CHECK(corner.y == Approx(track.anchor.y));
    const float offset = std::sqrt((corner.x - track.anchor.x) * (corner.x - track.anchor.x) +
                                   (corner.z - track.anchor.z) * (corner.z - track.anchor.z));
    CHECK(offset == Approx(kGroundGustRadius).margin(0.001F));
  }
  // LES COINS OPPOSÉS SE FONT FACE : leur milieu est l'ancre.
  CHECK((quad->corners[0].x + quad->corners[2].x) / 2.0F == Approx(track.anchor.x));
  CHECK((quad->corners[1].z + quad->corners[3].z) / 2.0F == Approx(track.anchor.z).margin(0.001F));
}

TEST_CASE("le premier coin est deja a un quart de tour", "[render][ground_gust]") {
  EffectTrack track = LiveTrack();
  track.tilt_degrees = 0;
  const std::optional<EffectWorldQuad> quad = ComposeGroundGustQuad(track);
  REQUIRE(quad.has_value());
  // ⚠ LE CLIENT AJOUTE QUATRE-VINGT-DIX DEGRÉS AVANT DE POSER SON PREMIER COIN.
  // Partir de l'inclinaison nue mettrait ce coin à l'est au lieu du sud.
  // Et la profondeur du client se retourne dans notre repère : son quart de
  // tour tombe à la profondeur NÉGATIVE.
  CHECK(quad->corners[0].x == Approx(track.anchor.x).margin(0.001F));
  CHECK(quad->corners[0].z == Approx(track.anchor.z - kGroundGustRadius).margin(0.001F));
}

TEST_CASE("l'inclinaison tourne le carre entier", "[render][ground_gust]") {
  EffectTrack right = LiveTrack();
  EffectTrack tournee = LiveTrack();
  tournee.tilt_degrees = 90;
  const std::optional<EffectWorldQuad> a = ComposeGroundGustQuad(right);
  const std::optional<EffectWorldQuad> b = ComposeGroundGustQuad(tournee);
  REQUIRE(a.has_value());
  REQUIRE(b.has_value());
  // UN QUART DE TOUR RAMÈNE LE CARRÉ SUR LUI-MÊME, décalé d'un coin.
  CHECK(b->corners[0].x == Approx(a->corners[1].x).margin(0.001F));
  CHECK(b->corners[0].z == Approx(a->corners[1].z).margin(0.001F));
}

TEST_CASE("les coins sont ABSOLUS", "[render][ground_gust]") {
  EffectTrack track = LiveTrack();
  track.anchor = {0.0F, 0.0F, 0.0F};
  const std::optional<EffectWorldQuad> quad = ComposeGroundGustQuad(track);
  REQUIRE(quad.has_value());
  // ⚠ L'ANCRE PORTE DÉJÀ LA POSITION DU MONDE : la figure ne prend aucune
  // origine de nœud, et une ancre nulle centre bien le carré sur l'origine.
  CHECK(quad->corners[0].x == Approx(0.0F).margin(0.001F));
  CHECK(quad->corners[0].y == Approx(0.0F));
}

TEST_CASE("une piste eteinte ou muette ne rend rien", "[render][ground_gust]") {
  EffectTrack faded = LiveTrack();
  faded.active = false;
  CHECK_FALSE(ComposeGroundGustQuad(faded).has_value());
  EffectTrack silent = LiveTrack();
  silent.alpha = 0;
  CHECK_FALSE(ComposeGroundGustQuad(silent).has_value());
}

TEST_CASE("au-dela des quatre teintes, RIEN n'est dessine", "[render][ground_gust]") {
  for (std::uint8_t mark = 0; mark < kGroundGustColors; ++mark) {
    EffectTrack track = LiveTrack();
    track.mark[kGroundGustMarkOfColor] = mark;
    const std::optional<EffectWorldQuad> quad = ComposeGroundGustQuad(track);
    REQUIRE(quad.has_value());
    CHECK(quad->tint[0] == kGroundGustPalette[mark][0]);
    CHECK(quad->tint[1] == kGroundGustPalette[mark][1]);
    CHECK(quad->tint[2] == kGroundGustPalette[mark][2]);
    CHECK(quad->tint[3] == track.alpha);
  }
  // ⚠ L'AIGUILLAGE DU CLIENT N'A PAS DE CAS PAR DÉFAUT : une marque de plus ne
  // peint pas davantage.
  EffectTrack hors = LiveTrack();
  hors.mark[kGroundGustMarkOfColor] = static_cast<std::uint8_t>(kGroundGustColors);
  CHECK_FALSE(ComposeGroundGustQuad(hors).has_value());
}

TEST_CASE("les deux premieres teintes s'ajoutent, les deux autres non", "[render][ground_gust]") {
  CHECK(EffectGroundGustBlendMode(0) == kGroundGustAddedBlend);
  CHECK(EffectGroundGustBlendMode(1) == kGroundGustAddedBlend);
  CHECK(EffectGroundGustBlendMode(2) == kGroundGustOrdinaryBlend);
  CHECK(EffectGroundGustBlendMode(3) == kGroundGustOrdinaryBlend);
  EffectTrack bleue = LiveTrack();
  bleue.mark[kGroundGustMarkOfColor] = 0;
  EffectTrack blanche = LiveTrack();
  blanche.mark[kGroundGustMarkOfColor] = 2;
  CHECK(ComposeGroundGustQuad(bleue)->blend_destination == kBlendFactorAdd);
  CHECK(ComposeGroundGustQuad(blanche)->blend_destination == kBlendFactorOrdinary);
  CHECK(EffectGroundGustColor(0)[2] == 255);
}

TEST_CASE("l'image est retournee", "[render][ground_gust]") {
  const std::optional<EffectWorldQuad> quad = ComposeGroundGustQuad(LiveTrack());
  REQUIRE(quad.has_value());
  // ⚠ UN AUX DEUX PREMIERS COINS, ZÉRO AUX DEUX DERNIERS.
  CHECK(quad->texture_uv[0].y == Approx(1.0F));
  CHECK(quad->texture_uv[1].y == Approx(1.0F));
  CHECK(quad->texture_uv[2].y == Approx(0.0F));
  CHECK(quad->texture_uv[3].y == Approx(0.0F));
}

TEST_CASE("la bourrasque se fait attendre trois tics", "[render][ground_gust]") {
  std::uint32_t seed = 1U;
  std::array<EffectTrack, kEffectTracksPerNode> tracks =
      SeedEffectGroundGustTracks({0.0F, 0.0F, 0.0F}, {1.0F, 0.0F, 0.0F}, seed);
  for (std::int32_t tick = 0; tick < kGroundGustDelayTicks; ++tick) {
    AdvanceGroundGustTracks(tracks);
    CHECK(tracks[0].alpha == 0);
    CHECK(tracks[0].tilt_degrees == 0);
  }
  // LE QUATRIÈME TIC EST LE PREMIER UTILE : il tourne, mais ne peint pas encore.
  AdvanceGroundGustTracks(tracks);
  CHECK(tracks[0].ticks == 1);
  CHECK(tracks[0].tilt_degrees == 30);
  CHECK(tracks[0].alpha == 0);
}

TEST_CASE("l'alpha monte par vingt-cinq puis tombe par cinq", "[render][ground_gust]") {
  std::uint32_t seed = 1U;
  std::array<EffectTrack, kEffectTracksPerNode> tracks =
      SeedEffectGroundGustTracks({0.0F, 0.0F, 0.0F}, {1.0F, 0.0F, 0.0F}, seed);
  // Quatre tics pour atteindre le tic un, puis le tic deux ouvre la montée.
  for (std::int32_t tick = 0; tick < kGroundGustDelayTicks + 2; ++tick) {
    AdvanceGroundGustTracks(tracks);
  }
  REQUIRE(tracks[0].ticks == 2);
  CHECK(tracks[0].alpha == 25);
  // Neuf montées en tout, des tics deux à dix.
  for (std::int32_t tick = 0; tick < 8; ++tick) {
    AdvanceGroundGustTracks(tracks);
  }
  REQUIRE(tracks[0].ticks == 10);
  CHECK(tracks[0].alpha == 225);
  AdvanceGroundGustTracks(tracks);
  CHECK(tracks[0].ticks == 11);
  CHECK(tracks[0].alpha == 220);
}

TEST_CASE("la vitesse enfle, plus vite apres la montee", "[render][ground_gust]") {
  std::uint32_t seed = 1U;
  std::array<EffectTrack, kEffectTracksPerNode> tracks =
      SeedEffectGroundGustTracks({0.0F, 0.0F, 0.0F}, {1.0F, 0.0F, 0.0F}, seed);
  // ⚠ LE TIC DU CAP NE TOUCHE PAS LA VITESSE : elle vaut encore sa valeur de
  // semis au premier tic utile.
  for (std::int32_t tick = 0; tick < kGroundGustDelayTicks + 1; ++tick) {
    AdvanceGroundGustTracks(tracks);
  }
  REQUIRE(tracks[0].ticks == 1);
  CHECK(tracks[0].value[kGroundGustSlotOfSpeed] == Approx(kGroundGustStartSpeed));
  AdvanceGroundGustTracks(tracks);
  CHECK(tracks[0].value[kGroundGustSlotOfSpeed] == Approx(kGroundGustStartSpeed * 1.02F));
  for (std::int32_t tick = 0; tick < 9; ++tick) {
    AdvanceGroundGustTracks(tracks);
  }
  REQUIRE(tracks[0].ticks == 11);
  CHECK(tracks[0].value[kGroundGustSlotOfSpeed] ==
        Approx(kGroundGustStartSpeed * std::pow(1.02F, 9.0F) * 1.04F));
}

TEST_CASE("la meneuse file le long de son cap, sans changer de hauteur", "[render][ground_gust]") {
  std::uint32_t seed = 1U;
  std::array<EffectTrack, kEffectTracksPerNode> tracks =
      SeedEffectGroundGustTracks({0.0F, 0.0F, 0.0F}, {1.0F, 0.0F, 0.0F}, seed);
  const float height = tracks[0].anchor.y;
  for (std::int32_t tick = 0; tick < kGroundGustDelayTicks + 1; ++tick) {
    AdvanceGroundGustTracks(tracks);
  }
  // CAP ZÉRO : la bourrasque part vers l'est, et sa hauteur ne bouge pas.
  CHECK(tracks[0].azimuth_degrees == 0);
  CHECK(tracks[0].anchor.x == Approx(kGroundGustStartSpeed));
  CHECK(tracks[0].anchor.z == Approx(0.0F).margin(0.001F));
  CHECK(tracks[0].anchor.y == Approx(height));
}

TEST_CASE("l'echo passe tous les trois tics, dans l'ordre descendant", "[render][ground_gust]") {
  std::uint32_t seed = 1U;
  std::array<EffectTrack, kEffectTracksPerNode> tracks =
      SeedEffectGroundGustTracks({0.0F, 0.0F, 0.0F}, {1.0F, 0.0F, 0.0F}, seed);
  // Jusqu'au tic deux : la meneuse a de l'alpha, les échos n'ont rien encore.
  for (std::int32_t tick = 0; tick < kGroundGustDelayTicks + 2; ++tick) {
    AdvanceGroundGustTracks(tracks);
  }
  REQUIRE(tracks[0].alpha == 25);
  CHECK(tracks[1].alpha == 0);
  // Le tic trois déclenche le premier écho.
  AdvanceGroundGustTracks(tracks);
  REQUIRE(tracks[0].ticks == 3);
  CHECK(tracks[0].alpha == 50);
  CHECK(tracks[1].alpha == 40);
  CHECK(tracks[1].anchor.x == Approx(tracks[0].anchor.x));
  CHECK(tracks[1].tilt_degrees == tracks[0].tilt_degrees);
  // ⚠ ET LES DEUX SUIVANTES N'ONT RIEN REÇU : l'écho descend d'un cran par
  // passage. Les remplir d'un coup donnerait quatre carrés identiques.
  CHECK(tracks[2].alpha == 0);
  CHECK(tracks[3].alpha == 0);
  for (std::int32_t tick = 0; tick < 3; ++tick) {
    AdvanceGroundGustTracks(tracks);
  }
  REQUIRE(tracks[0].ticks == 6);
  CHECK(tracks[2].alpha == 32);
  CHECK(tracks[3].alpha == 0);
}

TEST_CASE("la mort de la meneuse fige les echos", "[render][ground_gust]") {
  std::uint32_t seed = 1U;
  std::array<EffectTrack, kEffectTracksPerNode> tracks =
      SeedEffectGroundGustTracks({0.0F, 0.0F, 0.0F}, {1.0F, 0.0F, 0.0F}, seed);
  for (std::int32_t tick = 0; tick < 500; ++tick) {
    AdvanceGroundGustTracks(tracks);
  }
  CHECK_FALSE(tracks[0].active);
  CHECK(tracks[0].alpha == 0);
  // ⚠ LE MOUVEMENT SORT TOUT ENTIER QUAND LA MENEUSE MEURT : les trois échos
  // gardent leur dernier alpha, et c'est la vie du nœud qui les efface. C'est
  // mesuré, pas choisi.
  CHECK(tracks[1].active);
  CHECK(tracks[1].alpha > 0);
}

TEST_CASE("l'inclinaison reste dans le tour", "[render][ground_gust]") {
  std::uint32_t seed = 1U;
  std::array<EffectTrack, kEffectTracksPerNode> tracks =
      SeedEffectGroundGustTracks({0.0F, 0.0F, 0.0F}, {1.0F, 0.0F, 0.0F}, seed);
  for (std::int32_t tick = 0; tick < 40; ++tick) {
    AdvanceGroundGustTracks(tracks);
    CHECK(tracks[0].tilt_degrees >= 0);
    CHECK(tracks[0].tilt_degrees < 360);
  }
}

TEST_CASE("le semis allume les quatre pistes, toutes transparentes", "[render][ground_gust]") {
  std::uint32_t seed = 7U;
  const std::array<EffectTrack, kEffectTracksPerNode> tracks =
      SeedEffectGroundGustTracks({5.0F, 2.0F, -1.0F}, {0.0F, 0.0F, 0.0F}, seed);
  for (std::size_t rang = 0; rang < tracks.size(); ++rang) {
    CHECK(tracks[rang].active);
    CHECK(tracks[rang].alpha == 0);
    CHECK(tracks[rang].radius_at_anchor == Approx(kGroundGustRadius));
    CHECK(tracks[rang].ticks == static_cast<std::int32_t>(rang) - kGroundGustDelayTicks);
    CHECK(tracks[rang].tilt_degrees == static_cast<std::int16_t>(rang));
    CHECK(tracks[rang].value[kGroundGustSlotOfSpeed] == Approx(kGroundGustStartSpeed));
    CHECK(tracks[rang].mark[kGroundGustMarkOfColor] == 0);
    CHECK(tracks[rang].anchor.x == Approx(5.0F));
    CHECK(tracks[rang].anchor.z == Approx(-1.0F));
    // ⚠ NOTRE REPÈRE MONTE : la bourrasque flotte AU-DESSUS du nœud, et le
    // tirage la redescend d'au plus quatre.
    CHECK(tracks[rang].anchor.y <= 2.0F + kGroundGustLiftAboveNode);
    CHECK(tracks[rang].anchor.y >
          2.0F + kGroundGustLiftAboveNode - static_cast<float>(kGroundGustLiftSpread));
  }
}

TEST_CASE("le semis tire UNE fois par piste", "[render][ground_gust]") {
  std::uint32_t four = 11U;
  static_cast<void>(SeedEffectGroundGustTracks({0.0F, 0.0F, 0.0F}, {0.0F, 0.0F, 0.0F}, four));
  std::uint32_t witness = 11U;
  for (std::size_t rang = 0; rang < kEffectTracksPerNode; ++rang) {
    static_cast<void>(ro::render::NextEffectNodeDraw(witness));
  }
  // ⚠ QUATRE TIRAGES, UN PAR PISTE — échos compris. Sauter ceux des échos
  // décalerait toute la suite du générateur.
  CHECK(four == witness);
}

TEST_CASE("le cap vient de la cible", "[render][ground_gust]") {
  std::uint32_t seed = 1U;
  const std::array<EffectTrack, kEffectTracksPerNode> vers_le_nord =
      SeedEffectGroundGustTracks({0.0F, 0.0F, 0.0F}, {0.0F, 0.0F, 1.0F}, seed);
  CHECK(vers_le_nord[0].azimuth_degrees == 90);
  std::uint32_t other = 1U;
  const std::array<EffectTrack, kEffectTracksPerNode> without_target =
      SeedEffectGroundGustTracks({0.0F, 0.0F, 0.0F}, {0.0F, 0.0F, 0.0F}, other);
  // ⚠ UNE CIBLE ABSENTE REND ZÉRO, et c'est mesuré : la division du client
  // donne un NaN, et sa boucle sort au premier degré.
  CHECK(without_target[0].azimuth_degrees == 0);
}

TEST_CASE("la table porte l'unique identifiant du sous-type 119", "[render][ground_gust]") {
  const std::span<const EffectGroundGustPattern> table = EffectGroundGustPatternTable();
  REQUIRE(table.size() == 1);
  CHECK(table.front().effect_id == kGustEffectId);
  CHECK(table.front().client_function == "Effect_Build_Scatter77FromParam");
  CHECK_FALSE(table.front().texture.empty());
  CHECK(EffectGroundGustPatternsFor(1U).empty());
}

TEST_CASE("le noeud seme porte sa forme et son image", "[render][ground_gust]") {
  std::uint32_t seed = 3U;
  const ro::render::PooledEffectNode node =
      SowEffectGroundGustNode(SinglePattern(), {2.0F, 0.0F, 7.0F}, {1.0F, 0.0F, 0.0F}, 600, seed);
  CHECK(node.shape == ro::render::PooledNodeShape::kGroundGust);
  CHECK(node.node.life_ticks == 600);
  REQUIRE(node.resource_paths.size() == 1);
  CHECK(node.resource_paths.front() == std::string(SinglePattern().texture));
  CHECK(node.ground_position.x == Approx(2.0F));
}
