#pragma once

#include <cstdint>
#include <span>
#include <string_view>

namespace ro::render {

// CE QU'UN ÉTAT FAIT NAÎTRE, ET CE QU'IL EMPORTE EN PARTANT.
//
// 🔴 TROIS MOMENTS, ET ILS NE SE FONDENT PAS.
//
//   · AU PAQUET — le client reçoit `ZC_MSG_STATE_CHANGE`, crée quelques effets
//     en propre, puis POSTE le message 110.
//   · À LA POSE — l'acteur traite ce message 110, qui porte `(état, posé)`.
//     C'est le gros de la table.
//   · AU REBÂTISSAGE — le client parcourt la liste d'états du JOUEUR et les
//     repose tous d'un coup sur son sprite neuf, en sautant ce que l'acteur
//     porte déjà pour ne pas doubler ce que les deux premiers ont posé.
//
// Les deux grands chemins se recoupent largement, et c'est ce qui les rend
// fiables : deux chemins écrits séparément, qui tombent d'accord. Mais ils ne
// sont pas égaux — certains états ne montrent leur effet qu'au rebâtissage.
//
// ⚠ PLUSIEURS CAS PARTAGENT UN SEMIS COMMUN : ils écrivent leur identifiant
// puis rejoignent le même appel. Chaque couple de la table vaut donc pour son
// CAS, et non pour un site d'appel.
//
// ⚠ CE N'EST PAS LA TABLE DES EFFETS NOMMÉS D'ÉTAT. Celle-là
// (`state_named_table`) donne des FICHIERS ; celle-ci donne des IDENTIFIANTS
// d'effet, et un état peut en poser plusieurs.

// Quand le client natif fait naître cet effet.
enum class StateEffectMoment : std::uint8_t {
  // Gardé par le drapeau « posé » : il ne naît qu'à la POSE de l'état.
  kOnApply,
  // Il ne naît qu'à la LEVÉE — un éclat de fin, et non une aura.
  kOnRelease,
  // Rien ne le garde : le client le fait naître aux DEUX instants.
  //
  // ⚠ ET LE DRAPEAU NE SE LIT PAS DANS LA CONDITION SEULE. Le répartiteur
  // n'écrit pas `if (posé) { ... }` mais `if (!posé) return;` puis la suite :
  // ce qui suit la sortie ne naît qu'à la POSE, et le ranger ici le ferait
  // naître aussi à la chute de l'état.
  kAlways,
};

// Un état, et l'effet qu'il fait naître.
struct StateEffectEntry {
  // L'identifiant EFST du serveur, celui que `status_effects.h` numérote.
  std::uint16_t state_id = 0;
  std::uint16_t effect_id = 0;
  StateEffectMoment moment = StateEffectMoment::kAlways;
  // 🔴 CET EFFET VIT JUSQU'À CE QU'ON LE RETIRE, et c'est ce qui sépare une
  // AURA d'un éclat. Le client déclare la durée de chaque effet dans une table
  // qu'il bâtit à l'exécution (`client_effect_durations.json`) ; `99999999` y est son écriture de
  // l'infini.
  //
  // La liste se lit d'elle-même : STEELBODY, AURABLADE, MARIONETTE, KAITE, les
  // trois chaleurs du Star Gladiator, CAMOUFLAGE, SHADOWFORM, MANHOLE. Ce sont
  // les auras et les marques au sol, et rien d'autre.
  //
  // ⚠ SANS CE DRAPEAU, NOUS LES JOUIONS UNE FOIS. Un `.str` d'aura fait deux
  // secondes ; le client, lui, le reboucle tant que l'état tient.
  bool endless = false;
};

// UN ÉTAT DONT L'EFFET SE CALCULE SUR SA VALEUR, et non sur son identifiant.
//
// 🔴 SIX DES DEUX CENT VINGT-CINQ GESTES RELEVÉS SONT DANS CE CAS, et aucune
// table de couples ne peut les porter : `EFST_ROLLINGCUTTER` montre un anneau
// par tour — `valeur + 1299`, de un à dix — et `EFST_DEVIL1` assombrit la vue
// par paliers. La valeur est le `val1` du paquet d'état, sur le fil depuis
// toujours ; c'est notre `Actor` qui la jetait.
//
// ⚠ ET LE CLIENT EFFACE TOUTE LA FAMILLE AVANT DE POSER LE RANG. Rolling Cutter
// retire les vingt effets des deux familles (1300..1309 et 1467..1476) à chaque
// changement de tour : sans cela, les anneaux s'empileraient.
struct StateValueEffectEntry {
  // L'identifiant EFST du serveur.
  std::uint16_t state_id = 0;
  // L'effet vaut `valeur + offset`.
  std::int32_t offset = 0;
  // CE QUE `/mineffect` PREND À LA PLACE, ou zéro si cette famille n'a pas de
  // jumelle allégée. Le client lit le réglage 229 à l'endroit même où il calcule
  // l'identifiant : Rolling Cutter pose 1300..1309 en plein et 1467..1476 en
  // allégé, deux dossiers pour les mêmes noms de fichiers.
  std::int32_t minimal_offset = 0;
  // Les bornes de la VALEUR, incluses. Hors d'elles, le client ne montre rien.
  std::int32_t min_value = 0;
  std::int32_t max_value = 0;
  // Le client ne le montre que sur SON propre personnage.
  bool own_player_only = false;
  // Toute la famille vit jusqu'à ce qu'on la retire — voir
  // `StateEffectEntry::endless`. Le générateur REFUSE si les rangs d'une même
  // famille ne s'accordent pas : une aura au rang deux et un éclat au rang
  // trois n'aurait aucun sens, et signalerait une durée mal lue.
  bool endless = false;
};

// Ce que cet état montre selon sa valeur, ou nul.
[[nodiscard]] std::span<const StateValueEffectEntry> StateValueEffectTable();
[[nodiscard]] const StateValueEffectEntry* StateValueEffectFor(std::uint16_t state_id);

// Un état, et le son que son changement joue.
struct StateSoundEntry {
  std::uint16_t state_id = 0;
  // Le chemin tel que le client l'écrit, décodé du CP949.
  std::string_view wave;
};

// Ce que le message 110 fait naître — le gros de la table.
[[nodiscard]] std::span<const StateEffectEntry> StateEffectTable();
[[nodiscard]] std::span<const StateEffectEntry> StateEffectsFor(std::uint16_t state_id);

// Ce que les deux receveuses du paquet créent AVANT de poster ce message.
[[nodiscard]] std::span<const StateEffectEntry> StatePacketEffectTable();
[[nodiscard]] std::span<const StateEffectEntry> StatePacketEffectsFor(std::uint16_t state_id);

// Ce que le rebâtissage du sprite du joueur fait naître.
[[nodiscard]] std::span<const StateEffectEntry> StateRebuildEffectTable();
[[nodiscard]] std::span<const StateEffectEntry> StateRebuildEffectsFor(std::uint16_t state_id);

// 🔴 CE QUI DISPARAÎT QUAND L'ÉTAT TOMBE, et que nous ne savions pas retirer :
// une aura d'état posée par le chemin de la pose restait pour toujours.
[[nodiscard]] std::span<const StateEffectEntry> StateRemovalTable();
[[nodiscard]] std::span<const StateEffectEntry> StateRemovalsFor(std::uint16_t state_id);

// Le son d'un changement d'état, ou une chaîne vide.
[[nodiscard]] std::span<const StateSoundEntry> StateSoundTable();
[[nodiscard]] std::string_view StateSoundFor(std::uint16_t state_id);

}  // namespace ro::render
