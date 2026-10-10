// Les quêtes. Voir l'en-tête pour les paquets.

#include "ro/net/quest_log.h"

#include <algorithm>
#include <utility>

#include "ro/core/text.h"
#include "ro/proto/ledger_packets.h"
#include "ro/proto/map_packets.h"
#include "ro/proto/quest_packets.h"

namespace ro::net {
namespace {

[[nodiscard]] sim::QuestRecord QuestOf(const proto::QuestEntry& entry) {
  sim::QuestRecord quest{.quest_id = entry.quest_id,
                         .active = entry.active,
                         .started_unix_s = entry.started_unix_s,
                         .expires_unix_s = entry.expires_unix_s};
  quest.hunts.reserve(entry.hunts.size());
  for (const proto::QuestHuntEntry& hunt : entry.hunts) {
    quest.hunts.push_back(sim::QuestHunt{.hunt_id = hunt.hunt_id,
                                         .mob_type = hunt.mob_type,
                                         .mob_id = hunt.mob_id,
                                         .level_min = hunt.level_min,
                                         .level_max = hunt.level_max,
                                         .killed = hunt.killed,
                                         .total = hunt.total,
                                         .mob_name = core::Utf8FromWire(hunt.mob_name)});
  }
  return quest;
}

[[nodiscard]] sim::QuestRecord QuestOf(const proto::QuestAdded& added) {
  sim::QuestRecord quest{
      .quest_id = added.quest_id, .active = added.active, .expires_unix_s = added.expires_unix_s};
  quest.hunts.reserve(added.hunts.size());
  for (const proto::QuestAddedHunt& hunt : added.hunts) {
    quest.hunts.push_back(sim::QuestHunt{.hunt_id = hunt.hunt_id,
                                         .mob_type = hunt.mob_type,
                                         .mob_id = hunt.mob_id,
                                         .level_min = hunt.level_min,
                                         .level_max = hunt.level_max,
                                         .killed = hunt.killed,
                                         .mob_name = core::Utf8FromWire(hunt.mob_name)});
  }
  return quest;
}

// Le rang d'une chasse dans sa quête, ou la taille de la liste si elle n'y est pas.
[[nodiscard]] std::size_t RankOf(const sim::QuestRecord& quest, std::uint32_t hunt_id) {
  return static_cast<std::size_t>(
      std::ranges::find(quest.hunts, hunt_id, &sim::QuestHunt::hunt_id) - quest.hunts.begin());
}

}  // namespace

QuestFrameResult ApplyQuestFrame(const Frame& frame, sim::QuestLog& log) {
  QuestFrameResult result;
  switch (frame.opcode) {
    case proto::QuestList::kOpcode: {
      const auto list = proto::DecodeQuestList(frame.bytes);
      if (!list) {
        result.outcome = FrameOutcome::kUndecodable;
        return result;
      }
      std::vector<sim::QuestRecord> quests;
      quests.reserve(list->quests.size());
      for (const proto::QuestEntry& entry : list->quests) {
        quests.push_back(QuestOf(entry));
      }
      log.Replace(std::move(quests));
      result.replaced = true;
      result.outcome = FrameOutcome::kApplied;
      return result;
    }

    case proto::QuestAdded::kOpcode: {
      const auto added = proto::DecodeQuestAdded(frame.bytes);
      if (!added) {
        result.outcome = FrameOutcome::kUndecodable;
        return result;
      }
      log.Add(QuestOf(*added));
      result.added = added->quest_id;
      result.outcome = FrameOutcome::kApplied;
      return result;
    }

    case proto::QuestHuntTotals::kOpcode: {
      const auto totals = proto::DecodeQuestHuntTotals(frame.bytes);
      if (!totals) {
        result.outcome = FrameOutcome::kUndecodable;
        return result;
      }
      // LE PREMIER CHAMP EST L'IDENTIFIANT DE LA QUÊTE, comme le client d'origine
      // le lit ; un serveur plus ancien y met celui de la chasse. Les entrées
      // suivent l'ordre des objectifs : sous une quête, la n-ième va à la n-ième
      // chasse, ce qui range aussi une chasse générique, dont le monstre est nul.
      bool any_known = totals->entries.empty();
      for (std::size_t rank = 0; rank < totals->entries.size(); ++rank) {
        const proto::QuestHuntTotal& entry = totals->entries[rank];
        any_known |=
            log.Find(entry.quest_or_hunt_id) != nullptr
                ? log.SetHuntTotalAt(entry.quest_or_hunt_id, rank, entry.mob_id, entry.total,
                                     entry.killed)
                : log.SetHuntTotal(entry.quest_or_hunt_id, entry.mob_id, entry.total, entry.killed);
      }
      result.outcome = any_known ? FrameOutcome::kApplied : FrameOutcome::kIgnored;
      return result;
    }

    case proto::QuestHuntProgress::kOpcode: {
      const auto progress = proto::DecodeQuestHuntProgress(frame.bytes);
      if (!progress) {
        result.outcome = FrameOutcome::kUndecodable;
        return result;
      }
      for (const proto::QuestHuntCount& entry : progress->entries) {
        if (log.UpdateHunt(entry.quest_id, entry.hunt_id, entry.total, entry.killed)) {
          const sim::QuestRecord* quest = log.Find(entry.quest_id);
          result.progressed.push_back(QuestProgress{.quest_id = entry.quest_id,
                                                    .hunt_rank = RankOf(*quest, entry.hunt_id),
                                                    .killed = entry.killed,
                                                    .total = entry.total});
        }
      }
      result.outcome = FrameOutcome::kApplied;
      return result;
    }

    case proto::QuestRemoved::kOpcode: {
      const auto removed = proto::DecodeQuestRemoved(frame.bytes);
      if (!removed) {
        result.outcome = FrameOutcome::kUndecodable;
        return result;
      }
      // 🔴 LA RAISON SE PREND MÊME QUAND LE RETRAIT N'ABOUTIT PAS : elle ne vaut
      // que pour ce retrait-ci, et ne doit pas habiller le suivant.
      result.ending = log.TakeEnding(removed->quest_id);
      if (log.Remove(removed->quest_id)) {
        result.removed = removed->quest_id;
        result.outcome = FrameOutcome::kApplied;
      } else {
        result.outcome = FrameOutcome::kIgnored;
      }
      return result;
    }

    case proto::QuestActiveNotice::kOpcode: {
      const auto notice = proto::DecodeQuestActiveNotice(frame.bytes);
      if (!notice) {
        result.outcome = FrameOutcome::kUndecodable;
        return result;
      }
      if (log.SetActive(notice->quest_id, notice->active)) {
        result.toggled = notice->quest_id;
        result.outcome = FrameOutcome::kApplied;
      } else {
        result.outcome = FrameOutcome::kIgnored;
      }
      return result;
    }

    case proto::ZcMoonlightQuestEnd::kOpcode: {
      proto::ZcMoonlightQuestEnd end;
      if (!proto::Decode(frame.bytes, end)) {
        result.outcome = FrameOutcome::kUndecodable;
        return result;
      }
      // Une raison que ce client ne connaît pas ne dit rien : le retrait qui
      // suit se montrera comme celui d'un serveur qui ne la dit pas.
      if (end.quest_id <= 0 || !sim::IsKnownQuestEndReason(end.reason)) {
        result.outcome = FrameOutcome::kIgnored;
        return result;
      }
      const auto reason = static_cast<sim::QuestEndReason>(end.reason);
      const std::uint32_t next_quest_id =
          reason == sim::QuestEndReason::kReplaced && end.next_quest_id > 0
              ? static_cast<std::uint32_t>(end.next_quest_id)
              : 0;
      log.NoteEnding(static_cast<std::uint32_t>(end.quest_id),
                     sim::QuestEnding{.reason = reason, .next_quest_id = next_quest_id});
      result.outcome = FrameOutcome::kApplied;
      return result;
    }

    default:
      return result;
  }
}

core::Result<void> RequestQuestActive(Connection& link, std::uint32_t quest_id, bool active) {
  link.Send(proto::EncodeQuestActiveRequest({.quest_id = quest_id, .active = active}));
  return {};
}

}  // namespace ro::net
