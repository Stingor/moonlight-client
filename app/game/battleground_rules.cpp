// Les règles pures du champ de bataille. Voir l'en-tête.

#include "game/battleground_rules.h"

#include <algorithm>
#include <string_view>

#include "ro/core/i18n.h"
#include "ro/core/printed.h"

namespace moonclient {
namespace {

// Les sons de la file, sous le dossier des sons du jeu.
constexpr std::string_view kRequestSound = "se_btg_request.wav";
constexpr std::string_view kReadySound = "se_btg_ready.wav";
constexpr std::string_view kForwardSound = "se_btg_forward.wav";

// UN GROUPE ENTIER DOIT TENIR DANS UN CAMP : ses membres connectés partent
// tous en file. Un terrain qui ne dit pas sa taille ne refuse rien.
[[nodiscard]] bool OutnumbersTeam(int online, int team_most) {
  return team_most > 0 && online > team_most;
}

}  // namespace

bool HasApplyDelay(std::span<const ro::sim::ActiveStatus> player_statuses) {
  return std::ranges::any_of(player_statuses, [](const ro::sim::ActiveStatus& status) {
    return status.effect == ro::sim::kEfstEntryQueueApplyDelay;
  });
}

int OnlinePartyMembers(std::span<const ro::sim::PartyMember> members) {
  return static_cast<int>(std::ranges::count_if(
      members, [](const ro::sim::PartyMember& member) { return !member.offline; }));
}

const char* ApplyRefusal(ro::sim::EntryQueueApplyKind kind, const ApplyFacts& facts) {
  // ⚠ LA MÊME PHRASE QUE LES FENÊTRES DISENT d'un bouton éteint hors liaison.
  if (!facts.link_ready) {
    return ro::core::Tr(
        "Liaison avec le serveur en cours de vérification : réessayez dans un instant.");
  }
  if (facts.on_battlefield) {
    return ro::core::Tr("[Vous ne pouvez pas vous inscrire depuis cette carte.]");
  }
  if (facts.apply_delayed) {
    return ro::core::Tr("[Vous devez attendre environ 1 minute avant de vous inscrire.]");
  }
  switch (kind) {
    case ro::sim::EntryQueueApplyKind::kParty:
      if (!facts.in_party) {
        return ro::core::Tr("[Vous devez être dans une party.]");
      }
      if (!facts.party_leader) {
        return ro::core::Tr("[Seul le chef de la party peut inscrire.]");
      }
      if (OutnumbersTeam(facts.party_online, facts.team_most)) {
        return ro::core::Tr("[Trop de membres de la party sont connectés.]");
      }
      return nullptr;
    case ro::sim::EntryQueueApplyKind::kGuild:
      if (!facts.in_guild) {
        return ro::core::Tr("[Vous devez être dans une guilde.]");
      }
      if (!facts.guild_master) {
        return ro::core::Tr("[Seul le maître de guilde peut inscrire.]");
      }
      if (OutnumbersTeam(facts.guild_online, facts.team_most)) {
        return ro::core::Tr("[Trop de membres de la guilde sont connectés.]");
      }
      return nullptr;
    case ro::sim::EntryQueueApplyKind::kSolo:
      return nullptr;
  }
  return nullptr;
}

const char* NoticeText(ro::sim::EntryQueueNoticeKind kind) {
  switch (kind) {
    case ro::sim::EntryQueueNoticeKind::kQueued:
      return ro::core::Tr("Inscription acceptée : vous êtes en file.");
    case ro::sim::EntryQueueNoticeKind::kRefusedElsewhere:
      return ro::core::Tr("Inscription refusée : le serveur en donne la raison dans le chat.");
    case ro::sim::EntryQueueNoticeKind::kQueueFinished:
      return ro::core::Tr("La file a pris fin : vous n'êtes plus inscrit.");
    case ro::sim::EntryQueueNoticeKind::kUnknownName:
      return ro::core::Tr("Inscription refusée : ce terrain est inconnu du serveur.");
    case ro::sim::EntryQueueNoticeKind::kUnknownType:
      return ro::core::Tr("Ce terrain n'accepte pas ce mode d'inscription.");
    case ro::sim::EntryQueueNoticeKind::kTooMany:
      return ro::core::Tr(
          "Inscription refusée : le nombre de joueurs ne convient pas à ce terrain.");
    case ro::sim::EntryQueueNoticeKind::kLevel:
      return ro::core::Tr("Inscription refusée : votre niveau ne convient pas à ce terrain.");
    case ro::sim::EntryQueueNoticeKind::kDuplicate:
      return ro::core::Tr("Vous êtes déjà en file.");
    case ro::sim::EntryQueueNoticeKind::kReconnect:
      return ro::core::Tr("Inscription refusée : reconnectez-vous, puis réessayez.");
    case ro::sim::EntryQueueNoticeKind::kLeaderOnly:
      return ro::core::Tr(
          "Inscription refusée : seul le chef de la party ou le maître de guilde inscrit son "
          "groupe.");
    case ro::sim::EntryQueueNoticeKind::kJobRefused:
      return ro::core::Tr("Inscription refusée : votre job n'est pas admis sur ce terrain.");
    case ro::sim::EntryQueueNoticeKind::kCancelled:
      return ro::core::Tr("Vous avez quitté la file.");
    case ro::sim::EntryQueueNoticeKind::kCancelRefused:
      return ro::core::Tr("Le serveur a refusé l'annulation : vous restez en file.");
    case ro::sim::EntryQueueNoticeKind::kDeclined:
      return ro::core::Tr("Invitation refusée : vous avez quitté la file.");
    case ro::sim::EntryQueueNoticeKind::kAdmitted:
      return ro::core::Tr("Entrée accordée : en route vers le champ de bataille.");
    case ro::sim::EntryQueueNoticeKind::kNoAnswer:
      return ro::core::Tr("Le serveur n'a pas répondu à l'inscription.");
  }
  return "";
}

// ⚠ UN GABARIT LITTÉRAL PAR CAS dans les deux textes qui suivent : le
// compilateur ne vérifie les arguments que d'un format qu'il voit.

std::string ConditionText(const BattlegroundField& field) {
  std::string text;
  if (field.level_min > 0 && field.level_max > 0) {
    text = ro::core::Printed(ro::core::Tr("Base level %d à %d"), field.level_min, field.level_max);
  } else if (field.level_min > 0) {
    text = ro::core::Printed(ro::core::Tr("Base level %d au moins"), field.level_min);
  } else if (field.level_max > 0) {
    text = ro::core::Printed(ro::core::Tr("Base level %d au plus"), field.level_max);
  }
  return text;
}

std::string TeamText(const BattlegroundField& field) {
  std::string text;
  if (field.team_min > 0 && field.team_min == field.team_max) {
    text = ro::core::Printed(ro::core::Tr("%d contre %d"), field.team_min, field.team_min);
  } else if (field.team_min > 0 && field.team_max > 0) {
    text = ro::core::Printed(ro::core::Tr("%d à %d par camp"), field.team_min, field.team_max);
  } else if (field.team_min > 0) {
    text = ro::core::Printed(ro::core::Tr("%d par camp au moins"), field.team_min);
  } else if (field.team_max > 0) {
    text = ro::core::Printed(ro::core::Tr("%d par camp au plus"), field.team_max);
  }
  return text;
}

InterfaceSoundFile SoundFileOf(ro::sim::EntryQueueSound sound) {
  std::string_view file;
  switch (sound) {
    case ro::sim::EntryQueueSound::kRequest:
      file = kRequestSound;
      break;
    case ro::sim::EntryQueueSound::kReady:
      file = kReadySound;
      break;
    case ro::sim::EntryQueueSound::kForward:
      file = kForwardSound;
      break;
  }
  return InterfaceSoundFile{.place = InterfaceSoundPlace::kSoundArchive, .path = std::string(file)};
}

}  // namespace moonclient
