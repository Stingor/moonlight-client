#pragma once

#include <array>
#include <cstddef>
#include <cstdint>
#include <string>
#include <string_view>

#include "ro/vfs/item_name.h"

namespace ro::vfs {

// LE LIEN D'OBJET DANS LA CONVERSATION : `<ITEML>…</ITEML>`.
//
// Maj+clic gauche sur une case pose ce texte dans la ligne de chat. Les autres
// joueurs ne reçoivent pas un nom mais cette BALISE, et leur client la relit
// pour afficher le nom décoré — raffinement, cartes, grade — et l'ouvrir au
// clic. C'est donc un format d'échange entre clients, pas un ornement local.
//
// ⚠ ELLE VIT DANS `vfs` AVEC LA COMPOSITION DES NOMS, et pour la même raison :
// c'est le texte d'un objet. `ui` ne la verra jamais — elle ne peut pas résoudre
// un identifiant en nom —, c'est `app` qui traduit dans les deux sens.
//
// 🔴 LES DEUX BOUTS SONT D'ACCORD, ET C'EST MESURÉ. La table de séparateurs du
// client (`"!#$%&'()*+,-/"`, rangs 3/4/5/7/9/10/11) et celle du serveur
// (`ItemDatabase::create_item_link`) coïncident exactement à partir de
// PACKETVER 20200724, qui est le nôtre.

// Les cinq options aléatoires qu'une pièce peut porter, comme le fil les envoie.
inline constexpr std::size_t kItemLinkOptionSlots = 5;

// LES SÉPARATEURS, chacun avec ce qu'il introduit.
inline constexpr char kItemLinkRefine = '%';
inline constexpr char kItemLinkView = '&';
inline constexpr char kItemLinkGrade = '\'';
inline constexpr char kItemLinkCard = ')';
inline constexpr char kItemLinkOptionId = '+';
inline constexpr char kItemLinkOptionParam = ',';
inline constexpr char kItemLinkOptionValue = '-';

// 🔴 UN CHAMP QUI N'EST PAS DANS LE FORMAT : L'ÉQUIPEMENT CASSÉ. Ni le client ni
// le serveur ne le transportent, et c'est une perte — un objet cassé se lit
// autrement. On le pose en DERNIER, derrière le séparateur de rang ZÉRO de la
// table du client : son décodeur le range dans une case que rien ne lit, et tout
// le reste de la balise reste intact pour qui ne connaît pas ce champ.
//
// ⚠ D'OÙ LA PRUDENCE : ajouté en dernier, pour qu'un lecteur qui s'arrêterait avant ait quand même
// tout le standard.
inline constexpr char kItemLinkDamaged = '!';

inline constexpr std::string_view kItemLinkOpen = "<ITEML>";
inline constexpr std::string_view kItemLinkClose = "</ITEML>";

struct ItemLinkOption {
  std::uint16_t id = 0;
  std::uint8_t param = 0;
  std::uint16_t value = 0;
};

// Ce qu'une balise transporte, dans les deux sens.
struct ItemLink {
  std::uint32_t item_id = 0;

  // OÙ LA PIÈCE PEUT SE PORTER — le masque `EQP_*` de la base, pas l'endroit où
  // elle est portée. C'est le seul champ cadré sur CINQ chiffres.
  std::uint32_t wearable_at = 0;

  std::uint32_t refine_level = 0;
  std::uint32_t view_id = 0;
  std::uint32_t grade = 0;

  std::array<std::uint32_t, kItemNameCardSlots> cards{};

  std::array<ItemLinkOption, kItemLinkOptionSlots> options{};
  std::size_t option_count = 0;

  // 🔴 LA SEULE INFORMATION DE TYPE QUE LA BALISE PORTE : le sixième caractère,
  // qui vaut le prédicat `ItemTypeIsDecorated` et rien de plus. Relue, elle ne
  // dit donc pas si l'objet est une arme ou une armure — seulement s'il se
  // décore. C'est exactement ce dont la composition du nom a besoin.
  bool equipable = false;

  // Le champ privé ci-dessus.
  bool damaged = false;
};

// COMBIEN DE CASES DE CARTES LA BALISE ÉCRIT.
//
// 🔴 LE CLIENT ET LE SERVEUR NE SONT PAS D'ACCORD ICI, et c'est le CLIENT qui
// tranche : c'est son texte que les autres joueurs reçoivent. Le serveur écrit
// toujours les quatre ; le client n'en écrit que le nombre d'emplacements de
// l'objet — sauf si l'une des quatre cases est non nulle, auquel cas il repasse
// à quatre.
//
// ⚠ ET CE SECOND CAS N'EST PAS UNE COQUETTERIE : sur un objet FORGÉ, ces quatre
// mots portent l'identité du forgeron. Les tronquer à zéro emplacement effacerait
// le nom du créateur du lien.
[[nodiscard]] std::size_t ItemLinkCardSlots(const ItemLink& link, std::uint8_t card_slots);

// Compose la balise. Rend une chaîne VIDE sans identifiant : une ouvrante sans sa
// fermante ferait une ligne illisible chez tout le monde, et mieux vaut rien.
//
// `card_slots` vient de la table de l'installation — la balise ne le porte pas.
[[nodiscard]] std::string BuildItemLink(const ItemLink& link, std::uint8_t card_slots);

// Relit une balise qui COMMENCE à `text[0]`. Rend `false` si ce n'en est pas une
// ou si elle ne porte pas d'identifiant.
//
// ⚠ `after` REÇOIT CE QUI SUIT LA FERMANTE, et la fin du texte quand elle
// manque : une ligne coupée par le serveur ne doit pas faire lire au-delà.
//
// ⚠ UN SÉPARATEUR INCONNU VOIT SA CHARGE SAUTÉE plutôt que de désynchroniser le
// reste : c'est ce qui laisse un client plus récent ajouter un champ sans casser
// le nôtre.
[[nodiscard]] bool ParseItemLink(std::string_view text, ItemLink* out, std::size_t* after);

// Où commence la prochaine balise à partir de `from`, ou `npos`.
[[nodiscard]] std::size_t FindItemLink(std::string_view text, std::size_t from);

// Les faits dont la composition du nom a besoin. ⚠ SANS LE NOM DU CRÉATEUR : la
// balise ne porte qu'un identifiant de personnage, dans deux de ses cases de
// cartes, et le traduire demande une question au serveur.
[[nodiscard]] ItemNameFacts ItemLinkNameFacts(const ItemLink& link);

}  // namespace ro::vfs
