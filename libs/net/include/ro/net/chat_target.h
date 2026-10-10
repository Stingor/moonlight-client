#pragma once

#include <cstdint>
#include <string>
#include <string_view>

#include "ro/core/error.h"
#include "ro/net/connection.h"

namespace ro::net {

// À QUI LE JOUEUR PARLE, et comment il le dit.
//
// Le client ne montre pas de sélecteur : la destination se lit DANS ce qu'on
// tape. Ce sont les sigils du jeu, ceux que les joueurs ont dans les doigts.
//
//   %texte        au GROUPE
//   $texte        à la GUILDE
//   ^texte        au canal des ALLIÉS
//   /w nom texte  à quelqu'un
//   le reste      à voix haute
//
// ⚠ LE CROISILLON DU SERVEUR DEVIENT UN ACCENT CIRCONFLEXE. rAthena nomme ses
// canaux « #ally » et attend ce nom-là SUR LE FIL ; c'est à la SAISIE que
// Moonlight emploie « ^ », parce que le croisillon sert déjà ailleurs. La
// traduction se fait donc ici, et le serveur ne voit jamais l'accent.
//
// ⚠ ET UN CANAL N'A PAS DE PAQUET À LUI : il passe par le CHUCHOTEMENT, dont le
// destinataire est le nom du canal. C'est `clif_parse_WisMessage` qui regarde le
// croisillon. Le `CZ_ALLY_CHAT` des clients récents existe, mais ce serveur-ci
// le fait lire par `clif_parse_dull` : l'employer parlerait dans le vide.

inline constexpr char kPartySigil = '%';
inline constexpr char kGuildSigil = '$';
inline constexpr char kAllySigil = '^';

// 🔴 ET LE CROISILLON N'EST PAS UN SIGIL — IL APPARTIENT AUX CHARCOMMANDES.
// C'est même la raison pour laquelle l'alliance est sur « ^ » : le correctif
// `AllianceChatHotkeySelector` de Moonlight réécrit l'immédiat du client
// justement pour RENDRE le croisillon à rAthena, qui lit « #pseudo @commande »
// (`charcommand_symbol`, posé à « # » dans la configuration de ce serveur).
//
// ⚠ EN FAIRE UN SIGIL DE CANAL CASSE LES CHARCOMMANDES, et sans un mot : la
// ligne partirait en CHUCHOTEMENT vers un canal nommé d'après le pseudo visé,
// que `channel_name2channel` ne trouverait pas — au lieu de partir en parole
// publique, où `is_atcommand` l'aurait interceptée. Un canal s'ARME au
// sélecteur (voir `ChatArming::channel`), il ne se tape pas.
inline constexpr std::string_view kWhisperCommand = "/w ";
inline constexpr std::string_view kWhisperCommandLong = "/whisper ";

// LES SYMBOLES DES COMMANDES DU SERVEUR (`atcommand_symbol`, `charcommand_symbol`
// de sa configuration) : « @ » pour soi, « # » pour un autre personnage.
inline constexpr char kAtCommandSymbol = '@';
inline constexpr char kCharCommandSymbol = '#';

// Un texte qui commence par l'un d'eux est une COMMANDE : le serveur l'exécute
// au lieu de la dire, et ne la diffuse pas — quand il la connaît et que le
// groupe du joueur y a droit. Sinon, elle part en parole ordinaire. ⚠ LE CLIENT
// NE SAIT PAS LEQUEL des deux : une commande garde la borne de la parole qui la
// porte (`ro/proto/chat_limits.h`).
[[nodiscard]] constexpr bool IsServerCommandText(std::string_view text) {
  return !text.empty() && (text.front() == kAtCommandSymbol || text.front() == kCharCommandSymbol);
}

// Ce qu'une ligne de saisie vise.
// ⚠ LE CLAN N'A PAS DE SIGIL, et c'est voulu : le client ne lui en donne pas
// non plus. Il ne s'atteint que par le sélecteur de mode — un caractère de plus
// à retenir pour une appartenance qu'on ne choisit même pas serait un mauvais
// échange.
enum class ChatTarget : std::uint8_t {
  kPublic,
  kParty,
  kGuild,
  kAlly,
  kClan,
  kWhisper,
  // UN CANAL DU SERVEUR, désigné par son nom dans `recipient` — sans le
  // croisillon, comme le serveur les range.
  //
  // ⚠ CE N'EST PAS UN DOUBLON DE `kAlly`. Les alliés sont un canal comme les
  // autres — `#ally` — et le sigil `^` lui reste, parce que c'est celui que les
  // joueurs ont dans les doigts ; mais il n'y a aucune raison qu'il soit le
  // SEUL atteignable, et la liste du serveur dit les autres. Voir
  // `sim::ChatChannel` et `ro/net/chat_channels.h`.
  kChannel,
  // LE CHAMP DE BATAILLE : les membres de notre équipe, sur sa carte.
  kBattlefield,
};

// LA PAROLE AU CHAMP DE BATAILLE, sans basculer la barre : « &texte » en tête
// de ligne — sur une carte de champ seulement, voir `ChatArming::on_battlefield`
// —, ou « /bc texte ».
//
// ⚠ LES MOTS DE COMMANDE — « /bc », « /w », « /whisper » — SE LISENT À LA
// CASSE ASCII PRÈS : « /BC texte » lu comme une parole publique partirait chez
// l'ennemi.
inline constexpr char kBattlefieldSigil = '&';
inline constexpr std::string_view kBattlefieldCommand = "/bc ";

// Ce que la ligne de saisie veut dire, une fois son sigil retiré.
struct ChatIntent {
  ChatTarget target = ChatTarget::kPublic;
  // Vide sauf pour un chuchotement.
  std::string recipient;
  std::string text;
};

// CE QUE LA BARRE A ARMÉ, hors de la ligne elle-même.
//
// ⚠ LES DEUX SONT DES RÉGLAGES DE BARRE, pas du texte : le mode vient du
// sélecteur, le destinataire de la box « Pseudo » à côté de la saisie. Groupés
// parce qu'ils répondent à la MÊME question et qu'ils s'ordonnent entre eux —
// les passer séparément inviterait à n'en transmettre qu'un.
struct ChatArming {
  ChatTarget mode = ChatTarget::kPublic;
  // ⚠ UNE VUE, et elle ne doit vivre que le temps de l'appel : l'intention rendue
  // recopie ce qu'elle en garde.
  std::string_view whisper_to;
  // LE CANAL ARMÉ, quand le mode est `kChannel` — sans le croisillon. Même
  // règle de durée de vie. ⚠ UN MODE `kChannel` SANS NOM NE S'ARME PAS : il
  // enverrait dans le vide, exactement comme un chuchotement sans destinataire.
  std::string_view channel;
  // LA CARTE EST UN CHAMP DE BATAILLE. ⚠ HORS DE L'UN D'EUX, « & » N'EST PAS UN
  // SIGIL : une ligne comme « &lt;3 » part entière, là où la barre est armée.
  // « /bc », lui, reste lu comme le champ partout — c'est une commande qu'on
  // tape exprès, et son refus hors carte s'explique.
  bool on_battlefield = false;
};

// Lit la destination d'une ligne. Une ligne vide, ou un sigil seul, rend une
// intention dont le texte est vide : l'appelant n'envoie alors rien. ⚠ LA LIGNE
// VIDE A POURTANT UNE DESTINATION, celle de la barre : c'est ce que l'étiquette
// montre avant la première lettre, et elle doit montrer le mode qu'on vient
// d'armer, pas « à voix haute ».
//
// ⚠ PREND UNE VUE, et la rend par VALEUR : découper la ligne en vues ferait
// pendre l'intention dès que l'appelant lâche sa chaîne.
//
// 🔴 TROIS CHEMINS DISENT OÙ LA LIGNE VA, ET ILS S'ORDONNENT :
//
//   1. CE QU'ON TAPE gagne toujours — un sigil, ou `/w nom`. Une ligne qui le
//      porte a déjà dit où elle va, et l'écraser rendrait les sigils
//      inutilisables dès qu'une barre est armée : ce sont eux que les joueurs
//      ont dans les doigts.
//   2. LA BOX « PSEUDO » ensuite : un nom écrit là EST une destination, et c'est
//      ainsi que le client chuchote sans taper `/w`.
//   3. LE MODE ARMÉ en dernier, pour tout ce qui ne dit rien.
//
// ⚠ ET LA RÈGLE EST ICI, PAS CHEZ SES DEUX APPELANTS. L'aperçu de la barre et
// l'envoi lisent la même ligne à deux moments différents ; deux copies de la
// règle finiraient par diverger, et la phrase partirait ailleurs que là où
// l'étiquette l'annonçait.
[[nodiscard]] ChatIntent ReadChatIntent(std::string_view line, const ChatArming& armed = {});

// Vrai pour une ligne qui part au serveur comme une parole : tout ce qui ne
// commence pas par une barre oblique, et les barres obliques qui ADRESSENT la
// phrase (`/w nom texte`, `/bc texte`). Les autres sont des commandes du client.
[[nodiscard]] bool IsSpokenLine(std::string_view line);

// Envoie ce que l'intention dit, à qui elle dit. `speaker` est le nom du
// personnage : trois des cinq destinations l'exigent en tête du texte.
// `on_battlefield` dit que la carte est un champ de bataille : la guilde y
// parle au relais du champ, qui garde une ligne plus longue.
[[nodiscard]] core::Result<void> SendChat(Connection& link, std::string_view speaker,
                                          const ChatIntent& intent, bool on_battlefield = false);

// UNE COMMANDE DU SERVEUR — « @storage », « @gstorage » —, envoyée COMME SI LE
// JOUEUR LA TAPAIT.
//
// 🔴 C'EST LE SEUL CHEMIN, et ce n'est pas un contournement. Beaucoup de gestes
// n'ont AUCUN paquet : le serveur n'expose l'ouverture d'un coffre par aucun
// opcode — `storage_storageopen` n'est atteignable que par un script de PNJ ou
// par `@storage`. Le client d'origine ne sait pas non plus ouvrir un coffre ;
// il attend qu'on le lui ouvre.
//
// Passer par la parole donne exactement ce que la main du joueur donnerait :
// mêmes droits de groupe, mêmes refus, même journalisation, et le refus arrive
// dans la conversation comme d'habitude. Rien n'est validé ici.
//
// 🔴 ET LE NOM EN TÊTE N'EST PAS DÉCORATIF. `clif_process_message` COMPARE ce
// nom à celui du personnage ; un nom qui ne correspond pas passe pour un client
// trafiqué et COUPE LA SESSION. C'est `SendChat` qui le pose, et c'est pour cela
// que ce verbe passe par lui plutôt que de composer sa propre trame.
[[nodiscard]] core::Result<void> SendServerCommand(Connection& link, std::string_view speaker,
                                                   std::string_view command);

}  // namespace ro::net
