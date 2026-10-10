#pragma once

#include <cstdint>

#include "ro/core/error.h"
#include "ro/net/connection.h"
#include "ro/net/world_update.h"
#include "ro/sim/viewed_equipment.h"
#include "ro/sim/world_fwd.h"

namespace ro::net {

// CE QUE LE PERSONNAGE PORTE, traduit dans le monde.
//
// ⚠ TROIS CONTENANTS, UN SEUL JEU D'OPCODES. Le sac, la charrette et le coffre
// arrivent par les mêmes paquets ; c'est le champ `invType` du paquet
// d'ouverture — répété dans chacun des suivants — qui dit lequel on remplit.
// Choisir d'après l'opcode rangerait tout au même endroit.
//
// Même contrat que `ApplyFrame` : rien n'est inventé, et `sim` ne voit aucun
// octet de protocole. `now_ms` est l'horloge de l'APPELANT.
//
// ⚠ ELLE N'A PAS DE DÉFAUT, et c'est la même raison que pour `ApplyNoticeFrame`
// et `ApplyWorldSkillFrame` : le journal des ramassages DATE ce qu'il garde, et
// un défaut daterait au hasard ce qu'un appelant pressé oublierait de dire — le
// bandeau montrerait alors des objets nés tous au même instant.
[[nodiscard]] FrameOutcome ApplyInventoryFrame(const Frame& frame, sim::World& world,
                                               std::uint32_t now_ms);

// ── Les opcodes que la structure ne porte pas ────────────────────────────────
//
// ⚠ CES TROIS-LÀ NE SE DÉDUISENT D'AUCUNE STRUCTURE, et c'est pour cela qu'ils
// sont écrits ici plutôt qu'engendrés. Le serveur ne les rattache à leur
// disposition par aucune règle : il les pose depuis son code, par une constante
// d'énumération (`inventorylistnormalType` et ses sœurs, `packets_struct.hpp`)
// ou par un `WFIFOW` nu. Le générateur produit donc leur disposition sans leur
// numéro, et le numéro se nomme ici.

// La liste des objets ORDINAIRES d'un contenant. `inventorylistnormalType`,
// `storageListNormalType`, `cartlistnormalType` — trois noms, une valeur.
inline constexpr std::uint16_t kItemListNormalOpcode = 0x0B09;

// La liste des ÉQUIPEMENTS du même contenant. `inventorylistequipType` et ses
// sœurs.
inline constexpr std::uint16_t kItemListEquipOpcode = 0x0B39;

// LE SEUIL DE SURCHARGE, en pour cent. ⚠ Ce n'est PAS le poids maximum : c'est
// la part au-delà de laquelle le jeu passe la jauge en rouge et arrête la
// régénération naturelle (`clif_weight_limit`, depuis `natural_heal_weight_rate`).
inline constexpr std::uint16_t kOverweightPercentOpcode = 0x0ADE;

// ── LES VERBES ──────────────────────────────────────────────────────────────
//
// ⚠ ILS ÉMETTENT, ET RIEN DE PLUS. Le sac ne bouge qu'en recevant la réponse du
// serveur : une demande refusée — surcharge, objet lié, carte impossible —
// laisserait sinon le sac dans un état que personne d'autre ne partage.
//
// ⚠ L'INDICE EST CELUI DU FIL, tel que le serveur l'a donné. Il lui retranche
// son décalage à la lecture ; le défaire ici le retrancherait deux fois.

[[nodiscard]] core::Result<void> RequestUseItem(Connection& link, std::uint16_t client_slot,
                                                std::uint32_t account_id);

// `wearable_at` dit où la pièce PEUT se porter, pas où elle ira : le serveur
// choisit, et ne s'en sert que pour trancher entre deux places possibles.
[[nodiscard]] core::Result<void> RequestEquip(Connection& link, std::uint16_t client_slot,
                                              std::uint32_t wearable_at);

[[nodiscard]] core::Result<void> RequestUnequip(Connection& link, std::uint16_t client_slot);

[[nodiscard]] core::Result<void> RequestDropItem(Connection& link, std::uint16_t client_slot,
                                                 std::uint16_t amount);

// Range ou sort un objet de l'onglet des favoris.
//
// ⚠ `favorite` DIT CE QU'ON VEUT OBTENIR, pas ce qu'on met sur le fil : le
// champ du paquet est inversé, et cette fonction existe surtout pour que
// l'inversion vive à UN seul endroit.
[[nodiscard]] core::Result<void> RequestFavorite(Connection& link, std::uint16_t client_slot,
                                                 bool favorite);

// « Je sors ceci de ma charrette. »
//
// ⚠ LE SERVEUR LE REFUSE PENDANT QU'UN COFFRE EST OUVERT, et il ne le dit
// pas : ce geste n'a pas d'accusé. C'est la liste de la charrette qui
// répondra — ou ne répondra pas.
// « Je range ceci dans ma charrette. » Même porte que le retour, et le même
// silence du serveur en cas de refus.
[[nodiscard]] core::Result<void> RequestInventoryToCart(Connection& link, std::uint16_t client_slot,
                                                        std::int32_t amount);

[[nodiscard]] core::Result<void> RequestCartToInventory(Connection& link, std::uint16_t client_slot,
                                                        std::int32_t amount);

// ── LE COFFRE ───────────────────────────────────────────────────────────────
//
// ⚠ IL NE S'OUVRE PAS D'ICI. Un coffre est une SESSION que le serveur ouvre —
// un PNJ, une compétence —, et le client ne fait que la refermer. Il n'y a donc
// pas de verbe d'ouverture, et ce n'est pas un oubli.
//
// 🔴 ET LES DEUX SENS NE PARLENT PAS DE LA MÊME CASE. Le serveur retranche deux
// à l'indice du sac et un à celui du coffre : chaque verbe veut la case du
// contenant D'OÙ L'OBJET PART, et les intervertir désigne l'objet d'à côté.

// « Je range ceci dans le coffre. » La case est celle du SAC.
[[nodiscard]] core::Result<void> RequestInventoryToStorage(Connection& link,
                                                           std::uint16_t client_slot,
                                                           std::int32_t amount);

// « Je sors ceci du coffre. » La case est celle du COFFRE.
[[nodiscard]] core::Result<void> RequestStorageToInventory(Connection& link,
                                                           std::uint16_t client_slot,
                                                           std::int32_t amount);

// « Je range ceci dans le coffre, depuis ma charrette. »
//
// ⚠ LE SERVEUR L'ACCEPTE PENDANT QU'UN COFFRE EST OUVERT, contrairement à
// l'échange sac <-> charrette : ces deux verbes-là ne passent pas par
// `pc_cant_act2`.
[[nodiscard]] core::Result<void> RequestCartToStorage(Connection& link, std::uint16_t client_slot,
                                                      std::int32_t amount);

[[nodiscard]] core::Result<void> RequestStorageToCart(Connection& link, std::uint16_t client_slot,
                                                      std::int32_t amount);

// « Je referme le coffre. »
//
// 🔴 CE N'EST PAS QUE REFERMER UNE FENÊTRE. Tant que la session dure, le serveur
// refuse en silence les échanges entre le sac et la charrette ; ne pas le dire
// laisserait le personnage bloqué sur un coffre que plus rien ne montre.
[[nodiscard]] core::Result<void> RequestCloseStorage(Connection& link);

// ── L'ÉQUIPEMENT D'UN AUTRE JOUEUR ──────────────────────────────────────────
//
// `CZ 0x02D6` demande, `ZC 0x0B37` montre. Le serveur revérifie tout — même
// carte, et que la cible laisse voir son équipement — et la plupart de ses
// refus sont SILENCIEUX : une demande peut ne jamais recevoir de réponse.

// ⚠ HORS DU MONDE, comme les titres : voir `ro/sim/viewed_equipment.h`.
[[nodiscard]] FrameOutcome ApplyViewEquipFrame(const Frame& frame, sim::ViewedEquipmentBook& book);

[[nodiscard]] core::Result<void> RequestViewEquip(Connection& link, std::uint32_t account_id);

}  // namespace ro::net
