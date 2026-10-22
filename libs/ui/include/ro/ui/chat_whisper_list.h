#pragma once

#include <cstddef>
#include <cstdint>
#include <optional>
#include <span>
#include <string>
#include <string_view>
#include <vector>

namespace ro::ui {

// LA LISTE DES DERNIERS CORRESPONDANTS, SOUS LA BOX « PSEUDO ».
//
// Elle se déroule dès que la box prend le clavier — clic, Tab, ou le clavier
// rendu par la barre — et se filtre à mesure qu'on y tape : ne restent que les
// noms qui CONTIENNENT les lettres tapées, sans tenir compte de la casse ni
// des accents.
//
// 🔴 LES TOUCHES, ET CE QU'ELLES NE VOLENT PAS :
//   · ↑ ↓ surlignent un nom, sans toucher au texte de la box ;
//   · Entrée et Tab prennent le nom surligné — et SEULEMENT lui : sans nom
//     surligné, Entrée envoie la ligne et Tab passe à la phrase, comme
//     d'habitude ;
//   · Échap ferme la liste, et rien d'autre : ni la box, ni la barre.
// La liste reste fermée sans nom à montrer : une liste vide ne s'ouvre pas.
//
// ⚠ `ui` NE POSSÈDE PAS LA LISTE : `app` la lui passe, la plus récente en
// tête. Ici ne vivent que le filtre et le clavier.

// LE DERNIER SENS D'UN ÉCHANGE avec ce correspondant.
enum class ChatWhisperWay : std::uint8_t {
  // Je lui ai chuchoté en dernier.
  kOutgoing,
  // Il m'a chuchoté en dernier.
  kIncoming,
};

// UN CORRESPONDANT RÉCENT, tel que la liste le montre.
struct ChatRecentWhisper {
  std::string_view name;
  ChatWhisperWay way = ChatWhisperWay::kOutgoing;
};

// Combien de correspondants la barre se rappelle, dans les deux sens. Le
// client garde une liste déroulante ; au-delà d'une poignée, la chercher coûte
// plus que de retaper.
inline constexpr int kChatWhisperNamesKept = 10;

// Aucun nom surligné.
inline constexpr int kNoWhisperChosen = -1;

// Ce que la liste garde d'une image à l'autre. ⚠ AUCUN ÉTAT DE JEU : ouverte
// ou non, le nom surligné, et ce qui décide du filtre.
struct ChatWhisperListState {
  bool open = false;
  // Le rang surligné PARMI LES RANGÉES MONTRÉES, ou `kNoWhisperChosen`.
  int chosen = kNoWhisperChosen;
  // CE QUE LA BOX PORTAIT QUAND ELLE A PRIS LE CLAVIER. Tant que son texte n'a
  // pas changé, la liste montre TOUT : la box garde le dernier destinataire, et
  // filtrer sur lui ne laisserait que lui.
  std::string opened_on;
  // Les lettres qui ont donné les rangées en cours : quand elles changent, le
  // surlignage s'efface.
  std::string filtered_on;
  // LE TEXTE SUR LEQUEL ÉCHAP A FERMÉ LA LISTE. Elle ne s'y rouvre pas ; la
  // première frappe qui change le texte la rouvre.
  std::optional<std::string> escaped_on;
  // La box avait le clavier à l'image précédente : les touches se décident
  // AVANT qu'elle ne se dessine, donc sur ce témoin.
  bool box_was_active = false;
  // LA PROCHAINE PRISE DE CLAVIER NE DÉROULE PAS LA LISTE. C'est celle qui
  // suit un envoi depuis la box : la barre lui rend le clavier, et la liste se
  // déroulerait à chaque phrase envoyée.
  bool quiet_next = false;
};

// Ce que la liste montre cette image : les rangs, dans `names`, des noms
// retenus par le filtre, dans leur ordre.
struct ChatWhisperListView {
  bool open = false;
  std::vector<std::size_t> rows;
};

// LE FILTRE : les rangs des noms qui contiennent `typed`, à la casse et aux
// accents près. Vide, `typed` les garde tous. ⚠ UTF-8 ENTIER : la comparaison
// se fait sur des clés repliées, jamais sur un octet isolé, et un caractère ne
// se coupe pas.
[[nodiscard]] std::vector<std::size_t> MatchRecentWhispers(std::span<const ChatRecentWhisper> names,
                                                           std::string_view typed);

// La box vient de prendre le clavier, avec ce texte-là.
void OpenWhisperList(ChatWhisperListState& state, std::string_view typed);

// La box a perdu le clavier, ou un nom a été pris.
void CloseWhisperList(ChatWhisperListState& state);

// Met l'état à jour pour le texte de la box, et rend ce qui se montre.
[[nodiscard]] ChatWhisperListView FollowWhisperList(ChatWhisperListState& state,
                                                    std::span<const ChatRecentWhisper> names,
                                                    std::string_view typed);

enum class ChatWhisperKey : std::uint8_t { kUp, kDown, kEnter, kTab, kEscape };

// CE QU'UNE TOUCHE A FAIT DE LA LISTE. `consumed` : la touche lui appartenait,
// et la box ne doit pas la voir. `picked` : le rang, dans `names`, du nom pris.
struct ChatWhisperKeyAnswer {
  bool consumed = false;
  std::optional<std::size_t> picked;
};

[[nodiscard]] ChatWhisperKeyAnswer PressWhisperListKey(ChatWhisperListState& state,
                                                       const ChatWhisperListView& view,
                                                       std::string_view typed, ChatWhisperKey key);

}  // namespace ro::ui
