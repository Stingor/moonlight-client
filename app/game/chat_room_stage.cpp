#include "game/chat_room_stage.h"

#include <algorithm>
#include <array>
#include <cstdio>
#include <span>
#include <string_view>
#include <utility>

#include "ro/core/i18n.h"
#include "ro/core/log.h"
#include "ro/vfs/chat_room_words.h"
#include "ro/vfs/trade_words.h"

namespace moonclient {
namespace {

using ro::vfs::ChatRoomText;
using ro::vfs::ChatRoomWord;

// LES COULEURS DE CE QUE LE SALON DIT DANS LA CONVERSATION, en `0xRRGGBB` :
// celles du client d'origine, le cyan d'une création réussie et le rouge d'un
// refus.
constexpr std::uint32_t kRoomMadeColor = 0x00FFFF;
constexpr std::uint32_t kRefusalColor = 0xFF0000;

// UN TITRE VENU D'UN AUTRE JOUEUR, posé dans un nom de fenêtre : ses « ## »
// deviendraient des identifiants, et « ### » changerait l'identité de la salle.
[[nodiscard]] std::string ShownTitle(std::string_view title) {
  std::string out;
  out.reserve(title.size());
  for (const char letter : title) {
    if (letter == '#' && !out.empty() && out.back() == '#') {
      out.push_back(' ');
    }
    out.push_back(letter);
  }
  return out;
}

// La place d'une phrase composée d'un titre de salon.
constexpr std::size_t kSentenceBytes = 192;

[[nodiscard]] std::string_view Typed(std::span<const char> buffer) {
  const auto end = std::ranges::find(buffer, '\0');
  return std::string_view(buffer.data(), static_cast<std::size_t>(end - buffer.begin()));
}

[[nodiscard]] ro::ui::ChatRoomLimits Limits() {
  return ro::ui::ChatRoomLimits{.title_bytes = ro::sim::kChatRoomTitleMostBytes,
                                .password_bytes = ro::sim::kChatRoomPasswordMostBytes,
                                .fewest_password_bytes = ro::sim::kChatRoomPasswordFewestBytes,
                                .fewest_seats = ro::sim::kChatRoomFewestSeats,
                                .most_seats = ro::sim::kChatRoomMostSeats};
}

// Où et quand le salon écrit dans la conversation.
struct ChatRoomSaid {
  ro::sim::World& world;
  const ro::vfs::ItemWords& words;
  std::uint32_t now_ms = 0;
};

void Say(const ChatRoomSaid& said, std::string_view text, std::uint32_t color) {
  if (!text.empty()) {
    said.world.RecordChat(ro::sim::ChatKind::kServer, 0, std::string(text), said.now_ms, color);
  }
}

// POURQUOI LE SALON NE S'OUVRIRAIT PAS ICI, déjà traduit ; vide quand rien ne
// s'y oppose.
[[nodiscard]] std::string BlockerText(ChatRoomBlocker blocker, const ro::vfs::ItemWords& words) {
  switch (blocker) {
    case ChatRoomBlocker::kNone:
      return {};
    case ChatRoomBlocker::kInRoom:
      return ro::core::Tr("Tu es déjà dans un salon.");
    case ChatRoomBlocker::kVending:
      return std::string(ro::vfs::TradeText(words, ro::vfs::TradeWord::kNoChatWhileVending));
    case ChatRoomBlocker::kBuyingStore:
      return ro::core::Tr("Ferme d'abord ton échoppe d'achat.");
    case ChatRoomBlocker::kBasicSkill:
      return ro::core::Tr("Il faut Basic Skill niveau 4 pour ouvrir un salon.");
    case ChatRoomBlocker::kNpcNear:
      return ro::core::Tr("Un PNJ est trop près : éloigne-toi de quelques cases.");
    case ChatRoomBlocker::kCellTaken:
      return ro::core::Tr("Une enseigne occupe déjà ta case : fais un pas de côté.");
  }
  return {};
}

// LE MOTIF D'UN REFUS D'ENTRÉE, déjà traduit.
//
// ⚠ LE MOT DE PASSE FAUX N'A PAS LE TEXTE DU CLIENT D'ORIGINE, qui reprend celui
// de la connexion — « identifiant ou mot de passe incorrect ». Il parle d'un
// identifiant que personne n'a tapé ici.
[[nodiscard]] std::string RefusalText(std::uint8_t reason, const ro::vfs::ItemWords& words) {
  switch (reason) {
    case ro::sim::kChatRoomRefusedFull:
      return std::string(ChatRoomText(words, ChatRoomWord::kRoomFull));
    case ro::sim::kChatRoomRefusedPassword:
      return ro::core::Tr("Le mot de passe est incorrect.");
    case ro::sim::kChatRoomRefusedExpelled:
      return std::string(ChatRoomText(words, ChatRoomWord::kKickedOut));
    case ro::sim::kChatRoomRefusedZeny:
      return std::string(ro::vfs::TradeText(words, ro::vfs::TradeWord::kLacksZeny));
    case ro::sim::kChatRoomRefusedLevelLow:
      return std::string(ChatRoomText(words, ChatRoomWord::kLevelTooLow));
    case ro::sim::kChatRoomRefusedLevelHigh:
      return std::string(ChatRoomText(words, ChatRoomWord::kLevelTooHigh));
    case ro::sim::kChatRoomRefusedJob:
      return std::string(ChatRoomText(words, ChatRoomWord::kWrongJob));
    default:
      return ro::core::Tr("Le serveur a refusé l'entrée dans ce salon.");
  }
}

}  // namespace

void ChatRoomStage::See(std::filesystem::path client_root) {
  if (client_root == client_root_ && seeded_) {
    return;
  }
  client_root_ = std::move(client_root);
  memory_ = ChatRoomMemory{};
  LoadChatRoomMemory(client_root_, memory_);
  seeded_ = false;
}

void ChatRoomStage::StartSession() {
  room_.Reset();
  asked_ = Asked::kNone;
  raise_room_ = false;
  join_.reset();
  leave_asked_ = false;
  error_.clear();
  error_field_ = ro::ui::ChatRoomField::kNone;
  password_error_.clear();
  settings_error_.clear();
}

ro::net::FrameOutcome ChatRoomStage::ApplyFrame(const ro::net::Frame& frame, ro::sim::World& world,
                                                const ro::vfs::ItemWords& words,
                                                std::uint32_t now_ms) {
  const ro::net::FrameOutcome outcome = ro::net::ApplyChatRoomFrame(frame, room_, world);
  SayNews(world, words, now_ms);
  return outcome;
}

bool ChatRoomStage::AskLeave() noexcept {
  if (!room_.in_room()) {
    return false;
  }
  leave_asked_ = true;
  return true;
}

ro::net::FrameOutcome ChatRoomStage::ClaimLine(const ro::net::Frame& frame,
                                               const ro::sim::World& world, std::uint32_t now_ms) {
  return ro::net::ClaimChatRoomLine(frame, room_, world, now_ms);
}

void ChatRoomStage::AskJoin(std::uint32_t room_id, std::uint8_t status) {
  join_ = JoinAsk{.room_id = room_id, .status = status};
}

void ChatRoomStage::Advance(ChatRoomWindows& windows, const ChatRoomContext& context) {
  // LA MÉMOIRE SE POSE UNE FOIS dans le formulaire : rouvert, il garde ce que le
  // joueur y a laissé.
  if (!seeded_) {
    seeded_ = true;
    const std::size_t kept = std::min(memory_.title.size(), windows.make.title.size() - 1);
    std::copy_n(memory_.title.begin(), kept, windows.make.title.begin());
    windows.make.title[kept] = '\0';
    windows.make.limit =
        std::clamp<int>(memory_.limit, ro::sim::kChatRoomFewestSeats, ro::sim::kChatRoomMostSeats);
    windows.make.is_public = memory_.is_public;
  }
  if (const Asked asked = std::exchange(asked_, Asked::kNone); asked != Asked::kNone) {
    if (room_.in_room()) {
      raise_room_ = true;
    } else if (asked == Asked::kToggle && windows.make.open) {
      // FERMER DOIT TOUJOURS MARCHER, quoi qu'il soit arrivé depuis l'ouverture.
      windows.make.open = false;
      error_.clear();
      error_field_ = ro::ui::ChatRoomField::kNone;
    } else {
      windows.make.open = true;
      windows.make.raise = true;
    }
  }
  FollowMake(windows, context);
  FollowRoom(windows, context);
  FollowPassword(windows, context);
  if (std::optional<JoinAsk> join = std::exchange(join_, std::nullopt); join.has_value()) {
    Join(windows, context, join->room_id, join->status);
  }
  if (std::exchange(leave_asked_, false) && room_.in_room()) {
    if (auto left = ro::net::RequestLeaveChatRoom(context.link, room_); !left) {
      ro::core::LogWarning("salon : {}", ro::core::Describe(left.error()));
    }
  }
  // UNE ENTRÉE AVEUGLE SE DÉFAIT : voir `sim::ChatRoom::TellBlindEntry`.
  if (auto owed = ro::net::RequestOwedChatRoomExit(context.link, room_); !owed) {
    ro::core::LogWarning("salon : {}", ro::core::Describe(owed.error()));
  }
  room_.ExpireSilentCreate(context.now_ms);
  room_.ExpireSilentExpels(context.now_ms);
  SayNews(context.world, context.words, context.now_ms);
  // Un formulaire fermé — Échap compris — oublie son erreur.
  if (!windows.make.open) {
    error_.clear();
    error_field_ = ro::ui::ChatRoomField::kNone;
  }

  // UN SALON NEUF : sa fenêtre repart de zéro, passe devant, et ce qui menait à
  // lui — formulaire ou invite — n'a plus rien à faire à l'écran.
  if (room_.in_room() && room_.room_seq() != shown_room_seq_) {
    shown_room_seq_ = room_.room_seq();
    windows.room = ro::ui::ChatRoomWindowState{};
    windows.room.raise = true;
    settings_error_.clear();
    windows.make.open = false;
    windows.password.open = false;
  }
  if (std::exchange(raise_room_, false)) {
    windows.room.raise = true;
  }
  Compose(windows, context);
}

void ChatRoomStage::FollowMake(ChatRoomWindows& windows, const ChatRoomContext& context) {
  const ro::ui::ChatRoomMakeAction answer = std::exchange(windows.make_answer, {});
  if (answer.edited_faulty_field) {
    error_.clear();
    error_field_ = ro::ui::ChatRoomField::kNone;
  }
  if (answer.closed) {
    error_.clear();
    error_field_ = ro::ui::ChatRoomField::kNone;
  }
  if (answer.create) {
    Create(windows, context);
  }
  if (answer.join_room.has_value()) {
    const auto found = std::ranges::find(nearby_, *answer.join_room, &NearbyChatRoom::room_id);
    if (found != nearby_.end()) {
      Join(windows, context, found->room_id, found->status);
    }
  }
}

void ChatRoomStage::Create(ChatRoomWindows& windows, const ChatRoomContext& context) {
  if (room_.creating()) {
    return;
  }
  ro::sim::ChatRoomSettings settings{
      .title = std::string(Typed(windows.make.title)),
      .password = std::string(Typed(windows.make.password)),
      .is_public = windows.make.is_public,
      .limit = static_cast<std::uint16_t>(std::clamp<int>(
          windows.make.limit, ro::sim::kChatRoomFewestSeats, ro::sim::kChatRoomMostSeats))};
  // ── LA VALIDATION, DANS L'ORDRE DU CLIENT D'ORIGINE ─────────────────────────
  // Même ordre et mêmes phrases : seule la forme change, un message dans la
  // fenêtre au lieu d'une boîte modale.
  const ChatRoomBlocker blocker = ChatRoomCreateBlocker(context.world, context.stores);
  if (blocker != ChatRoomBlocker::kNone) {
    SetError(BlockerText(blocker, context.words), ro::ui::ChatRoomField::kNone);
    return;
  }
  std::array<char, kSentenceBytes> sentence{};
  switch (ro::net::CheckChatRoomSettings(settings)) {
    case ro::net::ChatRoomSettingsFault::kNone:
      break;
    case ro::net::ChatRoomSettingsFault::kNoTitle:
      SetError(std::string(ChatRoomText(context.words, ChatRoomWord::kEnterTitle)),
               ro::ui::ChatRoomField::kTitle);
      return;
    case ro::net::ChatRoomSettingsFault::kTitleNotAscii:
      SetError(std::string(ChatRoomText(context.words, ChatRoomWord::kEnglishOnly)),
               ro::ui::ChatRoomField::kTitle);
      return;
    case ro::net::ChatRoomSettingsFault::kTitleTooLong:
      std::snprintf(sentence.data(), sentence.size(),
                    ro::core::Tr("Titre trop long : %zu octets pour %zu au maximum."),
                    settings.title.size(), ro::sim::kChatRoomTitleMostBytes);
      SetError(sentence.data(), ro::ui::ChatRoomField::kTitle);
      return;
    case ro::net::ChatRoomSettingsFault::kNoPassword:
      SetError(std::string(ChatRoomText(context.words, ChatRoomWord::kEnterPassword)),
               ro::ui::ChatRoomField::kPassword);
      return;
    case ro::net::ChatRoomSettingsFault::kPasswordTooShort:
      SetError(std::string(ChatRoomText(context.words, ChatRoomWord::kPasswordTooShort)),
               ro::ui::ChatRoomField::kPassword);
      return;
    case ro::net::ChatRoomSettingsFault::kPasswordTooLong:
      SetError(ro::core::Tr("Mot de passe trop long : huit caractères au plus."),
               ro::ui::ChatRoomField::kPassword);
      return;
    case ro::net::ChatRoomSettingsFault::kPasswordNotEncodable:
      SetError(ro::core::Tr("Ce mot de passe porte un caractère que le serveur ne connaît pas."),
               ro::ui::ChatRoomField::kPassword);
      return;
  }
  const ro::sim::ChatRoomSettings sent = settings;
  if (auto asked =
          ro::net::RequestCreateChatRoom(context.link, room_, std::move(settings), context.now_ms);
      !asked) {
    ro::core::LogWarning("salon : {}", ro::core::Describe(asked.error()));
    SetError(ro::core::Tr("Envoi impossible : pas de connexion."), ro::ui::ChatRoomField::kNone);
    return;
  }
  // 🔴 ON NE FERME PAS : la fenêtre reste, saisie intacte, jusqu'à la réponse.
  error_.clear();
  error_field_ = ro::ui::ChatRoomField::kNone;
  memory_.title = sent.title;
  memory_.limit = sent.limit;
  memory_.is_public = sent.is_public;
  memory_.RememberTitle(sent.title);
  static_cast<void>(SaveChatRoomMemory(client_root_, memory_));
}

void ChatRoomStage::FollowRoom(ChatRoomWindows& windows, const ChatRoomContext& context) {
  ro::ui::ChatRoomWindowAction answer = std::exchange(windows.room_answer, {});
  if (!room_.in_room()) {
    return;
  }
  if (answer.leave) {
    if (auto left = ro::net::RequestLeaveChatRoom(context.link, room_); !left) {
      ro::core::LogWarning("salon : {}", ro::core::Describe(left.error()));
    }
    return;
  }
  if (answer.expel.has_value()) {
    if (auto sent = ro::net::RequestChatRoomExpel(context.link, room_, *answer.expel); !sent) {
      ro::core::LogWarning("salon : {}", ro::core::Describe(sent.error()));
    } else {
      room_.NoteExpelSent(*answer.expel, context.now_ms);
    }
  }
  if (answer.give_owner.has_value()) {
    if (auto sent = ro::net::RequestChatRoomOwner(context.link, room_, *answer.give_owner); !sent) {
      ro::core::LogWarning("salon : {}", ro::core::Describe(sent.error()));
    }
  }
  if (answer.apply_settings) {
    ApplySettings(windows, context);
  }
}

void ChatRoomStage::ApplySettings(ChatRoomWindows& windows, const ChatRoomContext& context) {
  const ro::sim::ChatRoomSettings settings{
      .title = std::string(Typed(windows.room.settings_title)),
      .password = std::string(Typed(windows.room.settings_password)),
      .is_public = windows.room.settings_public,
      .limit = static_cast<std::uint16_t>(std::clamp<int>(windows.room.settings_limit,
                                                          ro::sim::kChatRoomFewestSeats,
                                                          ro::sim::kChatRoomMostSeats))};
  // UN REFUS SE DIT DANS LE VOLET, qui reste ouvert : le joueur corrige d'un
  // caractère au lieu de tout retaper.
  if (ro::net::CheckChatRoomSettings(settings) ==
      ro::net::ChatRoomSettingsFault::kPasswordNotEncodable) {
    settings_error_ =
        ro::core::Tr("Ce mot de passe porte un caractère que le serveur ne connaît pas.");
    return;
  }
  if (auto sent = ro::net::RequestChangeChatRoom(context.link, room_, settings); !sent) {
    ro::core::LogWarning("salon : {}", ro::core::Describe(sent.error()));
    settings_error_ = ro::core::Tr("Envoi impossible : pas de connexion.");
    return;
  }
  settings_error_.clear();
  windows.room.settings_wanted = false;
}

void ChatRoomStage::FollowPassword(ChatRoomWindows& windows, const ChatRoomContext& context) {
  const ro::ui::ChatRoomPasswordAction answer = std::exchange(windows.password_answer, {});
  if (answer.edited) {
    password_error_.clear();
  }
  if (answer.cancelled) {
    password_error_.clear();
    return;
  }
  if (answer.enter && windows.password.open) {
    if (auto sent = ro::net::RequestEnterChatRoom(context.link, room_, windows.password.room_id,
                                                  Typed(windows.password.input), context.now_ms);
        !sent) {
      ro::core::LogWarning("salon : {}", ro::core::Describe(sent.error()));
    }
  }
}

void ChatRoomStage::Join(ChatRoomWindows& windows, const ChatRoomContext& context,
                         std::uint32_t room_id, std::uint8_t status) {
  // DÉJÀ DANS UN SALON : le serveur refuserait le second. La salle passe devant.
  if (room_.in_room()) {
    raise_room_ = true;
    return;
  }
  switch (ChatRoomJoinStepOf(status)) {
    case ChatRoomJoinStep::kNone:
      return;
    case ChatRoomJoinStep::kAskPassword:
      windows.password = ro::ui::ChatRoomPasswordState{};
      windows.password.open = true;
      windows.password.room_id = room_id;
      password_error_.clear();
      return;
    case ChatRoomJoinStep::kEnter:
      if (auto sent =
              ro::net::RequestEnterChatRoom(context.link, room_, room_id, {}, context.now_ms);
          !sent) {
        ro::core::LogWarning("salon : {}", ro::core::Describe(sent.error()));
      }
      return;
  }
}

void ChatRoomStage::SayNews(ro::sim::World& world, const ro::vfs::ItemWords& words,
                            std::uint32_t now_ms) {
  const ChatRoomSaid said{.world = world, .words = words, .now_ms = now_ms};
  for (const ro::sim::ChatRoomNews& news : room_.TakeNews()) {
    switch (news.kind) {
      case ro::sim::ChatRoomNewsKind::kCreated:
        Say(said, ChatRoomText(words, ChatRoomWord::kRoomMade), kRoomMadeColor);
        break;
      case ro::sim::ChatRoomNewsKind::kTooManyRooms: {
        const std::string_view text = ChatRoomText(words, ChatRoomWord::kTooManyRooms);
        Say(said, text, kRefusalColor);
        SetError(std::string(text), ro::ui::ChatRoomField::kNone);
        break;
      }
      case ro::sim::ChatRoomNewsKind::kSameTitle: {
        // LE CAS QUI JUSTIFIE QUE LA FENÊTRE RESTE : un caractère à changer.
        const std::string_view text = ChatRoomText(words, ChatRoomWord::kSameTitle);
        Say(said, text, kRefusalColor);
        SetError(std::string(text), ro::ui::ChatRoomField::kTitle);
        break;
      }
      case ro::sim::ChatRoomNewsKind::kCreateSilent:
        SetError(ro::core::Tr("Aucune réponse du serveur. Réessaie."),
                 ro::ui::ChatRoomField::kNone);
        break;
      case ro::sim::ChatRoomNewsKind::kEnterRefused: {
        const std::string text = RefusalText(news.reason, words);
        Say(said, text, kRefusalColor);
        // L'INVITE RESTE OUVERTE, le motif dedans : corriger un mot de passe ne
        // demande pas de recliquer l'enseigne.
        password_error_ = text;
        break;
      }
      case ro::sim::ChatRoomNewsKind::kMemberJoined:
        room_.RecordLine(ro::sim::ChatKind::kServer, 0,
                         ro::vfs::ChatRoomTextWith(words, ChatRoomWord::kMemberEntered, news.name),
                         ro::net::kChatRoomArrivalColor, now_ms);
        break;
      case ro::sim::ChatRoomNewsKind::kMemberLeft:
      case ro::sim::ChatRoomNewsKind::kMemberExpelled:
        room_.RecordLine(
            ro::sim::ChatKind::kServer, 0,
            ro::vfs::ChatRoomTextWith(words,
                                      news.kind == ro::sim::ChatRoomNewsKind::kMemberLeft
                                          ? ChatRoomWord::kMemberLeft
                                          : ChatRoomWord::kMemberKicked,
                                      news.name),
            ro::net::kChatRoomDepartureColor, now_ms);
        break;
      case ro::sim::ChatRoomNewsKind::kExpelRefused: {
        // LE SERVEUR SE TAIT sur un membre protégé, le client d'origine aussi :
        // sans cette ligne, le clic n'aurait aucun effet visible.
        std::array<char, kSentenceBytes> text{};
        std::snprintf(text.data(), text.size(),
                      ro::core::Tr("%s n'a pas pu être expulsé : le serveur le protège."),
                      news.name.c_str());
        room_.RecordLine(ro::sim::ChatKind::kServer, 0, text.data(), kRefusalColor, now_ms);
        break;
      }
    }
  }
}

void ChatRoomStage::SetError(std::string text, ro::ui::ChatRoomField field) {
  error_ = std::move(text);
  error_field_ = field;
}

void ChatRoomStage::ComposeWords(const ro::vfs::ItemWords& words) {
  words_ =
      ro::ui::ChatRoomWordsView{.making_room = ChatRoomText(words, ChatRoomWord::kMakingRoom),
                                .room_setup = ChatRoomText(words, ChatRoomWord::kRoomSetup),
                                .title_label = ChatRoomText(words, ChatRoomWord::kTitleLabel),
                                .limit_label = ChatRoomText(words, ChatRoomWord::kLimitLabel),
                                .password_label = ChatRoomText(words, ChatRoomWord::kPasswordLabel),
                                .people_unit = ChatRoomText(words, ChatRoomWord::kPeopleUnit),
                                .public_word = ChatRoomText(words, ChatRoomWord::kPublic),
                                .private_word = ChatRoomText(words, ChatRoomWord::kPrivate),
                                .kick = ChatRoomText(words, ChatRoomWord::kKick),
                                .give_owner = ChatRoomText(words, ChatRoomWord::kGiveOwner),
                                .enter_password = ChatRoomText(words, ChatRoomWord::kEnterPassword),
                                .english_only = ChatRoomText(words, ChatRoomWord::kEnglishOnly)};
}

void ChatRoomStage::Compose(ChatRoomWindows& windows, const ChatRoomContext& context) {
  ComposeWords(context.words);
  nearby_ = NearbyChatRooms(context.world);

  windows.make_view = nullptr;
  if (windows.make.open) {
    nearby_views_.clear();
    for (const NearbyChatRoom& room : nearby_) {
      nearby_views_.push_back(ro::ui::ChatRoomNearbyView{
          .room_id = room.room_id,
          .title = room.title,
          .users = room.users,
          .limit = room.limit,
          .locked = room.status == ro::sim::kRoomPrivate,
          .joinable = (room.users < room.limit || room.limit == 0) &&
                      ChatRoomJoinStepOf(room.status) != ChatRoomJoinStep::kNone,
          .distance_cells = room.distance_cells});
    }
    blocker_ = BlockerText(ChatRoomCreateBlocker(context.world, context.stores), context.words);
    // LE TITRE DÉJÀ PRIS SE DIT PENDANT LA FRAPPE, et non après le refus.
    title_taken_.clear();
    if (const int taken = ChatRoomTitleTaken(nearby_, Typed(windows.make.title)); taken >= 0) {
      std::array<char, kSentenceBytes> sentence{};
      std::snprintf(sentence.data(), sentence.size(),
                    ro::core::Tr("Un salon visible porte déjà ce titre, à %d cases d'ici."),
                    nearby_[static_cast<std::size_t>(taken)].distance_cells);
      title_taken_ = sentence.data();
    }
    make_view_ = ro::ui::ChatRoomMakeView{.limits = Limits(),
                                          .words = words_,
                                          .waiting = room_.creating(),
                                          .error = error_,
                                          .error_field = error_field_,
                                          .blocker = blocker_,
                                          .title_taken = title_taken_,
                                          .ascii_required = true,
                                          .nearby = nearby_views_,
                                          .recent_titles = memory_.recent_titles};
    windows.make_view = &make_view_;
  }

  windows.room_view = nullptr;
  if (const std::optional<ro::sim::ChatRoomInside>& inside = room_.inside(); inside.has_value()) {
    // « Pub: titre (2/20) », comme la barre du client d'origine — une espace
    // après le préfixe, que sa traduction ne porte pas toujours.
    caption_ = std::string(ChatRoomText(context.words, inside->is_public()
                                                           ? ChatRoomWord::kPublicPrefix
                                                           : ChatRoomWord::kPrivatePrefix));
    if (!caption_.empty()) {
      caption_ += ' ';
    }
    caption_ += ShownTitle(inside->title) + " (" + std::to_string(inside->users) + "/" +
                std::to_string(inside->limit) + ")";
    members_.clear();
    for (const ro::sim::ChatRoomMember& member : inside->members) {
      members_.push_back(
          ro::ui::ChatRoomMemberView{.name = member.name,
                                     .owner = member.owner,
                                     .me = member.name == context.world.player_name()});
    }
    room_view_ = ro::ui::ChatRoomView{.limits = Limits(),
                                      .words = words_,
                                      .caption = caption_,
                                      .members = members_,
                                      .owned = inside->owned,
                                      .settings_error = settings_error_,
                                      .title = inside->title,
                                      .password = inside->password,
                                      .limit = inside->limit,
                                      .is_public = inside->is_public()};
    windows.room_view = &room_view_;
  }

  windows.password_view = nullptr;
  if (windows.password.open) {
    password_view_ = ro::ui::ChatRoomPasswordView{.limits = Limits(),
                                                  .words = words_,
                                                  .error = password_error_,
                                                  .waiting = !room_.EnterMayLeave(context.now_ms)};
    windows.password_view = &password_view_;
  }
}

}  // namespace moonclient
