// L'étage du champ de bataille. Voir l'en-tête.

#include "game/battleground_stage.h"

#include <string>
#include <utility>

#include "ro/core/i18n.h"
#include "ro/core/log.h"
#include "ro/core/printed.h"
#include "ro/net/battleground.h"
#include "ro/sim/world.h"

#include "game/battleground_bug_report.h"
#include "game/battleground_rules.h"

namespace moonclient {
namespace {

using ro::sim::EntryQueueNoticeKind;
using ro::sim::EntryQueuePhase;
using ro::ui::EntryQueueVerb;

// LES AVIS QUI S'IMPOSENT DANS LA BOÎTE DE RÉSULTAT, en plus du chat : les
// refus que le serveur explique, et l'annulation qu'il refuse. Le reste — en
// file, sorti, entré, ou refusé avec sa raison déjà écrite dans le chat — ne
// fait qu'une ligne.
[[nodiscard]] bool ShownInBox(EntryQueueNoticeKind kind) {
  switch (kind) {
    case EntryQueueNoticeKind::kQueueFinished:
    case EntryQueueNoticeKind::kUnknownName:
    case EntryQueueNoticeKind::kUnknownType:
    case EntryQueueNoticeKind::kTooMany:
    case EntryQueueNoticeKind::kLevel:
    case EntryQueueNoticeKind::kDuplicate:
    case EntryQueueNoticeKind::kReconnect:
    case EntryQueueNoticeKind::kLeaderOnly:
    case EntryQueueNoticeKind::kJobRefused:
    case EntryQueueNoticeKind::kCancelRefused:
    case EntryQueueNoticeKind::kNoAnswer:
      return true;
    case EntryQueueNoticeKind::kQueued:
    case EntryQueueNoticeKind::kRefusedElsewhere:
    case EntryQueueNoticeKind::kCancelled:
    case EntryQueueNoticeKind::kDeclined:
    case EntryQueueNoticeKind::kAdmitted:
      return false;
  }
  return false;
}

// Le type d'inscription d'un verbe des fenêtres ; aucun pour les autres.
[[nodiscard]] std::optional<ro::sim::EntryQueueApplyKind> ApplyKindOf(EntryQueueVerb verb) {
  switch (verb) {
    case EntryQueueVerb::kApplySolo:
      return ro::sim::EntryQueueApplyKind::kSolo;
    case EntryQueueVerb::kApplyParty:
      return ro::sim::EntryQueueApplyKind::kParty;
    case EntryQueueVerb::kApplyGuild:
      return ro::sim::EntryQueueApplyKind::kGuild;
    case EntryQueueVerb::kNothing:
    case EntryQueueVerb::kCancel:
    case EntryQueueVerb::kAccept:
    case EntryQueueVerb::kDecline:
    case EntryQueueVerb::kNoticeSeen:
      return std::nullopt;
  }
  return std::nullopt;
}

// UNE DEMANDE QUI N'A PAS PU PARTIR : le journal en garde la cause, le joueur
// lit qu'il peut réessayer.
[[nodiscard]] std::string SendFailure(const ro::core::Error& error) {
  ro::core::LogWarning("champ de bataille : envoi impossible ({})", ro::core::Describe(error));
  return ro::core::Tr("La demande n'a pas pu partir : réessayez dans un instant.");
}

}  // namespace

void BattlegroundStage::StartSession(BattlegroundWindows& windows) {
  // ⚠ LA TABLE DES TERRAINS RESTE : elle vient de l'installation, qui ne
  // change pas d'une partie à l'autre.
  queue_ = ro::sim::EntryQueue();
  seen_serial_ = queue_.phase_serial();
  was_queued_ = false;
  notices_.clear();
  sounds_.clear();
  notice_shown_.clear();
  bug_report_.reset();
  item_gesture_ = ro::ui::ChatLinkAsk();
  windows = BattlegroundWindows();
}

ro::net::FrameOutcome BattlegroundStage::ApplyFrame(const ro::net::Frame& frame,
                                                    ro::sim::World& world, std::uint32_t now_ms) {
  if (const ro::net::FrameOutcome queued = ro::net::ApplyEntryQueueFrame(frame, queue_, now_ms);
      queued != ro::net::FrameOutcome::kUnhandled) {
    return queued;
  }
  return ro::net::ApplyBattlefieldFrame(frame, world, banned_, now_ms,
                                        talk_mute_ != nullptr ? *talk_mute_ : ro::sim::TalkMute{});
}

void BattlegroundStage::Advance(BattlegroundWindows& windows, const BattlegroundContext& context) {
  EnsureTable(context.vfs);
  // L'échéance d'une inscription restée sans réponse tombe ici, avant que la
  // réponse de l'interface ne relance une demande.
  queue_.Advance(context.now_ms);
  ApplyAnswer(windows, context);
  // L'EFFECTIF SE REDEMANDE À SON RYTHME, fenêtre fermée comprise : la
  // pastille du menu et « en attente » rouverte le montrent à jour.
  if (context.link_ready && queue_.RankingDue(context.now_ms)) {
    if (const ro::core::Result<void> asked =
            ro::net::RequestEntryQueueRanking(context.link, queue_.field_name());
        !asked) {
      ro::core::LogWarning("champ de bataille : effectif non demandé ({})",
                           ro::core::Describe(asked.error()));
    }
    // ⚠ NOTÉE MÊME EN ÉCHEC : un nom que le fil refuse le refuserait à chaque
    // image.
    queue_.NoteRankingAsked(context.now_ms);
  }
  CollectQueueNews();
  FollowTransitions(windows);
  ComposeView(windows, context);
}

void BattlegroundStage::RequestOpen(BattlegroundWindows& windows) {
  // UNE BASCULE, comme les autres icônes : l'icône paraît enfoncée quand la
  // liste OU « en attente » est ouverte, et son clic ferme alors les deux.
  // Sinon, elle ouvre « en attente » en file, la liste hors file.
  if (windows.window.open || windows.window.standby_open) {
    windows.window.open = false;
    windows.window.standby_open = false;
    return;
  }
  if (queue_.queued()) {
    windows.window.standby_open = true;
    return;
  }
  windows.window.open = true;
}

std::vector<std::string> BattlegroundStage::TakeNotices() { return std::exchange(notices_, {}); }

std::optional<BugReportContext> BattlegroundStage::TakeBugReport() noexcept {
  return std::exchange(bug_report_, std::nullopt);
}

ro::ui::ChatLinkAsk BattlegroundStage::TakeItemGesture() noexcept {
  return std::exchange(item_gesture_, ro::ui::ChatLinkAsk());
}

std::vector<InterfaceSoundFile> BattlegroundStage::TakeSounds() {
  return std::exchange(sounds_, {});
}

void BattlegroundStage::EnsureTable(ro::vfs::Vfs* vfs) {
  if (table_tried_ || vfs == nullptr) {
    return;
  }
  table_tried_ = true;
  const ro::core::Result<std::vector<std::uint8_t>> bytes = vfs->Read(kBattlegroundFilePath);
  if (!bytes) {
    ro::core::LogWarning("champ de bataille : {} illisible ({})", kBattlegroundFilePath,
                         ro::core::Describe(bytes.error()));
    return;
  }
  ro::core::Result<BattlegroundFile> read = BattlegroundFile::Parse(
      std::string_view(reinterpret_cast<const char*>(bytes->data()), bytes->size()));
  if (!read) {
    ro::core::LogWarning("champ de bataille : {}", ro::core::Describe(read.error()));
    return;
  }
  for (const std::string& problem : read->problems()) {
    ro::core::LogWarning("champ de bataille : {} : {}", kBattlegroundFilePath, problem);
  }
  table_ = std::move(*read);
}

void BattlegroundStage::ApplyAnswer(BattlegroundWindows& windows,
                                    const BattlegroundContext& context) {
  const ro::ui::EntryQueueAction action = std::exchange(windows.answer, {});
  if (action.report_bug) {
    const bool chosen = table_.has_value() && action.field_rank < table_->fields().size();
    bug_report_ = BattlegroundBugReport(
        BattlegroundBugScene{.queue = &queue_,
                             .battlefield = &context.world.battlefield(),
                             .field = chosen ? &table_->fields()[action.field_rank] : nullptr,
                             .table_loaded = table_.has_value(),
                             .table_fields = table_.has_value() ? table_->fields().size() : 0,
                             .map_name = context.world.map_name(),
                             .on_battlefield = context.world.map_mode().battlefield,
                             .link_ready = context.link_ready,
                             .now_ms = context.now_ms});
  }
  item_gesture_ = action.link;
  if (action.verb == EntryQueueVerb::kNoticeSeen) {
    notice_shown_.clear();
    return;
  }

  if (const std::optional<ro::sim::EntryQueueApplyKind> kind = ApplyKindOf(action.verb);
      kind.has_value()) {
    // Le rang désigne la liste que l'image précédente montrait : celle de la
    // table, qui ne se relit pas.
    if (!table_.has_value() || action.field_rank >= table_->fields().size()) {
      return;
    }
    const BattlegroundField& field = table_->fields()[action.field_rank];
    // 🔴 REJUGÉE À LA RÉCEPTION : le monde a pu bouger depuis la vue. Refusée,
    // la boîte dit pourquoi ; acceptée sans le « oui » de la confirmation, la
    // demande ne part pas — le joueur n'a pas vu la question.
    if (const char* refusal = RefusalOf(*kind, field, context); refusal != nullptr) {
      notice_shown_ = refusal;
      return;
    }
    if (!action.confirmed) {
      return;
    }
    // 🔴 LE NOM QUI PART EST CELUI DU SERVEUR, jamais celui qu'on lit : un nom
    // traduit désignerait un terrain qu'il ne connaît pas.
    if (const ro::core::Result<void> sent =
            ro::net::RequestEntryQueueApply(context.link, *kind, field.server_name);
        !sent) {
      notice_shown_ = SendFailure(sent.error());
      return;
    }
    queue_.NoteApplied(field.server_name, *kind, context.now_ms);
    return;
  }

  switch (action.verb) {
    case EntryQueueVerb::kCancel:
      // 🔴 PENDANT UNE INVITATION, LE SERVEUR IGNORE L'ANNULATION : elle ne part
      // pas, et le joueur apprend qu'il doit répondre.
      if (queue_.phase() == EntryQueuePhase::kInvited ||
          queue_.phase() == EntryQueuePhase::kAccepted) {
        notice_shown_ = ro::core::Tr("Le terrain est prêt : entrez ou refusez.");
        return;
      }
      if (queue_.phase() != EntryQueuePhase::kQueued) {
        return;
      }
      if (!context.link_ready) {
        notice_shown_ = ApplyRefusal(ro::sim::EntryQueueApplyKind::kSolo, ApplyFacts{});
        return;
      }
      if (const ro::core::Result<void> sent =
              ro::net::RequestEntryQueueCancel(context.link, queue_.field_name());
          !sent) {
        notice_shown_ = SendFailure(sent.error());
        return;
      }
      queue_.NoteCancelSent();
      return;
    case EntryQueueVerb::kAccept:
    case EntryQueueVerb::kDecline: {
      if (queue_.phase() != EntryQueuePhase::kInvited) {
        return;
      }
      if (!context.link_ready) {
        notice_shown_ = ApplyRefusal(ro::sim::EntryQueueApplyKind::kSolo, ApplyFacts{});
        return;
      }
      const bool accept = action.verb == EntryQueueVerb::kAccept;
      if (const ro::core::Result<void> sent = ro::net::AnswerLobbyAdmission(
              context.link, accept, queue_.field_name(), queue_.lobby_name());
          !sent) {
        notice_shown_ = SendFailure(sent.error());
        return;
      }
      queue_.NoteInvitationAnswered(accept, context.now_ms);
      return;
    }
    case EntryQueueVerb::kNothing:
    case EntryQueueVerb::kApplySolo:
    case EntryQueueVerb::kApplyParty:
    case EntryQueueVerb::kApplyGuild:
    case EntryQueueVerb::kNoticeSeen:
      return;
  }
}

const char* BattlegroundStage::RefusalOf(ro::sim::EntryQueueApplyKind kind,
                                         const BattlegroundField& field,
                                         const BattlegroundContext& context) {
  const ro::sim::World& world = context.world;
  const std::optional<ro::sim::GuildSummary>& guild_summary = context.guild.summary();
  return ApplyRefusal(
      kind, ApplyFacts{.on_battlefield = world.map_mode().battlefield,
                       .apply_delayed = HasApplyDelay(world.player_statuses()),
                       .in_party = context.party.in_party(),
                       .party_leader = context.party.AmILeader(world.player_id()),
                       .party_online = OnlinePartyMembers(context.party.members()),
                       .in_guild = context.guild.in_guild(),
                       .guild_master = context.guild.IsMaster(context.own_char_id),
                       .guild_online = guild_summary.has_value() ? guild_summary->online_count : 0,
                       .team_most = field.team_max,
                       .link_ready = context.link_ready});
}

void BattlegroundStage::CollectQueueNews() {
  for (const ro::sim::EntryQueueNotice& notice : queue_.TakeNotices()) {
    const char* said = NoticeText(notice.kind);
    std::string line;
    // LE TERRAIN EN TÊTE, sous le nom que le joueur lit. ⚠ SAUF L'ENTRÉE
    // ACCORDÉE : elle nomme la carte, que la phrase n'a pas besoin de dire.
    if (!notice.field_name.empty() && notice.kind != EntryQueueNoticeKind::kAdmitted) {
      line = ro::core::Printed(ro::core::Tr("« %s » : %s"),
                               std::string(DisplayNameOf(notice.field_name)).c_str(), said);
    } else {
      line = said;
    }
    if (ShownInBox(notice.kind)) {
      notice_shown_ = line;
    }
    notices_.push_back(std::move(line));
  }
  for (const ro::sim::EntryQueueSound sound : queue_.TakeSounds()) {
    sounds_.push_back(SoundFileOf(sound));
  }
}

void BattlegroundStage::FollowTransitions(BattlegroundWindows& windows) {
  if (queue_.phase_serial() == seen_serial_) {
    return;
  }
  seen_serial_ = queue_.phase_serial();
  const bool queued = queue_.queued();
  // L'ENTRÉE EN FILE — notre demande acceptée, ou un chef qui nous inscrit — :
  // la liste a fait son office, « en attente » prend sa place.
  if (queued && !was_queued_) {
    windows.window.open = false;
    windows.window.standby_open = true;
  }
  // HORS FILE, « en attente » n'a plus rien à montrer. ⚠ LA LISTE RESTE : un
  // refus laisse choisir un autre terrain.
  if (!queued && queue_.phase() != EntryQueuePhase::kApplying) {
    windows.window.standby_open = false;
  }
  was_queued_ = queued;
}

std::string_view BattlegroundStage::DisplayNameOf(std::string_view server_name) const {
  if (table_.has_value()) {
    if (const BattlegroundField* field = table_->FindByServerName(server_name); field != nullptr) {
      if (const std::string_view name = field->name.In(ro::core::CurrentLanguage());
          !name.empty()) {
        return name;
      }
    }
  }
  return server_name;
}

}  // namespace moonclient
