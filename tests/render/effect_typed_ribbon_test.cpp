#include "ro/render/effect_typed_ribbon.h"

#include <algorithm>
#include <cmath>
#include <set>

#include <catch2/catch_approx.hpp>
#include <catch2/catch_test_macros.hpp>

#include "ro/render/effect_node_pool.h"
#include "ro/render/effect_typed_ribbon_seed.h"

using Catch::Approx;
using ro::render::ComposeTypedRibbonQuads;
using ro::render::EffectNodePool;
using ro::render::EffectTrack;
using ro::render::EffectTypedRibbonImagePaths;
using ro::render::EffectTypedRibbonPassFor;
using ro::render::EffectTypedRibbonPattern;
using ro::render::EffectTypedRibbonPatternsFor;
using ro::render::EffectTypedRibbonPatternTable;
using ro::render::EffectWorldQuad;
using ro::render::kEffectTrackSlots;
using ro::render::kEffectTracksPerNode;
using ro::render::kTypedRibbonLastSegment;
using ro::render::kTypedRibbonShortestLifeTicks;
using ro::render::PooledNodeShape;
using ro::render::SeedEffectTypedRibbonTracks;
using ro::render::SowEffectTypedRibbonNode;

namespace {

constexpr glm::vec3 kAtTheOrigin{0.0F, 0.0F, 0.0F};
// Deux quads par segment, et dix segments.
constexpr std::size_t kQuadsPerTrack = 2 * static_cast<std::size_t>(kTypedRibbonLastSegment);
constexpr std::size_t kBubbleImages = 27;

[[nodiscard]] float QuadArea(const EffectWorldQuad& quad) {
  const glm::vec3 ab = quad.corners[1] - quad.corners[0];
  const glm::vec3 ac = quad.corners[2] - quad.corners[0];
  const glm::vec3 ad = quad.corners[3] - quad.corners[0];
  return (glm::length(glm::cross(ab, ac)) + glm::length(glm::cross(ac, ad))) * 0.5F;
}

// Les quatre pistes d'un patron, déjà avancées de `tics` par le mouvement — le
// semis les donne nues, et la figure ne rend alors rien.
[[nodiscard]] std::array<EffectTrack, kEffectTracksPerNode> AdvancedTracks(std::uint8_t type,
                                                                           std::int32_t ticks) {
  const EffectTypedRibbonPattern pattern{0, "essai", "image%02d.tga", kBubbleImages, type};
  std::array<EffectTrack, kEffectTracksPerNode> tracks = SeedEffectTypedRibbonTracks(pattern);
  for (std::int32_t tick = 0; tick < ticks; ++tick) {
    ro::render::AdvanceSpinningRibbonTracks(tracks, kTypedRibbonShortestLifeTicks);
  }
  return tracks;
}

}  // namespace

TEST_CASE("le ruban typé naît nu et ne dessine rien", "[render][typed_ribbon]") {
  // ⚠ LA PROPRIÉTÉ QUI COMPTE : le semis donne un alpha nul ET des cases à zéro.
  // C'est le mouvement qui met l'onde et la lumière.
  const EffectTypedRibbonPattern& pattern = EffectTypedRibbonPatternsFor(831).front();
  const std::array<EffectTrack, kEffectTracksPerNode> neuves = SeedEffectTypedRibbonTracks(pattern);
  for (const EffectTrack& track : neuves) {
    CHECK(track.active);
    CHECK(track.alpha == 0);
    CHECK(track.degrees == 360);
    for (std::size_t case_ = 0; case_ < kEffectTrackSlots; ++case_) {
      CHECK(track.value[case_] == Approx(0.0F));
    }
    CHECK(ComposeTypedRibbonQuads(track, kAtTheOrigin, kBubbleImages).empty());
  }
}

TEST_CASE("la jupe monte au-dessus de son ancre", "[render][typed_ribbon]") {
  // Le client écrit son bord à une ordonnée NÉGATIVE, dans son repère qui
  // descend. Recopiée, elle enterrait la jupe.
  const std::array<EffectTrack, kEffectTracksPerNode> tracks = AdvancedTracks(0, 20);
  const std::vector<EffectWorldQuad> quads =
      ComposeTypedRibbonQuads(tracks.front(), kAtTheOrigin, kBubbleImages);
  REQUIRE_FALSE(quads.empty());
  float highest = 0.0F;
  for (const EffectWorldQuad& quad : quads) {
    for (const glm::vec3& corner : quad.corners) {
      CHECK(corner.y >= -0.001F);
      highest = std::max(highest, corner.y);
    }
  }
  CHECK(highest > 1.0F);
}

TEST_CASE("la jupe retourne sa profondeur avec son ordonnee", "[render][typed_ribbon]") {
  // ⚠ LE CHANGEMENT DE REPÈRE EST UN DEMI-TOUR AUTOUR DE X : à l'azimut de
  // quatre-vingt-dix degrés, où le client pose une profondeur positive, la
  // nôtre est négative. La garder ferait tourner l'anneau à l'envers.
  std::array<EffectTrack, kEffectTracksPerNode> tracks = AdvancedTracks(0, 20);
  tracks.front().azimuth_degrees = 90;
  const std::vector<EffectWorldQuad> quads =
      ComposeTypedRibbonQuads(tracks.front(), kAtTheOrigin, kBubbleImages);
  REQUIRE_FALSE(quads.empty());
  const glm::vec3 first_inner = quads.front().corners[0];
  CHECK(first_inner.z < 0.0F);
  CHECK(first_inner.x == Approx(0.0F).margin(0.01F));
}

TEST_CASE("chaque segment est peint DEUX fois", "[render][typed_ribbon]") {
  const std::array<EffectTrack, kEffectTracksPerNode> tracks = AdvancedTracks(0, 20);
  const std::vector<EffectWorldQuad> quads =
      ComposeTypedRibbonQuads(tracks.front(), kAtTheOrigin, kBubbleImages);
  REQUIRE(quads.size() == kQuadsPerTrack);

  // Les deux passes se suivent, et la seconde est PLUS VIVE : son alpha est
  // multiplié. Une seule passe donnerait un ruban deux fois plus pâle.
  for (std::size_t rang = 0; rang + 1 < quads.size(); rang += 2) {
    INFO("segment " << rang / 2);
    CHECK(quads[rang].corners[0] == quads[rang + 1].corners[0]);
    CHECK(quads[rang].corners[2] == quads[rang + 1].corners[2]);
    CHECK(quads[rang + 1].tint[3] > quads[rang].tint[3]);
    CHECK(QuadArea(quads[rang]) > 0.0F);
  }
}

TEST_CASE("les quatre rubans d'un nœud ne se superposent pas", "[render][typed_ribbon]") {
  // ⚠ LE RAYON DE BASE PORTE LE SINUS DE L'AZIMUT, et c'est ce qui les étage.
  // Les azimuts sont à quatre-vingt-dix degrés d'écart, donc leurs sinus valent
  // zéro, moins un, zéro et un : trois rayons distincts pour quatre pistes.
  const std::array<EffectTrack, kEffectTracksPerNode> tracks = AdvancedTracks(0, 20);
  std::set<std::int16_t> azimuts;
  std::set<std::int16_t> inclinaisons;
  for (const EffectTrack& track : tracks) {
    azimuts.insert(track.azimuth_degrees);
    inclinaisons.insert(track.tilt_degrees);
  }
  CHECK(azimuts.size() == kEffectTracksPerNode);
  CHECK(inclinaisons.size() == kEffectTracksPerNode);

  // Et leurs ondes ne battent pas en phase : au même tic, les quatre pistes
  // n'ont pas la même première case.
  std::set<float> premieres;
  for (const EffectTrack& track : tracks) {
    premieres.insert(track.value[1]);
  }
  CHECK(premieres.size() > 1);
}

TEST_CASE("le type décide de la seconde passe", "[render][typed_ribbon]") {
  SECTION("le type zéro triple son alpha") {
    CHECK(EffectTypedRibbonPassFor(0).alpha_factor == 3);
    CHECK(EffectTypedRibbonPassFor(0).trail_variant == 17);
    CHECK_FALSE(EffectTypedRibbonPassFor(0).slows_the_image);
  }
  SECTION("le type dix, et lui seul, ralentit son image") {
    CHECK(EffectTypedRibbonPassFor(10).slows_the_image);
    for (std::uint8_t type = 0; type <= 11; ++type) {
      if (type != 10) {
        INFO("type " << static_cast<int>(type));
        CHECK_FALSE(EffectTypedRibbonPassFor(type).slows_the_image);
      }
    }
  }
  SECTION("deux types consultent un réglage, et eux seuls") {
    std::set<int> consultants;
    for (std::uint8_t type = 0; type <= 11; ++type) {
      if (EffectTypedRibbonPassFor(type).doubles_when_setting_is_off) {
        consultants.insert(type);
      }
    }
    CHECK(consultants == std::set<int>{9, 11});
  }
  SECTION("un type inconnu retombe sur le type zéro") {
    CHECK(EffectTypedRibbonPassFor(200).trail_variant == EffectTypedRibbonPassFor(0).trail_variant);
  }
}

TEST_CASE("le réglage change l'alpha des deux types qui le consultent", "[render][typed_ribbon]") {
  const std::array<EffectTrack, kEffectTracksPerNode> tracks = AdvancedTracks(9, 20);
  const std::vector<EffectWorldQuad> allume =
      ComposeTypedRibbonQuads(tracks.front(), kAtTheOrigin, kBubbleImages, true);
  const std::vector<EffectWorldQuad> eteint =
      ComposeTypedRibbonQuads(tracks.front(), kAtTheOrigin, kBubbleImages, false);
  REQUIRE(allume.size() == eteint.size());
  REQUIRE(allume.size() >= 2);
  // ⚠ RÉGLAGE ÉTEINT, L'ALPHA DOUBLE. Figer une réponse rendrait ce type deux
  // fois trop vif ou deux fois trop pâle, et rien dans l'image ne le dirait.
  CHECK(eteint[1].tint[3] > allume[1].tint[3]);
  // Et la première passe ne bouge pas : le réglage ne porte que sur la seconde.
  CHECK(eteint[0].tint[3] == allume[0].tint[3]);
}

TEST_CASE("l'image tourne avec les tics", "[render][typed_ribbon]") {
  std::set<std::size_t> seen;
  for (std::int32_t tick = 1; tick <= 40; ++tick) {
    const std::array<EffectTrack, kEffectTracksPerNode> tracks = AdvancedTracks(0, tick);
    const std::vector<EffectWorldQuad> quads =
        ComposeTypedRibbonQuads(tracks.front(), kAtTheOrigin, kBubbleImages);
    REQUIRE_FALSE(quads.empty());
    seen.insert(quads.front().texture);
    for (const EffectWorldQuad& quad : quads) {
      CHECK(quad.texture < kBubbleImages);
    }
  }
  CHECK(seen.size() > 1);

  // ⚠ ET LE TYPE DIX LA RALENTIT D'UN TIERS : au même tic, son image est en
  // retard sur celle d'un type ordinaire. Il ne dessine que sa seconde passe.
  const std::array<EffectTrack, kEffectTracksPerNode> lentes = AdvancedTracks(10, 12);
  const std::array<EffectTrack, kEffectTracksPerNode> ordinaires = AdvancedTracks(0, 12);
  const std::vector<EffectWorldQuad> quads =
      ComposeTypedRibbonQuads(lentes.front(), kAtTheOrigin, kBubbleImages);
  const std::vector<EffectWorldQuad> reference =
      ComposeTypedRibbonQuads(ordinaires.front(), kAtTheOrigin, kBubbleImages);
  REQUIRE(quads.size() == static_cast<std::size_t>(kTypedRibbonLastSegment));
  REQUIRE_FALSE(reference.empty());
  CHECK(quads.front().texture < reference.front().texture);
}

TEST_CASE("un nœud sans image ne divise pas par zéro", "[render][typed_ribbon]") {
  const std::array<EffectTrack, kEffectTracksPerNode> tracks = AdvancedTracks(0, 20);
  const std::vector<EffectWorldQuad> quads =
      ComposeTypedRibbonQuads(tracks.front(), kAtTheOrigin, 0);
  REQUIRE_FALSE(quads.empty());
  for (const EffectWorldQuad& quad : quads) {
    CHECK(quad.texture == 0);
  }
}

TEST_CASE("l'onde monte jusqu'à quatre-vingt-sept, puis s'éteint", "[render][typed_ribbon]") {
  const EffectTypedRibbonPattern& pattern = EffectTypedRibbonPatternsFor(831).front();
  ro::render::PooledEffectNode node =
      SowEffectTypedRibbonNode(pattern, glm::vec3{5.0F, 0.0F, 7.0F}, 10);
  CHECK(node.shape == PooledNodeShape::kTypedRibbon);
  // ⚠ LA VIE NE DESCEND PAS SOUS CENT VINGT : le bâtisseur la relève.
  CHECK(node.node.life_ticks == kTypedRibbonShortestLifeTicks);
  CHECK(node.resource_paths.size() == kBubbleImages);

  EffectNodePool pool;
  pool.Add(node);
  for (int tick = 0; tick < 40; ++tick) {
    pool.Advance(16);
  }
  REQUIRE(pool.size() == 1);
  // ⚠ QUATRE-VINGT-SEPT, ET LE PLAFOND DE CENT N'EST JAMAIS ATTEINT. La montée
  // s'arrête au trentième tic et vaut trois par tic depuis zéro : vingt-neuf
  // montées, donc quatre-vingt-sept. Le client porte bien un écrêtage à cent,
  // mais aucun des dix rubans ne l'atteint — leur bâtisseur les sème à zéro.
  //
  // Ce cas de test attendait cent, et c'est LUI qui avait tort. Un plafond
  // présent dans le code n'est pas un plafond atteint, et confondre les deux
  // aurait fait chercher une faute dans le mouvement.
  CHECK(pool.nodes().front().tracks[0].alpha == 87);

  // Puis l'extinction, qui commence quarante-six tics avant la fin.
  for (int tick = 0; tick < 90; ++tick) {
    pool.Advance(16);
  }
  if (pool.size() == 1) {
    CHECK(pool.nodes().front().tracks[0].alpha < 100);
  }
}

TEST_CASE("les dix rubans typés du client sont en table", "[render][typed_ribbon]") {
  const std::span<const EffectTypedRibbonPattern> table = EffectTypedRibbonPatternTable();
  CHECK(table.size() == 10);
  CHECK(std::ranges::is_sorted(table, {}, &EffectTypedRibbonPattern::effect_id));

  std::set<std::uint8_t> types;
  for (const EffectTypedRibbonPattern& pattern : table) {
    types.insert(pattern.type);
    INFO("effet " << pattern.effect_id);
    CHECK(pattern.image_count > 0);
    // ⚠ LE JEU D'IMAGES SUIT LE TYPE : seul le type dix prend le feu, et il n'en
    // a que quatre. Les confondre en chargerait vingt-sept qui n'existent pas.
    CHECK(pattern.image_count == (pattern.type == 10 ? 4 : kBubbleImages));
    const std::vector<std::string> paths = EffectTypedRibbonImagePaths(pattern);
    CHECK(paths.size() == pattern.image_count);
    // Le modèle est bien NUMÉROTÉ, et les chemins sont tous distincts.
    CHECK(std::set<std::string>(paths.begin(), paths.end()).size() == paths.size());
    CHECK(paths.front().find('%') == std::string::npos);
    // ⚠ LA NUMÉROTATION COMMENCE À UN et finit au compte d'images.
    CHECK(paths.front().find("01.") != std::string::npos);
    CHECK(paths.back().find(pattern.type == 10 ? "04." : "27.") != std::string::npos);
    CHECK(EffectTypedRibbonPatternsFor(pattern.effect_id).size() == 1);
  }
  CHECK(types.size() == table.size());
  CHECK(EffectTypedRibbonPatternsFor(1).empty());
}

TEST_CASE("toute piste avancée d'un ruban relevé a de l'aire", "[render][typed_ribbon]") {
  for (const EffectTypedRibbonPattern& pattern : EffectTypedRibbonPatternTable()) {
    for (std::int32_t tick = 12; tick < 70; tick += 7) {
      const std::array<EffectTrack, kEffectTracksPerNode> tracks =
          AdvancedTracks(pattern.type, tick);
      for (const EffectTrack& track : tracks) {
        const std::vector<EffectWorldQuad> quads =
            ComposeTypedRibbonQuads(track, kAtTheOrigin, pattern.image_count);
        INFO("effet " << pattern.effect_id << " au tic " << tick);
        // Une passe pour le type qui ralentit son image, deux pour les autres.
        REQUIRE(quads.size() == (EffectTypedRibbonPassFor(pattern.type).slows_the_image
                                     ? kQuadsPerTrack / 2
                                     : kQuadsPerTrack));
        float area = 0.0F;
        for (const EffectWorldQuad& quad : quads) {
          area += QuadArea(quad);
        }
        CHECK(area > 0.0F);
      }
    }
  }
}
