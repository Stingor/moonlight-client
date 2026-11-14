#include "ro/render/effect_trail_history.h"

#include <catch2/catch_approx.hpp>
#include <catch2/catch_test_macros.hpp>

#include "ro/render/effect_track.h"

using ro::render::AdvanceEffectTrailHistory;
using ro::render::EffectNode;
using ro::render::EffectTrailStep;
using ro::render::kEffectTrailScaleFadeShare;

namespace {

constexpr std::size_t kStepCount = 5;

EffectNode LiveNode() {
  EffectNode node;
  node.alpha = 200.0F;
  node.sprite_scale = 2.0F;
  node.orbit_degrees = 45.0F;
  return node;
}

}  // namespace

TEST_CASE("Traînée : un historique VIDE ne coûte rien", "[render][trail]") {
  // Un nœud sans traînée n'en paie rien : c'est la réponse, pas une omission.
  std::vector<EffectTrailStep> aucune;
  AdvanceEffectTrailHistory(aucune, LiveNode(), {1.0F, 2.0F, 3.0F});
  CHECK(aucune.empty());
}

TEST_CASE("Traînée : la TÊTE prend l'état courant, sans rien perdre", "[render][trail]") {
  std::vector<EffectTrailStep> history(kStepCount);
  const EffectNode effect_node = LiveNode();
  AdvanceEffectTrailHistory(history, effect_node, {4.0F, 5.0F, 6.0F});
  CHECK(history.front().ground_position == glm::vec3{4.0F, 5.0F, 6.0F});
  CHECK(history.front().alpha == Catch::Approx(effect_node.alpha));
  CHECK(history.front().sprite_scale == Catch::Approx(effect_node.sprite_scale));
  CHECK(history.front().angle_degrees == Catch::Approx(effect_node.orbit_degrees));
}

TEST_CASE("Traînée : la file RECULE d'un cran, elle ne se recopie pas", "[render][trail]") {
  // ⚠ C'EST LE PIÈGE DE L'ORDRE. À l'endroit, l'état courant se recopierait dans
  // toutes les cases d'un seul tic, et la traînée deviendrait cinq copies
  // superposées — visible seulement quand le nœud bouge.
  std::vector<EffectTrailStep> history(kStepCount);
  const EffectNode effect_node = LiveNode();
  AdvanceEffectTrailHistory(history, effect_node, {0.0F, 0.0F, 0.0F});
  AdvanceEffectTrailHistory(history, effect_node, {10.0F, 0.0F, 0.0F});
  AdvanceEffectTrailHistory(history, effect_node, {20.0F, 0.0F, 0.0F});
  CHECK(history[0].ground_position.x == Catch::Approx(20.0F));
  CHECK(history[1].ground_position.x == Catch::Approx(10.0F));
  CHECK(history[2].ground_position.x == Catch::Approx(0.0F));
}

TEST_CASE("Traînée : chaque cran est plus PÂLE et plus PETIT que le précédent", "[render][trail]") {
  std::vector<EffectTrailStep> history(kStepCount);
  const EffectNode effect_node = LiveNode();
  for (std::size_t tick = 0; tick < kStepCount; ++tick) {
    AdvanceEffectTrailHistory(history, effect_node, {static_cast<float>(tick), 0.0F, 0.0F});
  }
  for (std::size_t rang = 1; rang < history.size(); ++rang) {
    INFO("cran " << rang);
    CHECK(history[rang].alpha < history[rang - 1].alpha);
    CHECK(history[rang].sprite_scale < history[rang - 1].sprite_scale);
    CHECK(history[rang].alpha >= 0.0F);
    CHECK(history[rang].sprite_scale >= 0.0F);
  }
}

TEST_CASE("Traînée : la taille s'éteint DEUX FOIS moins vite que l'alpha", "[render][trail]") {
  // Une traînée s'efface plus vite qu'elle ne rétrécit. Les égaliser donnerait
  // un sillage qui disparaît en pointe au lieu de s'estomper.
  std::vector<EffectTrailStep> history(kStepCount);
  const EffectNode effect_node = LiveNode();
  for (std::size_t tick = 0; tick < kStepCount; ++tick) {
    AdvanceEffectTrailHistory(history, effect_node, {});
  }
  const std::size_t last = history.size() - 1;
  const float part = static_cast<float>(last) / static_cast<float>(history.size());
  CHECK(history[last].alpha == Catch::Approx(effect_node.alpha - effect_node.alpha * part));
  CHECK(history[last].sprite_scale ==
        Catch::Approx(effect_node.sprite_scale -
                      effect_node.sprite_scale * part * kEffectTrailScaleFadeShare));
  // La perte d'alpha est bien le double de celle de la taille, en proportion.
  const float perte_alpha = (effect_node.alpha - history[last].alpha) / effect_node.alpha;
  const float size_loss =
      (effect_node.sprite_scale - history[last].sprite_scale) / effect_node.sprite_scale;
  CHECK(perte_alpha == Catch::Approx(size_loss / kEffectTrailScaleFadeShare));
}

TEST_CASE("Traînée : une longueur d'UN cran se réduit à la tête", "[render][trail]") {
  // Le cas limite doit rester lisible : un seul cran, c'est le nœud lui-même.
  std::vector<EffectTrailStep> history(1);
  const EffectNode effect_node = LiveNode();
  AdvanceEffectTrailHistory(history, effect_node, {7.0F, 8.0F, 9.0F});
  CHECK(history.front().ground_position == glm::vec3{7.0F, 8.0F, 9.0F});
  CHECK(history.front().alpha == Catch::Approx(effect_node.alpha));
}

TEST_CASE("Traînée : l'alpha du NŒUD s'allume, se tient, puis s'éteint", "[render][trail]") {
  // ⚠ C'EST L'ALPHA DU NŒUD ET NON CELUI D'UNE PISTE. Les deux coexistent dans
  // cette famille ; se tromper de compteur laisserait le sprite à l'alpha de sa
  // naissance, c'est-à-dire éteint.
  std::array<ro::render::EffectTrack, ro::render::kEffectTracksPerNode> tracks;
  tracks.front().active = true;
  EffectNode effect_node;
  effect_node.alpha = 0.0F;
  effect_node.life_ticks = 300;
  std::vector<EffectTrailStep> history(kStepCount);

  ro::render::AdvanceFadingTrailNode(tracks, effect_node, history, {});
  const float after_one_tick = effect_node.alpha;
  CHECK(after_one_tick > 0.0F);

  for (std::int32_t tick = 1; tick < 100; ++tick) {
    ro::render::AdvanceFadingTrailNode(tracks, effect_node, history, {});
  }
  const float au_plateau = effect_node.alpha;
  // ⚠ IL PLAFONNE À CENT CINQUANTE, et non à deux cent cinquante-cinq : c'est un
  // éclat discret, et le monter à fond changerait ce qu'on voit.
  CHECK(au_plateau == Catch::Approx(150.0F));

  // Puis il se TIENT : entre la montée et la descente, rien ne bouge.
  for (std::int32_t tick = 0; tick < 50; ++tick) {
    ro::render::AdvanceFadingTrailNode(tracks, effect_node, history, {});
  }
  CHECK(effect_node.alpha == Catch::Approx(au_plateau));

  // Et il s'éteint sur les cinquante derniers tics de la vie du nœud.
  for (std::int32_t tick = 0; tick < 200; ++tick) {
    ro::render::AdvanceFadingTrailNode(tracks, effect_node, history, {});
  }
  CHECK(effect_node.alpha == Catch::Approx(0.0F));
}

TEST_CASE("Traînée : une piste éteinte ne pousse pas l'alpha", "[render][trail]") {
  std::array<ro::render::EffectTrack, ro::render::kEffectTracksPerNode> tracks;
  EffectNode effect_node;
  effect_node.alpha = 0.0F;
  effect_node.life_ticks = 300;
  std::vector<EffectTrailStep> history(kStepCount);
  for (std::int32_t tick = 0; tick < 30; ++tick) {
    ro::render::AdvanceFadingTrailNode(tracks, effect_node, history, {});
  }
  CHECK(effect_node.alpha == Catch::Approx(0.0F));
  CHECK(tracks.front().ticks == 0);
}

TEST_CASE("Traînée : elle NAÎT repliée sur le nœud, déjà rampée", "[render][trail]") {
  // Le client remplit ses N traces de l'état courant à la création : toutes au
  // même point, et le premier décalage pose les rampes d'alpha et d'échelle.
  std::vector<EffectTrailStep> history;
  const EffectNode effect_node = LiveNode();
  ro::render::SeedEffectTrailHistory(history, 4, effect_node, {1.0F, 2.0F, 3.0F});
  REQUIRE(history.size() == 4);
  for (const EffectTrailStep& notch : history) {
    CHECK(notch.ground_position == glm::vec3{1.0F, 2.0F, 3.0F});
  }
  CHECK(history[0].alpha == Catch::Approx(effect_node.alpha));
  CHECK(history[3].alpha == Catch::Approx(effect_node.alpha * 0.25F));
  CHECK(history[1].sprite_scale ==
        Catch::Approx(effect_node.sprite_scale * (1.0F - 0.25F * kEffectTrailScaleFadeShare)));

  // Un compte de un — le défaut du constructeur — est un sprite sans traînée.
  ro::render::SeedEffectTrailHistory(history, 1, effect_node, {});
  CHECK(history.empty());
}

TEST_CASE("Traînée : le facteur d'une pose rapporte le cran au NŒUD", "[render][trail]") {
  // Les quads portent déjà l'échelle du nœud ; la tête vaut donc un, et les
  // crans suivants la rampe du client — jamais l'échelle absolue, qui la
  // dessinerait au carré.
  std::vector<EffectTrailStep> history(kStepCount);
  const EffectNode effect_node = LiveNode();
  for (std::size_t tick = 0; tick < kStepCount; ++tick) {
    AdvanceEffectTrailHistory(history, effect_node, {});
  }
  CHECK(ro::render::EffectTrailStepScaleFactor(history[0], effect_node) == Catch::Approx(1.0F));
  const float part = 2.0F / static_cast<float>(kStepCount);
  CHECK(ro::render::EffectTrailStepScaleFactor(history[2], effect_node) ==
        Catch::Approx(1.0F - part * kEffectTrailScaleFadeShare));
  // Un nœud sans échelle ne divise pas : le facteur vaut un.
  EffectNode plat;
  plat.sprite_scale = 0.0F;
  CHECK(ro::render::EffectTrailStepScaleFactor(history[2], plat) == Catch::Approx(1.0F));
}
