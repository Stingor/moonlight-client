#include "ro/net/whisper.h"

#include <algorithm>
#include <optional>
#include <string>
#include <string_view>
#include <utility>

#include "ro/core/ascii.h"
#include "ro/core/text.h"
#include "ro/proto/chat_limits.h"
#include "ro/proto/manual_packets.h"
#include "ro/proto/map_packets.h"
#include "ro/sim/chat_channel.h"
#include "ro/sim/world.h"

#include "frame_apply.h"

namespace ro::net {
namespace {

// LE DESTINATAIRE D'UN MESSAGE À UN PNJ : le serveur le lit comme un
// évènement de script, et n'envoie aucun accusé — quand ce PNJ existe. ⚠ S'IL
// N'EXISTE PAS, le serveur traite la ligne en chuchotement ordinaire, et
// l'accuse « absent » ; ce client ne peut pas le savoir d'avance, et cet
// accusé-là ne retrouvera pas son envoi.
constexpr std::string_view kNpcWhisperPrefix = "NPC:";
// Un nom de PNJ plus court que cela n'est pas lu comme un PNJ.
constexpr std::size_t kShortestNpcWhisperTarget = kNpcWhisperPrefix.size() + 1;

// LA RÉPONSE QUI REMPLACE L'ACCUSÉ quand le destinataire tient une échoppe
// hors ligne : le serveur la chuchote lui-même, après le nom du destinataire
// (`clif_parse_WisMessage`). C'est le seul envoi qui reçoit autre chose qu'un
// accusé, et il faut le retirer de la file, sans quoi tous les accusés
// suivants se décaleraient d'un rang.
//
// 🔴 ELLE SE RECONNAÎT À SA PHRASE ET AU NOM QU'ELLE CITE, JAMAIS À SON AUTEUR :
// le nom sous lequel le serveur chuchote est un réglage de sa configuration, que
// ce client ne peut pas connaître. Le nom cité doit être celui d'un envoi EN
// ATTENTE. ⚠ LA LIMITE : un joueur qui devinerait à qui l'on vient d'écrire, et
// le lui chuchoterait dans la seconde, retirerait cet envoi — son écho ne
// paraîtrait pas, et rien d'autre.
constexpr std::string_view kAutotradeReplySuffix =
    " is in autotrade mode and cannot receive whispered messages.";

[[nodiscard]] bool IsGuildMember(std::string_view name, const sim::GuildBook& guild) {
  return std::ranges::any_of(
      guild.members(), [name](const sim::GuildMember& member) { return member.name == name; });
}

}  // namespace

sim::WhisperRelation WhisperRelationOf(std::string_view name, bool from_staff,
                                       const WhisperCircle& circle) {
  if (circle.friends != nullptr && circle.friends->HasName(name)) {
    return sim::WhisperRelation::kFriend;
  }
  if (from_staff) {
    return sim::WhisperRelation::kStaff;
  }
  if (circle.guild != nullptr && IsGuildMember(name, *circle.guild)) {
    return sim::WhisperRelation::kGuildMember;
  }
  return sim::WhisperRelation::kStranger;
}

FrameOutcome ApplyWhisperFrame(const Frame& frame, sim::World& world, const WhisperCircle& circle,
                               std::uint32_t now_ms) {
  switch (frame.opcode) {
    case proto::WhisperNotice::kOpcode:
      return Decoded<proto::WhisperNotice>(frame, [&](const auto& packet) {
        // ⚠ IL NOMME SON AUTEUR SANS DIRE OÙ IL EST, et c'est normal : un
        // chuchotement traverse les cartes, et son auteur n'est pas forcément
        // dans le monde qu'on tient. La ligne porte donc son NOM.
        std::string sender = core::Utf8FromWire(packet.sender);
        std::string message = core::Utf8FromWire(packet.message);
        // LA RÉPONSE D'UNE ÉCHOPPE HORS LIGNE, pour un envoi qui l'attendait : le
        // serveur parle à la place du destinataire. Elle retire l'envoi, et se
        // range comme ce que le serveur écrit au joueur — ni conversation à
        // ouvrir, ni alarme.
        if (circle.outbox != nullptr && message.ends_with(kAutotradeReplySuffix) &&
            circle.outbox->Forget(std::string_view(message).substr(
                0, message.size() - kAutotradeReplySuffix.size()))) {
          world.RecordChat(sim::ChatKind::kServer, 0, std::move(message), now_ms);
          return FrameOutcome::kApplied;
        }
        const sim::WhisperRelation relation = WhisperRelationOf(sender, packet.from_staff, circle);
        world.RecordWhisper(sim::WhisperFacts{.side = sim::WhisperSide::kReceived,
                                              .peer_name = std::move(sender),
                                              .relation = relation,
                                              .from_staff = packet.from_staff,
                                              .sender_id = packet.sender_id},
                            std::move(message), now_ms);
        return FrameOutcome::kApplied;
      });

    case proto::ZcAckWhisper::kOpcode:
      return Decoded<proto::ZcAckWhisper>(frame, [&](const auto& packet) {
        // CE QU'EST DEVENU LE CHUCHOTEMENT QU'ON VIENT D'ENVOYER.
        //
        // ⚠ SANS LUI, UN CHUCHOTEMENT À QUELQU'UN QUI N'EST PAS LÀ EST MUET : le
        // serveur n'envoie RIEN d'autre, et le joueur attend une réponse qui ne
        // viendra jamais. C'est le seul paquet qui le dise.
        std::optional<sim::PendingWhisper> settled;
        if (circle.outbox != nullptr) {
          settled = circle.outbox->Settle(now_ms);
        }
        const auto refusal = static_cast<sim::WhisperRefusal>(packet.result);
        if (refusal == sim::WhisperRefusal::kNone) {
          // PARTI. L'écho se compose de ce qu'on avait gardé ; sans l'envoi,
          // il n'y a rien à montrer.
          if (!settled.has_value()) {
            return FrameOutcome::kIgnored;
          }
          const sim::WhisperRelation relation =
              WhisperRelationOf(settled->recipient, /*from_staff=*/false, circle);
          world.RecordWhisper(sim::WhisperFacts{.side = sim::WhisperSide::kSent,
                                                .peer_name = std::move(settled->recipient),
                                                .relation = relation},
                              std::move(settled->text), now_ms);
          return FrameOutcome::kApplied;
        }
        // ⚠ SANS TEXTE : la phrase se choisit à l'affichage, d'après le code,
        // dans la langue de l'interface.
        sim::WhisperFacts refused{.side = sim::WhisperSide::kUndelivered, .refusal = refusal};
        if (settled.has_value()) {
          refused.relation = WhisperRelationOf(settled->recipient, /*from_staff=*/false, circle);
          refused.peer_name = std::move(settled->recipient);
        }
        world.RecordWhisper(std::move(refused), std::string(), now_ms);
        return FrameOutcome::kApplied;
      });

    default:
      return FrameOutcome::kUnhandled;
  }
}

bool NoteWhisperSent(sim::WhisperOutbox& outbox, const ChatIntent& intent, std::uint32_t sent_ms,
                     const WhisperNoting& noting) {
  if (intent.target != ChatTarget::kWhisper || intent.recipient.empty() || intent.text.empty()) {
    return false;
  }
  // MUET : le serveur jette la ligne sans un mot, donc sans accusé.
  if (noting.muted) {
    return false;
  }
  // UN CANAL : « /w #nom » et la case « Pseudo » y mènent aussi — s'il en est
  // un pour le serveur. Le canal d'alliance n'y existe qu'en guilde, que
  // cette couche ne connaît pas : il reste un canal.
  if (intent.recipient.front() == sim::kChannelPrefix) {
    const std::string_view name = std::string_view(intent.recipient).substr(1);
    const bool taken =
        noting.channels.empty() ||
        std::ranges::any_of(noting.channels, [name](const sim::ChatChannel& one) {
          return core::AsciiEqualsFolded(one.name, name) && (one.can_write || one.guild_only);
        });
    if (taken) {
      return false;
    }
  }
  // UN PNJ : le serveur lit le préfixe sans égard pour la casse.
  if (intent.recipient.size() >= kShortestNpcWhisperTarget &&
      core::AsciiStartsWithFolded(intent.recipient, kNpcWhisperPrefix)) {
    return false;
  }
  // UNE COMMANDE : le serveur l'exécute et ne chuchote rien, donc n'accuse rien.
  if (IsServerCommandText(intent.text)) {
    return false;
  }
  // UN TEXTE TROP LONG : le serveur le refuse sans un mot, donc sans accusé.
  if (core::WireFromUtf8Text(intent.text).size() > proto::kChatTextMax) {
    return false;
  }
  outbox.Note(intent.recipient, intent.text, sent_ms);
  return true;
}

}  // namespace ro::net
