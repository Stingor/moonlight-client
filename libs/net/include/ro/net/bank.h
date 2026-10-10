#pragma once

#include <cstdint>

#include "ro/core/error.h"
#include "ro/net/connection.h"
#include "ro/net/world_update.h"
#include "ro/sim/world_fwd.h"

namespace ro::net {

// LA BANQUE DE ZENY : trois paquets reçus, trois paquets émis.
//
// 🔴 LE CLIENT N'OUVRE JAMAIS LA BANQUE LUI-MÊME, et c'est la clé de tout le
// reste. Le raccourci n'ouvre rien : il DEMANDE (`CZ_REQ_BANKING_CHECK`), et
// c'est la réponse `ZC_BANKING_CHECK` qui ouvre la fenêtre — ou la referme si
// elle était ouverte. Le paquet BASCULE.
//
// Deux conséquences qu'on paie cher à ignorer :
//   · pas de bouton « rafraîchir » — redemander le solde referme la banque ;
//   · ouvrir sans aller-retour afficherait le solde de la dernière ouverture.
//
// ⚠ LA DISPOSITION DE `0x09A6` EST L'INVERSE DE CELLE DES DEUX ACCUSÉS : le
// solde y est en premier et la raison ensuite, alors que `0x09A8`/`0x09AA`
// mettent la raison en tête. C'est bien le protocole, pas une erreur de relevé.
//
// ⚠ ET LES RAISONS NE SE NUMÉROTENT PAS PAREIL D'UN PAQUET À L'AUTRE : la
// raison 2 veut dire « le système travaille » à l'ouverture, « pas assez de
// zeny » sur un dépôt, « erreur inconnue » sur un retrait. C'est ici qu'elles
// deviennent des verdicts nommés — voir `ro/sim/bank.h`.
[[nodiscard]] FrameOutcome ApplyBankFrame(const Frame& frame, sim::World& world);

// ── LES VERBES ──────────────────────────────────────────────────────────────
//
// ⚠ ILS ÉMETTENT, ET RIEN DE PLUS, comme ceux du sac : le solde ne bouge qu'à
// la réponse du serveur, qui revalide tout (`pc_bank_deposit`,
// `pc_bank_withdraw`). Une avance d'affichage montrerait un dépôt que le
// serveur peut refuser.
//
// ⚠ `account_id` EST L'IDENTIFIANT DU JOUEUR SUR LA CARTE — le serveur de zone
// donne au personnage l'identifiant de son COMPTE, et c'est `world.player_id()`
// qui le porte. La banque est d'ailleurs au compte, pas au personnage.

// « Ouvre-moi la banque » — ou referme-la, si elle est ouverte. C'est le MÊME
// paquet dans les deux sens : le serveur ne tient aucun état, c'est sa réponse
// qui bascule la nôtre.
[[nodiscard]] core::Result<void> RequestBankCheck(Connection& link, std::uint32_t account_id);

// ⚠ LE MONTANT EST BORNÉ À `INT32` DE BOUT EN BOUT — voir `sim::kBankZenyMax`.
// Le champ du fil est signé côté serveur (`int32 Money`), qui refuse tout ce
// qui est négatif ou nul.
[[nodiscard]] core::Result<void> RequestBankDeposit(Connection& link, std::uint32_t account_id,
                                                    std::int32_t zeny);
[[nodiscard]] core::Result<void> RequestBankWithdraw(Connection& link, std::uint32_t account_id,
                                                     std::int32_t zeny);

}  // namespace ro::net
