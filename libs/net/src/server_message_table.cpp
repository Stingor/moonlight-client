// La table des messages par numéro. Voir `ro/net/server_message.h`.
//
// ⚠ AUCUN TEXTE NE PASSE PAR printf : le seul gabarit, celui qui porte `%d`,
// se remplit par substitution. Un `%` littéral s'écrit donc tel quel.

#include "server_message_table.h"

#include <algorithm>
#include <array>
#include <cstdint>
#include <span>

#include "ro/core/i18n.h"
#include "ro/net/server_message.h"

namespace ro::net {
namespace {

using core::TrText;

// TRIÉE PAR NUMÉRO CROISSANT, sans doublon : la recherche est dichotomique.
constexpr std::array kServerMessages = {
    ServerMessageEntry{52, TrText("Vous ne pouvez plus porter d'items : vous êtes en surcharge.")},
    ServerMessageEntry{
        220, TrText("Vous ne pouvez pas prendre cet item : la limite de poids serait dépassée.")},
    ServerMessageEntry{372, TrText("Vous ne pouvez pas équiper cet item.")},
    ServerMessageEntry{663, TrText("Vous ne pouvez pas utiliser cet item assis.")},
    ServerMessageEntry{1221,
                       TrText("Le membre du groupe n'a pas été invoqué car vous n'êtes pas le "
                              "chef du groupe.")},
    ServerMessageEntry{1222, TrText("Aucun membre du groupe à invoquer sur la carte actuelle.")},
    ServerMessageEntry{1266, TrText("Le contrat du mercenaire a expiré.")},
    ServerMessageEntry{1267, TrText("Le mercenaire est mort.")},
    ServerMessageEntry{1334, TrText("Ce skill ne peut pas être utilisé dans cette zone.")},
    ServerMessageEntry{1335, TrText("Cet item ne peut pas être utilisé dans cette zone.")},
    ServerMessageEntry{
        1357, TrText("Les informations d'équipement de ce personnage ne sont pas publiques.")},
    ServerMessageEntry{1435, TrText("Item utilisable uniquement à bord du Mado Gear.")},
    ServerMessageEntry{1519, TrText("Disponible uniquement avec un cart équipé.")},
    ServerMessageEntry{1570, TrText("Combinaison non convertible")},
    ServerMessageEntry{1572, TrText("Libérez de la place dans l'inventory")},
    ServerMessageEntry{1574, TrText("Réussite")},
    ServerMessageEntry{1575, TrText("Échec")},
    ServerMessageEntry{1581,
                       TrText("Les items non identifiés ne peuvent pas servir de matériaux.")},
    ServerMessageEntry{1691, TrText("[Bow] doit être équipé.")},
    ServerMessageEntry{1862, TrText("Encore %d secondes avant de pouvoir réutiliser l'item.")},
    ServerMessageEntry{1923,
                       TrText("Terminez l'action en cours (dialogue NPC, fabrication...) puis "
                              "réessayez.")},
    ServerMessageEntry{1931, TrText("Item inutilisable lorsque vous êtes sur une monture.")},
    ServerMessageEntry{2094,
                       TrText("Le chef de groupe ne peut être changé que sur la même carte.")},
    ServerMessageEntry{2183, TrText("Aucun item fusionnable disponible.")},
    ServerMessageEntry{
        2493, TrText("Ces munitions ne sont pas compatibles avec l'arme que vous équipez.")},
    ServerMessageEntry{2554, TrText("Une arme de type [Gatling Gun] doit être équipée.")},
    ServerMessageEntry{2555, TrText("Une arme de type [Shotgun] doit être équipée.")},
    ServerMessageEntry{2556, TrText("Une arme de type [Rifle] doit être équipée.")},
    ServerMessageEntry{2557, TrText("Une arme de type [Revolver] doit être équipée.")},
    ServerMessageEntry{2558, TrText("[Silver Bullet] doit être équipé.")},
    ServerMessageEntry{2561, TrText("Une arme de type [Grenade Launcher] doit être équipée.")},
    ServerMessageEntry{
        2963, TrText("Impossible de déléguer le rôle de chef de guilde pendant les heures de WoE")},
    ServerMessageEntry{
        2964, TrText("Vous devez attendre un jour avant de désigner un nouveau chef de guilde")},
    ServerMessageEntry{3015, TrText("Cet item est enregistré pour l'échange d'équipement.")},
    ServerMessageEntry{3022, TrText("La limite de possession dépasse 70 %, ou il reste moins de 10 "
                                    "emplacements libres dans l'inventory.")},
    ServerMessageEntry{3474, TrText("Aucun événement de contrôle de présence n'est en cours.")},
    ServerMessageEntry{3475, TrText("L'expérience de la guilde est au maximum : vous ne pouvez "
                                    "plus accumuler d'expérience de guilde.")},
    ServerMessageEntry{3476, TrText("L'expérience du personnage est au maximum : vous ne pouvez "
                                    "plus accumuler d'expérience de guilde.")},
    ServerMessageEntry{3826, TrText("Le total des items à vendre dépasse la quantité de zeny que "
                                    "vous pouvez posséder. \nVeuillez corriger la quantité et le "
                                    "prix.")},
    ServerMessageEntry{3837, TrText("Votre poids dépasse 70 %. Réduisez le poids et réessayez.")},
};

// LES REFUS ÉCRITS EN ROUGE par `ZC_MSG` et `ZC_MSG_COLOR`, même quand le
// paquet demande une autre couleur ; les autres messages par numéro ne
// rougissent jamais. Certains n'ont pas de texte dans la table : la couleur ne
// dépend pas d'elle. Les refus de surcharge (52, 220) restent blancs.
constexpr std::array<std::uint16_t, 5> kRedMessageIds = {1221, 1222, 1879, 1880, 2182};

// LES PLAFONDS DU TRIBUT D'EXPÉRIENCE À LA GUILDE : celle de la guilde est
// pleine, ou celle du personnage. Le client d'origine les range sous la case
// « EXP de contribution ».
constexpr std::array<std::uint16_t, 2> kGuildExpTributeMessageIds = {3475, 3476};

[[nodiscard]] constexpr bool SortedWithoutDuplicate() {
  for (std::size_t index = 1; index < kServerMessages.size(); ++index) {
    if (kServerMessages[index - 1].message_id >= kServerMessages[index].message_id) {
      return false;
    }
  }
  return std::ranges::is_sorted(kRedMessageIds) &&
         std::ranges::adjacent_find(kRedMessageIds) == kRedMessageIds.end() &&
         std::ranges::is_sorted(kGuildExpTributeMessageIds) &&
         std::ranges::adjacent_find(kGuildExpTributeMessageIds) == kGuildExpTributeMessageIds.end();
}
static_assert(SortedWithoutDuplicate(), "les tables de messages se cherchent par dichotomie");

}  // namespace

std::span<const ServerMessageEntry> ServerMessageTable() { return kServerMessages; }

const char* ServerMessageFrench(std::uint16_t message_id) {
  const auto found =
      std::ranges::lower_bound(kServerMessages, message_id, {}, &ServerMessageEntry::message_id);
  if (found == kServerMessages.end() || found->message_id != message_id) {
    return nullptr;
  }
  return found->french;
}

bool ServerMessageIsRed(std::uint16_t message_id) {
  return std::ranges::binary_search(kRedMessageIds, message_id);
}

bool ServerMessageIsGuildExpTribute(std::uint16_t message_id) {
  return std::ranges::binary_search(kGuildExpTributeMessageIds, message_id);
}

}  // namespace ro::net
