#pragma once

#include <cstddef>
#include <string_view>

namespace ro::proto {

// LA PLACE DU TEXTE D'UNE PAROLE, en octets, zéro terminal NON compris : la même
// pour toutes les destinations, chuchotement et canaux compris.
//
// ⚠ LE SERVEUR COMPTE LE TEXTE SEUL, nom et séparateur exclus
// (`clif_process_message`) : le texte, le zéro qu'on envoie, et un octet qu'il
// ajoute lui-même à cette version du protocole. Au-delà de 255, il rejette le
// message entier, sans réponse.
inline constexpr std::size_t kChatTextMax = 253;

// LE TAMPON DE CHAT DU SERVEUR, en octets, zéro terminal compris
// (`CHAT_SIZE_MAX`). Les bornes de DIFFUSION ci-dessous en dérivent : ce que le
// relais ne loge pas dans ce tampon, il le coupe, sans le dire à personne.
inline constexpr std::size_t kChatServerBufferBytes = 256;
// Le zéro terminal, que chaque relais prend sur ce tampon.
inline constexpr std::size_t kChatTerminatorBytes = 1;

// LA PLACE DE « Nom : texte » que le relais diffuse intacte, en octets, zéro
// terminal NON compris, pour la parole publique, le salon, le groupe et le champ
// de bataille (`clif_GlobalMessage`, `clif_party_message`, `clif_bg_message`) :
// le tampon moins son zéro. Au-delà, la ligne arrive tronquée.
inline constexpr std::size_t kChatNamedBodyMax = kChatServerBufferBytes - kChatTerminatorBytes;

// L'en-tête du paquet de chat de GUILDE, opcode et longueur.
inline constexpr std::size_t kGuildChatHeaderBytes = 4;
// LA MÊME PLACE POUR LA GUILDE, plus serrée : son relais (`clif_guild_message`)
// prend aussi l'en-tête de son paquet sur le tampon.
//
// ⚠ EN CHAMP DE BATAILLE, LA LIGNE DE GUILDE PART AU RELAIS DU CHAMP, qui garde
// `kChatNamedBodyMax`. Le serveur en décide sur l'ÉQUIPE du joueur ; le client
// ne connaît que la CARTE, et c'est sur elle qu'il choisit. Une carte de champ
// de bataille parcourue sans équipe reste au relais de guilde : une ligne de 252
// à 255 octets y serait coupée, et le client ne le voit pas.
inline constexpr std::size_t kChatGuildBodyMax =
    kChatServerBufferBytes - kGuildChatHeaderBytes - kChatTerminatorBytes;

// LE PLUS LONG NOM QU'UNE LIGNE DIFFUSÉE PUISSE PORTER, en octets : vingt-quatre,
// zéro compris (`NAME_LENGTH`). C'est la longueur que peut atteindre un faux nom
// (`@fakename`), que les relais de la parole publique, du salon, du groupe, de la
// guilde et du champ de bataille diffusent à la place du vrai. Un canal garde le
// vrai.
inline constexpr std::size_t kChatShownNameMax = 23;

// L'en-tête du paquet qui diffuse une ligne de CANAL : opcode, longueur,
// identifiant et couleur.
inline constexpr std::size_t kChannelChatHeaderBytes = 12;
// LA PLACE D'UNE LIGNE DE CANAL telle que son relais la diffuse,
// « alias nom : texte », en octets, zéro terminal NON compris
// (`channel_send`, puis `clif_channel_msg`). Au-delà, elle arrive tronquée.
//
// ⚠ LE TEXTE PART SANS LE NOM, mais la ligne diffusée le porte, et l'alias du
// canal avec : c'est elle qui se coupe.
inline constexpr std::size_t kChatChannelLineMax =
    kChatServerBufferBytes - kChannelChatHeaderBytes - kChatTerminatorBytes;
// Ce qui sépare l'alias du canal du nom de qui parle, dans la ligne diffusée.
inline constexpr std::string_view kChatChannelAliasSeparator = " ";
// Le plus long alias qu'un canal puisse porter, en octets : vingt, zéro compris.
// C'est ce qu'on compte pour un canal dont le serveur n'a pas dit l'alias.
inline constexpr std::size_t kChatChannelAliasMax = 19;

// ⚠ LE CLAN N'A PAS DE BORNE DE DIFFUSION à cette taille : son relais
// (`clif_clan_message`) borne sur le tampon d'émission entier. Seul le rejet du
// texte s'y applique.

// ⚠ UNE COMMANDE « @ » OU « # » GARDE LA BORNE DE LA PAROLE QUI LA PORTE. Le
// serveur l'intercepte à toutes les destinations, et n'en borne alors que le
// texte ; mais il la DIFFUSE en parole ordinaire quand le groupe du joueur n'y a
// pas droit, sur une carte `nocommand` ou face à un PNJ. Le client ne sait pas
// lesquelles le groupe exécute : il compte la diffusion, qui ne coupe jamais, au
// prix de quelques octets refusés à une longue commande que le serveur aurait
// exécutée — le nom et son séparateur, plus l'alias et l'en-tête pour un canal.

}  // namespace ro::proto
