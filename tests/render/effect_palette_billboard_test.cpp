#include "ro/render/effect_palette_billboard.h"

#include <algorithm>
#include <cmath>
#include <set>

#include <catch2/catch_approx.hpp>
#include <catch2/catch_test_macros.hpp>

#include "ro/render/effect_node_pool.h"
#include "ro/render/effect_palette_billboard_seed.h"

using Catch::Approx;
using ro::render::ComposePaletteBillboardQuad;
using ro::render::EffectNodePool;
using ro::render::EffectNodeScheduleLastSowTick;
using ro::render::EffectNodeScheduleSowsAt;
using ro::render::EffectPaletteBillboardColor;
using ro::render::EffectPaletteBillboardPattern;
using ro::render::EffectPaletteBillboardPatternsFor;
using ro::render::EffectPaletteBillboardPatternTable;
using ro::render::EffectTrack;
using ro::render::EffectWorldQuad;
using ro::render::kEffectTracksPerNode;
using ro::render::kPaletteBillboardSchedule;
using ro::render::kPaletteBillboardShades;
using ro::render::kPaletteBillboardShadeValues;
using ro::render::kSlotOfBillboardLift;
using ro::render::kSlotOfBillboardPalette;
using ro::render::kSlotOfBillboardShade;
using ro::render::kSlotOfBillboardSize;
using ro::render::PooledNodeShape;
using ro::render::SeedEffectPaletteBillboardTracks;
using ro::render::SowEffectPaletteBillboardNode;

namespace {

constexpr glm::vec3 kAtTheOrigin{0.0F, 0.0F, 0.0F};

[[nodiscard]] float QuadArea(const EffectWorldQuad& quad) {
  const glm::vec3 ab = quad.corners[1] - quad.corners[0];
  const glm::vec3 ac = quad.corners[2] - quad.corners[0];
  const glm::vec3 ad = quad.corners[3] - quad.corners[0];
  return (glm::length(glm::cross(ab, ac)) + glm::length(glm::cross(ac, ad))) * 0.5F;
}

// Les quatre pistes d'un patron, avancées par le mouvement — le semis les donne
// sans taille, et la figure ne rend alors rien.
[[nodiscard]] std::array<EffectTrack, kEffectTracksPerNode> Advanced(std::uint8_t palette,
                                                                     std::int32_t ticks) {
  const EffectPaletteBillboardPattern pattern{0, "essai", "image.tga", palette};
  std::uint32_t seed = 12345U;
  std::array<EffectTrack, kEffectTracksPerNode> tracks =
      SeedEffectPaletteBillboardTracks(pattern, seed);
  for (std::int32_t tick = 0; tick < ticks; ++tick) {
    ro::render::AdvancePaletteBillboardTracks(tracks, seed);
  }
  return tracks;
}

}  // namespace

TEST_CASE("l'éclat ne paraît pas tant que sa taille n'est pas calculée",
          "[render][palette_billboard]") {
  // ⚠ LA PROPRIÉTÉ QUI COMPTE : le bâtisseur laisse la taille à zéro, et c'est le
  // mouvement qui l'écrit. Aucun quad au premier tic, donc.
  const EffectPaletteBillboardPattern& pattern = EffectPaletteBillboardPatternsFor(861).front();
  std::uint32_t seed = 1U;
  const std::array<EffectTrack, kEffectTracksPerNode> neuves =
      SeedEffectPaletteBillboardTracks(pattern, seed);
  for (const EffectTrack& track : neuves) {
    CHECK(track.active);
    CHECK(track.value[kSlotOfBillboardSize] == Approx(0.0F));
    CHECK_FALSE(ComposePaletteBillboardQuad(track, kAtTheOrigin).has_value());
  }
}

TEST_CASE("trois pistes sur quatre attendent leur tour", "[render][palette_billboard]") {
  // ⚠ LES COMPTEURS PARTENT NÉGATIFS, et c'est cet échelonnement qui fait la
  // spirale. Les semer à zéro ferait partir les quatre éclats ensemble.
  const EffectPaletteBillboardPattern& pattern = EffectPaletteBillboardPatternsFor(861).front();
  std::uint32_t seed = 1U;
  const std::array<EffectTrack, kEffectTracksPerNode> neuves =
      SeedEffectPaletteBillboardTracks(pattern, seed);
  std::set<std::int32_t> departs;
  for (const EffectTrack& track : neuves) {
    departs.insert(track.ticks);
  }
  CHECK(departs == std::set<std::int32_t>{0, -20, -40, -60});

  // Au dixième tic, la première seule a paru.
  const std::array<EffectTrack, kEffectTracksPerNode> tot = Advanced(pattern.palette, 10);
  std::size_t visibles = 0;
  for (const EffectTrack& track : tot) {
    if (ComposePaletteBillboardQuad(track, kAtTheOrigin).has_value()) {
      ++visibles;
    }
  }
  CHECK(visibles == 1);

  // Au centième, les quatre.
  const std::array<EffectTrack, kEffectTracksPerNode> late = Advanced(pattern.palette, 100);
  visibles = 0;
  for (const EffectTrack& track : late) {
    if (ComposePaletteBillboardQuad(track, kAtTheOrigin).has_value()) {
      ++visibles;
    }
  }
  CHECK(visibles == kEffectTracksPerNode);
}

TEST_CASE("l'éclat est un quad DEBOUT, large comme il est haut", "[render][palette_billboard]") {
  const std::array<EffectTrack, kEffectTracksPerNode> tracks = Advanced(2, 30);
  const std::optional<EffectWorldQuad> quad =
      ComposePaletteBillboardQuad(tracks.front(), kAtTheOrigin);
  REQUIRE(quad.has_value());
  CHECK(QuadArea(*quad) > 0.0F);

  // Deux coins en bas, deux en haut, et les deux hauteurs diffèrent d'une taille.
  CHECK(quad->corners[0].y == Approx(quad->corners[1].y));
  CHECK(quad->corners[2].y == Approx(quad->corners[3].y));
  const float size = tracks.front().value[kSlotOfBillboardSize];
  CHECK(quad->corners[2].y - quad->corners[0].y == Approx(size));

  // ⚠ ET IL EST CENTRÉ À LA HAUTEUR DE LA CASE D'ÉLÉVATION, AU-DESSUS du sol :
  // le client l'écrit dans son repère qui descend, et le recopier enterrait le
  // panneau.
  const float lift = tracks.front().value[kSlotOfBillboardLift];
  REQUIRE(lift > 0.0F);
  CHECK((quad->corners[0].y + quad->corners[2].y) * 0.5F == Approx(lift));

  // ⚠ LES DEUX CÔTÉS SONT DIAMÉTRALEMENT OPPOSÉS : leur milieu est le centre.
  CHECK(quad->corners[0].x + quad->corners[1].x == Approx(0.0F).margin(0.001F));
  CHECK(quad->corners[0].z + quad->corners[1].z == Approx(0.0F).margin(0.001F));
}

TEST_CASE("la palette croise la nuance", "[render][palette_billboard]") {
  // ⚠ DEUX CHAMPS, PAS UN : la palette dit QUELS canaux prennent la nuance, la
  // nuance dit LAQUELLE. Sept fois quatre combinaisons, et elles diffèrent.
  std::set<std::array<std::uint8_t, 3>> seen;
  for (std::uint8_t palette = 2; palette <= 8; ++palette) {
    for (std::uint8_t nuance = 0; nuance < kPaletteBillboardShades; ++nuance) {
      seen.insert(EffectPaletteBillboardColor(palette, nuance));
    }
  }
  CHECK(seen.size() > 20);

  SECTION("la palette quatre ne teinte que le bleu") {
    const std::array<std::uint8_t, 3> color = EffectPaletteBillboardColor(4, 0);
    CHECK(color[0] == 255);
    CHECK(color[1] == 255);
    CHECK(color[2] == kPaletteBillboardShadeValues[0]);
  }
  SECTION("la palette huit teinte les trois") {
    const std::array<std::uint8_t, 3> color = EffectPaletteBillboardColor(8, 1);
    CHECK(color[0] == kPaletteBillboardShadeValues[1]);
    CHECK(color[1] == kPaletteBillboardShadeValues[1]);
    CHECK(color[2] == kPaletteBillboardShadeValues[1]);
  }
  SECTION("sauf à la dernière nuance, où elle rend le BLANC") {
    // ⚠ L'EXCEPTION, et elle est unique. L'oublier donnerait un gris uniforme
    // là où le client montre un éclat.
    const std::array<std::uint8_t, 3> color = EffectPaletteBillboardColor(8, 3);
    CHECK(color[0] == 255);
    CHECK(color[1] == 255);
    CHECK(color[2] == 255);
  }
  SECTION("une palette inconnue prend la branche par défaut, qui teinte vraiment") {
    const std::array<std::uint8_t, 3> color = EffectPaletteBillboardColor(99, 2);
    CHECK(color[0] == kPaletteBillboardShadeValues[2]);
    CHECK(color[1] == kPaletteBillboardShadeValues[2]);
    CHECK(color[2] == 255);
  }
}

TEST_CASE("les quatre nuances paraissent ensemble sur un même nœud",
          "[render][palette_billboard]") {
  // ⚠ LA NUANCE EST LE RANG DE LA PISTE, pas un paramètre. Les semer identiques
  // montrerait un seul éclat quatre fois.
  const std::array<EffectTrack, kEffectTracksPerNode> tracks = Advanced(3, 100);
  std::set<std::array<std::uint8_t, 3>> couleurs;
  for (const EffectTrack& track : tracks) {
    const std::optional<EffectWorldQuad> quad = ComposePaletteBillboardQuad(track, kAtTheOrigin);
    REQUIRE(quad.has_value());
    couleurs.insert({quad->tint[0], quad->tint[1], quad->tint[2]});
    // Et le mélange est une ADDITION : le client pousse zéro au seizième
    // paramètre.
    CHECK(quad->blend_source == 5);
    CHECK(quad->blend_destination == 2);
  }
  CHECK(couleurs.size() == kEffectTracksPerNode);
}

TEST_CASE("la piste RENAÎT au lieu de mourir", "[render][palette_billboard]") {
  // ⚠ LA CLÉ CROÎT DE DEUX POUR CENT PAR TIC, puis l'alpha tombe, et la piste
  // repart d'une clé de dix à un azimut neuf. Une piste qu'on laisserait mourir
  // éteindrait la spirale au bout de deux secondes.
  std::array<EffectTrack, kEffectTracksPerNode> tracks = Advanced(2, 60);
  const float grande = tracks.front().lift_key;
  CHECK(grande > 10.0F);

  tracks = Advanced(2, 200);
  bool vivante = false;
  for (const EffectTrack& track : tracks) {
    if (track.active && track.alpha > 0) {
      vivante = true;
    }
  }
  CHECK(vivante);
}

TEST_CASE("les neuf éclats du client sont en table", "[render][palette_billboard]") {
  const std::span<const EffectPaletteBillboardPattern> table = EffectPaletteBillboardPatternTable();
  // Les sept éclats de gel, et les deux cercles — 757 et 1122 — du bâtisseur jumeau.
  CHECK(table.size() == 9);
  CHECK(std::ranges::is_sorted(table, {}, &EffectPaletteBillboardPattern::effect_id));

  std::set<std::uint8_t> palettes;
  for (const EffectPaletteBillboardPattern& pattern : table) {
    palettes.insert(pattern.palette);
    INFO("effet " << pattern.effect_id);
    CHECK_FALSE(pattern.texture.empty());
    // ⚠ UN SEUL PATRON PAR IDENTIFIANT, alors que le relevé en liste deux : les
    // deux branches du bâtisseur posent le même nœud. En garder deux doublerait
    // la troupe.
    CHECK(EffectPaletteBillboardPatternsFor(pattern.effect_id).size() == 1);

    // Et toute piste avancée a de l'aire, une fois son retard passé.
    const std::array<EffectTrack, kEffectTracksPerNode> tracks = Advanced(pattern.palette, 120);
    for (const EffectTrack& track : tracks) {
      const std::optional<EffectWorldQuad> quad = ComposePaletteBillboardQuad(track, kAtTheOrigin);
      REQUIRE(quad.has_value());
      CHECK(QuadArea(*quad) > 0.0F);
    }
  }
  // Les deux cercles partagent la palette zéro ; chaque éclat de gel a la sienne.
  CHECK(palettes.size() == table.size() - 1);
  CHECK(EffectPaletteBillboardPatternsFor(1).empty());
}

TEST_CASE("les cercles de 757 et 1122 : palette zéro, alpha deux cents",
          "[render][palette_billboard]") {
  constexpr std::uint32_t kCirclePower = 1122;
  // 757 n'est, dans le répartiteur, qu'un saut dans le corps de 1122 : même image,
  // même octet, même bâtisseur.
  constexpr std::uint32_t kCirclePowerEffect = 757;
  const std::span<const EffectPaletteBillboardPattern> circle =
      EffectPaletteBillboardPatternsFor(kCirclePower);
  REQUIRE(circle.size() == 1);
  CHECK(circle.front().texture == "data\\texture\\effect\\emp shock.tga");
  CHECK(circle.front().palette == 0);
  CHECK(circle.front().alpha == 200);
  const std::span<const EffectPaletteBillboardPattern> twin =
      EffectPaletteBillboardPatternsFor(kCirclePowerEffect);
  REQUIRE(twin.size() == 1);
  CHECK(twin.front().texture == circle.front().texture);
  CHECK(twin.front().palette == circle.front().palette);
  CHECK(twin.front().alpha == circle.front().alpha);
  // Les éclats de gel, eux, partent à cent.
  CHECK(EffectPaletteBillboardPatternsFor(865).front().alpha == 100);
  std::uint32_t generator = 1;
  for (const EffectTrack& track : SeedEffectPaletteBillboardTracks(circle.front(), generator)) {
    CHECK(track.alpha == 200);
  }
}

TEST_CASE("le nœud semé rejoint la troupe et se met à tourner", "[render][palette_billboard]") {
  const EffectPaletteBillboardPattern& pattern = EffectPaletteBillboardPatternsFor(865).front();
  std::uint32_t seed = 7U;
  ro::render::PooledEffectNode node = SowEffectPaletteBillboardNode(
      pattern, glm::vec3{3.0F, 0.0F, 4.0F}, ro::render::kEffectNodeDefaultLifeTicks, seed);
  CHECK(node.shape == PooledNodeShape::kPaletteBillboard);
  CHECK(node.resource_paths.size() == 1);
  for (const EffectTrack& track : node.tracks) {
    CHECK(track.value[kSlotOfBillboardPalette] == Approx(static_cast<float>(pattern.palette)));
  }
  // Les nuances sont bien les quatre rangs.
  CHECK(node.tracks[0].value[kSlotOfBillboardShade] == Approx(0.0F));
  CHECK(node.tracks[3].value[kSlotOfBillboardShade] == Approx(3.0F));

  EffectNodePool pool;
  pool.Add(node);
  for (int tick = 0; tick < 30; ++tick) {
    pool.Advance(16);
  }
  REQUIRE(pool.size() == 1);
  // Le balayage a tourné, et la taille existe désormais.
  CHECK(pool.nodes().front().tracks[0].value[kSlotOfBillboardSize] > 0.0F);
}

TEST_CASE("l'éclat sème deux vagues, aux tics zéro et dix", "[render][palette_billboard]") {
  CHECK(EffectNodeScheduleSowsAt(kPaletteBillboardSchedule, 0));
  CHECK(EffectNodeScheduleSowsAt(kPaletteBillboardSchedule, 10));
  for (std::int32_t tick = 1; tick < 10; ++tick) {
    CHECK_FALSE(EffectNodeScheduleSowsAt(kPaletteBillboardSchedule, tick));
  }
  CHECK_FALSE(EffectNodeScheduleSowsAt(kPaletteBillboardSchedule, 20));
  CHECK(EffectNodeScheduleLastSowTick(kPaletteBillboardSchedule) == 10);
}
