#include "ro/net/refusal_text.h"

#include <string>

#include "ro/core/i18n.h"

namespace ro::net {
namespace {

// Le motif, quand le serveur en donne un que nous ne connaissons pas.
//
// ⚠ IL SE DIT QUAND MÊME, avec son numéro. Se taire laisserait le joueur devant
// une session coupée sans un mot, et le numéro est ce qui permet de nous le
// rapporter.
std::string UnknownReason(std::string_view what, unsigned code) {
  return std::string(what) + " (code " + std::to_string(code) + ")";
}

}  // namespace

std::string BanText(std::uint8_t code) {
  // LES CODES AU-DELÀ DE 15 NE SONT PAS ICI, et c'est délibéré : ce sont les
  // états de facturation de Gravity et les refus propres au Japon et à la
  // Corée — aucun site d'appel de ce serveur ne les émet, et leur inventer un
  // français serait écrire une réponse à une question que personne ne pose. Le
  // motif par défaut les couvre, avec leur numéro.
  switch (code) {
    case 0:  // BAN_UNFAIR
      return core::Tr("le serveur a mis fin à la session");
    case 1:  // server closed
      return core::Tr("le serveur ferme");
    case 2:  // ID already logged in
      return core::Tr("quelqu'un vient de se connecter avec ce compte");
    case 3:
      // timeout / too much lag chez Gravity — MAIS c'est aussi, et surtout, le
      // renvoi pour intégrité de ce serveur-ci. Les deux causes tiennent dans
      // la même phrase : dans un cas comme dans l'autre, le serveur n'a pas eu
      // de ce client ce qu'il attendait.
      return core::Tr("le serveur n'a plus rien reçu de ce client, ou n'a pas reconnu sa version");
    case 4:  // server full
      return core::Tr("le serveur est plein");
    case 5:  // underaged
      return core::Tr("ce compte n'a pas l'âge requis");
    case 8:  // Server still recognizes last connection
      return core::Tr(
          "le serveur tient encore la connexion précédente ; réessayer dans une minute");
    case 9:  // too many connections from this ip
      return core::Tr("trop de connexions depuis cette adresse");
    case 10:  // out of available time paid for
      return core::Tr("le temps de jeu payé pour ce compte est épuisé");
    case 15:  // disconnected by a GM
      return core::Tr("un membre de l'équipe a déconnecté ce compte");
    default:
      return UnknownReason(core::Tr("le serveur a mis fin à la session"), code);
  }
}

std::string LoginRefusalText(std::uint32_t code, std::string_view unblock_time) {
  // LE CODE 6 EST LE SEUL QUI LISE LA DATE, et il ne la trouve pas toujours :
  // voir l'en-tête. Sans elle, dire « jusqu'au » suivi de rien poserait la
  // question sans la réponse.
  constexpr std::uint32_t kBanned = 6;
  if (code == kBanned) {
    return unblock_time.empty()
               ? std::string("ce compte est banni ; le serveur n'a pas dit jusqu'à quand")
               : "ce compte est banni jusqu'au " + std::string(unblock_time);
  }

  switch (code) {
    case 0:  // Unregistered ID
      return "ce compte n'existe pas";
    case 1:  // Incorrect Password
      return "mot de passe incorrect";
    case 2:  // Account Expired
      return "ce compte a expiré";
    case 3:  // Rejected from server
      return "connexion refusée par le serveur";
    case 4:  // Blocked by GM
      return "ce compte a été bloqué par l'équipe";
    case 5:  // Not latest game EXE
      return "ce client n'est pas à la version que le serveur attend";
    case 7:  // Server Over-population
      return "le serveur est saturé";
    case 8:  // Account limit from company
      return "trop de comptes connectés depuis ce lieu";
    case 9:   // Ban by DBA
    case 11:  // Ban by GM
      return "ce compte a été banni par l'équipe";
    case 10:  // Email not confirmed
      return "l'adresse de courriel de ce compte n'a pas été confirmée";
    case 12:  // Working in DB
      return "maintenance en cours sur les comptes ; réessayer plus tard";
    case 13:  // Self Lock
      return "ce compte a été verrouillé par son propriétaire";
    case 14:  // Not Permitted Group
    case 15:  // Not Permitted Group
      return "ce compte n'a pas le droit d'entrer sur ce serveur";
    default:
      return UnknownReason("connexion refusée", code);
  }
}

std::string MakeCharRefusalText(std::uint8_t code) {
  switch (code) {
    case 0x00:  // 'Charname already exists' (-1)
      // ⚠ « PRIS », ET NON « PRIS PAR UN JOUEUR » : le serveur rend ce code
      // aussi pour ses noms réservés — celui de ses messages internes et celui
      // du spectateur —, qu'aucune recherche en base ne trouve.
      return "ce nom est déjà pris";
    case 0x01:  // 'You are underaged' (-3)
      return "ce compte est trop jeune pour créer un personnage";
    case 0x02:  // Symbols in Character Names are forbidden
      // Ce serveur ne l'émet pas ; son client d'origine le connaît, et un
      // serveur voisin pourrait le reprendre.
      return "les symboles sont interdits dans un nom de personnage";
    case 0x03:  // 'You are not elegible to open the Character Slot' (-4)
      return "cet emplacement n'est pas ouvert sur ce compte";
    case 0xFF:  // 'Char creation denied' (-2)
      // 🔴 LE FOURRE-TOUT, et c'est pourquoi le libellé énumère. Le serveur ne
      // dit pas laquelle de ses dix causes a mordu.
      return "création refusée : nom trop court, caractères interdits, "
             "emplacement déjà pris ou plafond de personnages atteint";
    default:
      return UnknownReason("création refusée", code);
  }
}

std::string DeleteReserveRefusalText(std::int32_t result) {
  switch (result) {
    case 0:  // character already queued for deletion
      return "la suppression de ce personnage est déjà programmée";
    case 3:  // database error, or character not found
      return "suppression impossible : personnage introuvable ou erreur de base";
    case 4:  // CHAR_DEL_RESTRICT_GUILD
      return "ce personnage est dans une GUILDE ; quitte-la d'abord";
    case 5:  // CHAR_DEL_RESTRICT_PARTY
      return "ce personnage est dans un GROUPE ; quitte-le d'abord";
    case 6:
      // ⚠ CE SERVEUR NE L'ÉMET PAS, et on le garde quand même : le client
      // d'origine le connaît, et un serveur voisin peut le reprendre. Un cas de plus ne coûte
      // qu'une ligne ; le cas manquant coûte un message faux.
      return "ce personnage tient une échoppe";
    default:
      return UnknownReason("suppression refusée", static_cast<unsigned>(result));
  }
}

std::string DeleteCancelRefusalText(std::int32_t result) {
  switch (result) {
    case 2:  // character not found, or database error
      return "annulation impossible : personnage introuvable ou erreur de base";
    default:
      return UnknownReason("annulation refusée", static_cast<unsigned>(result));
  }
}

std::string DeleteRefusalText(std::uint8_t code) {
  switch (code) {
    case 0:
      // 🔴 QUATRE CAUSES SOUS UN SEUL CODE, et le libellé d'origine — « adresse
      // email incorrecte » — est le plus trompeur des quatre : `char_delete`
      // y tombe aussi quand l'heure de suppression n'est ni posée ni atteinte.
      return "suppression refusée : l'adresse de courriel ne correspond pas, "
             "ou l'heure de suppression n'est pas encore atteinte";
    case 1:  // CHAR_DELETE_NOTFOUND
      return "suppression impossible : ce personnage est introuvable";
    case 2:  // CHAR_DELETE_GUILD ou CHAR_DELETE_PARTY
      // ⚠ LES DEUX SE CONFONDENT ICI, alors que la RÉSERVATION les distingue
      // (4 et 5). La confirmation ne dit donc pas lequel : l'écran invite à
      // quitter les deux.
      return "ce personnage est dans une guilde ou un groupe ; quitte-les d'abord";
    default:
      return UnknownReason("suppression refusée", code);
  }
}

std::string MoveRefusalText(std::uint16_t reason) {
  constexpr std::uint16_t kMoveFailed = 1;
  if (reason != kMoveFailed) {
    return UnknownReason(core::Tr("déplacement refusé"), reason);
  }
  return core::Tr(
      "déplacement refusé : aucun déplacement disponible pour ce personnage, place hors du "
      "compte, ou place occupée que le serveur n'échange pas");
}

std::string RenameRefusalText(std::uint32_t result) {
  switch (result) {
    case 1:
      return core::Tr("ce personnage n'a plus de renommage disponible");
    case 2:
      return core::Tr("renommage refusé : le serveur ne reconnaît pas ce personnage");
    case 3:
      return core::Tr("renommage refusé : erreur de base du serveur");
    case 4:
      return core::Tr("ce nom est déjà pris ; choisis-en un autre");
    case 5:
      return core::Tr("ce personnage est dans une guilde ; quitte-la d'abord");
    case 6:
      return core::Tr("ce personnage est dans un groupe ; quitte-le d'abord");
    case 7:
      return core::Tr("ce nom est trop long");
    case 8:
      // ⚠ LE SERVEUR Y RANGE TOUT CE QUE `char_check_char_name` REFUSE, hors
      // du nom déjà pris : trop court, lettres interdites, nom réservé.
      return core::Tr("ce nom n'est pas accepté : trop court, lettres interdites ou nom réservé");
    case 9:
      return core::Tr("le renommage est interdit sur ce serveur");
    default:
      return UnknownReason(core::Tr("renommage refusé"), result);
  }
}

}  // namespace ro::net
