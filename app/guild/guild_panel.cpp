// L'onglet Guilde vu de la partie : l'en-tête, les membres, les postes, les
// relations et les expulsions, puis ce qu'on fait des gestes. Les compétences
// et l'éditeur d'emblème ont leur fichier.

#include "guild/guild_panel.h"

#include <ctime>
#include <utility>

#include "ro/core/i18n.h"
#include "ro/core/local_time.h"
#include "ro/core/log.h"
#include "ro/core/printed.h"
#include "ro/net/chat_target.h"
#include "ro/net/guild.h"
#include "ro/net/skill.h"
#include "ro/sim/world.h"

#include "world/actor_styles.h"

namespace moonclient {
namespace {

using ro::core::Tr;

// Tant que l'onglet est à l'écran, la fenêtre se redemande à ce rythme.
constexpr std::uint32_t kRefreshEveryMs = 30'000;
// Combien de têtes NEUVES une image accepte de composer, et leur taille.
constexpr int kHeadBudget = 8;
constexpr std::int32_t kHeadBoxPixels = 32;

constexpr std::string_view kStorageCommand = "@guildstorage";
// 🔴 SANS ARGUMENT NI CONFIRMATION côté serveur : c'est la boîte de l'onglet,
// et son nom retapé, qui gardent ce geste.
constexpr std::string_view kDisbandCommand = "@breakguild";

void Complain(const ro::core::Result<void>& sent, std::string_view what) {
  if (!sent) {
    ro::core::LogWarning("guilde, {} : {}", what, ro::core::Describe(sent.error()));
  }
}

[[nodiscard]] const char* CreationText(ro::sim::GuildCreation result) {
  switch (result) {
    case ro::sim::GuildCreation::kCreated:
      return Tr("Guilde créée.");
    case ro::sim::GuildCreation::kAlreadyInGuild:
      return Tr("Tu es déjà dans une guilde.");
    case ro::sim::GuildCreation::kNameTaken:
      return Tr("Ce nom de guilde est déjà pris.");
    case ro::sim::GuildCreation::kEmperiumMissing:
      return Tr("Il te faut un Emperium pour créer une guilde.");
  }
  return Tr("Résultat inconnu.");
}

[[nodiscard]] const char* InviteAnswerText(ro::sim::GuildInviteAnswer answer) {
  switch (answer) {
    case ro::sim::GuildInviteAnswer::kAlreadyInGuild:
      return Tr("Ce joueur est déjà dans une guilde.");
    case ro::sim::GuildInviteAnswer::kRefused:
      return Tr("Invitation refusée.");
    case ro::sim::GuildInviteAnswer::kAccepted:
      return Tr("Invitation acceptée.");
    case ro::sim::GuildInviteAnswer::kGuildFull:
      return Tr("La guilde est pleine.");
  }
  return Tr("Résultat inconnu.");
}

// LA DERNIÈRE CONNEXION, à l'heure du joueur : « 26/09/26 14:05 ».
[[nodiscard]] std::string LastSeen(std::uint32_t stamp) {
  if (stamp == 0) {
    return {};
  }
  const auto moment = static_cast<std::time_t>(stamp);
  std::tm local = {};
  if (!ro::core::ToLocalTime(moment, local)) {
    return {};
  }
  // ⚠ L'ANNÉE SUR DEUX CHIFFRES S'ÉCRIT À LA MAIN : `%y` de `strftime` fait
  // refuser la bâtisse par g++ (-Wformat-y2k), qui y voit un piège de l'an 2000.
  constexpr int kCentury = 100;
  return ro::core::Printed("%02d/%02d/%02d %02d:%02d", local.tm_mday, local.tm_mon + 1,
                           local.tm_year % kCentury, local.tm_hour, local.tm_min);
}

}  // namespace

GuildPanel::GuildPanel(GuildPanelPieces pieces) : pieces_(std::move(pieces)) {
  if (pieces_.wardrobe != nullptr) {
    thumbnails_.emplace(*pieces_.wardrobe);
  }
}

void GuildPanel::RefreshMembers(const ro::sim::GuildBook& book, const ro::sim::StyleBook& styles,
                                GuildPlayer player, bool shown) {
  const std::span<const ro::sim::GuildMember> members = book.members();
  members_.clear();
  members_.reserve(members.size());
  // ⚠ LES DATES D'ABORD, LES VUES ENSUITE : les vues pointent dans ce vecteur,
  // qui ne doit plus bouger une fois qu'elles y visent.
  member_seen_.clear();
  member_seen_.reserve(members.size());
  for (const ro::sim::GuildMember& member : members) {
    member_seen_.push_back(LastSeen(member.last_login));
  }
  if (thumbnails_.has_value() && shown) {
    thumbnails_->BeginFrame(kHeadBudget);
  }
  for (std::size_t index = 0; index < members.size(); ++index) {
    const ro::sim::GuildMember& member = members[index];
    ro::ui::GuildMemberView view;
    view.account_id = member.account_id;
    view.char_id = member.char_id;
    view.name = member.name;
    view.job = pieces_.jobs != nullptr ? pieces_.jobs->Find(static_cast<std::uint32_t>(member.job))
                                       : std::string_view();
    view.level = member.level;
    view.position = member.position;
    view.position_label = book.PositionLabel(member.position);
    view.contribution = member.contribution;
    view.online = member.online;
    view.last_seen = member_seen_[index];
    view.last_login = member.last_login;
    view.self = member.char_id == player.char_id;
    view.master = member.position == ro::sim::kGuildMasterPosition;
    // ⚠ LA RAISON VIT LE TEMPS DE L'IMAGE : c'est une chaîne du catalogue.
    view.mvp_refusal =
        mvp_judge_ ? mvp_judge_(member.name, member.account_id == player.account_id) : nullptr;
    // LA TÊTE, pour un membre CONNECTÉ seulement, comme le client.
    if (member.online && shown && thumbnails_.has_value()) {
      ro::render::ActorThumbnails::Cell cell;
      cell.subject = ro::render::ActorThumbnails::Subject::kHead;
      cell.look.player = true;
      cell.look.job = static_cast<std::uint32_t>(member.job);
      cell.look.sex = static_cast<std::uint8_t>(member.sex);
      cell.look.hair = static_cast<std::uint16_t>(member.hair);
      cell.look.hair_palette = member.hair_palette;
      // 🔴 CE QUE SA RECETTE IMPOSE, s'il est en vue et en a une : la liste de
      // guilde porte la couleur d'avant, que la recette remplace.
      if (pieces_.wardrobe != nullptr) {
        ImposeStyleNumbers(member.account_id, cell.look, *pieces_.wardrobe, styles);
      }
      cell.box.width = kHeadBoxPixels;
      cell.box.height = kHeadBoxPixels;
      const ro::render::IconTexture& head = thumbnails_->Get(cell);
      if (head.usable()) {
        view.head = ro::ui::SkinImage{.id = static_cast<ImTextureID>(head.texture.name()),
                                      .width = static_cast<float>(head.width_px),
                                      .height = static_cast<float>(head.height_px)};
      }
    }
    members_.push_back(view);
  }
  view_.members = members_;
  view_.member_count = static_cast<int>(members.size());
}

void GuildPanel::RefreshRights(const ro::sim::GuildBook& book, GuildPlayer player) {
  // LE MAÎTRE, PAR LE MEILLEUR TÉMOIN : les onglets que le serveur ouvre. Il
  // n'ouvre l'annonce qu'à celui qu'il tient pour maître — exactement le
  // drapeau qu'il revérifie à chaque geste de maître.
  const std::optional<std::uint32_t>& menu = book.menu_flags();
  view_.master =
      menu.has_value() ? (*menu & ro::sim::kGuildMenuNotice) != 0 : book.IsMaster(player.char_id);
  view_.members_tab = book.MenuOpens(ro::sim::kGuildMenuMembers);
  view_.positions_tab = book.MenuOpens(ro::sim::kGuildMenuPositions);
  view_.bans_tab = book.MenuOpens(ro::sim::kGuildMenuBans);
  const ro::sim::GuildMember* own = book.FindMember(player.char_id);
  view_.position_found = own != nullptr;
  my_position_ = own != nullptr ? book.PositionLabel(own->position) : std::string();
  view_.my_position = my_position_;
  const ro::sim::GuildRights rights = book.RightsOf(player.char_id);
  view_.rights_known = rights.known;
  view_.may_invite = rights.may_invite;
  view_.may_expel = rights.may_expel;
  view_.may_storage = rights.may_storage;
}

void GuildPanel::Refresh(const ro::sim::GuildBook& book, const ro::sim::World& world,
                         ro::ui::GuildTabState& state, const GuildEmblems& emblems,
                         GuildPlayer player, bool shown, std::uint32_t now_ms) {
  view_ = ro::ui::GuildView();
  view_.mvp_invites_withdrawn = mvp_withdrawn_;
  invitation_ = book.invitation().has_value() ? book.invitation()->guild_name : std::string();
  view_.in_guild = book.in_guild();
  if (!view_.in_guild) {
    // ⚠ UNE RÉPONSE NE SE MONTRE QU'À LA DEMANDE QUI L'A PROVOQUÉE : celle d'une
    // création d'hier ne dit rien de celle d'aujourd'hui.
    if (book.creation().has_value() && book.creation_seq() > creation_seq_seen_) {
      creation_answer_ = CreationText(*book.creation());
      view_.creation_answer = creation_answer_;
      view_.creation_succeeded = *book.creation() == ro::sim::GuildCreation::kCreated;
    }
    return;
  }
  const ro::sim::GuildBelonging& belonging = *book.belonging();
  const auto& summary = book.summary();
  view_.guild_id = belonging.guild_id;
  // UNE AUTRE GUILDE QU'À L'IMAGE D'AVANT se redemande sans attendre la
  // cadence : ses postes et ses compétences ne viennent que sur demande.
  if (belonging.guild_id != seen_guild_id_) {
    seen_guild_id_ = belonging.guild_id;
    request_due_ = true;
  }
  view_.name = summary.has_value() ? std::string_view(summary->name) : belonging.name;
  if (summary.has_value()) {
    view_.master_name = summary->master_name;
    view_.land = summary->land;
    view_.level = summary->level;
    view_.member_most = summary->member_most;
    view_.online = summary->online_count;
    view_.average_level = summary->average_level;
    view_.exp = summary->exp;
    view_.next_exp = summary->next_exp;
  }
  view_.emblem = emblems.Of(belonging.guild_id);
  view_.notice_subject = book.notice_subject();
  view_.notice_body = book.notice_body();

  RefreshMembers(book, world.styles(), player, shown);
  RefreshRights(book, player);

  for (std::size_t index = 0; index < positions_.size(); ++index) {
    const ro::sim::GuildPosition& position = book.positions()[index];
    const std::string& label = book.PositionLabel(static_cast<std::int32_t>(index));
    positions_[index] = ro::ui::GuildPositionView{
        .name = label,
        .may_invite = (position.rights & ro::sim::kGuildMayInvite) != 0,
        .may_expel = (position.rights & ro::sim::kGuildMayExpel) != 0,
        .may_storage = (position.rights & ro::sim::kGuildMayStorage) != 0,
        .pay_rate = position.pay_rate,
        .has_name = position.has_name || !label.empty(),
        .has_rights = position.has_rights,
    };
  }
  view_.positions = positions_;

  relations_.clear();
  for (const ro::sim::GuildRelation& relation : book.relations()) {
    relations_.push_back(ro::ui::GuildRelationView{
        .guild_id = relation.guild_id,
        .enemy = relation.kind == ro::sim::GuildRelationKind::kEnemy,
        .name = relation.name,
    });
  }
  view_.relations = relations_;

  bans_.clear();
  for (const ro::sim::GuildBan& ban : book.bans()) {
    bans_.push_back(ro::ui::GuildBanView{.name = ban.name, .reason = ban.reason});
  }
  view_.bans = bans_;
  view_.bans_known = book.bans_known();

  RefreshSkills(book, world, player, now_ms);
  RefreshEmblemEditor(world, state, now_ms);
}

void GuildPanel::RequestIfDue(ro::net::Connection& link, bool shown, std::uint32_t now_ms) {
  if (!shown) {
    return;
  }
  if (!request_due_ && now_ms - requested_ms_ < kRefreshEveryMs) {
    return;
  }
  Complain(ro::net::RequestGuildWindow(link), "rafraîchissement");
  requested_ms_ = now_ms;
  request_due_ = false;
}

GuildElsewhere GuildPanel::Apply(const ro::ui::GuildAction& action, ro::ui::GuildTabState& state,
                                 const ro::sim::GuildBook& book, const ro::sim::World& world,
                                 GuildEmblems& emblems, ro::net::Connection& link,
                                 GuildPlayer player) {
  GuildElsewhere elsewhere;
  const std::int32_t guild_id = book.in_guild() ? book.belonging()->guild_id : 0;
  const auto guild = static_cast<std::uint32_t>(guild_id);
  using ro::ui::GuildVerb;
  switch (action.verb) {
    case GuildVerb::kNothing:
      break;
    case GuildVerb::kRefresh:
      request_due_ = true;
      break;
    case GuildVerb::kCreate:
      creation_seq_seen_ = book.creation_seq();
      Complain(ro::net::RequestGuildCreation(link, player.char_id, action.text), "création");
      break;
    case GuildVerb::kSaveNotice:
      Complain(ro::net::RequestGuildNotice(link, guild, action.subject, action.body), "annonce");
      break;
    case GuildVerb::kOpenStorage:
      Complain(ro::net::SendServerCommand(link, world.player_name(), kStorageCommand), "storage");
      break;
    case GuildVerb::kWhisper:
      elsewhere.whisper_to = action.text;
      break;
    case GuildVerb::kWhisperWindow:
      elsewhere.whisper_window = action.text;
      break;
    case GuildVerb::kSendMail:
      elsewhere.mail_to = action.text;
      break;
    case GuildVerb::kChangePosition:
      // 🔴 JAMAIS LE POSTE ZÉRO ICI : ce serait un transfert de direction.
      if (action.position > 0) {
        Complain(ro::net::RequestGuildMemberPosition(link, action.account_id, action.char_id,
                                                     action.position),
                 "changement de poste");
        request_due_ = true;
      }
      break;
    case GuildVerb::kTransferMaster:
      if (action.char_id != 0) {
        Complain(ro::net::RequestGuildMemberPosition(link, action.account_id, action.char_id,
                                                     ro::sim::kGuildMasterPosition),
                 "transfert de direction");
        request_due_ = true;
      }
      break;
    case GuildVerb::kExpel:
      Complain(
          ro::net::RequestGuildExpel(link, guild, action.account_id, action.char_id, action.text),
          "expulsion");
      request_due_ = true;
      break;
    case GuildVerb::kLeave:
      Complain(
          ro::net::RequestGuildLeave(link, guild, player.account_id, player.char_id, action.text),
          "départ");
      break;
    case GuildVerb::kDisband:
      Complain(ro::net::SendServerCommand(link, world.player_name(), kDisbandCommand),
               "dissolution");
      break;
    case GuildVerb::kInvite:
      invite_answer_seq_seen_ = book.invite_answer_seq();
      Complain(ro::net::RequestGuildInvite(link, action.text), "invitation");
      break;
    case GuildVerb::kEndRelation:
      Complain(ro::net::RequestGuildRelationEnd(link, action.guild_id,
                                                action.enemy ? ro::sim::GuildRelationKind::kEnemy
                                                             : ro::sim::GuildRelationKind::kAlly),
               "relation");
      request_due_ = true;
      break;
    case GuildVerb::kSavePositions: {
      std::vector<ro::net::GuildPositionEdit> edits;
      for (const ro::ui::GuildPositionEditView& edit : action.positions) {
        edits.push_back(ro::net::GuildPositionEdit{
            .position = edit.position,
            .rights = (edit.may_invite ? ro::sim::kGuildMayInvite : 0) |
                      (edit.may_expel ? ro::sim::kGuildMayExpel : 0) |
                      (edit.may_storage ? ro::sim::kGuildMayStorage : 0),
            .pay_rate = edit.pay_rate,
            .name = edit.name,
        });
      }
      if (!edits.empty()) {
        Complain(ro::net::RequestGuildPositions(link, edits), "postes");
      }
      break;
    }
    case GuildVerb::kSkillUp:
      // Le serveur renvoie la liste entière après une montée : on la laisse
      // corriger niveau et points plutôt que de les avancer à l'aveugle.
      Complain(ro::net::RequestSkillUp(link, action.skill_id), "montée de compétence");
      request_due_ = true;
      break;
    case GuildVerb::kUseSkill:
      // ⚠ SUR SOI : toutes les compétences de guilde se lancent ainsi.
      Complain(ro::net::RequestSkillOnTarget(link, action.skill_id,
                                             static_cast<std::uint16_t>(action.skill_level),
                                             world.player_id()),
               "compétence de guilde");
      break;
    case GuildVerb::kDescribeSkill:
      elsewhere.describe_skill = action.skill_id;
      break;
    case GuildVerb::kOpenEmblemEditor:
      OpenEmblemEditor(state, emblems, guild_id, action.from_current);
      break;
    case GuildVerb::kScanEmblemFolder:
      ScanEmblemFolder();
      break;
    case GuildVerb::kUploadEmblemFile:
      UploadFile(state, emblems, guild_id, action.file);
      break;
    case GuildVerb::kUploadCanvas:
      UploadCanvas(state, emblems, guild_id, action.text);
      break;
    case GuildVerb::kSaveCanvas:
      SaveCanvas(state, action.text);
      break;
    case GuildVerb::kLoadCurrentEmblem:
      state.emblem.report = LoadCurrentEmblem(state, emblems, guild_id)
                                ? Tr("Emblème actuel de la guilde chargé dans l'éditeur.")
                                : Tr("Emblème actuel illisible (pas encore téléchargé ?).");
      break;
    case GuildVerb::kLoadFileIntoCanvas:
      LoadFileIntoCanvas(state, action.file);
      break;
    case GuildVerb::kImportItemIcon:
      ImportItemIcon(state, action.item_id);
      break;
    case GuildVerb::kInviteMvp:
      elsewhere.mvp_invite = action.text;
      break;
  }
  return elsewhere;
}

void GuildPanel::Settle(ro::ui::GuildTabState& state, const ro::sim::GuildBook& book,
                        GuildEmblems& emblems, ro::net::Connection& link) {
  if (auto outcome = emblems.TakeUploadOutcome()) {
    std::string line;
    if (outcome->accepted()) {
      // 🔴 LE SERVICE A RANGÉ L'IMAGE, MAIS PERSONNE NE LE SAIT ENCORE : c'est
      // le serveur de carte qui publie la version, à notre annonce.
      Complain(ro::net::AnnounceGuildEmblem(link, outcome->guild_id, outcome->version),
               "annonce d'emblème");
      line = ro::core::Printed(
          Tr("Emblème déposé (version %d) : le serveur le publie à la guilde."), outcome->version);
    } else {
      line = ro::core::Printed(Tr("Envoi refusé : %s"), outcome->refusal.c_str());
    }
    state.emblem.report = line;
  }
  if (book.invite_answer().has_value() && book.invite_answer_seq() > invite_answer_seq_seen_) {
    invite_answer_seq_seen_ = book.invite_answer_seq();
    state.status = InviteAnswerText(*book.invite_answer());
  }
}

}  // namespace moonclient
