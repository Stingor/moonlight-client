#include "ro/render/effect_gliding_slash.h"

#include <algorithm>
#include <array>
#include <cmath>
#include <cstdint>
#include <optional>
#include <set>
#include <span>

#include <catch2/catch_approx.hpp>
#include <catch2/catch_test_macros.hpp>

#include "ro/render/effect_gliding_slash_seed.h"
#include "ro/render/effect_node_pool.h"
#include "ro/render/effect_pooled_node_shape.h"
#include "ro/render/effect_track.h"
#include "ro/render/effect_track_burst.h"

using Catch::Approx;
using ro::render::AdvanceGlidingSlashTracks;
using ro::render::ComposeGlidingSlashQuad;
using ro::render::EffectGlidingSlashBuilder;
using ro::render::EffectGlidingSlashColor;
using ro::render::EffectGlidingSlashFrame;
using ro::render::EffectGlidingSlashPattern;
using ro::render::EffectGlidingSlashPatternsFor;
using ro::render::EffectGlidingSlashPatternTable;
using ro::render::EffectGlidingSlashTexturesOf;
using ro::render::EffectTrack;
using ro::render::EffectWorldQuad;
using ro::render::kEffectTracksPerNode;
using ro::render::kGlidingSlashAlphaRise;
using ro::render::kGlidingSlashAnimatedMark;
using ro::render::kGlidingSlashBirthHeight;
using ro::render::kGlidingSlashDelayTicks;
using ro::render::kGlidingSlashFrameCount;
using ro::render::kGlidingSlashGreen;
using ro::render::kGlidingSlashGreenMark;
using ro::render::kGlidingSlashPale;
using ro::render::kGlidingSlashPlain;
using ro::render::kGlidingSlashPlainTexture;
using ro::render::kGlidingSlashRadius;
using ro::render::kGlidingSlashRisesUntilTick;
using ro::render::kGlidingSlashSpeed;
using ro::render::kGlidingSlashSpeedGainWhileFading;
using ro::render::kGlidingSlashSpeedGainWhileRising;
using ro::render::kGlidingSlashTicksPerFrame;
using ro::render::kGlidingSlashWaveTextures;
using ro::render::SeedEffectGlidingSlashTracks;
using ro::render::SowEffectGlidingSlashNode;

namespace {

[[nodiscard]] EffectTrack Vivante(std::uint8_t mark = 0, std::int16_t azimuth = 0) {
  EffectTrack track;
  track.active = true;
  track.alpha = 100;
  track.ticks = 5;
  track.azimuth_degrees = azimuth;
  track.radius_at_anchor = kGlidingSlashRadius;
  track.anchor = {10.0F, 8.0F, 20.0F};
  track.value[1] = kGlidingSlashSpeed;
  track.mark[1] = mark;
  return track;
}

[[nodiscard]] std::array<EffectTrack, kEffectTracksPerNode> Sown(
    EffectGlidingSlashBuilder batisseur, std::uint8_t octet = 0, std::int16_t azimuth = 0,
    std::uint32_t seed = 6U) {
  EffectGlidingSlashPattern pattern;
  pattern.builder = batisseur;
  pattern.mark = octet;
  pattern.azimuth_degrees = azimuth;
  return SeedEffectGlidingSlashTracks(pattern, {4.0F, 1.0F, 5.0F}, seed);
}

}  // namespace

TEST_CASE("la taillade est un carre PLAT tourne d'un huitieme de tour", "[render][effect][slash]") {
  // ⚠ RIEN N'Y EST COURBE, malgré le nom du client : quatre coins à la même
  // hauteur, sur un cercle, et le premier à quarante-cinq degrés de l'azimut.
  const std::optional<EffectWorldQuad> quad = ComposeGlidingSlashQuad(Vivante());
  REQUIRE(quad.has_value());
  for (const glm::vec3& corner : quad->corners) {
    CHECK(corner.y == Approx(8.0F));
    const float dx = corner.x - 10.0F;
    const float dz = corner.z - 20.0F;
    CHECK(std::sqrt(dx * dx + dz * dz) == Approx(kGlidingSlashRadius));
  }
  // Le premier coin est à quarante-cinq degrés dans le repère du client, donc à
  // moins quarante-cinq dans le nôtre, dont la profondeur est opposée : ses
  // deux écarts sont égaux et de signes contraires.
  CHECK(quad->corners[0].x - 10.0F == Approx(-(quad->corners[0].z - 20.0F)));
  CHECK(quad->corners[0].z - 20.0F < 0.0F);
  // Et les coins font le TOUR : le troisième est l'opposé du premier.
  CHECK(quad->corners[2].x - 10.0F == Approx(-(quad->corners[0].x - 10.0F)));
}

TEST_CASE("les coins de la taillade sont ABSOLUS", "[render][effect][slash]") {
  // ⚠ SON ANCRE PORTE LES TROIS COORDONNÉES, hauteur comprise, et le mouvement
  // la fait glisser. Y ajouter l'origine du nœud doublerait la position.
  EffectTrack far_point = Vivante();
  far_point.anchor = {110.0F, 8.0F, 20.0F};
  CHECK(ComposeGlidingSlashQuad(far_point)->corners[0].x ==
        Approx(ComposeGlidingSlashQuad(Vivante())->corners[0].x + 100.0F));
}

TEST_CASE("une piste eteinte ou muette ne rend rien", "[render][effect][slash]") {
  EffectTrack track = Vivante();
  track.active = false;
  CHECK_FALSE(ComposeGlidingSlashQuad(track).has_value());
  track = Vivante();
  track.alpha = 0;
  CHECK_FALSE(ComposeGlidingSlashQuad(track).has_value());
}

TEST_CASE("la SECONDE marque choisit la teinte", "[render][effect][slash]") {
  // ⚠ C'EST LA MARQUE UN, PAS LA ZÉRO : les deux bâtisseurs mettent la zéro à
  // zéro et rangent l'octet du site dans la suivante. Lire la première rendrait
  // les quatre taillades de la même couleur.
  CHECK(EffectGlidingSlashColor(kGlidingSlashAnimatedMark) == kGlidingSlashPale);
  CHECK(EffectGlidingSlashColor(kGlidingSlashGreenMark) == kGlidingSlashGreen);
  CHECK(EffectGlidingSlashColor(0) == kGlidingSlashPlain);
  CHECK(EffectGlidingSlashColor(7) == kGlidingSlashPlain);

  EffectTrack verte = Vivante(kGlidingSlashGreenMark);
  const std::optional<EffectWorldQuad> quad = ComposeGlidingSlashQuad(verte);
  REQUIRE(quad.has_value());
  CHECK(quad->tint[0] == 0);
  CHECK(quad->tint[1] == 255);
  CHECK(quad->tint[2] == 0);
  CHECK(quad->tint[3] == 100);
}

TEST_CASE("une seule marque fait DEFILER son image", "[render][effect][slash]") {
  // ⚠ UNE SUR TROIS, sur quatre images et une tous les SIX tics. Donner l'index
  // aux autres les laisserait sans image cinq fois sur six — leur bâtisseur
  // n'en charge qu'une.
  CHECK(EffectGlidingSlashFrame(kGlidingSlashAnimatedMark, 1) == 0);
  CHECK(EffectGlidingSlashFrame(kGlidingSlashAnimatedMark, kGlidingSlashTicksPerFrame) == 1);
  CHECK(EffectGlidingSlashFrame(kGlidingSlashAnimatedMark, 3 * kGlidingSlashTicksPerFrame) == 3);
  // Et l'index reboucle sur quatre.
  CHECK(EffectGlidingSlashFrame(kGlidingSlashAnimatedMark,
                                kGlidingSlashFrameCount * kGlidingSlashTicksPerFrame) == 0);
  // ⚠ LE COMPTEUR EST NÉGATIF PENDANT LE RETARD : le client rend alors la
  // PREMIÈRE image plutôt qu'un index négatif.
  CHECK(EffectGlidingSlashFrame(kGlidingSlashAnimatedMark, -20) == 0);
  CHECK(EffectGlidingSlashFrame(0, 3 * kGlidingSlashTicksPerFrame) == 0);
  CHECK(EffectGlidingSlashFrame(kGlidingSlashGreenMark, 3 * kGlidingSlashTicksPerFrame) == 0);
}

TEST_CASE("la taillade GLISSE et ACCELERE en s'effacant", "[render][effect][slash]") {
  // ⚠ SA VITESSE CROÎT PLUS VITE EN MOURANT QU'EN NAISSANT : deux pour cent par
  // tic tant que l'alpha monte, QUATRE ensuite. Le même facteur pour les deux
  // la laisserait traîner.
  std::array<EffectTrack, kEffectTracksPerNode> tracks;
  tracks[0] = Vivante(0, 0);
  tracks[0].ticks = 1;
  tracks[0].alpha = 0;
  std::uint32_t seed = 1U;
  const float start = tracks[0].anchor.x;

  AdvanceGlidingSlashTracks(tracks, false, {}, seed);
  CHECK(tracks[0].alpha == kGlidingSlashAlphaRise);
  CHECK(tracks[0].value[1] == Approx(kGlidingSlashSpeed * kGlidingSlashSpeedGainWhileRising));
  // L'azimut zéro fait glisser vers l'est.
  CHECK(tracks[0].anchor.x > start);
  CHECK(tracks[0].anchor.z == Approx(20.0F));

  tracks[0].ticks = kGlidingSlashRisesUntilTick;
  const float speed = tracks[0].value[1];
  AdvanceGlidingSlashTracks(tracks, false, {}, seed);
  CHECK(tracks[0].value[1] == Approx(speed * kGlidingSlashSpeedGainWhileFading));
  CHECK(tracks[0].alpha == kGlidingSlashAlphaRise - 5);
}

TEST_CASE("la taillade meurt par son ALPHA", "[render][effect][slash]") {
  // ⚠ PAS PAR SON ÂGE : quand la chute le fait passer sous zéro, le client
  // l'éteint. La laisser vivre ferait glisser un carré invisible à l'infini.
  std::array<EffectTrack, kEffectTracksPerNode> tracks;
  tracks[0] = Vivante();
  tracks[0].ticks = 1;
  tracks[0].alpha = 0;
  std::uint32_t seed = 1U;
  for (std::int32_t tick = 0; tick < 60; ++tick) {
    AdvanceGlidingSlashTracks(tracks, false, {}, seed);
  }
  CHECK_FALSE(tracks[0].active);
  CHECK(tracks[0].alpha == 0);
}

TEST_CASE("le premier tic ne fait NI alpha NI vitesse", "[render][effect][slash]") {
  // ⚠ LE CLIENT RANGE LA VISÉE ET LE RESTE DANS DES BRANCHES EXCLUSIVES : au
  // tic où le compteur passe de zéro à un, rien d'autre que la glissade. Les
  // fondre donnerait une taillade d'un tic d'avance.
  std::array<EffectTrack, kEffectTracksPerNode> tracks;
  tracks[0] = Vivante();
  tracks[0].ticks = 0;
  tracks[0].alpha = 0;
  std::uint32_t seed = 1U;
  AdvanceGlidingSlashTracks(tracks, false, {}, seed);
  CHECK(tracks[0].ticks == 1);
  CHECK(tracks[0].alpha == 0);
  CHECK(tracks[0].value[1] == Approx(kGlidingSlashSpeed));
}

TEST_CASE("le retard court sans rien faire", "[render][effect][slash]") {
  std::array<EffectTrack, kEffectTracksPerNode> tracks;
  tracks[0] = Vivante();
  tracks[0].ticks = -3;
  tracks[0].alpha = 0;
  const float start = tracks[0].anchor.x;
  std::uint32_t seed = 1U;
  AdvanceGlidingSlashTracks(tracks, false, {}, seed);
  CHECK(tracks[0].ticks == -2);
  CHECK(tracks[0].anchor.x == Approx(start));
}

TEST_CASE("un seul des deux sous-types VISE", "[render][effect][slash]") {
  // ⚠ RIEN DANS L'ÉTAT DU NŒUD NE LES DISTINGUE : c'est la FORME qui le dit.
  // Le deviner ferait viser une taillade sur deux à l'envers.
  const auto azimuth_after_one_tick = [](bool aimed, std::uint8_t mark) {
    std::array<EffectTrack, kEffectTracksPerNode> tracks;
    tracks[0] = Vivante(mark, 200);
    tracks[0].ticks = 0;
    std::uint32_t seed = 4U;
    AdvanceGlidingSlashTracks(tracks, aimed, {0.0F, 0.0F, 10.0F}, seed);
    return tracks[0].azimuth_degrees;
  };
  // Celui qui ne vise pas garde l'azimut de son site.
  CHECK(azimuth_after_one_tick(false, 0) == 200);
  // ⚠ ET LA MÊME MARQUE QUI ANIME L'IMAGE RESSERRE LE CAP : cinq degrés de
  // part et d'autre au lieu de quinze. Un octet, deux conséquences.
  const std::int16_t narrow = azimuth_after_one_tick(true, kGlidingSlashAnimatedMark);
  CHECK(std::abs(narrow - 90) <= 5);
  const std::int16_t large = azimuth_after_one_tick(true, 0);
  CHECK(std::abs(large - 90) <= 15);
}

TEST_CASE("les quatre pistes se suivent a un tic", "[render][effect][slash]") {
  // ⚠ VINGT-HUIT TICS AVANT DE PARAÎTRE, et un tic d'écart : la taillade
  // arrive APRÈS le coup. La semer à zéro la montrerait une seconde trop tôt.
  const std::array<EffectTrack, kEffectTracksPerNode> tracks =
      Sown(EffectGlidingSlashBuilder::kSonicWave, 1);
  for (std::size_t rang = 0; rang < tracks.size(); ++rang) {
    CHECK(tracks[rang].active);
    CHECK(tracks[rang].alpha == 0);
    CHECK(tracks[rang].ticks == static_cast<std::int32_t>(rang) - kGlidingSlashDelayTicks);
    CHECK(tracks[rang].radius_at_anchor == Approx(kGlidingSlashRadius));
    CHECK(tracks[rang].value[1] == Approx(kGlidingSlashSpeed));
    CHECK(tracks[rang].mark[0] == 0);
    CHECK(tracks[rang].mark[1] == 1);
  }
}

TEST_CASE("le second batisseur tire un retard de PLUS", "[render][effect][slash]") {
  // ⚠ UNE FOIS POUR LES QUATRE : deux taillades lancées ensemble ne partent pas
  // au même tic.
  std::set<std::int32_t> departs;
  for (std::uint32_t seed = 1U; seed < 30U; ++seed) {
    const std::array<EffectTrack, kEffectTracksPerNode> tracks =
        Sown(EffectGlidingSlashBuilder::kPurpleSlash, 0, 0, seed);
    departs.insert(tracks[0].ticks);
    CHECK(tracks[0].ticks <= -kGlidingSlashDelayTicks);
    CHECK(tracks[0].ticks >= -kGlidingSlashDelayTicks - 5);
    // Et l'écart d'un tic entre les quatre est conservé.
    CHECK(tracks[3].ticks == tracks[0].ticks + 3);
  }
  CHECK(departs.size() > 1);
}

TEST_CASE("les carres naissent AU-DESSUS du sol", "[render][effect][slash]") {
  // ⚠ LE CLIENT RETRANCHE DIX, ET SON REPÈRE DESCEND : recopier le signe les
  // enterrerait. Le tirage de cinq change de signe avec lui.
  std::set<float> hauteurs;
  for (const EffectTrack& track : Sown(EffectGlidingSlashBuilder::kSonicWave, 1)) {
    CHECK(track.anchor.y <= 1.0F + kGlidingSlashBirthHeight);
    CHECK(track.anchor.y > 1.0F + kGlidingSlashBirthHeight - 5.0F);
    CHECK(track.anchor.x == Approx(4.0F));
    CHECK(track.anchor.z == Approx(5.0F));
    hauteurs.insert(track.anchor.y);
  }
  CHECK(hauteurs.size() > 1);
}

TEST_CASE("le second batisseur garde l'azimut de son site", "[render][effect][slash]") {
  for (const EffectTrack& track : Sown(EffectGlidingSlashBuilder::kPurpleSlash, 0, 75)) {
    CHECK(track.azimuth_degrees == 75);
    // Et sa marque de teinte est ZÉRO, quoi que le patron porte.
    CHECK(track.mark[1] == 0);
  }
  // Le premier, lui, part à zéro et laisse le mouvement viser.
  CHECK(Sown(EffectGlidingSlashBuilder::kSonicWave, 2)[0].azimuth_degrees == 0);
}

TEST_CASE("un octet sur trois charge QUATRE images", "[render][effect][slash]") {
  // ⚠ LE RELEVÉ EN LISTAIT CINQ, parce que son parcours en avant voyait les
  // deux branches. En charger cinq donnerait à l'index d'image une case que le
  // client n'a pas.
  EffectGlidingSlashPattern anime;
  anime.builder = EffectGlidingSlashBuilder::kSonicWave;
  anime.mark = kGlidingSlashAnimatedMark;
  CHECK(EffectGlidingSlashTexturesOf(anime).size() == kGlidingSlashFrameCount);
  CHECK(EffectGlidingSlashTexturesOf(anime)[0] == kGlidingSlashWaveTextures[0]);

  EffectGlidingSlashPattern uni;
  uni.builder = EffectGlidingSlashBuilder::kSonicWave;
  uni.mark = 2;
  REQUIRE(EffectGlidingSlashTexturesOf(uni).size() == 1);
  CHECK(EffectGlidingSlashTexturesOf(uni)[0] == kGlidingSlashPlainTexture);

  EffectGlidingSlashPattern second;
  second.builder = EffectGlidingSlashBuilder::kPurpleSlash;
  second.mark = kGlidingSlashAnimatedMark;
  // ⚠ LE SECOND BÂTISSEUR N'EN CHARGE JAMAIS QU'UNE, quel que soit l'octet.
  CHECK(EffectGlidingSlashTexturesOf(second).size() == 1);
}

TEST_CASE("la table des taillades est triee et en refuse un", "[render][effect][slash]") {
  const std::span<const EffectGlidingSlashPattern> table = EffectGlidingSlashPatternTable();
  REQUIRE(table.size() == 4);
  CHECK(std::ranges::is_sorted(table, {}, &EffectGlidingSlashPattern::effect_id));
  CHECK(EffectGlidingSlashPatternsFor(361).front().mark == 0);
  CHECK(EffectGlidingSlashPatternsFor(720).front().mark == 1);
  CHECK(EffectGlidingSlashPatternsFor(1082).front().mark == 2);
  CHECK(EffectGlidingSlashPatternsFor(409).front().builder ==
        EffectGlidingSlashBuilder::kPurpleSlash);
  // ⚠ 733 EST ABSENT, et le relevé des nœuds se trompe à son sujet : son bloc
  // n'appelle jamais le bâtisseur, qui n'a que trois sites.
  CHECK(EffectGlidingSlashPatternsFor(733).empty());
}

TEST_CASE("la forme du noeud dit lequel vise", "[render][effect][slash]") {
  EffectGlidingSlashPattern aimed;
  aimed.builder = EffectGlidingSlashBuilder::kSonicWave;
  aimed.mark = 1;
  std::uint32_t seed = 3U;
  const ro::render::PooledEffectNode aveugle = SowEffectGlidingSlashNode(
      EffectGlidingSlashPattern{0, "", EffectGlidingSlashBuilder::kPurpleSlash, 0, 0}, {}, 100,
      seed);
  CHECK(aveugle.shape == ro::render::PooledNodeShape::kGlidingSlash);
  CHECK(aveugle.resource_paths.size() == 1);

  const ro::render::PooledEffectNode viseur = SowEffectGlidingSlashNode(aimed, {}, 100, seed);
  CHECK(viseur.shape == ro::render::PooledNodeShape::kAimedGlidingSlash);
  CHECK(viseur.resource_paths.size() == kGlidingSlashFrameCount);
}
