#pragma once

#include <cstddef>
#include <cstdint>
#include <optional>
#include <span>
#include <string>
#include <string_view>
#include <vector>

namespace ro::ui {

// LA LISTE DES COMMANDES, AU-DESSUS DE LA BARRE DE SAISIE.
//
// Elle s'ouvre quand la ligne COMMENCE par une barre oblique ou une arobase et
// qu'on en tape encore le premier mot ; elle se filtre à chaque lettre, sur le
// début du nom d'abord, puis sur celui des alias.
//
// ⚠ JAMAIS AU MILIEU D'UNE PHRASE : une commande n'existe qu'en tête de ligne,
// et dès qu'un blanc suit le nom, on écrit ses arguments — la liste se tait.
//
// 🔴 LES TOUCHES, ET CE QU'ELLES NE VOLENT PAS :
//   · ↑ ↓ choisissent une rangée, sans toucher au texte ;
//   · Tab complète, avec la rangée choisie ou la première ;
//   · Entrée ne complète QUE si l'on a choisi à ↑ ↓ — sinon elle envoie la
//     ligne comme d'habitude : « /sit » suivi d'Entrée doit s'asseoir, et non
//     compléter en « /sit » ;
//   · Échap ferme la liste, et la ligne reste.
// Une commande qui attend un argument se complète avec une espace.
//
// 🔴 L'ARGUMENT AUSSI, QUAND LA COMMANDE EN DÉCLARE LES CHOIX : « @job » suivi
// d'un blanc ouvre la même liste sur les choix de son argument, filtrés par ce
// qu'on tape après le blanc — sur la valeur (« 40 » → 4008, 4054…) comme sur le
// nom (« kni » → Knight, Lord Knight…). Les mêmes touches valent ; compléter
// remplace l'argument tapé par la valeur du choix. Une commande sans choix
// déclarés se tait dès le blanc, comme avant.
//
// ⚠ `ui` NE CONNAÎT AUCUNE COMMANDE : `app` lui passe la liste, déjà traduite
// et déjà filtrée par le groupe du joueur. Ici ne vivent que le filtrage par
// les lettres tapées et le clavier — ce que la liste montre, pas ce qu'elle
// contient.

// Combien de rangées se voient à la fois ; au-delà, la liste défile.
inline constexpr std::size_t kChatCommandRowsShown = 8;

// UN CHOIX POUR L'ARGUMENT D'UNE COMMANDE : « 4008  Lord Knight ».
struct ChatArgumentChoice {
  // Ce qui s'écrit dans la ligne à la place de l'argument tapé.
  std::string_view value;
  // Le nom montré après la valeur.
  std::string_view label;
  // Un second nom, en gris entre parenthèses ; vide quand il n'y en a pas.
  std::string_view detail;
  // La clé de recherche (`ro/core/search_key.h`) du nom et du second nom :
  // c'est elle que les lettres tapées filtrent.
  std::string_view search_key;
};

// UNE COMMANDE PROPOSÉE.
struct ChatCommandHint {
  // Ce qui l'ouvre en tête de ligne : `/` pour le client, `@` pour le serveur.
  char sigil = '/';
  // Le nom, sans le sigil.
  std::string_view name;
  // Les autres graphies, sans le sigil, séparées par une espace.
  std::string_view aliases;
  // Ce qu'elle attend, après le nom (« <nom> », « <carte> <x> <y> ») ; vide
  // sans argument connu.
  std::string_view params;
  // Ce qu'elle fait. La première ligne se lit dans la rangée, le tout au survol.
  std::string_view help;
  // La complétion pose une espace après le nom.
  bool takes_argument = false;
  // Les choix de son argument ; vide, la liste ne propose rien après le nom.
  std::span<const ChatArgumentChoice> argument_choices;
};

// UNE RANGÉE DE LA LISTE : la commande, et la graphie qui a répondu aux
// lettres tapées — c'est elle qui se complète.
struct ChatCommandMatch {
  std::size_t hint = 0;
  std::string_view spelled;
  bool by_alias = false;
};

// Ce que la liste garde d'une image à l'autre. ⚠ AUCUN ÉTAT DE JEU : la rangée
// choisie et ce qui a été fermé.
struct ChatCommandListState {
  // La rangée choisie, et si on l'a choisie à ↑ ↓ — c'est ce qui donne à
  // Entrée le droit de compléter.
  int chosen = 0;
  bool navigated = false;
  // La première rangée visible.
  int first_shown = 0;
  // La ligne qui a donné les rangées en cours : quand elle change, le choix
  // revient en tête.
  std::string query;
  // LA LIGNE SUR LAQUELLE LA LISTE A ÉTÉ FERMÉE — par Échap, une complétion ou
  // un rappel de l'historique. Elle ne s'y rouvre pas ; la première frappe qui
  // change la ligne la rouvre.
  std::string dismissed;
  // Une complétion faite à la souris attend que le champ la recharge.
  bool reload_field = false;
};

// Ce que la liste montre pour la ligne en cours.
//
// ⚠ DEUX LISTES EXCLUSIVES : les commandes (`matches`) pendant qu'on écrit le
// nom, ou les choix d'argument (`choice_rows`) quand `argument_of` est posé.
struct ChatCommandListView {
  bool open = false;
  std::vector<ChatCommandMatch> matches;
  // La commande dont on écrit l'argument.
  std::optional<std::size_t> argument_of;
  // Ce qui précède l'argument dans la ligne, blanc compris : « @job ».
  std::string argument_head;
  // Les rangs, dans `argument_choices`, des choix retenus.
  std::vector<std::size_t> choice_rows;
};

// Combien de rangées la liste porte, commandes ou choix d'argument.
[[nodiscard]] std::size_t ChatCommandRowCount(const ChatCommandListView& view) noexcept;

// LES LETTRES CHERCHÉES, sigil compris (« /si », « @ »), ou rien quand la
// ligne n'est pas un nom de commande en train de s'écrire.
[[nodiscard]] std::optional<std::string_view> ChatCommandQuery(std::string_view line);

// L'ARGUMENT EN TRAIN DE S'ÉCRIRE : la commande nommée en tête de ligne — par
// son nom ou un alias, casse latine indifférente —, ce qui précède l'argument,
// et ce qu'on en a tapé.
struct ChatArgumentQuery {
  std::size_t hint = 0;
  std::string_view head;
  std::string_view typed;
};

// Rien quand la ligne ne nomme pas, suivie d'un blanc, une commande qui déclare
// les choix de son argument.
[[nodiscard]] std::optional<ChatArgumentQuery> ChatArgumentQueryOf(
    std::span<const ChatCommandHint> hints, std::string_view line);

// Les rangs des choix que `typed` désigne, dans l'ordre de la source : ceux
// dont la valeur commence par `typed`, ou dont la clé de recherche le contient.
// Rien de tapé : tous.
[[nodiscard]] std::vector<std::size_t> MatchChatArguments(
    std::span<const ChatArgumentChoice> choices, std::string_view typed);

// Les rangées pour ces lettres : les noms qui commencent ainsi, dans l'ordre
// de la liste, puis les commandes dont un ALIAS commence ainsi. La casse
// latine ne compte pas.
[[nodiscard]] std::vector<ChatCommandMatch> MatchChatCommands(
    std::span<const ChatCommandHint> hints, std::string_view query);

// Met l'état à jour pour la ligne de cette image, et rend ce qui se montre.
[[nodiscard]] ChatCommandListView FollowChatCommandLine(ChatCommandListState& state,
                                                        std::span<const ChatCommandHint> hints,
                                                        std::string_view line);

// La ligne que donne cette rangée : sigil, graphie, et une espace si la
// commande attend un argument.
[[nodiscard]] std::string CompletedChatCommand(const ChatCommandHint& hint,
                                               const ChatCommandMatch& match);

enum class ChatCommandKey : std::uint8_t { kUp, kDown, kTab, kEnter, kEscape };

// CE QU'UNE TOUCHE A FAIT DE LA LISTE. `consumed` : la touche lui appartenait,
// et le champ ne doit pas la voir. `line` : la ligne complétée, à poser.
struct ChatCommandKeyAnswer {
  bool consumed = false;
  std::optional<std::string> line;
};

[[nodiscard]] ChatCommandKeyAnswer PressChatCommandKey(ChatCommandListState& state,
                                                       const ChatCommandListView& view,
                                                       std::span<const ChatCommandHint> hints,
                                                       std::string_view line, ChatCommandKey key);

// Le clic sur la rangée `row` : rend la ligne complétée, et ferme la liste —
// sauf quand la commande complétée propose aussitôt les choix de son argument.
[[nodiscard]] std::string PickChatCommand(ChatCommandListState& state,
                                          const ChatCommandListView& view,
                                          std::span<const ChatCommandHint> hints, std::size_t row);

// LA LIGNE VIENT D'AILLEURS QUE DU CLAVIER — un rappel de l'historique : la
// liste ne s'y ouvre pas. Sans cela, rappeler « /sit » ouvrirait la liste, et la
// flèche suivante la parcourrait au lieu de remonter l'historique.
void DismissChatCommandList(ChatCommandListState& state, std::string_view line);

// La première ligne d'une aide, celle qui se lit dans la rangée.
[[nodiscard]] std::string_view FirstHelpLine(std::string_view help);

}  // namespace ro::ui
