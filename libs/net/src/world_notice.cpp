#include "ro/net/world_notice.h"

#include <string>
#include <string_view>
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

// Les étiquettes qu'une annonce peut porter DEVANT son texte.
//
// ⚠ CE NE SONT PAS DES MOTS DU MESSAGE, et aucun champ ne les annonce : ce sont
// QUATRE OCTETS posés en tête, que le client lit comme un code. `clif_broadcast`
// écrit « blue » pour faire peindre en bleu et « ssss » pour dire « annonce de
// guerre d'Émpérium ». Les laisser passer ferait lire « blueLe serveur
// redémarre ».
//
// LE CLIENT EN CONNAÎT QUATRE, CE SERVEUR N'EN ÉMET QUE DEUX. Les quatre
// étiquettes sont « tool », « micc », « ssss » et « blue ».
// Les deux premières — l'annonce par objet et celle à la voix — ne sont dans
// AUCUNE source du serveur : les découper ici couperait quatre lettres à un
// message que personne n'envoie. Si elles paraissent un jour, elles se verront
// à l'écran, et c'est ici qu'on les ajoutera.
constexpr std::string_view kBroadcastBlueTag = "blue";
constexpr std::string_view kBroadcastWoeTag = "ssss";

// Une annonce lue : son texte sans étiquette, et sa case.
struct Announcement {
  std::string_view text;
  sim::ChatType type = sim::ChatType::kBroadcast;
};

// ⚠ L'ANNONCE DE SIÈGE A SA CASE, « Informations WOE » : le client d'origine
// range sous elle ce que « ssss » annonce, et sous la case des annonces tout le
// reste.
Announcement ReadAnnouncement(std::string_view message) {
  if (message.starts_with(kBroadcastWoeTag)) {
    return {.text = message.substr(kBroadcastWoeTag.size()), .type = sim::ChatType::kSiegeInfo};
  }
  if (message.starts_with(kBroadcastBlueTag)) {
    return {.text = message.substr(kBroadcastBlueTag.size())};
  }
  return {.text = message};
}

}  // namespace

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
      return Decoded<proto::ZcNotifyChat>(frame, [&frame, &world, now_ms](const auto& packet) {
        // Ce que quelqu'un d'AUTRE a dit. Le serveur écrit le nom devant, et le
        // garder rend la conversation lisible sans table de noms.
        AuthoredText said = ReadAuthoredText(frame, packet.Message);
        world.RecordChat(sim::ChatKind::kPublic, sim::ChatType::kPublicChat, packet.GID,
                         std::move(said.text), now_ms, 0, std::move(said.author));
        return FrameOutcome::kApplied;
      });

    case kPlayerChatOpcode:
      return Decoded<proto::ZcNotifyPlayerchat>(frame, [&world, now_ms](const auto& packet) {
        // Le même paquet porte deux choses : l'écho de sa propre parole, que le
        // serveur préfixe du nom, et ce qu'il écrit au joueur, qui ne vient de
        // personne. C'est le préfixe qui les sépare — et sans lui, une réponse
        // de commande paraîtrait au-dessus de la tête du joueur.
        const std::string said = core::Utf8FromWire(packet.Message);
        const std::string prefix =
            world.player_name() + std::string(proto::ChatRequest::kNameSeparator);
        const bool mine = !world.player_name().empty() && said.starts_with(prefix);
        // ⚠ LES DEUX SOUS LA CASE « CHAT PUBLIC » : c'est le paquet de la
        // parole publique, et le client d'origine range ses deux usages ensemble.
        world.RecordChat(mine ? sim::ChatKind::kSelf : sim::ChatKind::kServer,
                         sim::ChatType::kPublicChat, mine ? world.player_id() : 0, said, now_ms);
        return FrameOutcome::kApplied;
      });

    case proto::ZcBroadcast::kOpcode:
      return Decoded<proto::ZcBroadcast>(frame, [&world, now_ms](const auto& packet) {
        // UNE ANNONCE DU SERVEUR. Elle ne vient de personne : l'orateur est
        // ZÉRO, comme pour les réponses de commande, et rien ne paraît donc
        // au-dessus d'une tête.
        const Announcement announcement = ReadAnnouncement(packet.message);
        world.RecordChat(sim::ChatKind::kAnnouncement, announcement.type, 0,
                         core::Utf8FromWire(announcement.text), now_ms);
        return FrameOutcome::kApplied;
      });

    case proto::ZcBroadcast2::kOpcode:
      return Decoded<proto::ZcBroadcast2>(frame, [&world, now_ms](const auto& packet) {
        // LA MÊME ANNONCE, HABILLÉE : couleur, police, taille, hauteur à l'écran.
        // Rien à retirer en tête ici — la couleur est un CHAMP, et c'est
        // précisément pourquoi ce second paquet existe.
        //
        // ⚠ DE CE CHAMP, ON NE PREND QUE LA COULEUR. La police, la taille et la
        // hauteur à l'écran décrivent la bannière que le client d'origine fait
        // glisser en haut de l'écran, pas la ligne du fil.
        world.RecordChat(sim::ChatKind::kAnnouncement, 0, core::Utf8FromWire(packet.message),
                         now_ms, packet.fontColor);
        return FrameOutcome::kApplied;
      });

    case proto::WhisperNotice::kOpcode:
    case proto::ZcAckWhisper::kOpcode:
      // ⚠ SANS CERCLE : l'autre passe pour un inconnu, et un accusé ne retrouve
      // aucun envoi. La partie lit ces deux paquets AVANT le monde, avec ses amis
      // et sa file d'envois — voir `ro/net/whisper.h`.
      return ApplyWhisperFrame(frame, world, WhisperCircle{}, now_ms);

    case proto::ZcNotifyChatParty::kOpcode:
      return Decoded<proto::ZcNotifyChatParty>(frame, [&frame, &world, now_ms](const auto& packet) {
        // Le serveur écrit le nom du camarade DEVANT son texte, comme pour la
        // parole publique : la ligne se lit sans table de noms.
        const auto speaker_id = static_cast<std::uint32_t>(packet.AID);
        const bool mine = speaker_id != 0 && speaker_id == world.player_id();
        AuthoredText said = ReadAuthoredText(frame, packet.chatMsg);
        world.RecordChat(sim::ChatKind::kParty, sim::ChatType::kPartyChat, speaker_id,
                         std::move(said.text), now_ms, mine ? kOwnPartySpeechColor : 0,
                         std::move(said.author));
        return FrameOutcome::kApplied;
      });

    case proto::ZcGuildChat::kOpcode:
      return Decoded<proto::ZcGuildChat>(frame, [&frame, &world, now_ms](const auto& packet) {
        // ⚠ CELUI-CI NE NOMME PERSONNE DU TOUT : pas d'identifiant, pas de champ
        // de nom. L'auteur est dans le texte, ou dans la trame qui le précède.
        //
        // ⚠ L'ORATEUR RESTE ZÉRO MÊME QUAND L'AUTEUR EST CONNU : la parole de
        // guilde ne met pas de bulle au-dessus d'une tête.
        AuthoredText said = ReadAuthoredText(frame, packet.message);
        world.RecordChat(sim::ChatKind::kGuild, sim::ChatType::kGuildChat, 0, std::move(said.text),
                         now_ms, 0, std::move(said.author));
        return FrameOutcome::kApplied;
      });

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
        // ce n'est pas lui qui parle. Le client d'origine lui met tout de même
        // une bulle au-dessus de la tête ; nous avons tranché l'inverse pour
        // `ZC_NOTIFY_PLAYERCHAT`, et deux règles contraires pour la même chose se
        // contrediraient à l'écran.
        //
        // ⚠ SOUS LA CASE GÉNÉRALE dans les deux cas, PNJ compris : ce paquet ne
        // porte pas la parole d'un joueur.
        const bool mine = packet.speaker_id == world.player_id();
        world.RecordChat(mine ? sim::ChatKind::kServer : sim::ChatKind::kPublic,
                         sim::ChatType::kPublic, mine ? 0 : packet.speaker_id, said, now_ms,
                         packet.color);
        return FrameOutcome::kApplied;
      });

    case proto::ZcNotifyClanChat::kOpcode:
      return Decoded<proto::ZcNotifyClanChat>(frame, [&frame, &world, now_ms](const auto& packet) {
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
        AuthoredText said = ReadAuthoredText(frame, packet.Message);
        world.RecordChat(sim::ChatKind::kClan, sim::ChatType::kClanChat, 0, std::move(said.text),
                         now_ms, 0, std::move(said.author));
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
        // renvoi de cinq secondes. Fermer le jeu en disant pourquoi n'est pas
        // un fait du monde : c'est `app` qui le fait, par `EvictionOf`.
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
