// LES LIGNES DE PRÉSENCE : l'arène et le temps passé, comptés par le client
// depuis l'entrée que le serveur a dite, et une ligne par compte en arène.

#include "game/survivors_presence_labels.h"

#include <cstdint>
#include <string>
#include <string_view>
#include <vector>

#include <catch2/catch_test_macros.hpp>

#include "ro/net/survivors_presence.h"
#include "ro/sim/signboard.h"

using moonclient::PresenceLabel;
using moonclient::SurvivorsPresenceLabels;
using moonclient::SurvivorsPresenceSeen;

namespace {

constexpr std::uint32_t kAccountId = 2000123;
constexpr std::uint32_t kOtherAccountId = 2000042;
constexpr std::uint16_t kDesertId = 3;
// Entré il y a 7 minutes 42 secondes.
constexpr std::int64_t kEnteredMs = 1'000'000;
constexpr std::int64_t kNowMs = kEnteredMs + 462'000;

[[nodiscard]] SurvivorsPresenceSeen Seen(std::uint32_t account_id,
                                         ro::net::SurvivorsPresenceState state) {
  return SurvivorsPresenceSeen{
      .account_id = account_id, .state = state, .arena_id = kDesertId, .entered_at_ms = kEnteredMs};
}

}  // namespace

TEST_CASE("Présence survivors : la ligne dit l'arène et le temps passé", "[app][survivors]") {
  CHECK(PresenceLabel(Seen(kAccountId, ro::net::SurvivorsPresenceState::kPlaying),
                      "Désert de Sograt", kNowMs) == "En arène : Désert de Sograt, 07:42");
  CHECK(PresenceLabel(Seen(kAccountId, ro::net::SurvivorsPresenceState::kPaused),
                      "Désert de Sograt",
                      kNowMs) == "En pause dans l'arène : Désert de Sograt, 07:42");
  // Une arène que ce client ne connaît pas, et l'horloge avant l'entrée.
  CHECK(PresenceLabel(Seen(kAccountId, ro::net::SurvivorsPresenceState::kPlaying), "",
                      kEnteredMs - 1) == "En arène : Arène inconnue, 00:00");
}

TEST_CASE("Présence survivors : une ligne par compte en arène", "[app][survivors]") {
  const std::vector<SurvivorsPresenceSeen> seen = {
      Seen(kOtherAccountId, ro::net::SurvivorsPresenceState::kPlaying),
      Seen(kAccountId, ro::net::SurvivorsPresenceState::kPlaying)};
  SurvivorsPresenceLabels labels;
  labels.Compose(
      seen,
      [](std::uint16_t arena_id) {
        return arena_id == kDesertId ? std::string("Désert de Sograt") : std::string();
      },
      kNowMs);
  CHECK(labels.Of(kAccountId) == "En arène : Désert de Sograt, 07:42");
  CHECK(labels.Of(kOtherAccountId) == "En arène : Désert de Sograt, 07:42");
  CHECK(labels.Of(1).empty());

  // Refaites à l'image suivante : qui est sorti n'a plus de ligne.
  labels.Compose({}, {}, kNowMs);
  CHECK(labels.Of(kAccountId).empty());
}

TEST_CASE("Présence survivors : la bulle du salon de présence se tait", "[app][survivors]") {
  constexpr std::uint16_t kOpenRoomLimit = 20;
  const ro::sim::Signboard presence_room{.kind = ro::sim::SignboardKind::kChatRoom,
                                         .limit = moonclient::kPresenceRoomLimit};
  constexpr std::string_view kLine = "En arène : Désert de Sograt, 07:42";
  CHECK(moonclient::IsPresenceRoom(presence_room, kLine));
  // Hors de l'arène, le même salon reste celui d'un joueur.
  CHECK_FALSE(moonclient::IsPresenceRoom(presence_room, ""));
  // Un salon ouvert à plusieurs places, ou une échoppe, se peint toujours.
  ro::sim::Signboard open_room = presence_room;
  open_room.limit = kOpenRoomLimit;
  CHECK_FALSE(moonclient::IsPresenceRoom(open_room, kLine));
  ro::sim::Signboard shop = presence_room;
  shop.kind = ro::sim::SignboardKind::kVendingShop;
  CHECK_FALSE(moonclient::IsPresenceRoom(shop, kLine));
}
