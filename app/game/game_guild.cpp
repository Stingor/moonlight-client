// LA GUILDE DANS LA PARTIE : l'onglet de la fiche, les emblèmes, l'invitation.
//
// ⚠ À PART DE LA BOUCLE : la guilde n'y coûte que trois appels — ouvrir,
// rafraîchir avant le dessin, appliquer après.

#include <optional>
#include <string>
#include <utility>

#include "ro/core/log.h"
#include "ro/formats/assist_address.h"
#include "ro/net/guild.h"

#include "chat/banned_input.h"
#include "chat/player_link_judge.h"
#include "game/game.h"
#include "game/save_folder.h"
#include "game/whisper_stage.h"

namespace moonclient {
namespace {

// Le cache des emblèmes, sous le dossier de sauvegarde.
constexpr std::string_view kEmblemCacheFolder = "emblemes";

}  // namespace

void Game::OpenGuild() {
  guild_panel_.emplace(GuildPanelPieces{
      .vfs = vfs_,
      .jobs = &pieces_.job_names,
      .items = pieces_.wardrobe.has_value() ? &pieces_.wardrobe->catalog().items : nullptr,
      .item_icons = pieces_.item_icons.has_value() ? &*pieces_.item_icons : nullptr,
      .skills = pieces_.skill_tree.has_value() ? &*pieces_.skill_tree : nullptr,
      .skill_icons = pieces_.skill_icons.has_value() ? &*pieces_.skill_icons : nullptr,
      .wardrobe = pieces_.wardrobe.has_value() ? &*pieces_.wardrobe : nullptr,
      .client_root = client_root_,
  });
  // 🔴 SANS ADRESSE, PAS DE SERVICE : l'onglet se dessine, mais aucun emblème ne
  // se lit ni ne se dépose. C'est `moonclient.yml` qui la déclare, jamais nous.
  std::string origin =
      shell_.web().empty() ? std::string() : ro::formats::AssistOrigin(shell_.web());
  if (origin.empty()) {
    ro::core::LogWarning("service d'emblèmes : aucune adresse « web » dans moonclient.yml");
  } else {
    ro::core::LogInfo("service d'emblèmes : {}", origin);
  }
  guild_emblems_.emplace(
      std::move(origin),
      client_root_ / std::filesystem::path(kSaveFolder) / std::filesystem::path(kEmblemCacheFolder),
      &pieces_.overlay);
  guild_emblems_->SetIdentity(web::EmblemIdentity{.account_id = ticket_.account.account_id,
                                                  .token = ticket_.account.web_token,
                                                  .world_name = ticket_.account.world_name});
  shortcuts_->UseGuild(&guild_book_, ticket_.character_id);
}

void Game::RefreshGuild(std::uint32_t now_ms) {
  const bool shown =
      windows_.character.open && windows_.character.tab == ro::ui::CharacterTab::kGuild;
  const GuildPlayer player{.account_id = world_.player_id(), .char_id = ticket_.character_id};
  guild_emblems_->Advance(guild_book_, now_ms);
  guild_panel_->Settle(windows_.character.guild, guild_book_, *guild_emblems_, link_);
  guild_panel_->RequestIfDue(link_, shown && guild_book_.in_guild(), now_ms);
  // « INVITER DANS MON CARNET MVP » : le carnet seul sait qui peut y entrer ;
  // éteint, l'entrée disparaît.
  guild_panel_->WithdrawsMvpInvites(!settings_->options().pages().mvp_tracker.settings().enabled);
  guild_panel_->JudgesMvpInvites([this](std::string_view name, bool same_account) {
    return JudgeMvpInvite(
        name,
        MvpInviteCircle{.book = pieces_.mvp_tracker.book(),
                        .guild = guild_book_,
                        .friends = friends_,
                        .enabled = settings_->options().pages().mvp_tracker.settings().enabled,
                        .link_ready = pieces_.mvp_tracker.link_ready()},
        same_account);
  });
  guild_panel_->Refresh(guild_book_, world_, windows_.character.guild, *guild_emblems_, player,
                        shown, now_ms);
}

void Game::ApplyGuildAnswers(const UiStage::Answers& answers, std::uint32_t now_ms) {
  const GuildPlayer player{.account_id = world_.player_id(), .char_id = ticket_.character_id};
  // 🔴 UN NOM DE GUILDE OU UN AVIS QUI PORTE UN MOT INTERDIT NE PART PAS : le
  // refus s'écrit dans la conversation, et l'onglet ne voit rien partir. L'avis
  // s'éprouve par son objet, puis par son corps.
  ro::ui::GuildAction asked = answers.character.guild;
  std::optional<std::string> refused;
  BannedInput input = BannedInput::kGuildName;
  if (asked.verb == ro::ui::GuildVerb::kCreate) {
    refused = BannedInputRefusal(pieces_.banned_words, asked.text, input);
  } else if (asked.verb == ro::ui::GuildVerb::kSaveNotice) {
    input = BannedInput::kSentence;
    refused = BannedInputRefusal(pieces_.banned_words, asked.subject, input);
    if (!refused.has_value()) {
      refused = BannedInputRefusal(pieces_.banned_words, asked.body, input);
    }
  }
  if (refused.has_value()) {
    world_.RecordChat(ro::sim::ChatKind::kServer, 0, std::move(*refused), now_ms,
                      BannedInputColor(input));
    asked.verb = ro::ui::GuildVerb::kNothing;
  }
  const GuildElsewhere elsewhere = guild_panel_->Apply(asked, windows_.character.guild, guild_book_,
                                                       world_, *guild_emblems_, link_, player);
  // CHUCHOTER : le nom part dans la case du destinataire, et le clavier dans la
  // phrase — comme un lien qu'on vient de poser, la barre se déplie.
  ArmWhisper(windows_.chat.main.input, elsewhere.whisper_to);
  // LA CONVERSATION 1:1 À PART : elle s'ouvre, ou revient devant.
  if (!elsewhere.whisper_window.empty()) {
    pieces_.whispers.Open(elsewhere.whisper_window);
  }
  // ENVOYER UN COURRIER : l'écriture se demande au serveur, adressée au membre.
  if (!elsewhere.mail_to.empty()) {
    pieces_.mail.Compose(elsewhere.mail_to);
  }
  // L'INVITATION AU CARNET DE CHASSE part tout de suite.
  if (!elsewhere.mvp_invite.empty()) {
    pieces_.mvp_tracker.Invite(elsewhere.mvp_invite, link_);
  }
  // LA DESCRIPTION D'UNE COMPÉTENCE DE GUILDE : la fenêtre du grimoire.
  ro::ui::AskSkillDescription(windows_.skill_description, elsewhere.describe_skill);
  if (answers.guild_invitation.has_value() && guild_book_.invitation().has_value()) {
    const auto answered = ro::net::AnswerGuildInvitation(link_, guild_book_.invitation()->guild_id,
                                                         *answers.guild_invitation);
    if (!answered) {
      ro::core::LogWarning("guilde, réponse à l'invitation : {}",
                           ro::core::Describe(answered.error()));
    }
    guild_book_.ForgetInvitation();
  }
  // L'ALLIANCE PROPOSÉE : la réponse vise le compte qui l'a proposée.
  if (answers.guild_alliance.has_value() && guild_book_.alliance_offer().has_value()) {
    const auto answered = ro::net::AnswerGuildAlliance(
        link_, guild_book_.alliance_offer()->inviter_account_id, *answers.guild_alliance);
    if (!answered) {
      ro::core::LogWarning("guilde, réponse à l'alliance : {}",
                           ro::core::Describe(answered.error()));
    }
    guild_book_.ForgetAllianceOffer();
  }
}

}  // namespace moonclient
