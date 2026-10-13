#include "ro/net/world_notice.h"

#include <algorithm>
#include <charconv>
#include <cstddef>
#include <string>
#include <string_view>
#include <system_error>
#include <utility>

#include "ro/core/text.h"
#include "ro/net/chat_author.h"
#include "ro/net/refusal_text.h"
#include "ro/net/whisper.h"
#include "ro/proto/common_packets.h"
#include "ro/proto/manual_packets.h"
#include "ro/proto/map_packets.h"
#include "ro/proto/quest_marker_packets.h"
#include "ro/sim/chat_line.h"
#include "ro/sim/quest_markers.h"
#include "ro/sim/server_clock.h"
#include "ro/sim/world.h"

#include "frame_apply.h"

namespace ro::net {
namespace {

// La parole que le serveur adresse au joueur SEUL : son propre écho, et tout
// ce que le serveur lui écrit — la réponse d'une commande, un avertissement.
//
// ⚠ Cet opcode n'est pas dans la structure : le serveur l'écrit à la main
// (WFIFOW(fd,0) = 0x8e, dans src/map/clif.cpp), et le générateur ne peut donc
// pas le rattacher au type. Il est ici, seul endroit qui en ait besoin.
constexpr std::uint16_t kPlayerChatOpcode = 0x008e;

// L'ÉCHO DE SA PROPRE PAROLE DE GROUPE, en 0xRRGGBB : un jaune orangé, qui la
// distingue du rose des camarades. Le serveur renvoie la phrase à son auteur
// avec son identifiant, et c'est lui qui la désigne.
constexpr std::uint32_t kOwnPartySpeechColor = 0xFFC800;

// Le motif d'une éviction dont la trame ne se lit pas : celui d'une fin de
// session décidée par le serveur, sans plus de précision (`BAN_UNFAIR`).
constexpr std::uint8_t kUnreadEvictionCode = 0;

// Les étiquettes qu'une annonce `ZC_BROADCAST` peut porter DEVANT son texte.
//
// ⚠ CE NE SONT PAS DES MOTS DU MESSAGE, et aucun champ ne les annonce : ce sont
// QUATRE OCTETS posés en tête, que le client lit comme un code, SANS ÉGARD À LA
// CASSE (« BLUE » vaut « blue »). Les laisser passer ferait lire « blueLe
// serveur redémarre ».
//
//   « tool » + RRGGBB (six chiffres hexadécimaux) + texte   dans cette couleur
//   « micc » + nom (24 octets) + RRGGBB + texte             la voix d'un joueur
//   « blue », ou « 블루 » en CP949, + texte                  en cyan
//   « ssss » + texte                                         l'annonce de siège
//
// Ce serveur n'émet que « blue » et « ssss » ; les deux autres se lisent pareil
// le jour où un script les écrit.
constexpr std::size_t kBroadcastTagSize = 4;
constexpr std::string_view kBroadcastToolTag = "tool";
constexpr std::string_view kBroadcastMiccTag = "micc";
constexpr std::string_view kBroadcastBlueTag = "blue";
constexpr std::string_view kBroadcastBlueKoreanTag = "\xBA\xED\xB7\xE7";
constexpr std::string_view kBroadcastWoeTag = "ssss";

// La couleur écrite en texte : six chiffres hexadécimaux, RRGGBB.
constexpr std::size_t kTagColorDigits = 6;
constexpr int kHexadecimalBase = 16;
// Le nom de qui parle par « micc », zéro final compris s'il est plus court.
constexpr std::size_t kMiccNameSize = 24;
// ⚠ LE TEXTE D'UN « micc » TIENT EN MOINS DE 362 OCTETS : au-delà, l'annonce
// entière se tait.
constexpr std::size_t kMiccLongestText = 361;
// Ce qu'une annonce garde de sa charge utile : le reste est coupé.
constexpr std::size_t kLongestBroadcastPayload = 1023;

// LES COULEURS D'UNE ANNONCE, en 0xRRGGBB : du jaune, sauf « blue » en cyan et
// la couleur que « tool » ou « micc » écrivent.
constexpr std::uint32_t kAnnouncementYellow = 0xFFFF00;
constexpr std::uint32_t kAnnouncementCyan = 0x00FFFF;
// `ZC_BROADCAST2` porte sa couleur à l'endroit, sur les trois octets bas.
constexpr std::uint32_t kRgbMask = 0xFFFFFF;

// Une annonce lue : son texte sans étiquette, sa case et sa couleur. Une
// annonce qui ne se montre pas a un texte vide et `shown` faux. La voix d'un
// joueur porte aussi le nom de qui parle, octets du fil.
struct Announcement {
  std::string text;
  sim::ChatType type = sim::ChatType::kBroadcast;
  std::uint32_t color = kAnnouncementYellow;
  bool shown = true;
  bool voice = false;
  std::string voice_name;
};

// Les octets jusqu'au premier zéro, comme le client lit une chaîne.
[[nodiscard]] std::string_view UpToNul(std::string_view bytes) {
  return bytes.substr(0, bytes.find('\0'));
}

// Les chiffres hexadécimaux de TÊTE, comme une lecture `%lx` ; `fallback` s'il
// n'y en a aucun.
[[nodiscard]] std::uint32_t HexColor(std::string_view digits, std::uint32_t fallback) {
  std::uint32_t value = 0;
  const auto [end, error] =
      std::from_chars(digits.data(), digits.data() + digits.size(), value, kHexadecimalBase);
  static_cast<void>(end);
  return error == std::errc{} ? value : fallback;
}

// LE NOM SUFFIXÉ D'UN « # » SE REPLIE : ce qui précède le premier « # », puis
// tout depuis le premier « : », deux-points compris. « Nom#Tag: texte » devient
// « Nom: texte » ; ce qui sépare le « # » du « : », espaces comprises, se perd.
// Un « # » en tête, ou aucun, laisse le texte entier.
//
// ⚠ LE PREMIER « : » EST CHERCHÉ DANS TOUT LE TEXTE, avant le « # » compris :
// « a:b#c » devient « a:b:b#c », et c'est ce que le joueur voit aussi chez le
// client d'origine.
[[nodiscard]] std::string FoldHashName(std::string_view message) {
  const std::size_t hash = message.find('#');
  if (hash == std::string_view::npos || hash == 0) {
    return std::string(message);
  }
  std::string folded(message.substr(0, hash));
  if (const std::size_t colon = message.find(':'); colon != std::string_view::npos && colon > 0) {
    folded += message.substr(colon);
  }
  return folded;
}

// L'étiquette de tête, en minuscules ASCII ; vide sous quatre octets.
[[nodiscard]] std::string TagOf(std::string_view message) {
  if (message.size() < kBroadcastTagSize) {
    return {};
  }
  std::string tag(message.substr(0, kBroadcastTagSize));
  for (char& letter : tag) {
    if (letter >= 'A' && letter <= 'Z') {
      letter = static_cast<char>(letter - 'A' + 'a');
    }
  }
  return tag;
}

// « micc » : le nom, la couleur, puis le texte. Le texte se lit dans la charge
// BRUTE, parce que le nom s'arrête à son zéro et que la suite reste à lire.
[[nodiscard]] Announcement ReadVoiceAnnouncement(std::string_view payload) {
  const std::size_t color_at = kBroadcastTagSize + kMiccNameSize;
  const std::size_t text_at = color_at + kTagColorDigits;
  if (payload.size() < text_at) {
    return {.shown = false};
  }
  const std::string_view text = UpToNul(payload.substr(text_at));
  if (text.size() > kMiccLongestText) {
    return {.shown = false};
  }
  return {.text = std::string(text),
          .type = sim::ChatType::kPublic,
          .color = HexColor(payload.substr(color_at, kTagColorDigits), kAnnouncementYellow),
          .voice = true,
          .voice_name = std::string(UpToNul(payload.substr(kBroadcastTagSize, kMiccNameSize)))};
}

// ⚠ L'ANNONCE DE SIÈGE A SA CASE, « Informations WOE », et la voix d'un joueur
// la case générale ; tout le reste va sous la case des annonces. `message` est
// le texte jusqu'à son zéro, `payload` la charge entière.
[[nodiscard]] Announcement ReadAnnouncement(std::string_view message, std::string_view payload) {
  const std::string tag = TagOf(message);
  const std::string_view after_tag =
      message.size() < kBroadcastTagSize ? std::string_view{} : message.substr(kBroadcastTagSize);
  if (tag == kBroadcastToolTag) {
    const std::size_t text_at = std::min(after_tag.size(), kTagColorDigits);
    return {.text = std::string(after_tag.substr(text_at)),
            .color = HexColor(after_tag.substr(0, kTagColorDigits), kAnnouncementYellow)};
  }
  if (tag == kBroadcastBlueTag || tag == kBroadcastBlueKoreanTag) {
    return {.text = std::string(after_tag), .color = kAnnouncementCyan};
  }
  if (tag == kBroadcastMiccTag) {
    return ReadVoiceAnnouncement(payload.substr(0, kLongestBroadcastPayload));
  }
  if (tag == kBroadcastWoeTag) {
    return {.text = std::string(after_tag), .type = sim::ChatType::kSiegeInfo};
  }
  return {.text = FoldHashName(message)};
}

// Le texte, ses mots interdits masqués quand le filtre en tient.
[[nodiscard]] std::string Masked(const BroadcastFilter& filter, std::string utf8_text) {
  return filter.banned == nullptr ? utf8_text : filter.banned->Mask(utf8_text);
}

// La charge d'une trame à longueur variable : ce qui suit l'opcode et la
// longueur.
[[nodiscard]] std::string_view PayloadOf(const Frame& frame) {
  constexpr std::size_t kVariableHeaderSize = 4;
  if (frame.bytes.size() < kVariableHeaderSize) {
    return {};
  }
  return {reinterpret_cast<const char*>(frame.bytes.data()) + kVariableHeaderSize,
          frame.bytes.size() - kVariableHeaderSize};
}

}  // namespace

FrameOutcome ApplyBroadcastFrame(const Frame& frame, sim::World& world,
                                 const BroadcastFilter& filter, std::uint32_t now_ms) {
  switch (frame.opcode) {
    case proto::ZcBroadcast::kOpcode:
      return Decoded<proto::ZcBroadcast>(frame, [&](const auto& packet) {
        // UNE ANNONCE DU SERVEUR. Elle ne vient de personne : l'orateur est
        // ZÉRO, comme pour les réponses de commande, et rien ne paraît donc
        // au-dessus d'une tête.
        const Announcement announcement = ReadAnnouncement(packet.message, PayloadOf(frame));
        if (!announcement.shown) {
          return FrameOutcome::kIgnored;
        }
        std::string text = core::Utf8FromWire(announcement.text);
        // ⚠ SEULE LA VOIX D'UN JOUEUR SE FILTRE : « tool », « blue », « ssss »
        // et l'annonce sans étiquette passent telles quelles.
        if (announcement.voice) {
          if (filter.ignores && filter.ignores(core::Utf8FromWire(announcement.voice_name))) {
            return FrameOutcome::kIgnored;
          }
          text = Masked(filter, std::move(text));
        }
        world.RecordChat(sim::ChatKind::kAnnouncement, announcement.type, 0, std::move(text),
                         now_ms, announcement.color);
        return FrameOutcome::kApplied;
      });

    case proto::ZcBroadcast2::kOpcode:
      return Decoded<proto::ZcBroadcast2>(frame, [&](const auto& packet) {
        // LA MÊME ANNONCE, HABILLÉE : couleur, police, taille, hauteur à l'écran.
        // Aucune étiquette en tête ici — la couleur est un CHAMP, et c'est
        // précisément pourquoi ce second paquet existe. Le nom suffixé d'un
        // « # » s'y replie comme dans une annonce sans étiquette.
        //
        // ⚠ LE MASQUE PASSE AVANT LE REPLI, sur le texte entier : un mot que le
        // repli recollerait de part et d'autre du « # » ne se masque pas.
        //
        // ⚠ DE CE CHAMP, ON NE PREND QUE LA COULEUR. La police, la taille et la
        // hauteur à l'écran décrivent la bannière que le client d'origine fait
        // glisser en haut de l'écran, pas la ligne du fil.
        world.RecordChat(sim::ChatKind::kAnnouncement, 0,
                         FoldHashName(Masked(filter, core::Utf8FromWire(packet.message))), now_ms,
                         packet.fontColor & kRgbMask);
        return FrameOutcome::kApplied;
      });

    default:
      return FrameOutcome::kUnhandled;
  }
}

FrameOutcome ApplySpokenFrame(const Frame& frame, sim::World& world, const sim::BannedWords* banned,
                              std::uint32_t now_ms, sim::TalkMute mute) {
  const auto masked = [banned](std::string text) {
    return sim::BannedWords::MaskWith(banned, text);
  };
  switch (frame.opcode) {
    case proto::ZcNotifyChat::kOpcode:
      return Decoded<proto::ZcNotifyChat>(frame, [&](const auto& packet) {
        // Ce que quelqu'un d'AUTRE a dit. Le serveur écrit le nom devant, et le
        // garder rend la conversation lisible sans table de noms.
        AuthoredText said = ReadAuthoredText(frame, packet.Message);
        if (mute.hides_talk()) {
          world.ShowSpeech(packet.GID, masked(std::move(said.text)), now_ms);
          return FrameOutcome::kApplied;
        }
        world.RecordChat(sim::ChatKind::kPublic, sim::ChatType::kPublicChat, packet.GID,
                         masked(std::move(said.text)), now_ms, 0, std::move(said.author));
        return FrameOutcome::kApplied;
      });

    case kPlayerChatOpcode:
      return Decoded<proto::ZcNotifyPlayerchat>(frame, [&](const auto& packet) {
        // Le même paquet porte deux choses : l'écho de sa propre parole, que le
        // serveur préfixe du nom, et ce qu'il écrit au joueur, qui ne vient de
        // personne. C'est le préfixe qui les sépare — et sans lui, une réponse
        // de commande paraîtrait au-dessus de la tête du joueur.
        //
        // ⚠ LE PRÉFIXE SE RECONNAÎT AVANT LE MASQUE : un nom qui porte un mot
        // interdit reste le nôtre.
        const std::string said = core::Utf8FromWire(packet.Message);
        const std::string prefix =
            world.player_name() + std::string(proto::ChatRequest::kNameSeparator);
        const bool mine = !world.player_name().empty() && said.starts_with(prefix);
        // ⚠ LES DEUX SOUS LA CASE « CHAT PUBLIC » : c'est le paquet de la
        // parole publique, et le client d'origine range ses deux usages ensemble.
        world.RecordChat(mine ? sim::ChatKind::kSelf : sim::ChatKind::kServer,
                         sim::ChatType::kPublicChat, mine ? world.player_id() : 0, masked(said),
                         now_ms);
        return FrameOutcome::kApplied;
      });

    case proto::ZcNotifyChatParty::kOpcode:
      return Decoded<proto::ZcNotifyChatParty>(frame, [&](const auto& packet) {
        // Le serveur écrit le nom du camarade DEVANT son texte, comme pour la
        // parole publique : la ligne se lit sans table de noms.
        const auto speaker_id = static_cast<std::uint32_t>(packet.AID);
        const bool mine = speaker_id != 0 && speaker_id == world.player_id();
        AuthoredText said = ReadAuthoredText(frame, packet.chatMsg);
        world.RecordChat(sim::ChatKind::kParty, sim::ChatType::kPartyChat, speaker_id,
                         masked(std::move(said.text)), now_ms, mine ? kOwnPartySpeechColor : 0,
                         std::move(said.author));
        return FrameOutcome::kApplied;
      });

    case proto::ZcGuildChat::kOpcode:
      return Decoded<proto::ZcGuildChat>(frame, [&](const auto& packet) {
        // ⚠ CELUI-CI NE NOMME PERSONNE DU TOUT : pas d'identifiant, pas de champ
        // de nom. L'auteur est dans le texte, ou dans la trame qui le précède.
        //
        // ⚠ L'ORATEUR RESTE ZÉRO MÊME QUAND L'AUTEUR EST CONNU : la parole de
        // guilde ne met pas de bulle au-dessus d'une tête.
        if (mute.hides_guild()) {
          return FrameOutcome::kApplied;
        }
        AuthoredText said = ReadAuthoredText(frame, packet.message);
        world.RecordChat(sim::ChatKind::kGuild, sim::ChatType::kGuildChat, 0,
                         masked(std::move(said.text)), now_ms, 0, std::move(said.author));
        return FrameOutcome::kApplied;
      });

    case proto::ZcNotifyClanChat::kOpcode:
      return Decoded<proto::ZcNotifyClanChat>(frame, [&](const auto& packet) {
        // LE CLAN, QUI N'EST PAS LA GUILDE : on y entre par sa classe, et un
        // personnage peut tenir les deux à la fois — les mêler donnerait un
        // onglet « Guilde » qui montre les paroles d'un groupe qu'on n'a pas
        // choisi.
        //
        // ⚠ SON CHAMP DE NOM EST VIDE, ET C'EST VOULU CÔTÉ SERVEUR : « le nom de
        // l'émetteur devrait aussi être rempli ici, mais le client ne le demande
        // pas, et comme il est aussi dans le message nous ne le remplissons
        // pas » (clif_clan_message). L'auteur est donc dans le texte, ou dans
        // la trame qui le précède, comme pour la guilde.
        if (mute.hides_guild()) {
          return FrameOutcome::kApplied;
        }
        AuthoredText said = ReadAuthoredText(frame, packet.Message);
        world.RecordChat(sim::ChatKind::kClan, sim::ChatType::kClanChat, 0,
                         masked(std::move(said.text)), now_ms, 0, std::move(said.author));
        return FrameOutcome::kApplied;
      });

    default:
      return FrameOutcome::kUnhandled;
  }
}

FrameOutcome ApplyNoticeFrame(const Frame& frame, sim::World& world, std::uint32_t now_ms) {
  switch (frame.opcode) {
    case proto::ZcEmotion::kOpcode:
      return Decoded<proto::ZcEmotion>(frame, [&world, now_ms](const auto& packet) {
        // La VIEILLE annonce, celle des monstres et des scripts de PNJ : pas de
        // lot, donc le jeu de base. L'identifiant EST l'action du sprite des
        // émoticônes ; voir kEmotionSpritePath.
        const sim::Emotion shown{sim::kBaseEmotionPack, packet.type};
        return world.RecordEmotion(static_cast<std::uint32_t>(packet.GID), shown, now_ms)
                   ? FrameOutcome::kApplied
                   : FrameOutcome::kIgnored;
      });

    case proto::ZcPlayCashEmotion::kOpcode:
      return Decoded<proto::ZcPlayCashEmotion>(frame, [&world, now_ms](const auto& packet) {
        // Celle du client MODERNE : c'est par là que passe l'émoticône d'un
        // joueur, lot compris. Le lot zéro est le jeu de base.
        const sim::Emotion shown{packet.pack_id, packet.emotion_id};
        return world.RecordEmotion(packet.GID, shown, now_ms) ? FrameOutcome::kApplied
                                                              : FrameOutcome::kIgnored;
      });

    case proto::QuestMarkerNotice::kOpcode:
      return Decoded<proto::QuestMarkerNotice>(frame, [&world](const auto& packet) {
        // ⚠ AUCUN ACTEUR N'EST EXIGÉ : la marque se garde pour le PNJ qui n'est
        // pas encore en vue. Voir `sim::QuestMarkers`.
        world.quest_markers().Tell(packet.npc_id, sim::QuestMarker{.cell_x = packet.x,
                                                                   .cell_y = packet.y,
                                                                   .effect = packet.effect,
                                                                   .color = packet.color});
        return FrameOutcome::kApplied;
      });

    case proto::ZcNotifyChat::kOpcode:
    case kPlayerChatOpcode:
    case proto::ZcNotifyChatParty::kOpcode:
    case proto::ZcGuildChat::kOpcode:
    case proto::ZcNotifyClanChat::kOpcode:
      // ⚠ SANS LISTE : rien ne se masque. La partie lit ces paquets AVANT le
      // monde, avec les mots interdits de l'installation.
      return ApplySpokenFrame(frame, world, nullptr, now_ms);

    case proto::ZcBroadcast::kOpcode:
    case proto::ZcBroadcast2::kOpcode:
      // ⚠ SANS FILTRE : rien ne se tait ni ne se masque. La partie lit ces deux
      // paquets AVANT le monde, avec ses noms ignorés et ses mots interdits.
      return ApplyBroadcastFrame(frame, world, BroadcastFilter{}, now_ms);

    case proto::WhisperNotice::kOpcode:
    case proto::ZcAckWhisper::kOpcode:
      // ⚠ SANS CERCLE : l'autre passe pour un inconnu, et un accusé ne retrouve
      // aucun envoi. La partie lit ces deux paquets AVANT le monde, avec ses amis
      // et sa file d'envois — voir `ro/net/whisper.h`.
      return ApplyWhisperFrame(frame, world, WhisperCircle{}, now_ms);

    case proto::ColoredNotice::kOpcode:
      return Decoded<proto::ColoredNotice>(frame, [&frame, &world, now_ms](const auto& packet) {
        // ⚠ C'EST L'IDENTIFIANT QUI DIT DE QUOI IL S'AGIT, pas le texte : zéro
        // pour un canal, sinon l'acteur qui parle. Voir `ColoredNotice`, où la
        // règle est recoupée avec les deux fonctions du serveur.
        if (packet.speaker_id == 0) {
          // LE CANAL D'UNE ALLIANCE a sa case, « Chat d'alliance » ; seul
          // l'auteur qui précède la ligne le distingue des autres canaux.
          AuthoredText channel = ReadAuthoredText(frame, packet.message);
          const sim::ChatType type = channel.channel_type == proto::kChatAuthorAllyChannelType
                                         ? sim::ChatType::kAllianceChat
                                         : sim::DefaultChatType(sim::ChatKind::kChannel);
          world.RecordChat(sim::ChatKind::kChannel, type, 0, std::move(channel.text), now_ms,
                           packet.color, std::move(channel.author));
          return FrameOutcome::kApplied;
        }
        const std::string said = core::Utf8FromWire(packet.message);
        // ⚠ CE QUE LE SERVEUR RÉPOND AU JOUEUR PORTE L'IDENTIFIANT DU JOUEUR, et
        // ce n'est pas lui qui parle. Ni ligne à son nom, ni bulle : le client
        // d'origine ne cherche l'orateur que parmi les AUTRES acteurs, et le
        // joueur n'y figure pas.
        //
        // ⚠ SOUS LA CASE GÉNÉRALE dans les deux cas, PNJ compris : ce paquet ne
        // porte pas la parole d'un joueur.
        //
        // ⚠ NI MASQUE NI `TalkMute` : le client d'origine écrit ce paquet tel
        // qu'il arrive, et aucune des deux bascules ne le tait.
        const bool mine = packet.speaker_id == world.player_id();
        world.RecordChat(mine ? sim::ChatKind::kServer : sim::ChatKind::kPublic,
                         sim::ChatType::kPublic, mine ? 0 : packet.speaker_id, said, now_ms,
                         packet.color);
        // ⚠ LA BULLE PREND LA COULEUR DU PAQUET, et la lit dans l'AUTRE ORDRE
        // que la ligne : bleu-vert-rouge, le rouge en dernier. Un serveur qui
        // veut une bulle juste envoie donc sa couleur retournée.
        if (!mine) {
          world.ShowSpeech(packet.speaker_id, said, now_ms, packet.color);
        }
        return FrameOutcome::kApplied;
      });

    case proto::ZcNotifyTime::kOpcode:
      return Decoded<proto::ZcNotifyTime>(frame, [&world, now_ms](const auto& packet) {
        // L'HORLOGE DU SERVEUR, NUE — c'est le seul paquet dont ce soit l'unique
        // objet. `clif_notify_time` n'écrit que `client_tick(gettick())`, la
        // MÊME horloge que le `moveStartTime` d'une marche : le décalage se
        // mesure donc ici sans avoir à le démêler d'autre chose. Voir
        // `ServerClock::NoteServerTick`.
        world.server_clock().NoteServerTick(packet.time, now_ms);
        return FrameOutcome::kApplied;
      });

    case proto::ScNotifyBan::kOpcode:
      return Decoded<proto::ScNotifyBan>(frame, [&world, now_ms](const auto& packet) {
        // L'ÉVICTION, ET LE SEUL PAQUET QUI EN DONNE LE MOTIF.
        //
        // 🔴 IL ARRIVE EN PLEINE PARTIE, et il était lu NULLE PART : le serveur
        // coupe la liaison juste après (`set_eof`), si bien que le joueur ne
        // voyait que « le serveur a fermé la connexion » — la phrase qu'on dit
        // aussi d'une panne de réseau. C'est par là qu'arrive le renvoi pour
        // intégrité, vingt secondes après l'entrée, et il ne ressemblait à rien
        // d'autre qu'à une coupure.
        //
        // ⚠ LE MONDE N'EN GARDE QUE LA LIGNE DE CONVERSATION, à la suite de ce
        // que le serveur y a écrit juste avant — l'avis de patch précède le
        // renvoi de cinq secondes. Dire la perte de liaison et ramener à
        // l'écran de connexion n'est pas un fait du monde : c'est `app` qui le
        // fait, par `EvictionOf`.
        world.RecordChat(sim::ChatKind::kServer, 0, BanText(packet.result), now_ms);
        return FrameOutcome::kApplied;
      });

    case proto::ZcPingLive::kOpcode:
      return Decoded<proto::ZcPingLive>(frame, [](const auto&) {
        // LE BATTEMENT DE CŒUR DE LA CONNEXION. Il ne porte rien, pas même une
        // heure, et il n'y a rien à en faire : la socket a déjà dit tout ce
        // qu'il disait en le livrant. On le décode quand même, pour que sa
        // longueur soit tenue comme celle des autres.
        return FrameOutcome::kIgnored;
      });

    default:
      return FrameOutcome::kUnhandled;
  }
}

std::optional<std::uint8_t> EvictionOf(const Frame& frame) {
  if (frame.opcode != proto::ScNotifyBan::kOpcode) {
    return std::nullopt;
  }
  // ⚠ UNE TRAME ILLISIBLE RESTE UNE ÉVICTION : l'opcode suffit à dire que le
  // serveur coupe. Elle prend alors le motif générique, comme pendant la
  // connexion.
  proto::ScNotifyBan packet;
  if (!proto::Decode(frame.bytes, packet)) {
    return kUnreadEvictionCode;
  }
  return packet.result;
}

}  // namespace ro::net
