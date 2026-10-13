#pragma once

#include <cstddef>
#include <cstdint>
#include <deque>
#include <optional>
#include <string>
#include <string_view>

namespace ro::sim {

// LES CHUCHOTEMENTS : ce qu'une ligne en dit de plus qu'une autre, et ce qui
// attend la réponse du serveur.
//
// 🔴 LE SERVEUR NE RENVOIE PAS CE QU'ON A CHUCHOTÉ. Il répond à chaque envoi
// par un accusé qui ne porte qu'un code — parti, absent, ignoré — et rien du
// texte ni du destinataire. L'écho que le joueur lit, « ( To Nom ) : … », se
// compose donc chez nous, de ce qu'on avait gardé au moment d'envoyer.

// LE SENS D'UNE LIGNE DE CHUCHOTEMENT, ou `kNone` pour toute autre ligne.
enum class WhisperSide : std::uint8_t {
  kNone,
  // Ce qu'un autre nous a chuchoté.
  kReceived,
  // L'écho de notre envoi, composé quand le serveur l'a accusé parti.
  kSent,
  // Un envoi que le serveur n'a pas remis : absent, ignoré, refusé.
  kUndelivered,
};

// CE QUE L'AUTRE EST POUR NOUS, au moment où la ligne arrive. C'est elle qui
// choisit l'en-tête de la ligne, comme dans le client d'origine.
//
// ⚠ ELLE N'EST PAS SYMÉTRIQUE : on peut tenir quelqu'un pour ami sans qu'il nous
// tienne pour tel, et deux joueurs lisent alors la même conversation sous deux
// en-têtes différents. Ce n'est pas un défaut d'affichage.
//
// ⚠ ET ELLE SE FIGE À L'ARRIVÉE : un ami ajouté plus tard ne réécrit pas ce
// qui a été dit avant.
enum class WhisperRelation : std::uint8_t {
  kStranger,
  // Dans NOTRE liste d'amis. Elle l'emporte sur tout le reste.
  kFriend,
  // Un membre du staff, que le serveur signale par un drapeau du paquet.
  kStaff,
  // Dans NOTRE guilde.
  kGuildMember,
};

// CE QU'UN ACCUSÉ DIT D'UN ENVOI, tel que le serveur le code (`e_ack_whisper`).
// ⚠ UN SERVEUR PLUS RÉCENT PEUT EN ENVOYER D'AUTRES : la valeur se garde telle
// quelle, et l'affichage a un repli pour ce qu'il ne connaît pas.
enum class WhisperRefusal : std::uint8_t {
  kNone = 0,
  // Personne de ce nom n'est connecté.
  kOffline = 1,
  // Le destinataire nous ignore.
  kIgnored = 2,
  // Le destinataire ignore tout le monde.
  kIgnoresAll = 3,
};

// Ce qu'une ligne de chuchotement porte de plus que son texte.
struct WhisperFacts {
  WhisperSide side = WhisperSide::kNone;
  // L'AUTRE, dans les deux sens : l'auteur d'une ligne reçue, le destinataire
  // d'un écho ou d'un refus. ⚠ UN NOM ET NON UN IDENTIFIANT : un chuchotement
  // traverse les cartes, et son auteur n'est pas forcément dans le monde qu'on
  // tient. Vide pour un refus dont l'envoi n'a pas été retrouvé.
  std::string peer_name;
  WhisperRelation relation = WhisperRelation::kStranger;
  // LE CODE D'UN REFUS, tel que l'accusé le porte. C'est lui qui choisit la
  // phrase à l'affichage : la ligne d'un refus n'a pas de texte à elle.
  WhisperRefusal refusal = WhisperRefusal::kNone;
  // REÇU D'UN MEMBRE DE L'ÉQUIPE, que le serveur signale par un drapeau du
  // paquet. ⚠ PAS LA RELATION : un ami de l'équipe reste un ami pour l'en-tête,
  // mais son chuchotement se pose quand même en haut de l'écran.
  bool from_staff = false;
};

// UN ENVOI QUI ATTEND SON ACCUSÉ. Le texte est celui que le joueur a écrit,
// en UTF-8, balises de liens comprises : c'est lui que l'écho montrera.
struct PendingWhisper {
  std::string recipient;
  std::string text;
  std::uint32_t sent_ms = 0;
};

// COMBIEN DE TEMPS UN ENVOI ATTEND SON ACCUSÉ : la durée de vie que le serveur
// donne lui-même à un chuchotement relayé vers une autre carte. Au-delà, il
// l'a oublié, et aucun accusé ne viendra plus.
inline constexpr std::uint32_t kWhisperAnswerWaitMs = 60'000;
// Combien d'envois peuvent attendre à la fois. Bien au-delà de ce qu'une main
// tape en une seconde : la borne n'est là que pour qu'un serveur muet ne fasse
// pas grandir la file sans fin.
inline constexpr std::size_t kMostPendingWhispers = 32;

// LES ENVOIS QUI ATTENDENT LEUR ACCUSÉ, du plus ancien au plus récent.
//
// 🔴 L'APPARIEMENT SE FAIT DANS L'ORDRE, et c'est le seul possible : l'accusé
// ne nomme personne. ⚠ L'ORDRE N'EST POURTANT PAS GARANTI : un destinataire
// absent de la carte passe par le serveur de personnages, dont l'accusé revient
// en différé — un envoi suivant, à un joueur présent, peut être accusé avant
// lui, et les deux échos s'échangent alors. C'est rare : il faut deux envois en
// quelques millisecondes.
//
// ⚠ CERTAINS ENVOIS N'ONT PAS D'ACCUSÉ, et l'appelant écarte ceux qu'il sait
// reconnaître avant de les noter — voir `ro::net::NoteWhisperSent`. Un autre
// n'en reçoit pas non plus — le destinataire tient une échoppe hors ligne, et le
// serveur répond par un chuchotement à lui —, d'où `Forget`. Ce qui reste
// inconnaissable ici attend `kWhisperAnswerWaitMs` avant d'être oublié.
class WhisperOutbox {
 public:
  void Note(std::string recipient, std::string text, std::uint32_t sent_ms);

  // L'envoi que l'accusé qui arrive concerne : le plus ancien, une fois oubliés
  // ceux qui ont attendu plus longtemps que le serveur ne les garde. Rien quand
  // la file est vide — un accusé d'un envoi parti avant la connexion.
  [[nodiscard]] std::optional<PendingWhisper> Settle(std::uint32_t now_ms);

  // Retire le plus ancien envoi à ce destinataire, quand le serveur a répondu
  // autrement que par un accusé. Rend faux si aucun ne l'attendait.
  bool Forget(std::string_view recipient);

  // Une partie s'achève : les accusés des envois d'avant n'arriveront plus.
  void Clear() noexcept { pending_.clear(); }

  [[nodiscard]] std::size_t size() const noexcept { return pending_.size(); }
  [[nodiscard]] const std::deque<PendingWhisper>& pending() const noexcept { return pending_; }

 private:
  std::deque<PendingWhisper> pending_;
};

}  // namespace ro::sim
