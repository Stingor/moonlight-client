// LE VOILE DE L'ENTRÉE EN JEU : la main ne passe que sur une image déjà noire.

#include "lobby/enter_veil.h"

#include <catch2/catch_test_macros.hpp>

using moonclient::lobby::EnterVeil;

namespace {

constexpr float kFadeSeconds = 0.26F;
constexpr std::uint32_t kStartMs = 5000;
// Une image ordinaire, et une image beaucoup plus lente que tout le fondu.
constexpr std::uint32_t kFrameMs = 16;
constexpr std::uint32_t kSlowFrameMs = 900;
// Juste avant la fin du fondu.
constexpr std::uint32_t kAlmostFadedMs = 250;

}  // namespace

TEST_CASE("Voile : il monte, puis couvre, et seulement après une image noire",
          "[app][lobby][veil]") {
  EnterVeil veil;
  veil.Follow(false, kStartMs);
  CHECK(veil.stage() == EnterVeil::Stage::kNone);
  CHECK(veil.seconds(kStartMs) == 0.0F);

  veil.Follow(true, kStartMs);
  veil.Drawn(kStartMs, kFadeSeconds);
  CHECK(veil.stage() == EnterVeil::Stage::kRising);
  CHECK_FALSE(veil.covering());

  // 🔴 LE SERVEUR PEUT RÉPONDRE TOUT DE SUITE : tant qu'aucune image noire
  // n'a été dessinée, la main ne passe pas.
  std::uint32_t now = kStartMs;
  while (now - kStartMs < kAlmostFadedMs) {
    now += kFrameMs;
    veil.Follow(true, now);
    veil.Drawn(now, kFadeSeconds);
  }
  CHECK_FALSE(veil.covering());
  now += kFrameMs;
  veil.Follow(true, now);
  veil.Drawn(now, kFadeSeconds);
  CHECK(veil.covering());
}

TEST_CASE("Voile : une image lente couvre d'un coup, sans rien présumer", "[app][lobby][veil]") {
  EnterVeil veil;
  veil.Follow(true, kStartMs);
  veil.Drawn(kStartMs, kFadeSeconds);
  CHECK_FALSE(veil.covering());
  veil.Follow(true, kStartMs + kSlowFrameMs);
  // ⚠ AVANT LE DESSIN, le temps écoulé ne suffit pas : c'est l'image dessinée
  // qui compte.
  CHECK_FALSE(veil.covering());
  veil.Drawn(kStartMs + kSlowFrameMs, kFadeSeconds);
  CHECK(veil.covering());
}

TEST_CASE("Voile : un refus le fait tomber, le choix suivant repart de zéro",
          "[app][lobby][veil]") {
  EnterVeil veil;
  veil.Follow(true, kStartMs);
  veil.Drawn(kStartMs + kSlowFrameMs, kFadeSeconds);
  REQUIRE(veil.covering());

  veil.Follow(false, kStartMs + kSlowFrameMs + kFrameMs);
  CHECK(veil.stage() == EnterVeil::Stage::kNone);

  const std::uint32_t again = kStartMs + 2 * kSlowFrameMs;
  veil.Follow(true, again);
  CHECK(veil.stage() == EnterVeil::Stage::kRising);
  CHECK(veil.seconds(again) == 0.0F);
}

TEST_CASE("Voile : l'entrée qui se prolonge reste couverte", "[app][lobby][veil]") {
  // Un serveur lent : le noir tient, il ne retombe pas.
  EnterVeil veil;
  veil.Follow(true, kStartMs);
  veil.Drawn(kStartMs + kSlowFrameMs, kFadeSeconds);
  for (std::uint32_t frame = 1; frame < 10; ++frame) {
    veil.Follow(true, kStartMs + kSlowFrameMs * (frame + 1));
    veil.Drawn(kStartMs + kSlowFrameMs * (frame + 1), kFadeSeconds);
    CHECK(veil.covering());
  }
}
