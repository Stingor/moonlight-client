#include <array>
#include <cstdint>
#include <span>
#include <string_view>

#include <catch2/catch_approx.hpp>
#include <catch2/catch_test_macros.hpp>

#include "ro/render/effect_node.h"
#include "ro/render/effect_node_pool.h"
#include "ro/render/effect_particle_figures.h"
#include "ro/render/effect_pooled_node_shape.h"
#include "ro/render/effect_track.h"
#include "ro/render/effect_track_motion.h"
#include "ro/render/effect_white_burst_seed.h"

using Catch::Approx;
using ro::render::AdvanceRisingParticleTrack;
using ro::render::EffectRisingParticleIsDrawn;
using ro::render::EffectTrack;
using ro::render::EffectWhiteBurstPattern;
using ro::render::EffectWhiteBurstPatternsFor;
using ro::render::EffectWhiteBurstPatternTable;
using ro::render::kEffectTracksPerNode;
using ro::render::NextEffectNodeDraw;
using ro::render::PooledNodeShape;
using ro::render::SeedEffectWhiteBurstTracks;
using ro::render::SowEffectWhiteBurstNode;

namespace {

// LES QUATRE IDENTIFIANTS que le bâtisseur sert, et rien d'autre.
constexpr std::array<std::uint32_t, 4> kServis = {227, 798, 807, 810};

// Le style deux est le seul dont le retard se tire par pas de DEUX.
constexpr std::uint32_t kEvenDelayEffectId = 798;

// Une graine quelconque, mais FIXE : deux semis de la même graine doivent
// rendre la même figure, et c'est ce qui rend ces tests reproductibles.
constexpr std::uint32_t kSeed = 20250716U;

constexpr glm::vec3 kAnchor = {12.5F, 0.0F, -7.25F};

[[nodiscard]] const EffectWhiteBurstPattern& PatternOf(std::uint32_t effect_id) {
  const std::span<const EffectWhiteBurstPattern> all = EffectWhiteBurstPatternsFor(effect_id);
  REQUIRE(all.size() == 1);
  return all.front();
}

// Le retard le plus long que ce patron puisse tirer, en valeur absolue.
[[nodiscard]] std::int32_t LongestDelay(const EffectWhiteBurstPattern& pattern) {
  return pattern.shortest_delay_ticks + pattern.delay_step_ticks * (pattern.delay_draw_bound - 1);
}

}  // namespace

TEST_CASE("la table des bouffées blanches sert quatre identifiants, un patron chacun") {
  const std::span<const EffectWhiteBurstPattern> table = EffectWhiteBurstPatternTable();
  REQUIRE(table.size() == kServis.size());

  std::uint32_t previous = 0;
  for (const EffectWhiteBurstPattern& pattern : table) {
    // La recherche par identifiant est une `lower_bound` : la table DOIT être
    // triée, et un tri perdu rendrait des spans vides sans rien casser d'autre.
    REQUIRE(pattern.effect_id > previous);
    previous = pattern.effect_id;
    REQUIRE(pattern.client_function == std::string_view{"Effect_Build_WhiteBurstParticles2b"});
    REQUIRE(pattern.texture.starts_with("data\\texture\\"));
    REQUIRE(pattern.half_diagonal > 0.0F);
    REQUIRE(pattern.life_ticks > 0);
  }
  for (const std::uint32_t each_effect_id : kServis) {
    REQUIRE(EffectWhiteBurstPatternsFor(each_effect_id).size() == 1);
  }
  // Un identifiant que le bâtisseur ne sert pas ne doit rien rendre.
  REQUIRE(EffectWhiteBurstPatternsFor(228).empty());
}

TEST_CASE("la naissance sème autant de bouffées que la boucle de son site") {
  // La boucle qui entoure le bâtisseur : une seule bouffée par effet faisait de
  // l'aura d'Intimidate quatre carrés.
  constexpr std::array<std::int32_t, 4> kAtBirth = {20, 16, 20, 10};
  for (std::size_t rank = 0; rank < kServis.size(); ++rank) {
    CHECK(EffectWhiteBurstPatternsFor(kServis[rank]).front().sowings_at_birth == kAtBirth[rank]);
  }
}

TEST_CASE("toute marque que la bouffée peut poser est une marque que la figure DESSINE") {
  // ⚠ C'EST LA PREUVE QUE CETTE FAMILLE N'A DEMANDÉ AUCUNE FIGURE. Le cas par
  // défaut du dessinateur du client n'émet RIEN : une marque hors de sa table
  // ne donnerait pas un carré blanc mais un carré absent, et l'identifiant
  // serait porté sans que rien ne se voie.
  for (const EffectWhiteBurstPattern& pattern : EffectWhiteBurstPatternTable()) {
    for (std::int32_t draw = 0; draw < pattern.mark_draw_bound; ++draw) {
      const auto mark = static_cast<std::uint8_t>(pattern.lowest_mark + draw);
      INFO("effet " << pattern.effect_id << ", marque " << static_cast<int>(mark));
      REQUIRE(EffectRisingParticleIsDrawn(mark));
    }
  }
}

TEST_CASE("la bouffée sème quatre pistes ancrées, éteintes et au même rayon") {
  for (const EffectWhiteBurstPattern& pattern : EffectWhiteBurstPatternTable()) {
    std::uint32_t generator = kSeed;
    const std::array<EffectTrack, kEffectTracksPerNode> tracks =
        SeedEffectWhiteBurstTracks(pattern, kAnchor, generator);

    for (const EffectTrack& track : tracks) {
      INFO("effet " << pattern.effect_id);
      REQUIRE(track.active);
      // L'alpha part de ZÉRO : c'est le mouvement qui l'allume, et une piste
      // semée allumée apparaîtrait d'un bloc au lieu de monter.
      REQUIRE(track.alpha == 0);
      REQUIRE(track.anchor.x == Approx(kAnchor.x));
      REQUIRE(track.anchor.y == Approx(kAnchor.y));
      REQUIRE(track.anchor.z == Approx(kAnchor.z));
      REQUIRE(track.radius_at_anchor == Approx(12.0F));
      // La croissance du rayon est UN ET DEMI, et c'est ce qui sépare cette
      // bouffée de la particule montante de l'aîné, qui croît d'un.
      REQUIRE(track.value[0] == Approx(1.5F));
      REQUIRE(track.value[1] == Approx(pattern.half_diagonal));

      // Les quatre angles tirés tiennent tous dans le tour.
      REQUIRE(track.tilt_degrees >= 0);
      REQUIRE(track.tilt_degrees < 360);
      REQUIRE(track.degrees >= 0);
      REQUIRE(track.degrees < 360);
      REQUIRE(track.azimuth_degrees >= 0);
      REQUIRE(track.azimuth_degrees < 360);
      REQUIRE(track.value[2] >= 0.0F);
      REQUIRE(track.value[2] < 360.0F);

      REQUIRE(track.mark[0] >= pattern.lowest_mark);
      REQUIRE(track.mark[0] < pattern.lowest_mark + pattern.mark_draw_bound);

      // Le retard est NÉGATIF ou nul, et dans la fenêtre du style.
      REQUIRE(track.ticks <= -pattern.shortest_delay_ticks);
      REQUIRE(track.ticks >= -LongestDelay(pattern));
    }
  }
}

TEST_CASE("la même graine rejoue la même bouffée, une autre en donne une différente") {
  const EffectWhiteBurstPattern& pattern = PatternOf(807);
  std::uint32_t un = kSeed;
  std::uint32_t two = kSeed;
  const std::array<EffectTrack, kEffectTracksPerNode> a =
      SeedEffectWhiteBurstTracks(pattern, kAnchor, un);
  const std::array<EffectTrack, kEffectTracksPerNode> b =
      SeedEffectWhiteBurstTracks(pattern, kAnchor, two);
  for (std::size_t rang = 0; rang < kEffectTracksPerNode; ++rang) {
    REQUIRE(a[rang].ticks == b[rang].ticks);
    REQUIRE(a[rang].tilt_degrees == b[rang].tilt_degrees);
  }

  // ⚠ ET LES QUATRE PISTES NE SE RESSEMBLENT PAS. Le générateur avance à chaque
  // tirage ; si les quatre sortaient identiques, on verrait une particule là où
  // le client en montre quatre.
  const bool all_same = a[0].tilt_degrees == a[1].tilt_degrees &&
                        a[1].tilt_degrees == a[2].tilt_degrees &&
                        a[2].tilt_degrees == a[3].tilt_degrees;
  REQUIRE_FALSE(all_same);
}

TEST_CASE("un style qui ne tire pas sa marque NE CONSOMME PAS le générateur") {
  // ⚠ C'EST LA MESURE LA PLUS FRAGILE DE CETTE FAMILLE. Le client écrit sa
  // constante sans consulter `rand()` aux styles deux et « autre » ; tirer
  // quand même, fût-ce pour jeter le résultat, décalerait d'un cran les quatre
  // tirages d'angle qui suivent — et ce sont eux qui donnent sa forme à la
  // bouffée. Rien dans l'image ne dirait que c'est faux.
  //
  // Un COMPTE de tirages, donc `std::size_t` : il se multiplie par
  // `kEffectTracksPerNode`, qui en est un aussi, et deux comptes qui se
  // comparent ne doivent pas différer de signe.
  constexpr std::size_t kDrawsWithoutMark = 5;

  const EffectWhiteBurstPattern& sans = PatternOf(227);
  REQUIRE(sans.mark_draw_bound == 1);
  std::uint32_t semeur = kSeed;
  static_cast<void>(SeedEffectWhiteBurstTracks(sans, kAnchor, semeur));

  std::uint32_t counter = kSeed;
  for (std::size_t remainder = 0; remainder < kDrawsWithoutMark * kEffectTracksPerNode;
       ++remainder) {
    static_cast<void>(NextEffectNodeDraw(counter));
  }
  REQUIRE(semeur == counter);

  // Et le style qui la tire, lui, consomme un tirage de plus par piste.
  const EffectWhiteBurstPattern& with = PatternOf(807);
  REQUIRE(with.mark_draw_bound > 1);
  std::uint32_t other_seeder = kSeed;
  static_cast<void>(SeedEffectWhiteBurstTracks(with, kAnchor, other_seeder));
  for (std::size_t remainder = 0; remainder < kEffectTracksPerNode; ++remainder) {
    static_cast<void>(NextEffectNodeDraw(counter));
  }
  REQUIRE(other_seeder == counter);
}

TEST_CASE("le retard du style deux est PAIR, et peut être nul") {
  const EffectWhiteBurstPattern& pattern = PatternOf(kEvenDelayEffectId);
  REQUIRE(pattern.delay_step_ticks == 2);
  REQUIRE(pattern.shortest_delay_ticks == 0);

  // ⚠ UNE FOURCHETTE CONTINUE AURAIT DONNÉ LES IMPAIRS AUSSI, et étalé la
  // bouffée deux fois plus finement que le client ne le fait. La propriété se
  // vérifie sur assez de graines pour qu'un pas de un se voie.
  bool seen_zero_delay = false;
  for (std::uint32_t seed = 1; seed <= 64U; ++seed) {
    std::uint32_t generator = seed;
    for (const EffectTrack& track : SeedEffectWhiteBurstTracks(pattern, kAnchor, generator)) {
      REQUIRE(track.ticks % 2 == 0);
      REQUIRE(track.ticks > -52);
      seen_zero_delay = seen_zero_delay || track.ticks == 0;
    }
  }
  REQUIRE(seen_zero_delay);

  // Les trois autres ont un PLANCHER : ils ne partent jamais au premier tic.
  for (const std::uint32_t each_effect_id : {227U, 807U, 810U}) {
    REQUIRE(PatternOf(each_effect_id).shortest_delay_ticks > 0);
  }
}

TEST_CASE("la vie d'une bouffée est celle de sa piste la plus lente, au tic près") {
  for (const EffectWhiteBurstPattern& pattern : EffectWhiteBurstPatternTable()) {
    // La piste la plus lente : celle qui a tiré le retard le plus long, et dont
    // la marque est la plus basse — c'est elle qui décide si l'alpha monte de
    // vingt-cinq ou de quinze.
    EffectTrack slow;
    slow.active = true;
    slow.ticks = -LongestDelay(pattern);
    slow.mark[0] = pattern.lowest_mark;
    slow.value[0] = 1.5F;

    EffectTrack dormant = slow;
    for (std::int32_t remainder = 0; remainder < pattern.life_ticks - 1; ++remainder) {
      AdvanceRisingParticleTrack(dormant);
    }
    INFO("effet " << pattern.effect_id << ", vie " << pattern.life_ticks);
    // ⚠ LE TIC D'AVANT NE SUFFIT PAS. Sans cette moitié-ci, une vie deux fois
    // trop longue passerait le test — et le nœud resterait dans la troupe à ne
    // plus rien montrer.
    REQUIRE(dormant.alpha > 0);

    AdvanceRisingParticleTrack(dormant);
    REQUIRE(dormant.alpha == 0);
  }
}

TEST_CASE("un nœud de bouffée prend la forme de la particule montante") {
  const EffectWhiteBurstPattern& pattern = PatternOf(810);
  std::uint32_t generator = kSeed;
  const ro::render::PooledEffectNode node = SowEffectWhiteBurstNode(pattern, kAnchor, generator);

  // ⚠ LA FORME EST CELLE DE L'AÎNÉ, et c'est ce qui rend cette famille
  // gratuite : même sous-type, donc même figure et même mouvement. Lui en
  // donner une propre aurait obligé à recopier les deux.
  REQUIRE(node.shape == PooledNodeShape::kRisingParticles);
  REQUIRE(node.node.life_ticks == pattern.life_ticks);
  REQUIRE(node.resource_paths.size() == 1);
  REQUIRE(node.resource_paths.front() == pattern.texture);
  REQUIRE(node.ground_position.x == Approx(kAnchor.x));

  // Le 810 est le seul à porter la SECONDE texture du bâtisseur.
  REQUIRE(pattern.texture.ends_with("white03.bmp"));
  for (const std::uint32_t each_effect_id : {227U, 798U, 807U}) {
    REQUIRE(PatternOf(each_effect_id).texture.ends_with("white01.bmp"));
  }
}
