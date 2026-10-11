#pragma once

#include <cstddef>
#include <cstdint>
#include <optional>
#include <string>

#include "ro/sim/whisper.h"

namespace ro::sim {

// LA CONVERSATION : ce qui s'y dit, et d'où chaque ligne vient.
//
// À part du monde pour la même raison que le dialogue d'un PNJ : c'est une
// famille à elle, que la chatbox fera grandir — onglets, filtres, couleurs — et
// `world.h` touchait son plafond de lignes.

// D'OÙ VIENT UNE LIGNE DE CONVERSATION.
//
// ⚠ C'EST LE GENRE QUI DÉCIDE DE SON ALLURE : la couleur qui la porte, et si
// elle paraît aussi au-dessus d'une tête. L'onglet qui la montre se décide sur
// son TYPE, plus bas — la case de filtre que le joueur coche. Le
// déduire de l'orateur ne marche pas — un chuchotement et une parole publique
// viennent tous deux d'un joueur —, et le déduire de l'opcode obligerait chaque
// lecteur à connaître le protocole.
enum class ChatKind : std::uint8_t {
  // Ce que quelqu'un dit à voix haute, à portée de vue.
  kPublic,
  // L'écho de sa propre voix, que le serveur renvoie.
  kSelf,
  // Ce que le serveur écrit au joueur SEUL : la réponse d'une commande, un refus.
  kServer,
  // Ce qu'il dit à tout le monde.
  kAnnouncement,
  kWhisper,
  kParty,
  kGuild,
  // Un canal, celui des allies compris.
  kChannel,
  // LE CLAN, qui n'est pas la guilde : on y entre par sa CLASSE, on n'en
  // choisit pas les membres, et un personnage peut tenir les deux à la fois.
  // Le serveur lui donne son propre paquet (`ZC_NOTIFY_CLAN_CHAT`).
  kClan,
  // CE QUE LE SAC REÇOIT ET CE QU'IL REFUSE : un objet entré dans
  // l'inventaire, un ramassage qui n'a pas abouti.
  //
  // ⚠ PAS « SERVEUR », bien que le serveur les envoie toutes deux. TOUTE
  // entrée dans le sac en écrit une — coffre repris, achat en boutique,
  // récompense de quête, courrier —, si bien qu'un coffre vidé en aligne
  // quarante d'un coup. Rangées sous « serveur », s'en défaire emporterait
  // aussi les réponses d'@commande et les échecs de compétence. Le client
  // d'origine leur réserve la même case à part.
  kItem,
  // LES LIGNES DE COMBAT : un coup que je reçois ou que je porte, et un coup
  // qu'un membre connecté de mon groupe reçoit ou porte. Le client leur donne
  // deux cases à part, et c'est par elles qu'un joueur les tait.
  kBattle,
  kPartyBattle,
  // L'EXPÉRIENCE GAGNÉE OU PERDUE, une ligne par gain : un monstre tué en
  // écrit deux, la base et le métier. Le client lui donne sa case à part.
  kExp,
  // CE QUI TOUCHE L'APPEL D'URGENCE (`/call`) : la réponse du serveur quand le
  // joueur l'accepte ou le refuse. Elle revient à chaque connexion, d'où sa
  // case : le joueur peut la taire.
  kCall,
  // LA PAROLE DU CHAMP DE BATAILLE, réservée à notre équipe.
  kBattlefield,
};

// LE TYPE D'UNE LIGNE, celui que les filtres d'un onglet cochent.
//
// ⚠ CE N'EST PAS LE GENRE, et les deux coexistent. Le genre dit comment la
// ligne SE MONTRE — sa couleur, sa bulle, l'en-tête d'un chuchotement ; le type
// dit dans quelle CASE le joueur la range. Ce sont les vingt-cinq cases du
// client d'origine, dans son ordre et avec ses numéros, plus une vingt-sixième
// pour les annonces, une vingt-septième pour le relai Discord et une
// vingt-huitième pour l'armement et le lancer de ses propres skills : les joueurs
// connaissent cette liste, et un onglet qui ne la reprendrait pas les
// obligerait à réapprendre où va chaque message.
//
// ⚠ UN TYPE QU'AUCUN MESSAGE N'ALIMENTE ENCORE GARDE SA PLACE : la case existe,
// se coche, et attend la ligne qui la remplira.
enum class ChatType : std::uint8_t {
  // Ce qui n'a pas de case plus précise : réponses du serveur, canaux.
  kPublic = 0,
  kPublicChat = 1,
  kWhisper = 2,
  kPartyChat = 3,
  kGuildChat = 4,
  kAllianceChat = 5,
  kItemGetDrop = 6,
  kEquipmentOnOff = 7,
  kAbnormalStatus = 8,
  kPartyItem = 9,
  kPartyStatus = 10,
  kSkillFailure = 11,
  kPartySetup = 12,
  kDamagedEquipment = 13,
  kSiegeInfo = 14,
  kPartySearch = 15,
  kBattle = 16,
  kPartyBattle = 17,
  kExperience = 18,
  kQuestInfo = 19,
  kBattlefield = 20,
  kClanChat = 21,
  kCall = 22,
  kRepaymentExp = 23,
  kEquipAttributes = 24,
  // Les annonces du serveur, que le client d'origine ne savait pas filtrer.
  kBroadcast = 25,
  // CE QUI SE DIT SUR LE CANAL DISCORD DU SERVEUR, relayé dans le jeu.
  kDiscord = 26,
  // CE QUE LE GRIMOIRE ÉCRIT QUAND LE JOUEUR ARME OU LANCE UN SKILL : « Nom :
  // choisissez une cible. », « Nom lancé. ». Une ligne par sort, d'où sa case
  // à part, que les onglets laissent DÉCOCHÉE D'OFFICE. L'échec d'envoi n'y
  // va pas : il garde la case d'un message du serveur.
  kSkillUse = 27,
};
inline constexpr std::size_t kChatTypeCount = 28;

// LE TYPE QU'UNE LIGNE PREND QUAND SON AUTEUR N'EN DIT PAS DAVANTAGE.
//
// ⚠ UN SITE QUI EN SAIT PLUS LE DONNE LUI-MÊME : un refus de compétence est
// un message du serveur, mais sa case est « échecs de compétence ».
[[nodiscard]] constexpr ChatType DefaultChatType(ChatKind kind) {
  switch (kind) {
    case ChatKind::kPublic:
    case ChatKind::kSelf:
      return ChatType::kPublicChat;
    case ChatKind::kAnnouncement:
      return ChatType::kBroadcast;
    case ChatKind::kWhisper:
      return ChatType::kWhisper;
    case ChatKind::kParty:
      return ChatType::kPartyChat;
    case ChatKind::kGuild:
      return ChatType::kGuildChat;
    case ChatKind::kClan:
      return ChatType::kClanChat;
    case ChatKind::kItem:
      return ChatType::kItemGetDrop;
    case ChatKind::kBattle:
      return ChatType::kBattle;
    case ChatKind::kPartyBattle:
      return ChatType::kPartyBattle;
    case ChatKind::kExp:
      return ChatType::kExperience;
    case ChatKind::kCall:
      return ChatType::kCall;
    case ChatKind::kBattlefield:
      return ChatType::kBattlefield;
    case ChatKind::kServer:
    case ChatKind::kChannel:
      break;
  }
  return ChatType::kPublic;
}

// QUI A ÉCRIT UNE LIGNE, quand le serveur le dit à part du texte : la parole
// d'un joueur, ou un message du relais Discord.
//
// ⚠ CE N'EST PAS L'ORATEUR (`speaker_id`) : un orateur non nul met une bulle
// au-dessus d'une tête, et la parole de guilde, de clan ou d'un canal n'en met
// pas.
struct ChatAuthor {
  // Zéro tous deux sous un faux nom : on ne le désigne que par son nom.
  std::uint32_t account_id = 0;
  std::uint32_t char_id = 0;
  // Le nom AFFICHÉ, en UTF-8.
  std::string name;
  // Où le nom se trouve dans le texte de la ligne, en octets UTF-8.
  std::uint32_t name_offset = 0;
  std::uint32_t name_length = 0;
  // Où commence ce qu'il dit, derrière « Nom : », en octets UTF-8.
  std::uint32_t said_offset = 0;
  // DISCORD seulement : l'identifiant de l'auteur, zéro s'il est inconnu, et
  // le chemin de son avatar relatif au CDN, déjà validé, vide s'il n'en a pas.
  std::uint64_t discord_user_id = 0;
  std::string avatar_path;
};

// Une ligne de la conversation, telle qu'elle est arrivée.
struct ChatLine {
  // SON RANG, jamais réemployé, dans l'ordre où elle est arrivée. Il sert à
  // désigner « tout ce qui précède » sans toucher au tampon — une fenêtre qui
  // se vide masque, elle ne détruit pas, parce que le tampon est PARTAGÉ par
  // tous les onglets.
  //
  // ⚠ IL NE SURVIT PAS À LA PARTIE, et il ne le peut pas : les rangs sont
  // réattribués à chaque session, donc un rang relu d'un fichier ne désignerait
  // plus rien. Ce qui s'appuie dessus vaut pour la session en cours.
  std::uint64_t seq = 0;
  ChatKind kind = ChatKind::kServer;
  // La case de filtre où elle se range — voir `ChatType`.
  ChatType type = ChatType::kPublic;
  // Qui l'a dite, ou ZÉRO pour ce qui ne vient de personne.
  std::uint32_t speaker_id = 0;
  // CE QU'UN CHUCHOTEMENT PORTE DE PLUS : son sens, l'autre, et ce que l'autre
  // est pour nous. `side` vaut `kNone` sur toute autre ligne.
  //
  // ⚠ LE TEXTE NE PORTE PAS L'EN-TÊTE : « ( From Nom ) : » se compose à
  // l'affichage, parce qu'une conversation à part montre la même ligne sous une
  // autre forme que le fil.
  WhisperFacts whisper;
  std::string text;
  std::uint32_t said_ms = 0;
  // LA COULEUR QUE LE SERVEUR A DEMANDÉE, en `0xRRGGBB`, ou ZÉRO pour « il
  // n'en a pas demandé » — c'est alors celle du genre qui vaut.
  //
  // ⚠ TROIS PAQUETS EN PORTENT UNE, et la jeter les rendait tous de la même
  // teinte : l'annonce habillée (`ZC_BROADCAST2`), le canal, et la parole
  // colorée d'un PNJ ou du serveur (`ZC_NPC_CHAT`). C'est même la RAISON
  // d'être du second paquet d'annonce : le premier n'a pas de champ pour elle.
  //
  // ⚠ ZÉRO N'EST PAS LE NOIR, et ce n'est pas une perte : sur le fond sombre
  // de la conversation, un texte noir ne se lirait pas — aucun serveur ne
  // l'envoie.
  std::uint32_t color = 0;
  // Son auteur, quand le serveur l'a décrit ; le texte reste ENTIER.
  std::optional<ChatAuthor> author;
};

// COMBIEN DE LIGNES LA CONVERSATION GARDE, PAR GENRE. Au-delà, la plus
// ancienne de CE genre-là s'en va.
//
// 🔴 PAR GENRE, ET NON EN TOUT, et la différence se paie en lignes
// perdues. Un tampon commun fait disparaître ce qui est RARE à cause de ce qui
// est BAVARD : un mot de guilde s'efface parce que la carte parle beaucoup,
// alors que c'est justement celui-là qu'on remonte chercher. Les onglets
// filtrent par genre ; la borne suit la même découpe que la question.
inline constexpr int kChatLinesKeptPerKind = 1000;
// Les bornes du réglage. En deçà du plancher, une conversation active efface
// ce qu'on vient de lire ; au-delà du plafond, on garde une session entière
// que personne ne relit.
inline constexpr int kFewestChatLinesKept = 50;
inline constexpr int kMostChatLinesKept = 5000;

}  // namespace ro::sim
