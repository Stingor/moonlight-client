// Ce que le serveur seul sait d'un objet. Voir l'en-tête pour la forme des
// trames.

#include "ro/net/item_facts.h"

#include <algorithm>
#include <string>
#include <utility>
#include <vector>

#include "ro/core/byte_reader.h"
#include "ro/core/text.h"
#include "ro/net/player_settings.h"
#include "ro/proto/manual_packets.h"
#include "ro/proto/map_packets.h"

namespace ro::net {
namespace {

// Ce que l'octet `src` dit d'une source : une récompense de MVP.
constexpr std::uint8_t kSourceMvpReward = 1;
// Ce que l'octet `is_skill` vaut pour un objet.
constexpr std::uint8_t kAsksAboutItem = 0;
// Ce que `status` vaut quand le serveur connaît l'objet.
constexpr std::uint8_t kScriptsKnown = 0;
// Une source porte au moins [mob:4][rate:4][boss:1][src:1][namelen:1].
constexpr std::size_t kSourceLeastSize = 11;
// Un membre de combo porte au moins [id:4][namelen:1].
constexpr std::size_t kMemberLeastSize = 5;

// Un nom du JEU, donc de sa page de codes.
//
// 🔴 TRONQUÉ AU PREMIER NUL. La longueur annoncée est celle du nom de la base
// du serveur, bornée à vingt-quatre octets, et ce nom porte son remplissage :
// « Poring » arrivait suivi de dix-huit NUL, dessinés en glyphes « caractère
// manquant ». La longueur est toujours consommée entière.
[[nodiscard]] std::string NameOf(core::ByteReader& reader, std::size_t length) {
  return core::Utf8FromCp1252(std::string(reader.FixedString(length)));
}

// Une chaîne préfixée de sa longueur sur seize bits, gardée TELLE QUELLE : un
// script est du code, et ses octets s'affichent comme le serveur les a écrits.
[[nodiscard]] bool ReadScript(core::ByteReader& reader, std::string& into) {
  if (reader.remaining() < 2) {
    return false;
  }
  const std::size_t length = reader.U16();
  if (reader.remaining() < length) {
    return false;
  }
  const std::span<const std::uint8_t> raw = reader.Bytes(length);
  into.assign(reinterpret_cast<const char*>(raw.data()), raw.size());
  return reader.ok();
}

[[nodiscard]] FrameOutcome ApplySourcesFrame(const Frame& frame, sim::ItemFacts& facts) {
  proto::ZcMoonlightTechdata packet;
  if (!proto::Decode(frame.bytes, packet)) {
    return FrameOutcome::kUndecodable;
  }
  // ⚠ LA RÉPONSE D'UNE COMPÉTENCE PASSE PAR LE MÊME OPCODE : elle n'est pas
  // pour nous, et `skill_facts` la lit.
  if (packet.is_skill != kAsksAboutItem) {
    return FrameOutcome::kUnhandled;
  }
  core::ByteReader reader(frame.bytes);
  reader.Seek(kItemSourcesFixedSize);
  sim::ItemDropSources told;
  // ⚠ UNE RÉPONSE SANS EN-TÊTE DE LISTE EST UNE LISTE VIDE : c'est ainsi que le
  // serveur dit « aucune source ».
  if (reader.remaining() >= 3) {
    const std::size_t count = reader.U8();
    told.truncated = reader.U8() != 0;
    told.treasure_excluded = reader.U8();
    for (std::size_t rank = 0; rank < count; ++rank) {
      // 🔴 LE COMPTE EST CRU, MAIS PAS SUR PAROLE : une trame tronquée en
      // annoncerait plus qu'elle n'en porte.
      if (reader.remaining() < kSourceLeastSize) {
        break;
      }
      sim::ItemDropSource source;
      source.monster_id = reader.U32();
      source.rate_hundredths = reader.U32();
      source.boss_rank = reader.U8();
      source.mvp_reward = reader.U8() == kSourceMvpReward;
      const std::size_t length = reader.U8();
      if (reader.remaining() < length) {
        break;
      }
      source.name = NameOf(reader, length);
      told.sources.push_back(std::move(source));
    }
  }
  if (!reader.ok()) {
    return FrameOutcome::kUndecodable;
  }
  facts.TellSources(packet.id, static_cast<sim::ItemSourceScope>(packet.scope), std::move(told));
  return FrameOutcome::kApplied;
}

[[nodiscard]] FrameOutcome ApplyScriptsFrame(const Frame& frame, sim::ItemFacts& facts) {
  proto::ZcMoonlightItemscript packet;
  if (!proto::Decode(frame.bytes, packet)) {
    return FrameOutcome::kUndecodable;
  }
  sim::ItemScripts told;
  told.status = packet.status;
  if (packet.status == kScriptsKnown) {
    core::ByteReader reader(frame.bytes);
    reader.Seek(kItemScriptsFixedSize);
    // ⚠ UNE TRAME COUPÉE GARDE CE QU'ELLE PORTE : les trois scripts lus restent
    // vrais, même si les combos qui suivent manquent.
    if (ReadScript(reader, told.main) && ReadScript(reader, told.equip) &&
        ReadScript(reader, told.unequip) && reader.remaining() >= 1) {
      const std::size_t combos = reader.U8();
      for (std::size_t rank = 0; rank < combos && reader.remaining() >= 1; ++rank) {
        sim::ItemCombo combo;
        const std::size_t members = reader.U8();
        bool whole = true;
        for (std::size_t member = 0; member < members; ++member) {
          if (reader.remaining() < kMemberLeastSize) {
            whole = false;
            break;
          }
          sim::ItemComboMember part;
          part.item_id = reader.U32();
          const std::size_t length = reader.U8();
          if (reader.remaining() < length) {
            whole = false;
            break;
          }
          part.name = NameOf(reader, length);
          combo.members.push_back(std::move(part));
        }
        if (!whole || !ReadScript(reader, combo.script)) {
          break;
        }
        told.combos.push_back(std::move(combo));
      }
    }
  }
  facts.TellScripts(packet.id, std::move(told));
  return FrameOutcome::kApplied;
}

}  // namespace

FrameOutcome ApplyItemFactsFrame(const Frame& frame, sim::ItemFacts& facts) {
  if (frame.opcode == proto::ZcMoonlightSettings::kOpcode) {
    // ⚠ `kIgnored` : les réglages eux-mêmes sont l'affaire du monde, qui les lit
    // de son côté. On n'y prend que la liste.
    const std::vector<PlayerSetting> settings = ReadPlayerSettings(frame.bytes);
    if (settings.empty()) {
      return FrameOutcome::kUndecodable;
    }
    std::vector<std::uint32_t> listed;
    for (const PlayerSetting& setting : settings) {
      // Zéro est « efface » : la suite reconstruit la liste.
      if (setting.id == kSettingAutolootAdd && setting.value != 0 &&
          std::ranges::find(listed, setting.value) == listed.end()) {
        listed.push_back(setting.value);
      }
    }
    facts.TellAutoloot(std::move(listed));
    return FrameOutcome::kIgnored;
  }
  if (frame.opcode == proto::ZcAckReqnameBygid::kOpcode) {
    proto::ZcAckReqnameBygid packet;
    if (!proto::Decode(frame.bytes, packet)) {
      return FrameOutcome::kUndecodable;
    }
    facts.TellCreator(packet.CID, core::Utf8FromCp1252(packet.name));
    return FrameOutcome::kApplied;
  }
  if (frame.opcode == proto::ZcMoonlightTechdata::kOpcode) {
    return ApplySourcesFrame(frame, facts);
  }
  if (frame.opcode == proto::ZcMoonlightItemscript::kOpcode) {
    return ApplyScriptsFrame(frame, facts);
  }
  return FrameOutcome::kUnhandled;
}

core::Result<void> RequestItemSources(Connection& link, std::uint32_t item_id,
                                      sim::ItemSourceScope scope) {
  proto::CzMoonlightReqTechdata request;
  // ⚠ LA LONGUEUR S'ÉCRIT : le serveur la LIT.
  request.packetLength = static_cast<std::int16_t>(proto::CzMoonlightReqTechdata::kWireSize);
  request.id = item_id;
  request.is_skill = kAsksAboutItem;
  request.scope = static_cast<std::uint8_t>(scope);
  link.Send(proto::Encode(request));
  return {};
}

void RequestAutoloot(Connection& link, std::uint32_t item_id, bool listed) {
  SendPlayerSetting(link, listed ? kSettingAutolootAdd : kSettingAutolootRemove, item_id);
}

core::Result<void> RequestCreatorName(Connection& link, std::uint32_t character_id) {
  proto::CreatorNameRequest request;
  request.character_id = character_id;
  auto frame = proto::Encode(request);
  if (!frame) {
    return std::unexpected(frame.error());
  }
  link.Send(*frame);
  return {};
}

core::Result<void> RequestItemScripts(Connection& link, std::uint32_t item_id) {
  proto::CzMoonlightReqItemscript request;
  request.packetLength = static_cast<std::int16_t>(proto::CzMoonlightReqItemscript::kWireSize);
  request.id = item_id;
  link.Send(proto::Encode(request));
  return {};
}

}  // namespace ro::net
