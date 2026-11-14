#include <catch2/catch_approx.hpp>
#include <catch2/catch_test_macros.hpp>

#include "ro/render/effect_track.h"
#include "ro/render/effect_track_motion.h"

using ro::render::AdvanceFanBladeTracks;
using ro::render::EffectTrack;
using ro::render::kEffectTracksPerNode;

namespace {

constexpr std::size_t kBladeCount = 10;
constexpr std::size_t kSlotOfFirstBlade = 1;
constexpr std::size_t kSlotOfLastBlade = kSlotOfFirstBlade + kBladeCount - 1;
constexpr std::size_t kMarkOfStyle = 4;

// Les sept premières lames atteignent leur plafond ; la montée s'arrête avant
// que les trois dernières n'atteignent le leur, et toutes trois s'arrêtent au
// plafond de la septième.
constexpr std::size_t kBladesThatReachTheirCeiling = 7;
constexpr float kHighestAlphaReached = 190.0F;

// Les styles que le semis du sous-type 147 emploie : deux ouvrent le bouquet à
// la fin, le troisième non.
constexpr std::uint8_t kStyleThatStays = 5;
constexpr std::uint8_t kFirstStyleThatOpens = 6;

std::array<EffectTrack, kEffectTracksPerNode> OneLive(std::uint8_t style, float radius) {
  std::array<EffectTrack, kEffectTracksPerNode> tracks;
  tracks[0].active = true;
  tracks[0].radius_at_anchor = radius;
  tracks[0].mark[kMarkOfStyle] = style;
  return tracks;
}

void Advance(std::array<EffectTrack, kEffectTracksPerNode>& tracks, std::int32_t ticks) {
  for (std::int32_t tick = 0; tick < ticks; ++tick) {
    AdvanceFanBladeTracks(tracks, 0, 0);
  }
}

}  // namespace

TEST_CASE("Éventail : les angles de caméra se rangent même quand le nœud dort", "[render][track]") {
  std::array<EffectTrack, kEffectTracksPerNode> endormies;
  AdvanceFanBladeTracks(endormies, 111, 222);
  CHECK(endormies[0].style == 111);
  CHECK(endormies[1].style == 222);
  CHECK(endormies[0].ticks == 0);
}

TEST_CASE("Éventail : CHAQUE LAME a son propre éclat", "[render][track]") {
  // ⚠ C'EST TOUT LE DÉGRADÉ. Dix cases d'affilée portent dix alphas, un par
  // lame, et chacune a son propre plafond : soixante-dix pour la première, vingt
  // de plus à chaque suivante. Les lire comme une seule valeur donnerait dix
  // lames de même éclat, c'est-à-dire une tache au lieu d'un éventail.
  std::array<EffectTrack, kEffectTracksPerNode> tracks = OneLive(kStyleThatStays, 21.0F);
  Advance(tracks, 19);
  for (std::size_t lame = 1; lame < kBladesThatReachTheirCeiling; ++lame) {
    INFO("lame " << lame);
    CHECK(tracks[0].value[kSlotOfFirstBlade + lame] >
          tracks[0].value[kSlotOfFirstBlade + lame - 1]);
  }
  CHECK(tracks[0].value[kSlotOfFirstBlade] == Catch::Approx(70.0F));
  // ⚠ ET LE DÉGRADÉ S'APLATIT AU BOUT. La montée dure vingt tics à dix par tic :
  // les trois derniers plafonds, au-delà de cent quatre-vingt-dix, sont hors
  // d'atteinte. Attendre deux cent cinquante de la dernière lame serait attendre
  // une valeur que ce mouvement ne produit jamais.
  for (std::size_t lame = kBladesThatReachTheirCeiling; lame < kBladeCount; ++lame) {
    INFO("lame " << lame);
    CHECK(tracks[0].value[kSlotOfFirstBlade + lame] == Catch::Approx(kHighestAlphaReached));
  }
}

TEST_CASE("Éventail : NEUF lames retombent, la dixième reste", "[render][track]") {
  // ⚠ LA BOUCLE DE CHUTE DU CLIENT NE FAIT QUE NEUF TOURS. Étendre la chute à la
  // dixième lame l'éteindrait avant le quatre-vingt-dixième tic, et le bouquet
  // ne s'ouvrirait plus jamais — la panne se verrait à l'écran, pas ici.
  std::array<EffectTrack, kEffectTracksPerNode> tracks = OneLive(kStyleThatStays, 21.0F);
  Advance(tracks, 19);
  const float last_shard = tracks[0].value[kSlotOfLastBlade];
  Advance(tracks, 200);
  for (std::size_t lame = 0; lame + 1 < kBladeCount; ++lame) {
    INFO("lame " << lame);
    CHECK(tracks[0].value[kSlotOfFirstBlade + lame] == Catch::Approx(0.0F));
  }
  CHECK(tracks[0].value[kSlotOfLastBlade] == Catch::Approx(last_shard));
}

TEST_CASE("Éventail : le rayon SE RESSERRE d'abord, puis cesse", "[render][track]") {
  std::array<EffectTrack, kEffectTracksPerNode> tracks = OneLive(kStyleThatStays, 21.0F);
  Advance(tracks, 10);
  CHECK(tracks[0].radius_at_anchor < 21.0F);
  // Le resserrement s'arrête au cinquantième tic : le laisser courir
  // effondrerait le bouquet sur son ancre.
  Advance(tracks, 45);
  const float after = tracks[0].radius_at_anchor;
  Advance(tracks, 20);
  CHECK(tracks[0].radius_at_anchor == Catch::Approx(after));
}

TEST_CASE("Éventail : deux styles OUVRENT le bouquet à la fin, le troisième non",
          "[render][track]") {
  // ⚠ C'EST CE QUI SÉPARE LES TROIS STYLES DU SEMIS. Passé le quatre-vingt-dixième
  // tic, la dernière lame de deux d'entre eux s'efface de cinq par tic, et le
  // rayon enfle tant qu'elle vit. Le troisième garde sa lame et son rayon.
  std::array<EffectTrack, kEffectTracksPerNode> ouvre = OneLive(kFirstStyleThatOpens, 21.0F);
  std::array<EffectTrack, kEffectTracksPerNode> tient = OneLive(kStyleThatStays, 21.0F);
  Advance(ouvre, 95);
  Advance(tient, 95);
  const float opened_radius = ouvre[0].radius_at_anchor;
  const float held_radius = tient[0].radius_at_anchor;
  const float open_blade = ouvre[0].value[kSlotOfLastBlade];
  Advance(ouvre, 10);
  Advance(tient, 10);
  CHECK(ouvre[0].radius_at_anchor > opened_radius);
  CHECK(ouvre[0].value[kSlotOfLastBlade] < open_blade);
  CHECK(tient[0].radius_at_anchor == Catch::Approx(held_radius));
  CHECK(tient[0].value[kSlotOfLastBlade] == Catch::Approx(kHighestAlphaReached));

  // Et l'ouverture CESSE avec la lame : le rayon ne croît pas sans fin.
  Advance(ouvre, 200);
  CHECK(ouvre[0].value[kSlotOfLastBlade] == Catch::Approx(0.0F));
  const float final_radius = ouvre[0].radius_at_anchor;
  Advance(ouvre, 50);
  CHECK(ouvre[0].radius_at_anchor == Catch::Approx(final_radius));
}

TEST_CASE("Éventail : une piste éteinte ne bouge pas", "[render][track]") {
  std::array<EffectTrack, kEffectTracksPerNode> tracks = OneLive(kStyleThatStays, 21.0F);
  tracks[0].active = false;
  Advance(tracks, 40);
  CHECK(tracks[0].ticks == 0);
  CHECK(tracks[0].radius_at_anchor == Catch::Approx(21.0F));
  CHECK(tracks[0].value[kSlotOfFirstBlade] == Catch::Approx(0.0F));
}
