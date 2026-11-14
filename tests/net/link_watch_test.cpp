// Le chien de garde de la liaison : ses seuils, le gel de la boucle, la reprise.

#include "ro/net/link_watch.h"

#include <cstdint>

#include <catch2/catch_test_macros.hpp>

using ro::net::kLinkSilentLossMs;
using ro::net::kLinkUnsteadyMs;
using ro::net::kLoopStallMs;
using ro::net::LinkHealth;
using ro::net::LinkWatch;

namespace {

constexpr std::uint32_t kStartMs = 100'000;
// Une image ordinaire, bien sous le seuil de gel.
constexpr std::uint32_t kFrameMs = 16;

// Fait tourner la boucle de `from_ms` à `to_ms`, image par image, sans rien
// recevoir. Rend l'instant de la dernière image.
std::uint32_t RunSilent(LinkWatch& watch, std::uint32_t from_ms, std::uint32_t to_ms) {
  std::uint32_t now_ms = from_ms;
  while (now_ms < to_ms) {
    now_ms += kFrameMs;
    watch.NoteLoop(now_ms);
  }
  return now_ms;
}

}  // namespace

TEST_CASE("Garde de liaison : rien d'armé, rien à juger", "[net][liaison]") {
  const LinkWatch watch;
  CHECK(watch.Health(kStartMs) == LinkHealth::kSteady);
}

TEST_CASE("Garde de liaison : instable à quinze secondes, perdue à trente", "[net][liaison]") {
  LinkWatch watch;
  watch.Restart(kStartMs);

  std::uint32_t now_ms = RunSilent(watch, kStartMs, kStartMs + kLinkUnsteadyMs - kFrameMs);
  CHECK(watch.Health(now_ms) == LinkHealth::kSteady);

  now_ms = RunSilent(watch, now_ms, kStartMs + kLinkUnsteadyMs);
  CHECK(watch.Health(now_ms) == LinkHealth::kUnsteady);

  now_ms = RunSilent(watch, now_ms, kStartMs + kLinkSilentLossMs - kFrameMs);
  CHECK(watch.Health(now_ms) == LinkHealth::kUnsteady);

  now_ms = RunSilent(watch, now_ms, kStartMs + kLinkSilentLossMs);
  CHECK(watch.Health(now_ms) == LinkHealth::kLost);
}

TEST_CASE("Garde de liaison : une trame reçue efface le silence", "[net][liaison]") {
  LinkWatch watch;
  watch.Restart(kStartMs);
  std::uint32_t now_ms = RunSilent(watch, kStartMs, kStartMs + kLinkUnsteadyMs);
  REQUIRE(watch.Health(now_ms) == LinkHealth::kUnsteady);

  // LE BANDEAU DISPARAÎT À LA PREMIÈRE TRAME, quelle qu'elle soit.
  watch.NoteReceived(now_ms);
  CHECK(watch.Health(now_ms) == LinkHealth::kSteady);

  // ET LE SILENCE SE RECOMPTE DE LÀ, pas du départ.
  now_ms = RunSilent(watch, now_ms, now_ms + kLinkSilentLossMs - kFrameMs);
  CHECK(watch.Health(now_ms) != LinkHealth::kLost);
}

TEST_CASE("Garde de liaison : une boucle gelée ne compte pas pour un silence", "[net][liaison]") {
  // 🔴 UNE FENÊTRE TENUE À LA SOURIS GÈLE LA BOUCLE, et pendant ce temps on ne
  // lit rien. Quarante secondes sans image ne prouvent pas que le serveur se
  // tait : on ne l'écoutait pas.
  LinkWatch watch;
  watch.Restart(kStartMs);
  std::uint32_t now_ms = RunSilent(watch, kStartMs, kStartMs + kLinkUnsteadyMs);
  REQUIRE(watch.Health(now_ms) == LinkHealth::kUnsteady);

  constexpr std::uint32_t kFrozenMs = 40'000;
  now_ms += kFrozenMs;
  watch.NoteLoop(now_ms);
  CHECK(watch.Health(now_ms) == LinkHealth::kSteady);

  // ⚠ ET LA GARDE REPREND : le silence qui suit le gel se compte, et finit
  // par condamner une liaison vraiment morte.
  now_ms = RunSilent(watch, now_ms, now_ms + kLinkSilentLossMs);
  CHECK(watch.Health(now_ms) == LinkHealth::kLost);
}

TEST_CASE("Garde de liaison : un écart sous le seuil de gel reste un silence", "[net][liaison]") {
  // ⚠ LE SEUIL DE GEL NE DOIT PAS TOUT EXCUSER : une image lente reste une
  // image, et le silence qu'elle couvre compte.
  LinkWatch watch;
  watch.Restart(kStartMs);
  std::uint32_t now_ms = kStartMs;
  while (now_ms < kStartMs + kLinkSilentLossMs) {
    now_ms += kLoopStallMs;
    watch.NoteLoop(now_ms);
  }
  CHECK(watch.Health(now_ms) == LinkHealth::kLost);
}

TEST_CASE("Garde de liaison : un départ neuf repart de zéro", "[net][liaison]") {
  LinkWatch watch;
  watch.Restart(kStartMs);
  std::uint32_t now_ms = RunSilent(watch, kStartMs, kStartMs + kLinkSilentLossMs);
  REQUIRE(watch.Health(now_ms) == LinkHealth::kLost);

  // Le changement de serveur de zone, la carte chargée : l'horloge repart.
  watch.Restart(now_ms);
  CHECK(watch.Health(now_ms) == LinkHealth::kSteady);
}

TEST_CASE("Garde de liaison : un passage de zone sans réponse a une fin", "[net][liaison]") {
  // 🔴 L'ENTRÉE SUR UN AUTRE SERVEUR N'EXPIRE PAS D'ELLE-MÊME : sans cette borne,
  // un serveur qui accepte puis se tait laisse la partie muette pour toujours.
  CHECK_FALSE(ro::net::ZoneMoveOverdue(kStartMs, kStartMs));
  CHECK_FALSE(ro::net::ZoneMoveOverdue(kStartMs, kStartMs + kLinkSilentLossMs - kFrameMs));
  CHECK(ro::net::ZoneMoveOverdue(kStartMs, kStartMs + kLinkSilentLossMs));
  // ⚠ UN INSTANT DANS LE DÉSORDRE NE COMPTE PAS POUR UN DÉLAI.
  CHECK_FALSE(ro::net::ZoneMoveOverdue(kStartMs, kStartMs - kFrameMs));
}
