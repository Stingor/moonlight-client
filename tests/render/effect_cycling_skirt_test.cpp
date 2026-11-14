#include "ro/render/effect_cycling_skirt.h"

#include <set>

#include <catch2/catch_approx.hpp>
#include <catch2/catch_test_macros.hpp>

#include "ro/render/effect_cycling_skirt_seed.h"
#include "ro/render/effect_node_pool.h"
#include "ro/render/effect_track_motion.h"

using Catch::Approx;
using ro::render::AdvanceGravityWaveTracks;
using ro::render::ComposeCyclingSkirtQuads;
using ro::render::ComposeEffectTrackSkirt;
using ro::render::EffectCyclingSkirtPattern;
using ro::render::EffectCyclingSkirtPatternsFor;
using ro::render::EffectCyclingSkirtPatternTable;
using ro::render::EffectCyclingSkirtTexturePaths;
using ro::render::EffectCyclingSkirtVariantFor;
using ro::render::EffectTrack;
using ro::render::EffectWorldQuad;
using ro::render::kCyclingSkirtAlphaStep;
using ro::render::kCyclingSkirtBubbleCount;
using ro::render::kCyclingSkirtLiftKeys;
using ro::render::kCyclingSkirtTopCount;
using ro::render::kEffectTracksPerNode;
using ro::render::PooledNodeShape;
using ro::render::SeedEffectCyclingSkirtTracks;
using ro::render::SowEffectCyclingSkirtNode;

namespace {

constexpr std::uint32_t kBeginSpell = 830;
constexpr glm::vec3 kAtOrigin{0.0F, 0.0F, 0.0F};

[[nodiscard]] EffectCyclingSkirtPattern Pattern(std::int16_t alpha, std::uint8_t style) {
  return {kBeginSpell, "Effect_InitWaterBubblesOrTop", alpha, style};
}

}  // namespace

TEST_CASE("les images se numerotent a partir de un", "[render][cycling_skirt]") {
  const std::vector<std::string> bulles = EffectCyclingSkirtTexturePaths(0);
  REQUIRE(bulles.size() == kCyclingSkirtBubbleCount);
  // ⚠ LE CLIENT FORMATE AVEC L'INDICE PLUS UN : la première bulle est la
  // première, pas la zéroième. La manquer ne charge aucune image du premier coup.
  CHECK(bulles.front() == "data\\texture\\effect\\W_bubble01.tga");
  CHECK(bulles.back() == "data\\texture\\effect\\W_bubble27.tga");

  const std::vector<std::string> dessus = EffectCyclingSkirtTexturePaths(2);
  REQUIRE(dessus.size() == kCyclingSkirtTopCount);
  CHECK(dessus.front() == "data\\texture\\effect\\W_A_top01.tga");
  CHECK(dessus.back() == "data\\texture\\effect\\W_A_top29.tga");

  // ⚠ AU-DELÀ DE TROIS, LE CLIENT NE CHARGE RIEN : son nœud n'a pas d'image.
  CHECK(EffectCyclingSkirtTexturePaths(4).empty());
  // ET LE STYLE UN PREND LES BULLES, non le dessus : la borne est à un.
  CHECK(EffectCyclingSkirtTexturePaths(1).front() == bulles.front());
  CHECK(EffectCyclingSkirtTexturePaths(3).front() == dessus.front());
}

TEST_CASE("la teinte ne suit pas le style", "[render][cycling_skirt]") {
  // Un style par jeu teinte, l'autre non : ce n'est pas une échelle.
  CHECK(EffectCyclingSkirtVariantFor(0) == 0);
  CHECK(EffectCyclingSkirtVariantFor(1) == 4);
  CHECK(EffectCyclingSkirtVariantFor(2) == 0);
  CHECK(EffectCyclingSkirtVariantFor(3) == 4);
}

TEST_CASE("les quatre pistes sont echelonnees par leur alpha et leur cle",
          "[render][cycling_skirt]") {
  const std::array<EffectTrack, kEffectTracksPerNode> tracks =
      SeedEffectCyclingSkirtTracks(Pattern(45, 1));
  std::set<std::int16_t> azimuts;
  for (std::size_t rang = 0; rang < tracks.size(); ++rang) {
    CHECK(tracks[rang].active);
    CHECK(tracks[rang].lift_key == Approx(kCyclingSkirtLiftKeys[rang]));
    azimuts.insert(tracks[rang].azimuth_degrees);
  }
  // QUATRE AZIMUTS DISTINCTS : c'est ce qui imbrique les quatre anneaux.
  CHECK(azimuts.size() == 4);

  // ⚠ LA DERNIÈRE PISTE PORTE L'ALPHA DU SITE, et les trois autres montent de
  // quarante-cinq en quarante-cinq au-dessus d'elle.
  CHECK(tracks.back().alpha == 45);
  CHECK(tracks[0].alpha == 45 + 3 * kCyclingSkirtAlphaStep);
  CHECK(tracks[1].alpha == tracks[0].alpha - kCyclingSkirtAlphaStep);

  // ⚠ ET AUCUNE CLÉ NE SOULÈVE LA JUPE : les quatre valent entre vingt-sept et
  // trente, et la figure ne se décale qu'à deux valeurs exactes, 6,001 et
  // 24,000999. La jupe reste donc au plan de sa piste.
  for (const EffectTrack& track : tracks) {
    CHECK(track.lift_key != Approx(6.0009999F));
    CHECK(track.lift_key != Approx(24.000999F));
  }
}

TEST_CASE("la jupe qui defile est la jupe, a l'image pres", "[render][cycling_skirt]") {
  EffectTrack track = SeedEffectCyclingSkirtTracks(Pattern(45, 1)).front();
  const std::vector<EffectWorldQuad> nue = ComposeEffectTrackSkirt(track, 4, kAtOrigin);
  const std::vector<EffectWorldQuad> defilante =
      ComposeCyclingSkirtQuads(track, 4, kCyclingSkirtBubbleCount, kAtOrigin);
  // ⚠ MÊME GÉOMÉTRIE, AU QUAD PRÈS : les deux dessins du client portent les
  // mêmes constantes, et seul le modulo les sépare.
  REQUIRE(nue.size() == defilante.size());
  REQUIRE_FALSE(nue.empty());
  for (std::size_t rang = 0; rang < nue.size(); ++rang) {
    CHECK(nue[rang].corners[0] == defilante[rang].corners[0]);
    CHECK(nue[rang].tint == defilante[rang].tint);
  }
}

TEST_CASE("l'image tourne avec le compteur de la piste", "[render][cycling_skirt]") {
  std::array<EffectTrack, kEffectTracksPerNode> tracks =
      SeedEffectCyclingSkirtTracks(Pattern(45, 1));
  std::set<std::size_t> seen;
  for (std::int32_t tick = 0; tick < static_cast<std::int32_t>(kCyclingSkirtBubbleCount); ++tick) {
    const std::vector<EffectWorldQuad> skirt =
        ComposeCyclingSkirtQuads(tracks.front(), 4, kCyclingSkirtBubbleCount, kAtOrigin);
    if (!skirt.empty()) {
      seen.insert(skirt.front().texture);
      // TOUS LES QUADS D'UN TIC PARTAGENT LEUR IMAGE.
      for (const EffectWorldQuad& quad : skirt) {
        CHECK(quad.texture == skirt.front().texture);
      }
    }
    AdvanceGravityWaveTracks(tracks, 200);
  }
  // ⚠ LE JEU ENTIER DÉFILE EN VINGT-SEPT TICS, et pas moins : une image sautée
  // se verrait comme un saut dans l'animation.
  CHECK(seen.size() > 1);

  // SANS IMAGE, RIEN NE SE DESSINE plutôt qu'une division par zéro.
  CHECK(ComposeCyclingSkirtQuads(tracks.front(), 4, 0, kAtOrigin).empty());
}

TEST_CASE("le mouvement est le corps commun, onde autour de la CLE", "[render][cycling_skirt]") {
  std::array<EffectTrack, kEffectTracksPerNode> tracks =
      SeedEffectCyclingSkirtTracks(Pattern(45, 1));
  const float radius = tracks.front().radius_at_anchor;
  const std::int16_t tilt = tracks.front().tilt_degrees;
  AdvanceGravityWaveTracks(tracks, 200);
  // LE RAYON CROÎT DE SEPT CENTIÈMES, l'inclinaison descend d'un.
  CHECK(tracks.front().radius_at_anchor == Approx(radius + 0.07F));
  CHECK(tracks.front().tilt_degrees == tilt - 1);
  CHECK(tracks.front().ticks == 1);

  // ⚠ L'ONDE BAT AUTOUR DE LA CLÉ, non du rayon : les longueurs de segment
  // tournent autour de trente, pas autour de quatre.
  CHECK(tracks.front().value[0] == Approx(tracks.front().lift_key));
  CHECK(tracks.front().value[5] > 1.0F);
}

TEST_CASE("le noeud seme porte tout son jeu d'images", "[render][cycling_skirt]") {
  const std::span<const EffectCyclingSkirtPattern> table = EffectCyclingSkirtPatternTable();
  REQUIRE_FALSE(table.empty());
  const std::span<const EffectCyclingSkirtPattern> spell =
      EffectCyclingSkirtPatternsFor(kBeginSpell);
  // ⚠ UN SITE EN POSE DEUX, à deux alphas et deux styles.
  REQUIRE(spell.size() == 2);
  CHECK(spell[0].base_alpha != spell[1].base_alpha);

  const ro::render::PooledEffectNode node =
      SowEffectCyclingSkirtNode(spell.front(), glm::vec3{1.0F, 2.0F, 3.0F}, 120);
  CHECK(node.shape == PooledNodeShape::kCyclingSkirt);
  CHECK(node.resource_paths.size() == kCyclingSkirtBubbleCount);
  CHECK(node.node.life_ticks == 120);
}

TEST_CASE("le lot du noeud change d'image a chaque tic, avec le compteur",
          "[render][cycling_skirt]") {
  // Le dessinateur prend `compteur % nombre d'images` ; la composition choisit
  // l'image du lot par `node.frame`. Laisse aux quatre tics par image du
  // constructeur, le lot defilerait quatre fois trop lentement.
  ro::render::EffectNodePool pool;
  pool.Add(SowEffectCyclingSkirtNode(Pattern(45, 1), kAtOrigin, 200));
  for (int tick = 1; tick <= 5; ++tick) {
    pool.Advance(ro::render::kEffectTickMs);
    REQUIRE(pool.size() == 1);
    const ro::render::PooledEffectNode& node = pool.nodes().front();
    CHECK(node.node.frame == node.tracks.front().ticks);
  }
  CHECK(pool.nodes().front().node.frame >= 4);
}

TEST_CASE("la jupe court jusqu'au bout de son balayage, sa case bornee a vingt",
          "[render][cycling_skirt]") {
  // 🔴 LE CLIENT AVANCE TANT QUE L'ANGLE CUMULÉ RESTE DANS LE BALAYAGE, et ne
  // borne que son index : 315 degrés au pas de 15 donnent vingt-deux points,
  // le dernier relisant la vingtième case — vingt et un quads, et non vingt.
  EffectTrack track;
  track.active = true;
  track.alpha = 100;
  track.degrees = 315;
  track.radius_at_anchor = 3.0F;
  track.value.fill(10.0F);
  CHECK(ComposeEffectTrackSkirt(track, 0, kAtOrigin).size() == 21);
  // Un tour entier au pas de dix-huit : vingt quads, refermés.
  track.degrees = 360;
  CHECK(ComposeEffectTrackSkirt(track, 0, kAtOrigin).size() == 20);
}
