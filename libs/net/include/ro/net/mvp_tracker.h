#pragma once

#include <cstddef>
#include <cstdint>
#include <span>
#include <string>
#include <string_view>
#include <vector>

#include "ro/core/error.h"
#include "ro/net/connection.h"
#include "ro/net/world_update.h"
#include "ro/sim/mvp_tracker.h"

namespace ro::net {

// LE CARNET DE CHASSE MVP SUR LE FIL — `ZC 0x0F31`, `ZC 0x0F32` et `CZ 0x0F30`,
// réservés à un client reconnu par la poignée de main. Le serveur ne diffuse
// les observations du groupe qu'à qui a annoncé la surface du carnet
// (`kUiCapMvpTracker`, voir `ro/net/ui_caps.h`).
//
//   ZC 0x0F31 : [op:2][len:2][kind:1][server_time:i64][count:2] puis count entrées
//     kind 0 catalogue, 57 o : [slot:2][mob:2][slot_kind:1][delay1_ms:4][delay2_ms:4]
//                               [map_xs:2][map_ys:2][map:16][name:24]
//     kind 1 instantané / 2 delta, 61 o : [slot:2][source:1][mob:2][kill:i64][exact:i64]
//                               [tomb_x:i16][tomb_y:i16][by_user:4][reported_at:i64][by_name:24]
//     kind 3 favoris, 2 o : [slot:2]
//   ZC 0x0F32 : [op:2][len:2][kind:1][result:1][group_id:4][owner:4][name:32][count:1]
//     puis count membres de 31 o : [user_id:4][level:i16][online:1][name:24]
//   CZ 0x0F30 : [op:2][len:2 = 13 + n][cmd:1][a:4][b:4][texte : n octets CP1252, sans NUL]
//     puis, si la commande porte un identifiant : [0x00][request_id:4], len += 5
//
// LES QUEUES de `ZC 0x0F32`, derrière la dernière entrée, seulement pour un
// client qui a annoncé `kUiCapMvpTrackerExt` :
//   kind 0 groupe : [self_user_id:4] ; kind 1 invitation : [origin:1] ;
//   kind 2 résultat, si la commande portait un identifiant : [request_id:4][cmd:1].
// Une queue absente laisse la trame d'avant ; un octet au-delà d'une queue
// connue s'ignore — une queue future s'ajoute après.
inline constexpr std::size_t kMvpStateFixedSize = 15;
inline constexpr std::size_t kMvpCatalogueEntrySize = 57;
inline constexpr std::size_t kMvpObservationEntrySize = 61;
inline constexpr std::size_t kMvpFavoriteEntrySize = 2;
inline constexpr std::size_t kMvpGroupFixedSize = 47;
inline constexpr std::size_t kMvpMemberEntrySize = 31;
inline constexpr std::size_t kMvpCommandFixedSize = 13;
inline constexpr std::size_t kMvpGroupSelfTrailerSize = 4;
inline constexpr std::size_t kMvpInviteOriginTrailerSize = 1;
inline constexpr std::size_t kMvpResultTagTrailerSize = 5;
// Le suffixe d'identifiant d'une commande : le NUL qui le signale, puis lui.
// 🔴 IL SE RECONNAÎT À SON NUL : le texte, lui, n'en porte jamais.
inline constexpr std::size_t kMvpCommandRequestIdSize = 5;
// LE PLAFOND DU TEXTE D'UNE COMMANDE, après conversion. ⚠ Le serveur n'en lit
// que 31 octets.
inline constexpr std::size_t kMvpCommandTextMost = 64;
// Les tailles des champs texte du fil, NUL compris.
inline constexpr std::size_t kMvpMapNameBytes = 16;
inline constexpr std::size_t kMvpNameBytes = 24;
inline constexpr std::size_t kMvpGroupNameBytes = 32;
// 🔴 LES HEURES DU FIL SE BORNENT DÈS LA LECTURE, à [0, la fin de l'an 9999]
// en secondes UNIX : ce sont des octets qu'on ne contrôle pas, et les couches
// qui en tirent fenêtres, âges et échéances y ajoutent des durées sans
// saturer. Une heure négative se lit zéro, « inconnue ».
inline constexpr std::int64_t kMvpLatestWireSeconds = 253'402'300'799;

enum class MvpStateKind : std::uint8_t {
  kCatalogue = 0,
  kSnapshot = 1,
  kDelta = 2,
  kFavorites = 3,
};
enum class MvpGroupKind : std::uint8_t { kGroup = 0, kInvitation = 1, kResult = 2 };
enum class MvpCommand : std::uint8_t {
  kSnapshot = 1,
  kCreateGroup = 2,
  kDissolveGroup = 3,
  kInvite = 4,
  kAcceptInvite = 5,
  kDeclineInvite = 6,
  kLeaveGroup = 7,
  kKick = 8,
  kFavorite = 9,
  kReportManual = 10,
};

// Applique une trame `0x0F31` ou `0x0F32`.
//
// `kUnhandled` : un autre opcode. `kUndecodable` : un en-tête trop court, ou une
// queue plus courte que `count` entrées (l'horloge, elle, est déjà recalée).
// `kIgnored` : un kind d'état inconnu. `kApplied` sinon.
[[nodiscard]] FrameOutcome ApplyMvpTrackerFrame(const Frame& frame, sim::MvpTrackerBook& book,
                                                std::int64_t local_now_s, std::uint32_t now_ms);

// La plus longue commande. Elle tient dans la longueur 16 bits du fil.
inline constexpr std::size_t kMvpCommandMostBytes =
    kMvpCommandFixedSize + kMvpCommandTextMost + kMvpCommandRequestIdSize;

// La trame d'une commande ; le texte, DÉJÀ en CP1252, est coupé à son premier
// NUL puis à `kMvpCommandTextMost` octets. `request_id` non nul : le suffixe
// d'identifiant suit, et le serveur le rendra avec le résultat.
[[nodiscard]] std::vector<std::uint8_t> EncodeMvpCommand(MvpCommand command, std::uint32_t a,
                                                         std::uint32_t b,
                                                         std::string_view text_wire,
                                                         std::uint32_t request_id = 0);

// ENVOIE UNE COMMANDE. ⚠ LA CONVERSION APPARTIENT AU PRODUCTEUR, ici : le texte
// passe d'UTF-8 en CP1252 (un caractère hors de la page devient « ? »), puis
// est coupé à 64 octets. Les noms sont des IDENTIFIANTS que le serveur compare
// octet par octet.
//
// Rien n'est appliqué d'avance : c'est la réponse du serveur qui change le
// carnet. Ne peut pas échouer : l'encodage est total, et l'envoi met en file.
void RequestMvpCommand(Connection& link, MvpCommand command, std::uint32_t a, std::uint32_t b,
                       std::string_view text_utf8 = {}, std::uint32_t request_id = 0);

// Le texte de la saisie manuelle : "x,y" si la tombe est connue, puis "|nom"
// si le nom n'est pas vide ; "|nom" seul quand la tombe est inconnue ; vide si
// rien. Le nom reste en UTF-8 : la conversion se fait à l'envoi.
[[nodiscard]] std::string MvpManualReportText(int tomb_x, int tomb_y,
                                              std::string_view shared_by_utf8);

}  // namespace ro::net
