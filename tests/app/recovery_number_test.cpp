// LE CHIFFRE DE LA VIE ET DE LA MAGIE RENDUES : ce que l'application tire de
// l'effet à valeur que `ZC_RECOVERY` pose sur le joueur (voir
// tests/net/recovery_test.cpp pour ce que la trame fait du monde).

#include <optional>

#include <catch2/catch_test_macros.hpp>

#include "ro/net/recovery.h"
#include "ro/render/floating_number.h"

#include "world/hit_popup_look.h"
#include "world/hit_popup_rules.h"

TEST_CASE("Récupération : les effets sont ceux dont l'effet à valeur fait compter le chiffre",
          "[app][recovery]") {
  // Le chiffre suit le chemin de l'effet à valeur : il ne paraît que si les
  // deux numéros sont les mêmes.
  CHECK(ro::net::kHpRecoveryEffect == ro::render::kHpOverTimeEffect);
  CHECK(ro::net::kSpRecoveryEffect == ro::render::kSpOverTimeEffect);

  // Vert pour la vie, bleu pour la magie, l'allure d'un soin : ce que le
  // client passe à son message de chiffre.
  const moonclient::HitPopupLook look;
  const std::optional<ro::render::FloatingNumber> life =
      moonclient::EffectNumber(ro::net::kHpRecoveryEffect, look);
  REQUIRE(life.has_value());
  CHECK(life->color == ro::render::kHealNumberColor);
  CHECK(life->style == ro::render::NumberStyle::kHealing);
  const std::optional<ro::render::FloatingNumber> magic =
      moonclient::EffectNumber(ro::net::kSpRecoveryEffect, look);
  REQUIRE(magic.has_value());
  CHECK(magic->color == ro::render::kSpRecoveryNumberColor);
  CHECK(magic->style == ro::render::NumberStyle::kHealing);
}

TEST_CASE("Récupération : un montant nul s'écrit « 0 », un négatif aussi", "[app][recovery]") {
  // Le client ramène un négatif à zéro et écrit le chiffre : il ne se tait pas.
  CHECK(moonclient::ShownEffectAmount(35) == 35U);
  CHECK(moonclient::ShownEffectAmount(0) == 0U);
  CHECK(moonclient::ShownEffectAmount(-12) == 0U);
}
