#include "ro/sim/chat_room.h"

#include <algorithm>
#include <utility>

namespace ro::sim {

void ChatRoom::NoteCreateSent(ChatRoomSettings settings, std::uint32_t now_ms) {
  creating_ = PendingCreate{.settings = std::move(settings), .sent_ms = now_ms};
}

void ChatRoom::TellCreateAnswer(std::uint8_t status, std::string_view self_name) {
  // Une réponse sans demande vient d'une demande que ce client n'a pas faite :
  // rien à ouvrir, rien à dire.
  // Toute réponse libère aussi une demande d'entrée en attente.
  enter_sent_ms_.reset();
  std::optional<ChatRoomSettings> sent;
  if (creating_.has_value()) {
    sent = std::move(creating_->settings);
  } else {
    sent = std::move(late_create_);
  }
  creating_.reset();
  late_create_.reset();
  if (!sent.has_value()) {
    return;
  }
  switch (status) {
    case kChatRoomCreated: {
      // LA DEMANDE EST LA SEULE SOURCE : le créateur ne reçoit pas l'enseigne
      // de son salon, et la réponse ne dit que « c'est fait ».
      ChatRoomInside room;
      room.title = sent->title;
      room.status = sent->is_public ? kRoomPublic : kRoomPrivate;
      room.users = 1;
      room.limit = sent->limit;
      room.password = sent->is_public ? std::string() : sent->password;
      room.members.push_back(ChatRoomMember{.name = std::string(self_name), .owner = true});
      room.owned = true;
      Open(std::move(room));
      Say(ChatRoomNewsKind::kCreated);
      return;
    }
    case kChatRoomTooMany:
      Say(ChatRoomNewsKind::kTooManyRooms);
      return;
    case kChatRoomSameTitle:
      Say(ChatRoomNewsKind::kSameTitle);
      return;
    default:
      return;
  }
}

void ChatRoom::ExpireSilentCreate(std::uint32_t now_ms) {
  if (creating_.has_value() && now_ms - creating_->sent_ms > kChatRoomCreateSilenceMs) {
    late_create_ = std::move(creating_->settings);
    creating_.reset();
    Say(ChatRoomNewsKind::kCreateSilent);
  }
}

bool ChatRoom::EnterMayLeave(std::uint32_t now_ms) const noexcept {
  if (inside_.has_value()) {
    return false;
  }
  return !enter_sent_ms_.has_value() || now_ms - *enter_sent_ms_ > kChatRoomEnterSpacingMs;
}

void ChatRoom::NoteEnterSent(std::uint32_t now_ms) { enter_sent_ms_ = now_ms; }

void ChatRoom::TellEnterRefused(std::uint8_t reason) {
  // La réponse libère la demande : on peut retenter sans attendre la seconde.
  enter_sent_ms_.reset();
  Say(ChatRoomNewsKind::kEnterRefused, reason);
}

void ChatRoom::TellEntered(ChatRoomInside room, std::string_view self_name) {
  enter_sent_ms_.reset();
  room.owned = std::ranges::any_of(room.members, [self_name](const ChatRoomMember& member) {
    return member.owner && member.name == self_name;
  });
  room.users = static_cast<std::uint16_t>(room.members.size());
  Open(std::move(room));
}

void ChatRoom::TellBlindEntry() {
  enter_sent_ms_.reset();
  leave_owed_ = true;
}

void ChatRoom::TellMemberJoined(std::uint16_t users, std::string name) {
  if (!inside_.has_value()) {
    return;
  }
  inside_->users = users;
  // Le créateur peut recevoir son propre nom par ce chemin : ne pas le doubler.
  const bool known = std::ranges::any_of(
      inside_->members, [&name](const ChatRoomMember& member) { return member.name == name; });
  if (!known) {
    inside_->members.push_back(ChatRoomMember{.name = name, .owner = false});
  }
  Say(ChatRoomNewsKind::kMemberJoined, 0, std::move(name));
}

void ChatRoom::TellMemberLeft(std::uint16_t users, std::string_view name, bool expelled,
                              std::string_view self_name) {
  if (!inside_.has_value()) {
    return;
  }
  // C'EST LE JOUEUR QUI PART, ou qu'on expulse : le salon se ferme, sans un mot
  // dans le salon — il n'y est plus pour le lire.
  if (!self_name.empty() && name == self_name) {
    Leave();
    return;
  }
  inside_->users = users;
  std::erase_if(inside_->members,
                [name](const ChatRoomMember& member) { return member.name == name; });
  std::erase_if(expels_, [name](const PendingExpel& expel) { return expel.name == name; });
  Say(expelled ? ChatRoomNewsKind::kMemberExpelled : ChatRoomNewsKind::kMemberLeft, 0,
      std::string(name));
}

void ChatRoom::TellSettings(std::uint16_t limit, std::uint16_t users, std::uint8_t status,
                            std::string title) {
  if (!inside_.has_value()) {
    return;
  }
  inside_->limit = limit;
  inside_->users = users;
  inside_->status = status;
  inside_->title = std::move(title);
}

void ChatRoom::TellRole(std::uint32_t role, std::string_view name, std::string_view self_name) {
  if (!inside_.has_value()) {
    return;
  }
  const bool owner = role == kChatRoomOwnerRole;
  for (ChatRoomMember& member : inside_->members) {
    if (member.name == name) {
      member.owner = owner;
    }
  }
  if (!self_name.empty() && name == self_name) {
    inside_->owned = owner;
  }
}

void ChatRoom::TellRoomGone(std::uint32_t room_id) {
  if (inside_.has_value() && inside_->room_id == room_id && room_id != 0) {
    Leave();
  }
}

void ChatRoom::NoteExpelSent(std::string name, std::uint32_t now_ms) {
  if (!inside_.has_value()) {
    return;
  }
  // Une seconde demande pour le même membre relance la montre.
  std::erase_if(expels_, [&name](const PendingExpel& expel) { return expel.name == name; });
  expels_.push_back(PendingExpel{.name = std::move(name), .sent_ms = now_ms});
}

void ChatRoom::ExpireSilentExpels(std::uint32_t now_ms) {
  if (!inside_.has_value()) {
    expels_.clear();
    return;
  }
  std::erase_if(expels_, [this, now_ms](PendingExpel& expel) {
    if (now_ms - expel.sent_ms < kChatRoomExpelSilenceMs) {
      return false;
    }
    // ⚠ PARTI ENTRE-TEMPS SANS QU'ON L'AIT VU SORTIR, il n'y a rien à dire.
    const bool still_here = std::ranges::any_of(
        inside_->members,
        [&expel](const ChatRoomMember& member) { return member.name == expel.name; });
    if (still_here) {
      Say(ChatRoomNewsKind::kExpelRefused, 0, std::move(expel.name));
    }
    return true;
  });
}

void ChatRoom::NoteSettingsSent(const ChatRoomSettings& settings) {
  if (inside_.has_value()) {
    inside_->password = settings.is_public ? std::string() : settings.password;
  }
}

void ChatRoom::Leave() {
  inside_.reset();
  expels_.clear();
  lines_.clear();
}

void ChatRoom::RecordLine(ChatKind kind, std::uint32_t speaker_id, std::string text,
                          std::uint32_t color, std::uint32_t said_ms,
                          std::optional<ChatAuthor> author) {
  if (!inside_.has_value()) {
    return;
  }
  ++lines_seen_;
  lines_.push_back(ChatLine{.seq = lines_seen_,
                            .kind = kind,
                            .type = DefaultChatType(kind),
                            .speaker_id = speaker_id,
                            .text = std::move(text),
                            .said_ms = said_ms,
                            .color = color,
                            .author = std::move(author)});
  if (lines_.size() > kChatRoomLinesKept) {
    lines_.erase(lines_.begin());
  }
}

void ChatRoom::Reset() {
  // ⚠ LE RANG DES SALONS NE REPART PAS DE ZÉRO : la fenêtre reconnaît un salon
  // neuf à ce qu'il change, et le premier de la partie suivante porterait sinon
  // le même que le premier de celle-ci.
  const std::uint32_t seq = room_seq_;
  *this = ChatRoom();
  room_seq_ = seq;
}

void ChatRoom::Open(ChatRoomInside room) {
  // UN SALON N'EST PAS UNE CONVERSATION QUI SE POURSUIT : les lignes du
  // précédent n'ont rien à faire dans le suivant.
  lines_.clear();
  expels_.clear();
  inside_ = std::move(room);
  ++room_seq_;
}

void ChatRoom::Say(ChatRoomNewsKind kind, std::uint8_t reason, std::string name) {
  news_.push_back(ChatRoomNews{.kind = kind, .reason = reason, .name = std::move(name)});
}

}  // namespace ro::sim
