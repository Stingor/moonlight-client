#include <algorithm>
#include <array>
#include <cmath>
#include <numbers>
#include <set>
#include <string>

#include <catch2/catch_approx.hpp>
#include <catch2/catch_test_macros.hpp>

#include "ro/render/effect_node_pool.h"
#include "ro/render/effect_pulsing_rings_seed.h"
#include "ro/render/effect_track_burst.h"
#include "ro/render/effect_track_figures.h"

using Catch::Approx;
using ro::render::AdvanceRadialBurstTracks;
using ro::render::ComposeEffectTrackSkirt;
using ro::render::EffectNodePool;
using ro::render::EffectPulsingRingsPattern;
using ro::render::EffectPulsingRingsPatternsFor;
using ro::render::EffectPulsingRingsPatternTable;
using ro::render::EffectTrack;
using ro::render::EffectWorldQuad;
using ro::render::kEffectRadialBurstRegisterTrack;
using ro::render::kEffectRadialBurstRingCount;
using ro::render::kEffectTracksPerNode;
using ro::render::kPulsingRingLiftKey;
using ro::render::PooledNodeShape;
using ro::render::SeedEffectPulsingRingsTracks;
using ro::render::SowEffectPulsingRingsNode;

namespace {

// UN PATRON D'ESSAI : seul le mode change d'un cas à l'autre, et c'est lui qui
// porte tout le mouvement.
[[nodiscard]] EffectPulsingRingsPattern Pattern(std::int16_t mode, std::int16_t delay_step = 10) {
  return EffectPulsingRingsPattern{0, "essai", "image.tga", 16, mode, 100, delay_step, 0, 0};
}

[[nodiscard]] std::array<EffectTrack, kEffectTracksPerNode> Advanced(std::int16_t mode,
                                                                     std::int32_t ticks) {
  std::array<EffectTrack, kEffectTracksPerNode> tracks =
      SeedEffectPulsingRingsTracks(Pattern(mode));
  for (std::int32_t tick = 0; tick < ticks; ++tick) {
    AdvanceRadialBurstTracks(tracks);
  }
  return tracks;
}

}  // namespace

TEST_CASE("le semis pose trois anneaux et un registre", "[render][effect][pulsing]") {
  const std::array<EffectTrack, kEffectTracksPerNode> tracks =
      SeedEffectPulsingRingsTracks(Pattern(4));

  // LES TROIS PREMIÈRES VIVENT, la quatrième non : elle ne se dessine pas.
  CHECK(tracks[0].active);
  CHECK(tracks[1].active);
  CHECK(tracks[2].active);
  CHECK_FALSE(tracks[kEffectRadialBurstRegisterTrack].active);

  // ⚠ ET LE REGISTRE PORTE LE MODE DANS SON STYLE. C'est la seule trace du mode
  // dans le nœud : le perdre perdrait tout le mouvement.
  CHECK(tracks[kEffectRadialBurstRegisterTrack].style == 4);
  CHECK(tracks[kEffectRadialBurstRegisterTrack].ticks == 0);

  // LES TROIS PARTENT L'UN APRÈS L'AUTRE.
  CHECK(tracks[0].ticks == 0);
  CHECK(tracks[1].ticks == -10);
  CHECK(tracks[2].ticks == -20);

  // Décalés en azimut et en inclinaison, et fermés sur le tour entier.
  CHECK(tracks[0].azimuth_degrees == 0);
  CHECK(tracks[1].azimuth_degrees == 25);
  CHECK(tracks[2].azimuth_degrees == 50);
  CHECK(tracks[0].tilt_degrees == 2);
  CHECK(tracks[1].tilt_degrees == 3);
  CHECK(tracks[2].tilt_degrees == 4);
  for (std::size_t rang = 0; rang < kEffectRadialBurstRingCount; ++rang) {
    CHECK(tracks[rang].degrees == 360);
    CHECK(tracks[rang].style == 100);
    CHECK(tracks[rang].lift_key == kPulsingRingLiftKey);
    // Ils naissent sans rayon et sans alpha : c'est le mouvement qui les ouvre.
    CHECK(tracks[rang].radius_at_anchor == 0.0F);
    CHECK(tracks[rang].alpha == 0);
    CHECK(tracks[rang].value[0] == 0.0F);
    CHECK(tracks[rang].value[20] == 0.0F);
    // ⚠ LES VINGT ET UNE MARQUES VALENT UN, et non zéro.
    CHECK(tracks[rang].mark[0] == 1);
    CHECK(tracks[rang].mark[20] == 1);
  }
}

TEST_CASE("le pas de retard de l'exception double", "[render][effect][pulsing]") {
  // L'exception qui se referme espace ses anneaux de vingt tics au lieu de dix.
  const std::array<EffectTrack, kEffectTracksPerNode> tracks =
      SeedEffectPulsingRingsTracks(Pattern(3, 20));
  CHECK(tracks[0].ticks == 0);
  CHECK(tracks[1].ticks == -20);
  CHECK(tracks[2].ticks == -40);
}

TEST_CASE("l'age global n'avance qu'une fois par tic", "[render][effect][pulsing]") {
  // ⚠ TROIS PISTES VIVENT, UNE SEULE COMPTE. Si chacune faisait avancer l'âge,
  // les anneaux s'useraient en un tiers du temps.
  const std::array<EffectTrack, kEffectTracksPerNode> tracks = Advanced(1, 12);
  CHECK(tracks[kEffectRadialBurstRegisterTrack].ticks == 12);
}

TEST_CASE("les anneaux retardes ne bougent pas avant leur tour", "[render][effect][pulsing]") {
  const std::array<EffectTrack, kEffectTracksPerNode> tracks = Advanced(1, 5);
  // Le premier a déjà grandi de cinq demi-unités ; les deux autres attendent.
  CHECK(tracks[0].radius_at_anchor == Approx(2.5F));
  CHECK(tracks[1].radius_at_anchor == 0.0F);
  CHECK(tracks[1].alpha == 0);
  CHECK(tracks[2].radius_at_anchor == 0.0F);
  // Leurs compteurs, eux, montent bien — c'est ainsi qu'ils rattrapent.
  CHECK(tracks[1].ticks == -5);
  CHECK(tracks[2].ticks == -15);
}

TEST_CASE("le mode ordinaire ouvre le rayon et monte l'alpha", "[render][effect][pulsing]") {
  // Un demi par tic pour le rayon, vingt-quatre par tic pour l'alpha.
  CHECK(Advanced(1, 1)[0].radius_at_anchor == Approx(0.5F));
  CHECK(Advanced(1, 1)[0].alpha == 24);

  // ⚠ LA MONTÉE S'ARRÊTE AU DIXIÈME TIC, si bien qu'elle ne compte que NEUF
  // pas : le dixième tic n'en est plus un. L'alpha culmine donc à 216, et le
  // plafond de 240 n'est jamais atteint par ce mode.
  CHECK(Advanced(1, 10)[0].alpha == 216);
  CHECK(Advanced(1, 14)[0].alpha == 216);

  // ⚠ ET LA CHUTE NE COMMENCE PAS AU DIXIÈME TIC MAIS AU SEUIL DU RAYON : elle
  // attend que celui-ci passe sept, donc le quinzième tic.
  CHECK(Advanced(1, 15)[0].radius_at_anchor == Approx(7.5F));
  CHECK(Advanced(1, 15)[0].alpha == 216 - 15);
}

TEST_CASE("le plafond du mode qui scintille n'est jamais atteint", "[render][effect][pulsing]") {
  // ⚠ QUATRE PAR TIC PENDANT NEUF PAS FONT TRENTE-SIX, et le plafond vaut
  // quarante : il existe dans le client et ne sert jamais. L'écrire comme un
  // palier attendu ferait passer un test pour la mesure.
  CHECK(Advanced(4, 10)[0].alpha == 36);
  CHECK(Advanced(4, 14)[0].alpha == 36);
}

TEST_CASE("le plafond du mode qui rayonne coupe la montee", "[render][effect][pulsing]") {
  // Vingt-quatre par tic, plafond cent vingt : il est atteint au cinquième pas
  // et ne bouge plus.
  CHECK(Advanced(5, 5)[0].alpha == 120);
  CHECK(Advanced(5, 9)[0].alpha == 120);
}

TEST_CASE("le mode qui se referme renait des le premier tic", "[render][effect][pulsing]") {
  // ⚠ IL EST SEMÉ À RAYON NUL ET SON RAYON DÉCROÎT : il passe donc sous le seuil
  // au premier tic, son alpha tombe, et il RENAÎT à quatorze. C'est la
  // renaissance qui le place, pas le semis — lui donner quatorze au semis
  // avancerait tout le cycle d'un tic.
  const std::array<EffectTrack, kEffectTracksPerNode> after_one = Advanced(3, 1);
  CHECK(after_one[0].ticks == 0);
  CHECK(after_one[0].radius_at_anchor == Approx(14.0F));
  CHECK(after_one[0].alpha == 12);

  // Puis il se referme d'un quart par tic.
  CHECK(Advanced(3, 5)[0].radius_at_anchor == Approx(14.0F - 4 * 0.25F));

  // Sa montée dure VINGT tics au lieu de dix, et atteint bien son plafond.
  CHECK(Advanced(3, 20)[0].alpha == 240);
  CHECK(Advanced(3, 21)[0].alpha == 240);
}

TEST_CASE("la piste renait au lieu de mourir", "[render][effect][pulsing]") {
  // L'alpha culmine à 216, tombe de quinze à partir du quinzième tic, et touche
  // zéro au vingt-neuvième : la piste y repart à rayon nul.
  const std::array<EffectTrack, kEffectTracksPerNode> renaissance = Advanced(1, 29);
  CHECK(renaissance[0].ticks == 0);
  CHECK(renaissance[0].radius_at_anchor == 0.0F);
  // ⚠ ET ELLE REMONTE SON ALPHA AU MÊME TIC : le client lit le compteur APRÈS la
  // renaissance. Le lire avant laisserait un tic noir à chaque cycle.
  CHECK(renaissance[0].alpha == 24);
  CHECK(renaissance[0].active);
  // L'âge global, lui, ne se remet pas à zéro : c'est lui qui compte les cycles.
  CHECK(renaissance[kEffectRadialBurstRegisterTrack].ticks == 29);
}

TEST_CASE("les anneaux finissent par mourir", "[render][effect][pulsing]") {
  // Passé cent vingt tics d'âge global, le mode ordinaire cesse de renaître.
  const std::array<EffectTrack, kEffectTracksPerNode> late = Advanced(1, 400);
  CHECK_FALSE(late[0].active);
  CHECK_FALSE(late[1].active);
  CHECK_FALSE(late[2].active);
}

TEST_CASE("le ruban s'ouvre en un quart de sinus", "[render][effect][pulsing]") {
  // ⚠ LES VINGT ET UNE CASES REÇOIVENT LA CLÉ DE LA PISTE, et rien d'autre : la
  // même valeur que la jupe compare pour décider de s'élever.
  const std::array<EffectTrack, kEffectTracksPerNode> au_dixieme = Advanced(1, 10);
  for (const float length : au_dixieme[0].value) {
    CHECK(length == Approx(kPulsingRingLiftKey));
  }

  // Au cinquième tic elle est modulée par le sinus de quarante-cinq degrés.
  const float expected = kPulsingRingLiftKey * std::sin(45.0F * std::numbers::pi_v<float> / 180.0F);
  CHECK(Advanced(1, 5)[0].value[0] == Approx(expected));
  CHECK(Advanced(1, 5)[0].value[20] == Approx(expected));

  // Et passé le dixième tic elle reste pleine.
  CHECK(Advanced(1, 13)[0].value[7] == Approx(kPulsingRingLiftKey));
}

TEST_CASE("la jupe des anneaux se dessine et s'eleve", "[render][effect][pulsing]") {
  const std::array<EffectTrack, kEffectTracksPerNode> tracks = Advanced(1, 8);
  const std::vector<EffectWorldQuad> skirt =
      ComposeEffectTrackSkirt(tracks[0], 16, glm::vec3{0.0F, 0.0F, 0.0F});
  REQUIRE_FALSE(skirt.empty());
  // ⚠ LA CLÉ VAUT 6,001 ET CE N'EST PAS UN ARRONDI : c'est l'une des deux
  // valeurs exactes qui soulèvent la jupe de deux unités. L'écrire six la
  // poserait au sol.
  CHECK(skirt.front().corners[0].y == Approx(2.0F));

  // Le registre, lui, ne donne aucun quad : il n'est pas actif.
  CHECK(ComposeEffectTrackSkirt(tracks[kEffectRadialBurstRegisterTrack], 16,
                                glm::vec3{0.0F, 0.0F, 0.0F})
            .empty());
}

TEST_CASE("le noeud seme rejoint la troupe", "[render][effect][pulsing]") {
  const EffectPulsingRingsPattern pattern = Pattern(1);
  EffectNodePool pool;
  pool.Add(SowEffectPulsingRingsNode(pattern, glm::vec3{1.0F, 0.0F, 2.0F},
                                     ro::render::kEffectNodeDefaultLifeTicks));
  REQUIRE(pool.size() == 1);
  CHECK(pool.nodes().front().shape == PooledNodeShape::kPulsingRings);
  CHECK(pool.nodes().front().trail_variant == 16);
  REQUIRE(pool.nodes().front().resource_paths.size() == 1);
  CHECK(pool.nodes().front().resource_paths.front() == "image.tga");

  for (int tick = 0; tick < 6; ++tick) {
    pool.Advance(16);
  }
  // La troupe fait bien battre le mouvement de cette forme.
  CHECK(pool.nodes().front().tracks[0].radius_at_anchor > 0.0F);
}

TEST_CASE("la table des anneaux tient ce qu'elle a mesure", "[render][effect][pulsing]") {
  const std::span<const EffectPulsingRingsPattern> table = EffectPulsingRingsPatternTable();
  REQUIRE_FALSE(table.empty());

  std::set<std::uint32_t> identifiants;
  for (const EffectPulsingRingsPattern& pattern : table) {
    identifiants.insert(pattern.effect_id);
    CHECK_FALSE(pattern.texture.empty());
    CHECK_FALSE(pattern.client_function.empty());
    // Les cinq modes mesurés, et aucun autre.
    CHECK((pattern.mode == 0 || pattern.mode == 1 || pattern.mode == 3 || pattern.mode == 4 ||
           pattern.mode == 5));
    CHECK((pattern.delay_step_ticks == 10 || pattern.delay_step_ticks == 20));
    CHECK((pattern.track_style == 100 || pattern.track_style == 3111));
  }
  // ⚠ UN IDENTIFIANT, UN NŒUD. Le 1569 appelle son bâtisseur depuis les deux
  // branches d'un test, avec le même argument : c'est un nœud, pas deux, et les
  // compter deux fois poserait six anneaux là où le client en pose trois.
  CHECK(identifiants.size() == table.size());

  // La table est triée : la recherche par identifiant en dépend.
  CHECK(std::ranges::is_sorted(table, {}, &EffectPulsingRingsPattern::effect_id));

  // LE PORTAIL ROUGE est le seul à se refermer, et le seul au style 3111.
  const std::span<const EffectPulsingRingsPattern> portal = EffectPulsingRingsPatternsFor(341);
  REQUIRE(portal.size() == 1);
  CHECK(portal.front().mode == 3);
  CHECK(portal.front().track_style == 3111);
  CHECK(portal.front().delay_step_ticks == 20);
  CHECK(portal.front().texture == "data\\texture\\effect\\ring_red.tga");

  // ⚠ ET SON JUMEAU VIOLET NE PARTAGE NI SA TEINTE NI SON MODE, bien que le même
  // bâtisseur les pose : c'est l'argument du site qui les sépare (ADR 0019).
  const std::span<const EffectPulsingRingsPattern> violet = EffectPulsingRingsPatternsFor(317);
  REQUIRE(violet.size() == 1);
  CHECK(violet.front().mode == 0);
  CHECK(violet.front().texture == "data\\texture\\effect\\ring_blue.tga");

  // Un identifiant qui n'est pas de cette famille ne rend rien.
  CHECK(EffectPulsingRingsPatternsFor(1).empty());
}
