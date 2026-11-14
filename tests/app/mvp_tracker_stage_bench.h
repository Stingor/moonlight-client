#pragma once

// LE BANC DES ESSAIS DE L'ÉTAGE DU CARNET MVP : un serveur en boucle locale,
// la liaison, la page de réglages, l'étage et ses fenêtres, et les trames du
// serveur écrites à la main, aux décalages du fil.

#include <array>
#include <cstddef>
#include <cstdint>
#include <filesystem>
#include <fstream>
#include <optional>
#include <span>
#include <sstream>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

#include <catch2/catch_test_macros.hpp>

#include "ro/net/connection.h"
#include "ro/net/mvp_tracker.h"
#include "ro/proto/map_packets.h"
#include "ro/render/actor_wardrobe.h"
#include "ro/render/sprite_catalog.h"
#include "ro/sim/whisper.h"
#include "ro/vfs/vfs.h"

#include "mvp/mvp_tracker_stage.h"
#include "support/loopback_link.h"
#include "support/loopback_server.h"
#include "support/temp_directory.h"
#include "support/wire_bytes.h"

namespace mvp_tracker_stage_bench {

using moonclient::MvpLink;
using moonclient::MvpTrackerContext;
using moonclient::MvpTrackerOptions;
using moonclient::MvpTrackerRow;
using moonclient::MvpTrackerRowId;
using moonclient::MvpTrackerStage;
using moonclient::MvpTrackerWindows;
using ro::net::Frame;
using ro::net::FrameOutcome;
using ro::net::MvpCommand;
using ro::test::LoopbackServer;
using ro::test_support::PutU16;
using ro::test_support::PutU32;
using ro::test_support::TempDirectory;

using Bytes = std::vector<std::uint8_t>;

inline constexpr std::int64_t kLocalS = 1700000000;
inline constexpr std::uint32_t kNowMs = 50000;
inline constexpr int kSilenceMs = 150;
inline constexpr std::uint16_t kStateOpcode = ro::proto::ZcMoonlightMvpState::kOpcode;
inline constexpr std::uint16_t kGroupOpcode = ro::proto::ZcMoonlightMvpGroup::kOpcode;
inline constexpr std::size_t kHeadBytes = 4;

inline void PutU8(Bytes& into, std::uint8_t value) { into.push_back(value); }

inline void PutI64(Bytes& into, std::int64_t value) {
  const auto raw = static_cast<std::uint64_t>(value);
  PutU32(into, static_cast<std::uint32_t>(raw & 0xFFFFFFFFU));
  PutU32(into, static_cast<std::uint32_t>(raw >> 32U));
}

inline void PutText(Bytes& into, std::string_view text, std::size_t width) {
  for (std::size_t at = 0; at < width; ++at) {
    into.push_back(at < text.size() ? static_cast<std::uint8_t>(text[at]) : std::uint8_t{0});
  }
}

[[nodiscard]] inline Frame Framed(std::uint16_t opcode, Bytes bytes) {
  bytes[2] = static_cast<std::uint8_t>(bytes.size() & 0xFFU);
  bytes[3] = static_cast<std::uint8_t>(bytes.size() >> 8U);
  return Frame{.opcode = opcode, .bytes = std::move(bytes)};
}

[[nodiscard]] inline Bytes StateHead(std::uint8_t kind, std::uint16_t count) {
  Bytes bytes;
  PutU16(bytes, kStateOpcode);
  PutU16(bytes, 0);
  PutU8(bytes, kind);
  PutI64(bytes, kLocalS);
  PutU16(bytes, count);
  return bytes;
}

struct Slot {
  std::uint16_t slot_id = 0;
  std::uint16_t mob_id = 0;
  std::uint32_t delay1_ms = 0;
  std::uint32_t delay2_ms = 0;
  std::string map;
  std::string name;
};

[[nodiscard]] inline Frame Catalogue(const std::vector<Slot>& slots) {
  Bytes bytes = StateHead(0, static_cast<std::uint16_t>(slots.size()));
  for (const Slot& slot : slots) {
    PutU16(bytes, slot.slot_id);
    PutU16(bytes, slot.mob_id);
    PutU8(bytes, 0);
    PutU32(bytes, slot.delay1_ms);
    PutU32(bytes, slot.delay2_ms);
    PutU16(bytes, 200);
    PutU16(bytes, 200);
    PutText(bytes, slot.map, ro::net::kMvpMapNameBytes);
    PutText(bytes, slot.name, ro::net::kMvpNameBytes);
  }
  return Framed(kStateOpcode, std::move(bytes));
}

struct Seen {
  std::uint16_t slot_id = 0;
  std::uint8_t source = 0;
  std::int64_t kill_s = 0;
  std::int64_t reported_s = 0;
  std::int16_t tomb_x = -1;
  std::int16_t tomb_y = -1;
  // Zéro : aucun instant exact.
  std::int64_t exact_s = 0;
};

// Kind 1 : l'instantané, qui remplace ; kind 2 : un delta, qui ajoute.
[[nodiscard]] inline Frame Observations(std::uint8_t kind, const std::vector<Seen>& seen) {
  Bytes bytes = StateHead(kind, static_cast<std::uint16_t>(seen.size()));
  for (const Seen& entry : seen) {
    PutU16(bytes, entry.slot_id);
    PutU8(bytes, entry.source);
    PutU16(bytes, 0);
    PutI64(bytes, entry.kill_s);
    PutI64(bytes, entry.exact_s);
    PutU16(bytes, static_cast<std::uint16_t>(entry.tomb_x));
    PutU16(bytes, static_cast<std::uint16_t>(entry.tomb_y));
    PutU32(bytes, 0);
    PutI64(bytes, entry.reported_s);
    PutText(bytes, "", ro::net::kMvpNameBytes);
  }
  return Framed(kStateOpcode, std::move(bytes));
}

[[nodiscard]] inline Frame Favorites(const std::vector<std::uint16_t>& slot_ids) {
  Bytes bytes = StateHead(3, static_cast<std::uint16_t>(slot_ids.size()));
  for (const std::uint16_t slot_id : slot_ids) {
    PutU16(bytes, slot_id);
  }
  return Framed(kStateOpcode, std::move(bytes));
}

// Kind 0 : le groupe, sans membre ; kind 1 : l'invitation.
[[nodiscard]] inline Frame Group(std::uint8_t kind, std::uint32_t group_id, std::string_view name) {
  Bytes bytes;
  PutU16(bytes, kGroupOpcode);
  PutU16(bytes, 0);
  PutU8(bytes, kind);
  PutU8(bytes, 0);
  PutU32(bytes, group_id);
  PutU32(bytes, 1);
  PutText(bytes, name, ro::net::kMvpGroupNameBytes);
  PutU8(bytes, 0);
  return Framed(kGroupOpcode, std::move(bytes));
}

// Kind 2 : le résultat d'une commande, qui ne dit pas à laquelle il répond.
[[nodiscard]] inline Frame Result(std::uint8_t code) {
  Bytes bytes;
  PutU16(bytes, kGroupOpcode);
  PutU16(bytes, 0);
  PutU8(bytes, 2);
  PutU8(bytes, code);
  PutU32(bytes, 0);
  PutU32(bytes, 0);
  PutText(bytes, "", ro::net::kMvpGroupNameBytes);
  PutU8(bytes, 0);
  return Framed(kGroupOpcode, std::move(bytes));
}

// Le résultat étiqueté d'un serveur qui sait nos queues : l'identifiant de la
// commande, et l'écho de son numéro.
[[nodiscard]] inline Frame TaggedResult(std::uint8_t code, std::uint32_t request_id,
                                        std::uint8_t command) {
  Frame frame = Result(code);
  PutU32(frame.bytes, request_id);
  PutU8(frame.bytes, command);
  return Framed(kGroupOpcode, std::move(frame.bytes));
}

// Une invitation qui dit d'où elle vient.
[[nodiscard]] inline Frame InvitationFrom(std::uint32_t group_id, std::uint8_t origin) {
  Frame frame = Group(1, group_id, "Les Chasseurs");
  PutU8(frame.bytes, origin);
  return Framed(kGroupOpcode, std::move(frame.bytes));
}

struct Member {
  std::uint32_t user_id = 0;
  std::string name;
};

// Le groupe et ses membres, tous en ligne ; `self_user_id` posé : la queue d'un
// serveur qui dit notre compte.
[[nodiscard]] inline Frame GroupWith(std::uint32_t group_id, std::uint32_t owner_user_id,
                                     const std::vector<Member>& members,
                                     std::optional<std::uint32_t> self_user_id = std::nullopt) {
  Bytes bytes;
  PutU16(bytes, kGroupOpcode);
  PutU16(bytes, 0);
  PutU8(bytes, 0);
  PutU8(bytes, 0);
  PutU32(bytes, group_id);
  PutU32(bytes, owner_user_id);
  PutText(bytes, "Chasse", ro::net::kMvpGroupNameBytes);
  PutU8(bytes, static_cast<std::uint8_t>(members.size()));
  for (const Member& member : members) {
    PutU32(bytes, member.user_id);
    PutU16(bytes, 99);
    PutU8(bytes, 1);
    PutText(bytes, member.name, ro::net::kMvpNameBytes);
  }
  if (self_user_id.has_value()) {
    PutU32(bytes, *self_user_id);
  }
  return Framed(kGroupOpcode, std::move(bytes));
}

struct Command {
  std::uint8_t cmd = 0;
  std::uint32_t a = 0;
  std::uint32_t b = 0;
  // Le texte, sans le suffixe d'identifiant.
  std::string text;
  // L'identifiant du suffixe ; zéro : la commande n'en portait pas.
  std::uint32_t request_id = 0;
};

[[nodiscard]] inline std::uint32_t U32At(const Bytes& bytes, std::size_t at) {
  return static_cast<std::uint32_t>(bytes[at]) | (static_cast<std::uint32_t>(bytes[at + 1]) << 8U) |
         (static_cast<std::uint32_t>(bytes[at + 2]) << 16U) |
         (static_cast<std::uint32_t>(bytes[at + 3]) << 24U);
}

// LE BANC : un serveur, la liaison, la page de réglages, l'étage et ses fenêtres.
struct Bench {
  LoopbackServer server;
  ro::net::Connection link;
  MvpTrackerOptions options;
  MvpTrackerStage stage;
  MvpTrackerWindows windows;

  // ⚠ LE BANC PART ÉTEINT, bien que la page parte allumée : chaque essai dit
  // quand il allume, et l'instantané demandé à l'allumage est son affaire.
  Bench() : link(Connect(server)) { Switch(false); }

  static ro::net::Connection Connect(LoopbackServer& listening) {
    REQUIRE(listening.listening());
    return ro::test::Connect(listening);
  }

  bool Advance(bool verified, std::uint32_t now_ms = kNowMs, std::int64_t local_s = kLocalS) {
    return stage.Advance(windows, MvpTrackerContext{.link = link,
                                                    .options = options,
                                                    .verified = verified,
                                                    .local_now_s = local_s,
                                                    .now_ms = now_ms,
                                                    .own_name = own_name});
  }

  // Le nom de notre personnage, passé à chaque avancée.
  std::string own_name;

  void Apply(const Frame& frame, std::uint32_t now_ms = kNowMs) {
    REQUIRE(stage.ApplyFrame(frame, kLocalS, now_ms) == FrameOutcome::kApplied);
  }

  void SwitchOn() { Switch(true); }

  // ALLUMÉ, POUR UN ESSAI QUI NE PORTE PAS SUR L'INSTANTANÉ : celui que le
  // carnet allumé redemande à chaque vérification n'est pas son objet, et
  // `Next` comme `Silent` le sautent.
  void SwitchOnPastSnapshots() {
    SwitchOn();
    skip_snapshots = true;
  }
  bool skip_snapshots = false;

  void Switch(bool on) {
    REQUIRE(options.Change({.changed = MvpTrackerRowId(MvpTrackerRow::kEnabled), .on = on}));
  }

  // La commande suivante, telle que le serveur la lit — l'instantané sauté
  // sous `skip_snapshots`.
  Command Next() {
    for (;;) {
      Command read = NextAny();
      if (!skip_snapshots || read.cmd != static_cast<std::uint8_t>(MvpCommand::kSnapshot)) {
        return read;
      }
    }
  }

  Command NextAny() {
    const Bytes head = server.Receive(kHeadBytes);
    REQUIRE(head.size() == kHeadBytes);
    REQUIRE(head[0] == 0x30);
    REQUIRE(head[1] == 0x0F);
    const std::size_t length =
        static_cast<std::size_t>(head[2]) | (static_cast<std::size_t>(head[3]) << 8U);
    REQUIRE(length >= ro::net::kMvpCommandFixedSize);
    const Bytes rest = server.Receive(length - kHeadBytes);
    REQUIRE(rest.size() == length - kHeadBytes);
    // LE SUFFIXE SE LIT COMME LE SERVEUR LE LIT : un NUL cinq octets avant la
    // fin, derrière le texte.
    std::size_t text_end = rest.size();
    std::uint32_t request_id = 0;
    if (rest.size() >= kTextAt + ro::net::kMvpCommandRequestIdSize &&
        rest[rest.size() - ro::net::kMvpCommandRequestIdSize] == 0) {
      text_end = rest.size() - ro::net::kMvpCommandRequestIdSize;
      request_id = U32At(rest, text_end + 1);
    }
    return Command{.cmd = rest[0],
                   .a = U32At(rest, 1),
                   .b = U32At(rest, 5),
                   .text = std::string(rest.begin() + kTextAt,
                                       rest.begin() + static_cast<std::ptrdiff_t>(text_end)),
                   .request_id = request_id};
  }

  // Où commence le texte, l'en-tête de quatre octets ôté.
  static constexpr std::size_t kTextAt = 9;

  [[nodiscard]] bool Silent() {
    for (;;) {
      const Bytes head = server.Receive(kHeadBytes, kSilenceMs);
      if (head.empty()) {
        return true;
      }
      if (!skip_snapshots || head.size() != kHeadBytes) {
        return false;
      }
      const std::size_t length =
          static_cast<std::size_t>(head[2]) | (static_cast<std::size_t>(head[3]) << 8U);
      if (length <= kHeadBytes) {
        return false;
      }
      const Bytes rest = server.Receive(length - kHeadBytes);
      if (rest.empty() || rest[0] != static_cast<std::uint8_t>(MvpCommand::kSnapshot)) {
        return false;
      }
    }
  }
};

inline constexpr std::uint8_t Raw(MvpCommand command) { return static_cast<std::uint8_t>(command); }

// La raison d'un refus, ou « offerte » quand il n'y en a pas.
[[nodiscard]] inline std::string Said(const char* refusal) {
  return refusal != nullptr ? std::string(refusal) : std::string("offerte");
}

// Un catalogue de trois créneaux, dont un scripté, et un groupe.
inline const std::vector<Slot> kSlots = {
    {.slot_id = 1,
     .mob_id = 1039,
     .delay1_ms = 3600000,
     .delay2_ms = 600000,
     .map = "gef_dun03",
     .name = "Baphomet"},
    {.slot_id = 2,
     .mob_id = 1038,
     .delay1_ms = 3600000,
     .delay2_ms = 600000,
     .map = "moc_pryd06",
     .name = "Osiris"},
    {.slot_id = 7, .mob_id = 0, .delay1_ms = 7200000, .map = "lhz_dun03"},
};

// LA RÉPONSE À L'INSTANTANÉ, d'un seul tenant comme le serveur l'écrit :
// catalogue, favoris, observations, groupe, puis l'invitation en attente s'il y
// en a une.
inline void ApplySnapshotReply(Bench& bench, std::uint32_t now_ms, std::uint32_t invite_group_id) {
  bench.Apply(Catalogue(kSlots), now_ms);
  bench.Apply(Favorites({}), now_ms);
  bench.Apply(Observations(1, {}), now_ms);
  bench.Apply(Group(0, 0, ""), now_ms);
  if (invite_group_id != 0) {
    bench.Apply(Group(1, invite_group_id, "Les Chasseurs"), now_ms);
  }
}

}  // namespace mvp_tracker_stage_bench
