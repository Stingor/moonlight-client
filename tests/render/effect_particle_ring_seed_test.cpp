#include "ro/render/effect_particle_ring_seed.h"

#include <set>
#include <string_view>

#include <catch2/catch_approx.hpp>
#include <catch2/catch_test_macros.hpp>

#include "ro/render/effect_node_pool.h"

using ro::render::EffectNodePool;
using ro::render::EffectParticleRingNodePattern;
using ro::render::EffectParticleRingNodePatternsFor;
using ro::render::EffectParticleRingNodePatternTable;
using ro::render::EffectParticleRingStyleSeed;
using ro::render::EffectParticleRingStyleSeedAt;
using ro::render::EffectParticleRingStyleSeedFor;
using ro::render::EffectParticleRingStyleSeedTable;
using ro::render::kEffectTracksPerNode;
using ro::render::SeedEffectParticleRingTracks;
using ro::render::SowEffectParticleRingNode;

namespace {

// Le seul style qui se resserre, et le seul qui change de teinte.
constexpr std::uint8_t kStyleThatShrinks = 2;
constexpr std::uint8_t kStyleThatTints = 3;
constexpr std::uint32_t kFirstEffect = 223;

}  // namespace

// ⚠ CE SOUS-TYPE SE MONTRE, DEPUIS QUE SON MOUVEMENT EST TRANSCRIT. Le
// commentaire qui tenait ici disait le contraire, et un test plus bas le
// démentait déjà : « les noeuds REVIENNENT, le mouvement etant porte ». Un
// commentaire dit la RÈGLE en vigueur, pas l'histoire du chantier.

// LES DEUX BÂTISSEURS DE CE SOUS-TYPE. Le numéro de style ne veut pas dire la
// même chose chez l'un et chez l'autre : ce sont eux qui le désambiguïsent.
constexpr std::string_view kSizedBuilder = "Effect_Init_ParticleRing4_Sized";
constexpr std::string_view kPlainBuilder = "Effect_Init_ParticleRing4";

TEST_CASE("Croix d'anneaux : les quatre pistes se déduisent de LEUR RANG", "[render][pool]") {
  // ⚠ C'EST TOUTE LA FIGURE. Le client écrit l'azimut de la piste i comme
  // quatre-vingt-dix fois i, et son rayon comme un plus i : quatre couronnes aux
  // quatre points cardinaux, de plus en plus larges.
  REQUIRE(EffectParticleRingStyleSeedAt(0) != nullptr);
  const std::array<ro::render::EffectTrack, kEffectTracksPerNode> tracks =
      SeedEffectParticleRingTracks(*EffectParticleRingStyleSeedAt(0));
  std::set<std::int16_t> azimuts;
  std::set<float> radii;
  for (std::size_t rang = 0; rang < tracks.size(); ++rang) {
    INFO("piste " << rang);
    CHECK(tracks[rang].active);
    CHECK(tracks[rang].azimuth_degrees == static_cast<std::int16_t>(90 * rang));
    CHECK(tracks[rang].radius_at_anchor == Catch::Approx(1.0F + static_cast<float>(rang)));
    CHECK(tracks[rang].alpha == 0);
    azimuts.insert(tracks[rang].azimuth_degrees);
    radii.insert(tracks[rang].radius_at_anchor);
  }
  CHECK(azimuts.size() == kEffectTracksPerNode);
  CHECK(radii.size() == kEffectTracksPerNode);
}

TEST_CASE("Croix d'anneaux : un seul style SE RESSERRE", "[render][table]") {
  // ⚠ C'EST LA « TAILLE RÉDUITE » QUE LE CLIENT NOMME LUI-MÊME : soixante-dix
  // degrés au lieu de quatre-vingts, et une clé de quinze au lieu de vingt-cinq.
  const EffectParticleRingStyleSeed* serre =
      EffectParticleRingStyleSeedFor(kSizedBuilder, kStyleThatShrinks);
  REQUIRE(serre != nullptr);
  CHECK(serre->tilt_degrees == 70);
  CHECK(serre->lift_key == Catch::Approx(15.0F));
  for (const EffectParticleRingStyleSeed& seed : EffectParticleRingStyleSeedTable()) {
    if (seed.style == kStyleThatShrinks && seed.client_function == kSizedBuilder) {
      continue;
    }
    INFO("style " << static_cast<int>(seed.style));
    CHECK(seed.tilt_degrees == 80);
    CHECK(seed.lift_key == Catch::Approx(25.0F));
  }
}

TEST_CASE("Croix d'anneaux : un seul style change de TEINTE", "[render][table]") {
  for (const EffectParticleRingStyleSeed& seed : EffectParticleRingStyleSeedTable()) {
    INFO("style " << static_cast<int>(seed.style));
    const bool tint = seed.style == kStyleThatTints && seed.client_function == kSizedBuilder;
    CHECK(seed.trail_variant == (tint ? 17 : 0));
  }
}

TEST_CASE("Croix d anneaux : les noeuds REVIENNENT, le mouvement etant porte", "[render][table]") {
  // Ce test a remplace un garde-silence, et c est ainsi que le silence devait
  // finir : la table est restee vide tant que AdvanceDualModeSprayTracks n
  // existait pas.
  const std::span<const EffectParticleRingNodePattern> table = EffectParticleRingNodePatternTable();
  REQUIRE(!table.empty());
  for (std::size_t rang = 1; rang < table.size(); ++rang) {
    CHECK(table[rang - 1].effect_id <= table[rang].effect_id);
  }
  for (const EffectParticleRingNodePattern& pattern : table) {
    INFO("effet " << pattern.effect_id);
    // ⚠ LA GRAINE SE TROUVE PAR SON RANG, et le rang doit désigner la graine du
    // BON bâtisseur : c'est la seule chose que le numéro de style ne dit plus.
    const EffectParticleRingStyleSeed* seed = EffectParticleRingStyleSeedAt(pattern.style_index);
    REQUIRE(seed != nullptr);
    CHECK(seed->client_function == pattern.client_function);
    CHECK(seed->style == pattern.style);
    CHECK(pattern.texture.starts_with("data"));
  }
}

TEST_CASE("Croix d anneaux : un STYLE ne designe rien sans son batisseur", "[render][table]") {
  // ⚠ C'EST LA REGLE QUE LE SECOND BATISSEUR A RENDUE NECESSAIRE. Les deux
  // fonctions emploient le style ZERO, et il ne dit pas la meme chose : chercher
  // une graine par son seul numero rendait celle du voisin une fois sur deux.
  // Un patron designe donc la sienne par son RANG.
  const EffectParticleRingStyleSeed* dimensionne = EffectParticleRingStyleSeedFor(kSizedBuilder, 0);
  const EffectParticleRingStyleSeed* nu = EffectParticleRingStyleSeedFor(kPlainBuilder, 0);
  REQUIRE(dimensionne != nullptr);
  REQUIRE(nu != nullptr);
  CHECK(dimensionne != nu);
  CHECK(dimensionne->style == nu->style);

  // ⚠ ET LA REGLE N'EST PAS UN COMPTE. Ce test a d'abord exige DEUX fonctions,
  // et il est tombe des qu'un troisieme batisseur du meme sous-type est entre —
  // alors que rien de ce qu'il garde n'avait change. Ce qui se prouve est que
  // PLUSIEURS fonctions cohabitent et que chacune retrouve SA graine ; leur
  // nombre, lui, est un etat du portage et non une propriete.
  std::set<std::string_view> fonctions;
  for (const EffectParticleRingNodePattern& pattern : EffectParticleRingNodePatternTable()) {
    fonctions.insert(pattern.client_function);
    INFO("effet " << pattern.effect_id);
    const EffectParticleRingStyleSeed* own =
        EffectParticleRingStyleSeedFor(pattern.client_function, pattern.style);
    REQUIRE(own != nullptr);
    CHECK(own == EffectParticleRingStyleSeedAt(pattern.style_index));
  }
  CHECK(fonctions.size() >= 2);
}

TEST_CASE("Croix d anneaux : le noeud prend la forme de la GERBE", "[render][pool]") {
  // Meme figure que les deux autres anneaux, mouvement different. Lui donner la
  // forme des anneaux a pistes le ferait onduler comme un sous-type 30.
  const std::span<const EffectParticleRingNodePattern> patterns =
      EffectParticleRingNodePatternsFor(kFirstEffect);
  REQUIRE(patterns.size() == 1);
  const ro::render::PooledEffectNode node =
      SowEffectParticleRingNode(patterns.front(), {3.0F, 0.0F, 4.0F});
  CHECK(node.shape == ro::render::PooledNodeShape::kDualModeSprayRings);
  CHECK(node.trail_variant ==
        EffectParticleRingStyleSeedAt(patterns.front().style_index)->trail_variant);
}

TEST_CASE("Croix d anneaux : la troupe lui donne son aire", "[render][pool]") {
  const std::span<const EffectParticleRingNodePattern> patterns =
      EffectParticleRingNodePatternsFor(kFirstEffect);
  REQUIRE_FALSE(patterns.empty());
  ro::render::EffectNodePool pool;
  pool.Add(SowEffectParticleRingNode(patterns.front(), {}));
  float plus_grande = 0.0F;
  for (std::int32_t tick = 0; tick < 60; ++tick) {
    pool.Advance(1000 / ro::render::kEffectTicksPerSecond);
    if (pool.nodes().empty()) {
      break;
    }
    for (const ro::render::EffectTrack& track : pool.nodes().front().tracks) {
      plus_grande = std::max(plus_grande, track.value[10]);
    }
  }
  CHECK(plus_grande > 0.0F);
}

TEST_CASE("deux batisseurs sur trois posent le mode lent de la gerbe", "[render][particle_ring]") {
  // Le mouvement compare ce champ a quatre-vingt-dix-neuf : egal, l'anneau
  // tourne, s'ouvre et se couche plus lentement.
  for (const EffectParticleRingStyleSeed& seed : EffectParticleRingStyleSeedTable()) {
    INFO(seed.client_function << " style " << static_cast<int>(seed.style));
    const bool lent = seed.client_function != "Effect_Init_ParticleRing4_Sized";
    CHECK(seed.spray_mode == Catch::Approx(lent ? 99.0F : 0.0F));
  }
  // Et le noeud le recoit de sa graine.
  for (const EffectParticleRingNodePattern& pattern : EffectParticleRingNodePatternTable()) {
    const EffectParticleRingStyleSeed* seed = EffectParticleRingStyleSeedAt(pattern.style_index);
    REQUIRE(seed != nullptr);
    CHECK(SowEffectParticleRingNode(pattern, {}).spray_mode == Catch::Approx(seed->spray_mode));
  }
}
