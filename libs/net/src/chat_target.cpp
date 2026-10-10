#include "ro/net/chat_target.h"

#include <utility>
#include <vector>

#include "ro/core/ascii.h"
#include "ro/core/text.h"
#include "ro/proto/battleground_packets.h"
#include "ro/proto/manual_packets.h"
#include "ro/sim/chat_channel.h"

namespace ro::net {
namespace {

using core::ErrorCode;

// Retire les espaces de tête. Un joueur qui tape « % salut » vise son groupe.
[[nodiscard]] std::string_view Trimmed(std::string_view text) {
  while (!text.empty() && text.front() == ' ') {
    text.remove_prefix(1);
  }
  return text;
}

template <typename Packet>
[[nodiscard]] core::Result<void> Send(Connection& link, const Packet& packet) {
  core::Result<std::vector<std::uint8_t>> frame = proto::Encode(packet);
  if (!frame) {
    return std::unexpected(frame.error());
  }
  link.Send(*frame);
  return {};
}

// LE MOT DE LA COMMANDE DU CHAMP, sans le blanc qui le sépare du texte : « /bc »
// seul vise le champ et ne dit rien.
[[nodiscard]] constexpr std::string_view BattlefieldCommandWord() {
  return kBattlefieldCommand.substr(0, kBattlefieldCommand.find(' '));
}

// CE QUE DIT UNE LIGNE SANS SIGIL : la barre décide, box « Pseudo » d'abord,
// mode armé ensuite.
[[nodiscard]] ChatIntent ArmedIntent(std::string_view line, const ChatArming& armed) {
  ChatIntent intent;
  intent.text = std::string(line);
  if (!armed.whisper_to.empty()) {
    // La box « Pseudo » passe devant le mode : un nom écrit là EST une
    // destination, et il est plus précis qu'un mode.
    intent.target = ChatTarget::kWhisper;
    intent.recipient = std::string(armed.whisper_to);
    return intent;
  }
  // ⚠ LE CHUCHOTEMENT NE S'ARME PAS EN MODE : il lui faut un destinataire, et
  // un mode n'en porte aucun. L'accepter enverrait la ligne dans le vide.
  if (armed.mode == ChatTarget::kWhisper) {
    intent.target = ChatTarget::kPublic;
    return intent;
  }
  // ⚠ ET UN CANAL ARMÉ SANS NOM NON PLUS, pour la même raison. Le nom, lui,
  // vient de la barre : c'est la seule destination armable qui en porte un.
  if (armed.mode == ChatTarget::kChannel) {
    if (armed.channel.empty()) {
      intent.target = ChatTarget::kPublic;
      return intent;
    }
    intent.target = ChatTarget::kChannel;
    intent.recipient = std::string(armed.channel);
    return intent;
  }
  intent.target = armed.mode;
  return intent;
}

}  // namespace

ChatIntent ReadChatIntent(std::string_view line, const ChatArming& armed) {
  ChatIntent intent;
  // ⚠ UNE LIGNE VIDE NE PORTE AUCUN SIGIL, ET SA DESTINATION EST CELLE QUE LA
  // BARRE A ARMÉE : c'est ce que l'étiquette doit montrer avant la première
  // lettre. Elle passe donc par la branche sans sigil, texte vide — et un texte
  // vide ne s'envoie pas.
  if (line.empty()) {
    return ArmedIntent(line, armed);
  }

  const bool short_whisper = core::AsciiStartsWithFolded(line, kWhisperCommand);
  if (short_whisper || core::AsciiStartsWithFolded(line, kWhisperCommandLong)) {
    const std::size_t after = short_whisper ? kWhisperCommand.size() : kWhisperCommandLong.size();
    const std::string_view rest = Trimmed(line.substr(after));
    const std::size_t space = rest.find(' ');
    intent.target = ChatTarget::kWhisper;
    if (space == std::string_view::npos) {
      // Un nom sans message : rien à envoyer, et surtout pas un chuchotement
      // vide qui ferait paraître une fenêtre chez l'autre.
      return intent;
    }
    intent.recipient = std::string(rest.substr(0, space));
    intent.text = std::string(Trimmed(rest.substr(space + 1)));
    if (intent.recipient.empty()) {
      intent.text.clear();
    }
    return intent;
  }

  // « /bc texte » parle au champ sans toucher au mode de la barre.
  if (core::AsciiStartsWithFolded(line, kBattlefieldCommand) ||
      core::AsciiEqualsFolded(line, BattlefieldCommandWord())) {
    intent.target = ChatTarget::kBattlefield;
    intent.text = std::string(Trimmed(line.substr(BattlefieldCommandWord().size())));
    return intent;
  }

  // ⚠ « & » N'EST UN SIGIL QUE SUR UNE CARTE DE CHAMP : ailleurs, la ligne ne
  // dit rien de sa destination, et c'est la barre qui décide.
  if (line.front() == kBattlefieldSigil && !armed.on_battlefield) {
    return ArmedIntent(line, armed);
  }
  switch (line.front()) {
    case kPartySigil:
      intent.target = ChatTarget::kParty;
      break;
    case kGuildSigil:
      intent.target = ChatTarget::kGuild;
      break;
    case kAllySigil:
      intent.target = ChatTarget::kAlly;
      break;
    case kBattlefieldSigil:
      intent.target = ChatTarget::kBattlefield;
      break;
    default:
      // ⚠ AUCUN SIGIL : c'est ici, et seulement ici, que la barre a son mot à
      // dire. Une ligne qui porte un sigil a déjà dit où elle va.
      return ArmedIntent(line, armed);
  }
  intent.text = std::string(Trimmed(line.substr(1)));
  return intent;
}

bool IsSpokenLine(std::string_view line) {
  // Lue sans barre armée : seule la ligne elle-même peut l'adresser.
  return !line.starts_with('/') || ReadChatIntent(line).target != ChatTarget::kPublic;
}

core::Result<void> SendChat(Connection& link, std::string_view speaker, const ChatIntent& intent,
                            bool on_battlefield) {
  if (intent.text.empty()) {
    return std::unexpected(core::Error{ErrorCode::kInvalidArgument, "rien a dire"});
  }
  // ⚠ LE TEXTE PART DANS LA PAGE DE CODES DU JEU. Le serveur relaie les octets
  // tels quels : envoyé en UTF-8, un accent arriverait en deux caractères chez
  // les autres joueurs.
  // ⚠ LA PHRASE BASCULE EN UTF-8 SI ELLE NE TIENT PAS EN 1252 — un emoji —
  // mais le NOM, lui, reste en 1252 : le serveur le compare à sa base.
  const std::string text = core::WireFromUtf8Text(intent.text);
  const std::string name = core::Cp1252FromUtf8(speaker);

  switch (intent.target) {
    case ChatTarget::kParty: {
      proto::PartyChatRequest request;
      request.speaker = name;
      request.text = text;
      return Send(link, request);
    }
    case ChatTarget::kClan: {
      proto::ClanChatRequest request;
      request.speaker = name;
      request.text = text;
      return Send(link, request);
    }
    case ChatTarget::kGuild: {
      proto::GuildChatRequest request;
      request.speaker = name;
      request.text = text;
      request.battlefield = on_battlefield;
      return Send(link, request);
    }
    case ChatTarget::kAlly: {
      // Le canal passe par le chuchotement, et son destinataire est son NOM.
      proto::WhisperRequest request;
      request.recipient = std::string(proto::WhisperRequest::kAllyChannel);
      request.text = text;
      return Send(link, request);
    }
    case ChatTarget::kChannel: {
      // ⚠ LE CROISILLON SE REMET ICI, et nulle part ailleurs : le monde range
      // les canaux SANS lui, parce que c'est ainsi que le serveur les range
      // lui-même — mais c'est sur lui que `clif_parse_WisMessage` reconnaît un
      // canal. L'oublier chuchoterait à un joueur du même nom.
      if (intent.recipient.empty()) {
        return std::unexpected(core::Error{ErrorCode::kInvalidArgument, "canal sans nom"});
      }
      proto::WhisperRequest request;
      request.recipient =
          std::string(1, sim::kChannelPrefix) + core::Cp1252FromUtf8(intent.recipient);
      request.text = text;
      return Send(link, request);
    }
    case ChatTarget::kWhisper: {
      proto::WhisperRequest request;
      request.recipient = core::Cp1252FromUtf8(intent.recipient);
      request.text = text;
      return Send(link, request);
    }
    case ChatTarget::kPublic: {
      proto::ChatRequest request;
      request.speaker = name;
      request.text = text;
      return Send(link, request);
    }
    case ChatTarget::kBattlefield: {
      // « Nom : texte », comme la parole publique : le serveur compare le nom
      // au personnage, puis relaie la ligne telle quelle à l'équipe.
      proto::BattlefieldChatRequest request;
      request.speaker = name;
      request.text = text;
      return Send(link, request);
    }
  }
  return std::unexpected(core::Error{ErrorCode::kInvalidArgument, "destination inconnue"});
}

core::Result<void> SendServerCommand(Connection& link, std::string_view speaker,
                                     std::string_view command) {
  // ⚠ LA DESTINATION EST LA PAROLE PUBLIQUE, et le serveur l'intercepte avant
  // de la dire à quiconque : `clif_process_message` passe la ligne à
  // `is_atcommand` et s'arrête là si c'en était une. Rien n'est prononcé.
  ChatIntent intent;
  intent.target = ChatTarget::kPublic;
  intent.text = std::string(command);
  return SendChat(link, speaker, intent);
}

}  // namespace ro::net
