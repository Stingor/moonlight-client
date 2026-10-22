#include "chat/chat_line_preview.h"

#include <algorithm>
#include <array>
#include <cstdint>
#include <optional>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

#include <imgui.h>

#include "ro/core/ascii.h"
#include "ro/core/i18n.h"
#include "ro/core/printed.h"
#include "ro/core/text.h"
#include "ro/net/chat_room.h"
#include "ro/net/chat_target.h"
#include "ro/proto/manual_packets.h"
#include "ro/sim/chat_channel.h"

#include "chat/announce_command.h"
#include "chat/chat_line_color.h"

namespace moonclient {
namespace {

using ro::net::ChatTarget;
using ro::sim::ChatKind;

// LA COMMANDE DU SERVEUR QUI LIE LA PAROLE PUBLIQUE À UN CANAL, son alias, et
// ses deux verbes : « @channel bindto #nom », « @channel unbind ».
constexpr std::string_view kChannelCommand = "channel";
constexpr std::string_view kChannelCommandAlias = "main";
constexpr std::string_view kBindVerb = "bindto";
constexpr std::string_view kUnbindVerb = "unbind";
// Les mots qu'on en lit : la commande, le verbe, le canal.
constexpr std::size_t kChannelCommandWords = 3;
// Ce qui sépare les mots d'une commande.
constexpr std::string_view kCommandBlanks = " \t";

// Ce qui sépare deux destinations dans l'aide de la barre, comme dans sa phrase
// traduite.
constexpr std::string_view kHelpEntryGap = "    ";

// Le libellé d'une destination, traduit. Le chuchotement n'en a pas : c'est le
// nom du correspondant qui s'affiche, et il n'est pas à traduire.
//
// 🔴 DANS UN SALON, LA PAROLE À VOIX HAUTE N'EST PLUS « ALENTOUR ». Le serveur
// la remet aux seuls occupants du salon (`clif_parse_GlobalMessage` envoie en
// `CHAT_WOS` dès que `sd->chatID` est posé, au lieu d'`AREA_CHAT_WOC`) : les
// passants de la carte ne l'entendent plus. L'annoncer « Alentour » mentirait
// sur qui la lira. Seule exception côté serveur : un canal lié par
// `@channel bindto` passe devant, et le libellé est alors le sien (`Refresh`).
[[nodiscard]] const char* LabelOfTarget(ChatTarget target, bool in_chat_room) {
  switch (target) {
    case ChatTarget::kPublic:
      return in_chat_room ? ro::core::Tr("Salon") : ro::core::Tr("Alentour");
    case ChatTarget::kParty:
      return ro::core::Tr("Groupe");
    case ChatTarget::kGuild:
      return ro::core::Tr("Guilde");
    case ChatTarget::kClan:
      return ro::core::Tr("Clan");
    case ChatTarget::kAlly:
      return ro::core::Tr("Alliés");
    case ChatTarget::kWhisper:
      return ro::core::Tr("Chuchotement");
    case ChatTarget::kChannel:
      // Un canal porte son propre libellé — son alias, ou son nom : c'est
      // justement ce qu'aucun mot traduit ne peut dire à sa place.
      return "";
    case ChatTarget::kBattlefield:
      return ro::core::Tr("Champ de bataille");
  }
  return "";
}

// Le nom du canal des alliés tel que le monde le range, sans son croisillon.
[[nodiscard]] std::string_view AllyChannelName() {
  return ro::proto::WhisperRequest::kAllyChannel.substr(1);
}

// Ce que pèse sur le fil un mot que le serveur relaie en page de codes du jeu :
// un nom, un alias.
[[nodiscard]] int WireBytesOf(std::string_view utf8) {
  return static_cast<int>(ro::core::Cp1252FromUtf8(utf8).size());
}

// LE CANAL DE CE NOM parmi ceux que le serveur a poussés, ou rien. ⚠ SANS TENIR
// COMPTE DE LA CASSE : le serveur range ses canaux ainsi, et « /w #Global »
// atteint « #global ».
[[nodiscard]] const ro::sim::ChatChannel* FindChannel(
    std::span<const ro::sim::ChatChannel> channels, std::string_view name) {
  const auto found = std::ranges::find_if(channels, [name](const ro::sim::ChatChannel& channel) {
    return ro::core::AsciiEqualsFolded(channel.name, name);
  });
  return found != channels.end() ? &*found : nullptr;
}

// LA BORNE DE DIFFUSION d'une ligne : ce que son relais garde, et ce qu'il y met
// AVANT le texte du joueur.
struct BroadcastBound {
  int line_max = 0;
  int overhead = 0;
};

// La plus serrée de deux bornes : celle qui laisse le moins au texte.
[[nodiscard]] BroadcastBound Tighter(const BroadcastBound& left, const BroadcastBound& right) {
  return left.line_max - left.overhead <= right.line_max - right.overhead ? left : right;
}

// Un canal diffuse « alias nom : texte ». ⚠ UN CANAL DONT LE SERVEUR N'A PAS DIT
// L'ALIAS compte le plus long qu'il puisse porter : compter moins promettrait une
// place que le relais couperait.
[[nodiscard]] BroadcastBound ChannelBound(std::string_view channel_name, int named_bytes,
                                          std::span<const ro::sim::ChatChannel> channels) {
  const ro::sim::ChatChannel* channel = FindChannel(channels, channel_name);
  const int alias_bytes = channel != nullptr ? WireBytesOf(channel->alias)
                                             : static_cast<int>(ro::proto::kChatChannelAliasMax);
  return BroadcastBound{.line_max = kWireChannelLineMax,
                        .overhead = alias_bytes +
                                    static_cast<int>(ro::proto::kChatChannelAliasSeparator.size()) +
                                    named_bytes};
}

// LA BORNE QUE LE RELAIS DE CETTE DESTINATION APPLIQUE, quand il en a une.
//
// ⚠ LA LIGNE QUI PART ET CELLE QUI EST DIFFUSÉE NE SONT PAS LA MÊME. Un
// chuchotement ou un canal part sans le nom de qui parle — le serveur le sait
// par la connexion —, mais la ligne qu'un canal DIFFUSE le porte, alias en
// tête. Les destinations qui portent le nom l'exigent en tête du texte, et
// rAthena REJETTE le message s'il ne correspond pas.
//
// ⚠ ET LE NOM DIFFUSÉ N'EST PAS TOUJOURS LE SIEN : un faux nom le remplace
// partout sauf dans un canal, qui garde le vrai.
[[nodiscard]] std::optional<BroadcastBound> BroadcastOf(const ro::net::ChatIntent& intent,
                                                        const ChatWireSpeaker& speaker) {
  const int separator_bytes = static_cast<int>(ro::proto::ChatRequest::kNameSeparator.size());
  const int name_bytes = WireBytesOf(speaker.name);
  const int real_named = name_bytes + separator_bytes;
  const int shown_named =
      (speaker.may_wear_fake_name
           ? std::max(name_bytes, static_cast<int>(ro::proto::kChatShownNameMax))
           : name_bytes) +
      separator_bytes;
  switch (intent.target) {
    case ChatTarget::kPublic: {
      const BroadcastBound spoken{.line_max = kWireNamedBodyMax, .overhead = shown_named};
      // 🔴 LIÉE À UN CANAL, LA PAROLE À VOIX HAUTE PART À SON RELAIS, salon
      // compris. Il ne la prend que si le joueur a le droit d'y parler, ce que
      // la liste ne dit pas pour un canal qu'il administre : la plus serrée des
      // deux bornes ne coupe jamais.
      if (speaker.bound_channel.empty()) {
        return spoken;
      }
      return Tighter(spoken, ChannelBound(speaker.bound_channel, real_named, speaker.channels));
    }
    case ChatTarget::kParty:
    case ChatTarget::kBattlefield:
      return BroadcastBound{.line_max = kWireNamedBodyMax, .overhead = shown_named};
    case ChatTarget::kGuild:
      return BroadcastBound{.line_max = speaker.battlefield ? kWireNamedBodyMax : kWireGuildBodyMax,
                            .overhead = shown_named};
    case ChatTarget::kClan:
      return std::nullopt;
    case ChatTarget::kAlly:
      return ChannelBound(AllyChannelName(), real_named, speaker.channels);
    case ChatTarget::kChannel:
      return ChannelBound(intent.recipient, real_named, speaker.channels);
    case ChatTarget::kWhisper:
      // « /w #nom » mène à un canal, et c'est son relais qui diffuse.
      if (!intent.recipient.empty() && intent.recipient.front() == ro::sim::kChannelPrefix) {
        return ChannelBound(std::string_view(intent.recipient).substr(1), real_named,
                            speaker.channels);
      }
      return std::nullopt;
  }
  return std::nullopt;
}

// ⚠ CE QUI PART, ET NON CE QUI EST TAPÉ : un emoji tient sur un caractère à
// l'écran et pèse quatre octets sur le fil, parce que la phrase bascule alors
// en UTF-8. Compter autrement annoncerait une place qu'on n'a pas.
[[nodiscard]] ChatWireRoom RoomOf(const ro::net::ChatIntent& intent,
                                  const ChatWireSpeaker& speaker) {
  const int spent = static_cast<int>(ro::core::WireFromUtf8Text(intent.text).size());
  const ChatWireRoom text_room{.bytes_left = kWireTextMax - spent,
                               .text_max = kWireTextMax,
                               .overflow = ChatOverflow::kDropped};
  const std::optional<BroadcastBound> broadcast = BroadcastOf(intent, speaker);
  if (!broadcast.has_value()) {
    return text_room;
  }
  const ChatWireRoom broadcast_room{.bytes_left = broadcast->line_max - broadcast->overhead - spent,
                                    .text_max = broadcast->line_max - broadcast->overhead,
                                    .overflow = ChatOverflow::kTruncated};
  ChatWireRoom room = broadcast_room.bytes_left < text_room.bytes_left ? broadcast_room : text_room;
  // ⚠ LE MOTIF DIT CE QUI ARRIVERAIT VRAIMENT, et non la borne la plus serrée :
  // un texte qui dépasse le rejet est jeté entier, même quand la diffusion
  // l'aurait coupé plus tôt. La place, elle, reste celle de la plus serrée.
  if (spent > kWireTextMax) {
    room.overflow = ChatOverflow::kDropped;
  }
  return room;
}

// Ce que le serveur met devant une annonce pour la rejouer en commande.
constexpr std::string_view kBroadcastReplayPrefix = "@kami ";
constexpr std::string_view kLocalBroadcastReplayPrefix = "@lkami ";

// LA PLACE D'UNE ANNONCE DU STAFF, quand la ligne en est une.
//
// ⚠ ELLE NE PASSE NI PAR LE REJET NI PAR LA DIFFUSION d'une parole : le serveur
// la rejoue en commande dans son tampon de chat, préfixe devant, et coupe ce qui
// n'y tient pas. Le nom, quand l'annonce le porte, est compris dans ce qui part.
[[nodiscard]] std::optional<ChatWireRoom> AnnounceRoomOf(std::string_view outgoing,
                                                         std::string_view speaker) {
  const AnnounceCommand command = ReadAnnounceCommand(outgoing);
  if (command.kind != AnnounceCommandKind::kBroadcast &&
      command.kind != AnnounceCommandKind::kLocalBroadcast) {
    return std::nullopt;
  }
  const std::string_view replay = command.kind == AnnounceCommandKind::kBroadcast
                                      ? kBroadcastReplayPrefix
                                      : kLocalBroadcastReplayPrefix;
  const int line_max = static_cast<int>(ro::proto::kChatServerBufferBytes -
                                        ro::proto::kChatTerminatorBytes - replay.size());
  const int named_bytes =
      command.named
          ? WireBytesOf(speaker) + static_cast<int>(ro::proto::ChatRequest::kNameSeparator.size())
          : 0;
  return ChatWireRoom{.bytes_left = line_max - WireBytesOf(AnnouncedText(command, speaker)),
                      .text_max = line_max - named_bytes,
                      .overflow = ChatOverflow::kTruncated};
}

// LA PLACE D'UNE LIGNE, annonce ou parole : la règle unique du compteur et de la
// retenue.
[[nodiscard]] ChatWireRoom LineRoomOf(std::string_view outgoing, const ro::net::ChatIntent& intent,
                                      const ChatWireSpeaker& speaker) {
  if (const std::optional<ChatWireRoom> announce = AnnounceRoomOf(outgoing, speaker.name);
      announce.has_value()) {
    return *announce;
  }
  return RoomOf(intent, speaker);
}

// LA TEINTE D'UN CANAL, telle que le SERVEUR la donne — et personne d'autre ne
// peut la donner : c'est la moitié de la raison d'être du paquet qui la porte.
// Celle qu'il ne nomme pas est celle des canaux dans le fil.
[[nodiscard]] ro::ui::ChatSendMode ChannelLook(std::uint32_t rgb) {
  return ro::ui::ChatSendMode{.color = ColorOfChatLine(ChatKind::kChannel, rgb),
                              .backdrop = kChatLineBackdropColor};
}

// LA TEINTE D'UNE DESTINATION : celle que prendra la ligne une fois revenue dans
// le fil. ⚠ C'EST LA MÊME TABLE QUE CELLE DU FIL, et non une table à elle : une
// entrée « Groupe » d'une couleur et une phrase de groupe d'une autre font
// croire au joueur que sa phrase est partie ailleurs.
[[nodiscard]] ro::ui::ChatSendMode LookOfTarget(ChatTarget target, bool in_chat_room,
                                                std::span<const ro::sim::ChatChannel> channels) {
  const auto of_kind = [](ChatKind kind) {
    return ro::ui::ChatSendMode{.color = ColorOfChatKind(kind), .backdrop = kChatLineBackdropColor};
  };
  switch (target) {
    case ChatTarget::kPublic:
      // ⚠ DANS UN SALON, LA LIGNE REVIENT DANS SA FENÊTRE, claire : avec la
      // teinte de son propre écho, sur le fond ordinaire. Hors salon, la parole
      // à voix haute est l'état ordinaire, et la teinter ferait croire à un
      // mode armé.
      if (in_chat_room) {
        return ro::ui::ChatSendMode{
            .color = ColorOfChatLine(ChatKind::kSelf, ro::net::kChatRoomOwnSpeechColor)};
      }
      return ro::ui::ChatSendMode{};
    case ChatTarget::kParty:
      return of_kind(ChatKind::kParty);
    case ChatTarget::kGuild:
      return of_kind(ChatKind::kGuild);
    case ChatTarget::kClan:
      return of_kind(ChatKind::kClan);
    case ChatTarget::kAlly: {
      // ⚠ LES ALLIÉS SONT UN CANAL, et leurs lignes reviennent dans la teinte
      // que le serveur donne à `#ally` — quand il l'a poussé.
      const ro::sim::ChatChannel* ally = FindChannel(channels, AllyChannelName());
      return ChannelLook(ally != nullptr ? ally->color : 0);
    }
    case ChatTarget::kWhisper:
      return of_kind(ChatKind::kWhisper);
    case ChatTarget::kChannel:
      // Chaque canal a la sienne : voir `ChannelLook`, appelée canal par canal.
      return ChannelLook(0);
    case ChatTarget::kBattlefield:
      return of_kind(ChatKind::kBattlefield);
  }
  return ro::ui::ChatSendMode{};
}

}  // namespace

void ChatLinePreviewMaker::FollowSentCommand(std::string_view text,
                                             std::span<const ro::sim::ChatChannel> channels) {
  if (text.empty() || text.front() != ro::net::kAtCommandSymbol) {
    return;
  }
  // Les mots de la commande, comme le serveur les découpe : aux blancs.
  std::array<std::string_view, kChannelCommandWords> words{};
  std::size_t count = 0;
  std::string_view rest = text.substr(1);
  while (count < words.size()) {
    const std::size_t start = rest.find_first_not_of(kCommandBlanks);
    if (start == std::string_view::npos) {
      break;
    }
    rest.remove_prefix(start);
    const std::size_t end = std::min(rest.find_first_of(kCommandBlanks), rest.size());
    words[count++] = rest.substr(0, end);
    rest.remove_prefix(end);
  }
  // ⚠ LE NOM DE LA COMMANDE SE COMPARE SANS CASSE, SON ALIAS À L'IDENTIQUE :
  // c'est ainsi que le serveur range l'un et l'autre.
  const std::string_view command = words[0];
  if (!ro::core::AsciiEqualsFolded(command, kChannelCommand) && command != kChannelCommandAlias) {
    return;
  }
  const std::string_view verb = words[1];
  if (ro::core::AsciiEqualsFolded(verb, kUnbindVerb)) {
    bound_channel_.clear();
    return;
  }
  const std::string_view named = words[2];
  if (!ro::core::AsciiEqualsFolded(verb, kBindVerb) || named.empty() ||
      named.front() != ro::sim::kChannelPrefix) {
    return;
  }
  const std::string_view name = named.substr(1);
  if (name.empty()) {
    return;
  }
  const ro::sim::ChatChannel* known = FindChannel(channels, name);
  bound_channel_ = known != nullptr ? known->name : std::string(name);
}

void ChatLinePreviewMaker::Refresh(const char* outgoing, const ChatWireSpeaker& speaker,
                                   const ro::net::ChatArming& armed, bool in_chat_room) {
  const std::string_view line =
      outgoing != nullptr ? std::string_view(outgoing) : std::string_view();
  const std::span<const ro::sim::ChatChannel> channels = speaker.channels;
  // La règle du sigil et du mode armé vit dans `net`, avec les sigils : voir
  // `ReadChatIntent`.
  intent_ = ro::net::ReadChatIntent(line, armed);

  // 🔴 LIÉE À UN CANAL, LA PAROLE À VOIX HAUTE EN PREND LE LIBELLÉ ET LA TEINTE,
  // salon compris : c'est là qu'elle part. Sauf dans un canal où le joueur n'a
  // pas le droit d'écrire — le serveur la dit alors comme d'habitude.
  const ro::sim::ChatChannel* bound =
      speaker.bound_channel.empty() ? nullptr : FindChannel(channels, speaker.bound_channel);
  const bool public_bound =
      !speaker.bound_channel.empty() && (bound == nullptr || bound->can_write);
  bound_label_.clear();
  if (public_bound) {
    bound_label_ = bound != nullptr && !bound->alias.empty()
                       ? bound->alias
                       : std::string(1, ro::sim::kChannelPrefix) +
                             (bound != nullptr ? bound->name : std::string(speaker.bound_channel));
  }

  // ⚠ LE CHUCHOTEMENT N'EST PAS UNE DESTINATION QU'ON ARME : il lui faut un
  // destinataire, et un mode armé sans nom enverrait dans le vide. Il se dit
  // par son sigil, ou par la box qui vient avec (entrée A10).
  choices_.clear();
  for (const ChatTarget fixed : {ChatTarget::kPublic, ChatTarget::kParty, ChatTarget::kGuild,
                                 ChatTarget::kClan, ChatTarget::kAlly}) {
    choices_.push_back(SendChoice{.target = fixed});
  }
  // LE CHAMP DE BATAILLE NE S'ARME QUE SUR SA CARTE : ailleurs, le serveur n'a
  // aucune équipe à qui relayer, et la ligne se perdrait sans un mot.
  if (speaker.battlefield) {
    choices_.push_back(SendChoice{.target = ChatTarget::kBattlefield});
  }
  // ⚠ ET UN CANAL EN LECTURE SEULE NON PLUS : le serveur le pousse pour qu'on
  // sache le RECONNAÎTRE, pas pour qu'on y écrive. L'armer enverrait une ligne
  // que le serveur refuserait sans un mot.
  //
  // ⚠ LE CANAL D'ALLIANCE Y FIGURE COMME LES AUTRES, et son doublon avec le
  // sigil « ^ » est voulu : le sigil reste parce que les joueurs l'ont dans les
  // doigts, la liste est là parce qu'il n'y a aucune raison qu'il soit le seul
  // canal atteignable. Le serveur le marque « en guilde seulement » ; le
  // MASQUER demanderait de connaître sa propre guilde, qu'aucun paquet traité
  // ici ne donne — un joueur sans guilde le verra donc, et le serveur refusera.
  //
  // ⚠ LES LIBELLÉS SE RÉSERVENT AVANT D'ÊTRE REGARDÉS : `modes_` porte des VUES
  // dessus, et un agrandissement du vecteur en plein remplissage les ferait
  // pendre. D'où deux passes, et non une.
  channel_labels_.clear();
  channel_labels_.reserve(channels.size());
  std::vector<ro::ui::ChatSendMode> channel_looks;
  channel_looks.reserve(channels.size());
  for (const ro::sim::ChatChannel& channel : channels) {
    if (!channel.can_write) {
      continue;
    }
    choices_.push_back(SendChoice{.target = ChatTarget::kChannel, .channel = channel.name});
    channel_labels_.push_back(channel.alias.empty()
                                  ? std::string(1, ro::sim::kChannelPrefix) + channel.name
                                  : channel.alias);
    channel_looks.push_back(ChannelLook(channel.color));
  }

  modes_.clear();
  modes_.reserve(choices_.size());
  std::size_t channel_rank = 0;
  for (const SendChoice& choice : choices_) {
    if (public_bound && choice.target == ChatTarget::kPublic) {
      ro::ui::ChatSendMode mode = ChannelLook(bound != nullptr ? bound->color : 0);
      mode.label = bound_label_;
      modes_.push_back(mode);
      continue;
    }
    if (choice.target != ChatTarget::kChannel) {
      ro::ui::ChatSendMode mode = LookOfTarget(choice.target, in_chat_room, channels);
      mode.label = LabelOfTarget(choice.target, in_chat_room);
      // La parole à voix haute de la carte du relai part aussi sur Discord. Liée
      // à un canal, elle a pris la branche du dessus, sans marque.
      mode.relayed = speaker.relayed_to_discord && choice.target == ChatTarget::kPublic;
      modes_.push_back(mode);
      continue;
    }
    ro::ui::ChatSendMode mode = channel_looks[channel_rank];
    mode.label = channel_labels_[channel_rank];
    modes_.push_back(mode);
    ++channel_rank;
  }
  preview_.modes = modes_;
  preview_.mode = 0;
  for (int rank = 0; rank < static_cast<int>(choices_.size()); ++rank) {
    const SendChoice& choice = choices_[static_cast<std::size_t>(rank)];
    if (choice.target != armed.mode) {
      continue;
    }
    // ⚠ LE GENRE NE SUFFIT PAS POUR UN CANAL : ils partagent tous le même, et
    // s'arrêter au premier armerait « #map » quand le joueur a choisi « #global ».
    if (choice.target == ChatTarget::kChannel && choice.channel != armed.channel) {
      continue;
    }
    preview_.mode = rank;
  }
  // ⚠ DES VUES REFAITES À CHAQUE IMAGE : voir `RecentWhispers::Views`.
  preview_.recent_whispers = recent_.Views();

  // Un chuchotement montre SON destinataire : c'est la seule information que le
  // mot « Chuchotement » n'apporte pas, et celle qu'on vérifie avant d'envoyer.
  // Un canal aussi, et pour la même raison — avec son croisillon, parce que
  // c'est ainsi que le joueur le tape et le lit.
  if (intent_.target == ChatTarget::kChannel && !intent_.recipient.empty()) {
    target_ = std::string(1, ro::sim::kChannelPrefix) + intent_.recipient;
  } else if (intent_.target == ChatTarget::kWhisper && !intent_.recipient.empty()) {
    target_ = intent_.recipient;
  } else if (intent_.target == ChatTarget::kPublic && public_bound) {
    target_ = bound_label_;
  } else {
    target_ = LabelOfTarget(intent_.target, in_chat_room);
  }

  // ⚠ LES SIGILS SONT CEUX DU CLIENT, lus dans `net` : une aide qui en
  // annoncerait d'autres enverrait le joueur droit dans le mur. Le croisillon du
  // serveur devient d'ailleurs un accent circonflexe à la saisie, et c'est
  // exactement le genre de détail qu'on ne devine pas.
  help_ = ro::core::Printed(ro::core::Tr("Où part ce que tu écris. En tête de phrase :\n"
                                         "  %c groupe    %c guilde    %c alliés    %s pseudo"),
                            ro::net::kPartySigil, ro::net::kGuildSigil, ro::net::kAllySigil,
                            std::string(ro::net::kWhisperCommand).c_str());
  // Le sigil du champ ne sert que sur sa carte : c'est là seulement qu'on l'annonce.
  if (speaker.battlefield) {
    help_ += kHelpEntryGap;
    help_ +=
        ro::core::Printed(ro::core::Tr("%c ou %s champ de bataille"), ro::net::kBattlefieldSigil,
                          std::string(ro::net::kBattlefieldCommand).c_str());
  }

  preview_.target = target_;
  preview_.help = help_;
  // 🔴 LA MARQUE DISCORD SUIT LA DESTINATION RÉSOLUE, pas le mode armé : un
  // sigil ou la case « Pseudo » envoient ailleurs, une commande du client ne
  // part pas, et la parole liée à un canal passe au relais de ce canal. Une
  // commande du serveur (« @autoloot ») s'exécute avant le relai : elle ne
  // part pas non plus. Le serveur relaie le reste de la voix haute de Gonryun,
  // salon compris.
  preview_.relayed = speaker.relayed_to_discord && intent_.target == ChatTarget::kPublic &&
                     !public_bound && ro::net::IsSpokenLine(line) &&
                     !ro::net::IsServerCommandText(intent_.text);
  preview_.relay_tip =
      preview_.relayed
          ? ro::core::Tr("Ce que vous dites ici part aussi sur Discord, case cochée ou non.")
          : "";
  // ⚠ INCONNUE PLUTÔT QUE FAUSSE tant qu'on n'a pas de personnage : le nom entre
  // dans le compte, et l'annoncer sans lui promettrait une place qui n'existe
  // pas.
  preview_.budget_known = !speaker.name.empty();
  if (!preview_.budget_known) {
    preview_.bytes_left = 0;
    return;
  }
  preview_.bytes_left = LineRoomOf(line, intent_, speaker).bytes_left;
}

ChatWireRoom ChatWireRoomOf(std::string_view outgoing, const ChatWireSpeaker& speaker,
                            const ro::net::ChatArming& armed) {
  return LineRoomOf(outgoing, ro::net::ReadChatIntent(outgoing, armed), speaker);
}

int ChatWireBytesLeft(std::string_view outgoing, const ChatWireSpeaker& speaker,
                      const ro::net::ChatArming& armed) {
  return ChatWireRoomOf(outgoing, speaker, armed).bytes_left;
}

ChatWireRoom ChatIntentRoomOf(const ro::net::ChatIntent& intent, const ChatWireSpeaker& speaker) {
  return RoomOf(intent, speaker);
}

std::string OverlongChatNotice(const ChatWireRoom& room) {
  std::string text;
  if (room.overflow == ChatOverflow::kTruncated) {
    text = ro::core::Printed(
        ro::core::Tr("Message trop long de %d (limite %d) : il arriverait tronqué. "
                     "Raccourcis-le."),
        -room.bytes_left, room.text_max);
  } else {
    text = ro::core::Printed(
        ro::core::Tr("Message trop long de %d (limite %d) : le serveur le jetterait "
                     "sans un mot. Raccourcis-le."),
        -room.bytes_left, room.text_max);
  }
  return text;
}

std::optional<std::string> HoldIfOverlong(ro::ui::ChatInputState& input, const std::string& typed,
                                          const ChatWireRoom& room, bool speaker_known,
                                          std::vector<PendingChatLink>* links) {
  if (!speaker_known || room.bytes_left >= 0) {
    return std::nullopt;
  }
  // ⚠ LA LIGNE RETENUE PASSE DEVANT ce qui attendait déjà : c'est l'ordre dans
  // lequel ses liens ont été posés, donc celui que l'élagage consomme.
  std::string carried = typed;
  const std::string after = std::string(input.text.data()) + input.pending_insert;
  if (!after.empty()) {
    std::string joined = carried;
    if (!joined.empty() && joined.back() != ' ' && after.front() != ' ') {
      joined.push_back(' ');
    }
    joined += after;
    if (joined.size() + 1 <= input.text.size()) {
      carried = std::move(joined);
    } else {
      // Ce qui ne tient plus se perd, et ses liens avec : les laisser poser
      // compterait au plafond des libellés que la saisie ne porte pas.
      PruneChatLinks(carried, links);
    }
  }
  input.text[0] = '\0';
  input.pending_insert = std::move(carried);
  return OverlongChatNotice(room);
}

SettledChatLine SettleChatLine(const std::string& said, ro::ui::ChatInputState& input,
                               std::vector<PendingChatLink>* links, const ChatWireSpeaker& speaker,
                               const ro::net::ChatArming& armed) {
  const std::vector<PendingChatLink> none;
  const std::string outgoing = ResolveChatLinks(said, links != nullptr ? *links : none);
  if (std::optional<std::string> held = HoldIfOverlong(
          input, said, ChatWireRoomOf(outgoing, speaker, armed), !speaker.name.empty(), links);
      held.has_value()) {
    return {.outgoing = std::nullopt, .held_notice = std::move(held)};
  }
  // La saisie est repartie à zéro : les libellés n'y sont plus.
  if (links != nullptr) {
    links->clear();
  }
  return {.outgoing = outgoing, .held_notice = std::nullopt};
}

}  // namespace moonclient
