// Les commandes du joueur qui ne touchent que la liaison, les options ou la
// conversation. Voir `player_command.h` pour la lecture, `game_player_commands`
// pour les autres.

#include <array>
#include <cstddef>
#include <format>
#include <string>
#include <utility>

#include "ro/core/i18n.h"
#include "ro/net/action.h"
#include "ro/net/party.h"
#include "ro/net/user_count.h"
#include "ro/sim/world.h"
#include "ro/ui/entity_bars.h"

#include "chat/chat_view.h"
#include "chat/emotion_commands.h"
#include "chat/setting_toggle_command.h"
#include "options/deal_refusal_options.h"
#include "options/percent.h"
#include "world/click_action.h"

namespace moonclient {
namespace {

using ro::core::Tr;

// L'ÉCHO D'UN RÉGLAGE, et de ce que le client du joueur énumère : le jaune pâle
// qu'il leur donne, en `0xRRGGBB`.
constexpr std::uint32_t kEchoColor = 0xFFFF64;

void Say(const ChatDispatch& to, std::string text, std::uint32_t said_ms, std::uint32_t color = 0) {
  to.world->RecordChat(ro::sim::ChatKind::kServer, 0, std::move(text), said_ms, color);
}

// LA FORME À SUIVRE, dite quand la ligne ne la respecte pas.
[[nodiscard]] const char* UsageOf(PlayerCommandKind kind) {
  switch (kind) {
    case PlayerCommandKind::kDeal:
      return Tr("/deal (nom du personnage) : propose un échange à un joueur visible");
    case PlayerCommandKind::kOrganizeParty:
      return Tr("/organize (nom du groupe) : fonde un groupe");
    case PlayerCommandKind::kInviteToParty:
      return Tr("/invite (nom du personnage) : invite dans le groupe");
    case PlayerCommandKind::kExpelFromParty:
      return Tr("/expel (nom du personnage) : exclut du groupe");
    case PlayerCommandKind::kCreateGuild:
      return Tr("/guild (nom de la guilde) : fonde une guilde");
    case PlayerCommandKind::kInviteToGuild:
      return Tr("/guildinvite (nom du personnage) : invite dans la guilde");
    case PlayerCommandKind::kBreakGuild:
      return Tr("/breakguild (nom de la guilde) : dissout la guilde");
    default:
      return "";
  }
}

// CE QUE `/help` ÉCRIT : les commandes que ce client prend en charge.
void SayHelp(const ChatDispatch& to, std::uint32_t said_ms) {
  const std::array<const char*, 12> lines = {
      Tr("Commandes du client :"),
      Tr("/w nom message, %message (groupe), $message (guilde), /gc, /cl, /bc : parler à "
         "quelqu'un ou à un canal"),
      Tr("Ctrl+Entrée : au groupe — Alt+Entrée : à la guilde"),
      Tr("/sit, /stand : s'asseoir ou se lever"),
      Tr("/deal nom : proposer un échange — /notrade : refuser d'office les échanges"),
      Tr("/organize nom, /invite nom, /leave, /expel nom : le groupe — /accept, /refuse : "
         "les invitations"),
      Tr("/guild nom, /guildinvite nom, /breakguild nom : la guilde"),
      Tr("/v N, /bv N : volume des bruitages et de la musique, de 0 à 127 — /sound, /bgm : "
         "les couper"),
      Tr("/who : joueurs connectés — /emotion : les émoticônes — /where : sa position"),
      Tr("/ex nom, /in nom : bloquer ou rétablir les chuchotements d'un joueur"),
      Tr("/savechat : enregistrer la conversation — /battlemode : replier la barre"),
      Tr("/monsterhp, /emblem, /window, /miss, /sf, /quake : basculer le réglage de la page "
         "d'options"),
  };
  for (const char* line : lines) {
    Say(to, line, said_ms, kEchoColor);
  }
}

void SayEmotionList(const ChatDispatch& to, std::uint32_t said_ms) {
  Say(to, Tr("[Liste des émoticônes]"), said_ms, kEchoColor);
  for (const std::string_view line : EmotionListLines()) {
    Say(to, std::string(line), said_ms, kEchoColor);
  }
}

// `/v` ET `/bv` : posés par le chemin de la fenêtre d'options, puis dits.
// Sans nombre, la commande dit le volume en cours.
void RunVolume(const ChatDispatch& to, const PlayerCommand& command, std::uint32_t said_ms) {
  if (to.options == nullptr) {
    return;
  }
  const bool music = command.kind == PlayerCommandKind::kMusicVolume;
  int volume = 0;
  if (command.volume.has_value()) {
    volume = *command.volume;
    to.options->Change(
        ro::ui::OptionsWindowAction{.changed = static_cast<int>(music ? GameOption::kMusicVolume
                                                                      : GameOption::kSoundVolume),
                                    .value = VolumeFractionOf(volume) * kPercent},
        to.option_targets, said_ms);
  } else if (const ro::audio::AudioEngine* audio = to.option_targets.audio; audio != nullptr) {
    volume = CommandVolumeOf(music ? audio->music_volume() : audio->sound_volume());
  } else {
    return;
  }
  Say(to,
      std::format("{} {}", music ? Tr("Volume de la musique") : Tr("Volume des bruitages"), volume),
      said_ms, kEchoColor);
}

// `/notrade` : la case de Gameplay, basculée par le même chemin que la
// fenêtre, et l'écho que le client du joueur écrit.
void ToggleDealRefusal(const ChatDispatch& to, std::uint32_t said_ms) {
  if (to.options == nullptr) {
    return;
  }
  const bool refuses = !to.options->pages().deal_refusal.refuses_deals();
  to.options->Change(
      ro::ui::OptionsWindowAction{.changed = DealRefusalRowId(DealRefusalRow::kRefuseDeals),
                                  .on = refuses},
      to.option_targets, said_ms);
  Say(to,
      refuses ? Tr("Les demandes d'échange sont refusées d'office. [/nt ON]")
              : Tr("Les demandes d'échange sont de nouveau accueillies. [/nt OFF]"),
      said_ms, kEchoColor);
}

// L'ÉTAT EN COURS de ce qu'une bascule inverse, tel que sa case le montre.
[[nodiscard]] bool SettingToggleIsOn(const GameOptions& options, SettingToggle toggle) {
  switch (toggle) {
    case SettingToggle::kMonsterHp:
      return options.entity_bars()
          .kinds[static_cast<std::size_t>(ro::ui::EntityBarKind::kMonster)]
          .shown;
    case SettingToggle::kGuildEmblem:
      return options.entity_names().guild_emblem;
    case SettingToggle::kWindowSnap:
      return options.align_grid().magnetic;
    case SettingToggle::kMissShown:
      return options.damage_look().miss;
    case SettingToggle::kSkillFailShown:
      return options.skill_fail_shown();
    case SettingToggle::kScreenQuake:
      return options.graphics().screen_quake();
  }
  return false;
}

}  // namespace

void RunSettingToggle(const ChatDispatch& to, SettingToggle toggle, std::uint32_t said_ms) {
  if (to.options == nullptr) {
    return;
  }
  const bool on = !SettingToggleIsOn(*to.options, toggle);
  for (const ro::ui::OptionsWindowAction& change : SettingToggleChanges(toggle, on)) {
    to.options->Change(change, to.option_targets, said_ms);
  }
  Say(to, SettingToggleEcho(toggle, on), said_ms, kSettingToggleEchoColor);
}

void RunPlayerCommand(const ChatDispatch& to, PlayerCommand command, std::uint32_t said_ms) {
  if (command.malformed) {
    Say(to, UsageOf(command.kind), said_ms);
    return;
  }
  const bool online = to.link->is_open();
  switch (command.kind) {
    case PlayerCommandKind::kNone:
      return;
    case PlayerCommandKind::kSitOrStand:
      if (const ro::sim::Actor* self = to.world->player(); online && self != nullptr) {
        ReportRefusal(ro::net::RequestSit(*to.link, self->posture != ro::sim::Posture::kSitting));
      }
      return;
    case PlayerCommandKind::kAllowPartyInvites:
    case PlayerCommandKind::kRefusePartyInvites:
      // L'ÉCHO DU SERVEUR ÉCRIT LA LIGNE, comme au client du joueur.
      if (online) {
        ReportRefusal(ro::net::RequestPartyInviteRefusal(
            *to.link, command.kind == PlayerCommandKind::kRefusePartyInvites));
      }
      return;
    case PlayerCommandKind::kUserCount:
      if (online) {
        ReportRefusal(ro::net::RequestUserCount(*to.link));
      }
      return;
    case PlayerCommandKind::kToggleDealRefusal:
      ToggleDealRefusal(to, said_ms);
      return;
    case PlayerCommandKind::kHelp:
      SayHelp(to, said_ms);
      return;
    case PlayerCommandKind::kEmotionList:
      SayEmotionList(to, said_ms);
      return;
    case PlayerCommandKind::kSoundVolume:
    case PlayerCommandKind::kMusicVolume:
      RunVolume(to, command, said_ms);
      return;
    case PlayerCommandKind::kDeal:
    case PlayerCommandKind::kOrganizeParty:
    case PlayerCommandKind::kInviteToParty:
    case PlayerCommandKind::kLeaveParty:
    case PlayerCommandKind::kExpelFromParty:
    case PlayerCommandKind::kCreateGuild:
    case PlayerCommandKind::kInviteToGuild:
    case PlayerCommandKind::kBreakGuild:
      if (to.player_asks != nullptr) {
        to.player_asks->push_back(std::move(command));
      }
      return;
  }
}

}  // namespace moonclient
