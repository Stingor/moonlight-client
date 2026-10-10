#pragma once

#include <cstdint>
#include <deque>
#include <optional>
#include <span>
#include <string>
#include <string_view>
#include <vector>

#include "ro/net/chat_target.h"
#include "ro/proto/chat_limits.h"
#include "ro/sim/chat_channel.h"
#include "ro/sim/world_fwd.h"
#include "ro/ui/chat_input.h"

#include "chat/chat_item_link.h"

namespace moonclient {

// LES BORNES DU SERVEUR, telles que rAthena les pose, et que l'encodeur
// applique à l'identique.
//
// ⚠ LE REJET COMPTE LE TEXTE SEUL, pour toutes les destinations : nom et
// séparateur n'y entrent pas (`clif_process_message`), et il jette TOUT le
// message, pas seulement ce qui dépasse.
//
// ⚠ LA DIFFUSION COMPTE CE QUE LES AUTRES LISENT, et sa borne est propre à
// chaque relais : « Nom : texte » à voix haute, au salon et au groupe ; plus
// serrée pour la guilde ; « alias nom : texte » pour un canal, dont la ligne
// qui part ne porte pourtant pas le nom. Au-delà, le relais tronque. Le
// chuchotement et le clan n'en ont pas à cette taille.
//
// ⚠ ET ELLES ONT VOCATION À MONTER : le fil ne les a jamais imposées
// (`CZ_REQUEST_CHAT` porte sa longueur sur seize bits), elles viennent du tampon
// du client d'origine. Voir ADR 0033, entrée D1 — le jour où elles bougent,
// elles bougent des DEUX côtés, et ce sont ces constantes-ci qu'il faudra suivre.
inline constexpr int kWireTextMax = static_cast<int>(ro::proto::kChatTextMax);
inline constexpr int kWireNamedBodyMax = static_cast<int>(ro::proto::kChatNamedBodyMax);
inline constexpr int kWireGuildBodyMax = static_cast<int>(ro::proto::kChatGuildBodyMax);
inline constexpr int kWireChannelLineMax = static_cast<int>(ro::proto::kChatChannelLineMax);

// CE QU'IL ADVIENT D'UNE LIGNE TROP LONGUE.
enum class ChatOverflow : std::uint8_t {
  // Le serveur la jette entière, sans un mot : le TEXTE dépasse son rejet.
  kDropped,
  // Le relais la diffuse coupée : le texte passe, la ligne diffusée non.
  kTruncated,
};

// LA PLACE D'UNE LIGNE, en octets du fil, sous la plus serrée de ses bornes.
struct ChatWireRoom {
  // Ce qui reste. ZÉRO, la ligne part et le serveur la garde ; NÉGATIVE, elle
  // est trop longue.
  int bytes_left = 0;
  // La longueur que le TEXTE peut atteindre à cette destination, sous ce nom.
  int text_max = 0;
  // Ce qui arriverait à la ligne trop longue — au premier octet de trop, quand
  // elle tient encore.
  ChatOverflow overflow = ChatOverflow::kDropped;
};

// CE QUI, HORS DE LA LIGNE, EN FAIT LA PLACE.
//
// ⚠ UNE COMMANDE « @ » OU « # » N'Y CHANGE RIEN : elle garde la borne de la
// parole qui la porte, parce que le serveur la diffuse quand le groupe n'y a
// pas droit, et que le client ne sait pas lesquelles il exécute
// (`ro/proto/chat_limits.h`).
struct ChatWireSpeaker {
  // Le nom du personnage, vide tant qu'on n'est pas en jeu.
  std::string_view name;
  // Les canaux que le serveur a poussés : l'alias d'un canal entre dans la
  // ligne que son relais diffuse.
  std::span<const ro::sim::ChatChannel> channels;
  // LE CANAL OÙ `@channel bindto` A LIÉ LA PAROLE PUBLIQUE, sans croisillon ;
  // vide sinon. Le serveur passe alors la parole à voix haute et celle du salon
  // au relais de ce canal, sous « alias nom : texte ».
  std::string_view bound_channel;
  // LA CARTE EST UN CHAMP DE BATAILLE : la guilde y parle au relais du champ.
  bool battlefield = false;
  // LE JOUEUR A UN GROUPE DE STAFF, donc peut porter un faux nom (`@fakename`)
  // que le client ne voit pas : la diffusion compte alors le plus long nom
  // possible, et non le sien.
  bool may_wear_fake_name = false;
  // LA PAROLE PUBLIQUE PART AUSSI SUR DISCORD : la carte est celle du relai.
  bool relayed_to_discord = false;
};

// LA PLACE QUI RESTE à une ligne : la plus serrée de ses bornes, celle de la
// diffusion comptée seulement quand la destination en a une. Une annonce du
// staff (`/b`, `/nb`, `/lb`, `/nlb`) se mesure à la sienne : le serveur la
// rejoue en commande, préfixe compris, dans son tampon de chat.
//
// 🔴 C'EST LA LIGNE QUI PART QUI SE MESURE, chaque lien repris sous sa balise
// (`ResolveChatLinks`) : c'est elle que le serveur compte. Le compteur de la
// saisie et la retenue d'un envoi trop long en jugent par la même règle.
[[nodiscard]] ChatWireRoom ChatWireRoomOf(std::string_view outgoing, const ChatWireSpeaker& speaker,
                                          const ro::net::ChatArming& armed);

// La place qui reste seule : voir `ChatWireRoomOf`.
[[nodiscard]] int ChatWireBytesLeft(std::string_view outgoing, const ChatWireSpeaker& speaker,
                                    const ro::net::ChatArming& armed);

// LA PLACE D'UNE INTENTION DÉJÀ LUE, quand la destination est figée et qu'aucun
// sigil ne doit être relu : la phrase d'une conversation 1:1.
[[nodiscard]] ChatWireRoom ChatIntentRoomOf(const ro::net::ChatIntent& intent,
                                            const ChatWireSpeaker& speaker);

// Le rouge clair d'un envoi retenu : ni le vert d'une réponse du serveur, ni le
// rouge vif d'une panne.
inline constexpr std::uint32_t kHeldChatColor = 0xFF6060;

// LA LIGNE QUI DIT POURQUOI UN ENVOI EST RETENU : de combien il dépasse la place
// du texte, et ce qui lui arriverait.
[[nodiscard]] std::string OverlongChatNotice(const ChatWireRoom& room);

// 🔴 UNE LIGNE TROP LONGUE EST RETENUE, et non envoyée : le serveur la
// jetterait sans un mot, ou la diffuserait tronquée. La saisie la reçoit telle
// qu'elle a été tapée, pour être raccourcie, et les liens posés restent posés :
// l'élagage lit la saisie DÉPÔT EN ATTENTE COMPRIS. Rend la ligne à écrire dans
// le fil, ou rien si l'envoi peut partir — `speaker_known` faux, la place est
// inconnue et rien ne se retient.
//
// ⚠ CE QUI ATTENDAIT DÉJÀ DANS LA SAISIE passe APRÈS la ligne retenue, dans
// l'ordre où leurs liens ont été posés. S'il n'y tient plus, il se perd, pas
// elle — et ses liens quittent `links` avec lui.
[[nodiscard]] std::optional<std::string> HoldIfOverlong(ro::ui::ChatInputState& input,
                                                        const std::string& typed,
                                                        const ChatWireRoom& room,
                                                        bool speaker_known,
                                                        std::vector<PendingChatLink>* links);

// CE QUE DEVIENT UNE LIGNE VALIDÉE : envoyée, balises reprises, ou retenue.
struct SettledChatLine {
  // La ligne qui part, chaque libellé repris en balise ; rien si elle est
  // retenue.
  std::optional<std::string> outgoing;
  // La ligne à écrire au fil quand elle est retenue.
  std::optional<std::string> held_notice;
};

// RÉSOUT, MESURE, PUIS RETIENT OU LIBÈRE une ligne validée de la saisie.
// Envoyée, ses liens quittent `links` ; retenue, elle revient dans le dépôt de
// la saisie avec eux.
//
// 🔴 À FAIRE JUSTE APRÈS LE DESSIN, AVANT TOUT AUTRE ÉCRIVAIN DE LA SAISIE. La
// validation l'a vidée : la ligne n'existe plus que dans `said`. Un lien posé
// avant ce point élaguerait `links` contre une saisie vide, et les liens de
// la ligne partiraient en texte brut.
[[nodiscard]] SettledChatLine SettleChatLine(const std::string& said, ro::ui::ChatInputState& input,
                                             std::vector<PendingChatLink>* links,
                                             const ChatWireSpeaker& speaker,
                                             const ro::net::ChatArming& armed);

// OÙ PART CE QU'ON ÉCRIT, ET CE QU'IL RESTE DE PLACE.
//
// ⚠ C'EST `app` QUI RÉPOND, et il ne peut pas en être autrement. La destination
// se lit dans les sigils, qui vivent dans `net` ; la place restante se compte
// dans la page de codes du fil, et elle dépend de la destination — un
// chuchotement ne porte pas le nom de celui qui parle, les trois autres si.
// `ui` ne connaît ni l'un ni l'autre : elle reçoit la réponse et l'affiche.
class ChatLinePreviewMaker {
 public:
  // UNE DESTINATION QUE LE SÉLECTEUR PROPOSE.
  //
  // ⚠ UN RANG NE SUFFIT PLUS À LA DIRE, et c'est ce que les canaux ont changé :
  // les cinq destinations fixes se distinguent par leur seul genre, un canal par
  // son NOM. Rendre le genre seul ferait armer « un canal » sans dire lequel.
  struct SendChoice {
    ro::net::ChatTarget target = ro::net::ChatTarget::kPublic;
    // Sans le croisillon, comme le monde les range. Vide sauf pour un canal.
    std::string channel;
  };

  // Relit la ligne en cours de frappe, TELLE QU'ELLE PARTIRAIT : chaque libellé
  // de lien repris sous sa balise (`ResolveChatLinks`), qui n'a pas sa taille.
  // `speaker` dit qui parle et où (`ChatWireSpeaker`) ; son nom vide, on n'est
  // pas en jeu et la place restante est INCONNUE plutôt que fausse. `armed` est
  // le mode que le joueur a choisi au sélecteur. `in_chat_room` dit que le joueur
  // occupe un salon : la parole à voix haute y part, et le sélecteur l'annonce
  // comme telle.
  //
  // ⚠ LE LIBELLÉ SUIT LE MODE ARMÉ DÈS LE CHOIX, ligne vide comprise : il n'a pas
  // à attendre la première lettre pour dire où elle partira.
  void Refresh(const char* outgoing, const ChatWireSpeaker& speaker,
               const ro::net::ChatArming& armed, bool in_chat_room = false);

  // SUIT LE LIEN DE LA PAROLE PUBLIQUE d'après une ligne partie au serveur :
  // « @channel bindto #nom » le pose, « @channel unbind » le lève. `channels`
  // rend au nom tapé la casse que le serveur lui donne.
  //
  // ⚠ IL SE POSE À L'ENVOI, SANS ATTENDRE LA CONFIRMATION, et c'est le côté sûr :
  // le serveur ne lie que le joueur qui a rejoint ce canal, ce que le client ne
  // sait pas, et sa réponse est un texte localisé. Un lien supposé à tort ne
  // coûte que des octets refusés ; un lien manqué laisserait couper la ligne. Il
  // ne se lève que sur « unbind » pour la même raison : quitter un canal peut
  // échouer, et le client ne le saurait pas.
  void FollowSentCommand(std::string_view text, std::span<const ro::sim::ChatChannel> channels);

  // Le canal où la parole publique est liée, sans croisillon ; vide sinon.
  [[nodiscard]] std::string_view bound_channel() const noexcept { return bound_channel_; }

  // Retient un destinataire. ⚠ LE PLUS RÉCENT EN TÊTE, et un nom déjà connu
  // REMONTE au lieu de se dupliquer : une liste où le même nom paraît trois
  // fois ne sert plus à choisir.
  void NoteWhisper(std::string_view name);

  // Ce que la partie d'avant avait retenu. ⚠ REMPLACE la liste, elle ne s'y
  // ajoute pas : appelée à l'ouverture, avant qu'on ait chuchoté à quiconque.
  void RestoreWhispers(std::span<const std::string> names);

  // Oublie tous les destinataires retenus. ⚠ LE FICHIER SUIT À LA FERMETURE :
  // c'est la même liste qui s'écrit, et la vider ici la vide aussi là-bas.
  void ForgetWhispers();

  // Les derniers destinataires, pour les écrire. Le plus récent en tête.
  [[nodiscard]] std::vector<std::string> whispered() const {
    return std::vector<std::string>(whispered_.begin(), whispered_.end());
  }

  // OÙ LA LIGNE PARTIRAIT SI ON LA VALIDAIT MAINTENANT.
  //
  // ⚠ CE N'EST PAS CE QUI SERT À ENVOYER, et ce ne peut pas l'être : cet
  // aperçu est calculé avant que la fenêtre ne se dessine, donc sur le texte
  // de l'image PRÉCÉDENTE. Une lettre tapée et validée dans la même image y
  // manquerait. L'envoi relit la ligne — mais avec la même règle, qui vit
  // dans `ReadChatIntent` et nulle part ailleurs.
  [[nodiscard]] const ro::net::ChatIntent& intent() const noexcept { return intent_; }

  // Les destinations que le sélecteur propose : les cinq du client, puis les
  // canaux du serveur où le joueur a le droit d'écrire.
  //
  // ⚠ REFAITE À CHAQUE IMAGE, comme les vues de destinataires : la lire après
  // un `Refresh` de plus la ferait pendre. `app` la lit dans la même image.
  [[nodiscard]] std::span<const SendChoice> choices() const noexcept { return choices_; }

  [[nodiscard]] const ro::ui::ChatLinePreview& preview() const noexcept { return preview_; }

 private:
  ro::ui::ChatLinePreview preview_;
  ro::net::ChatIntent intent_;
  // Les textes que la vue ne fait que regarder : ils doivent survivre à l'image.
  std::string target_;
  std::string help_;
  // Le canal lié, et le libellé que la parole à voix haute prend tant qu'il l'est.
  std::string bound_channel_;
  std::string bound_label_;
  // Les libellés des modes, traduits une fois par image et gardés ici pour la
  // même raison.
  std::vector<ro::ui::ChatSendMode> modes_;
  // Ce que le sélecteur propose, et les libellés des CANAUX — que rien d'autre
  // ne possède, à la différence des cinq destinations fixes dont le libellé est
  // une chaîne statique.
  //
  // ⚠ LES DEUX SE REMPLISSENT DANS CET ORDRE, ET LE SECOND EST RÉSERVÉ D'ABORD :
  // `modes_` porte des VUES sur `channel_labels_`, qu'un agrandissement du
  // vecteur ferait pendre.
  std::vector<SendChoice> choices_;
  std::vector<std::string> channel_labels_;
  // Les derniers destinataires, et les vues que la barre en reçoit.
  //
  // ⚠ IL NE SURVIT PAS À LA PARTIE, pas encore : le client garde le sien, et
  // c'est un fichier de plus — qui porterait, lui aussi, avec qui l'on parle en
  // privé. Voir ADR 0033, entrée A10.
  std::deque<std::string> whispered_;
  std::vector<std::string_view> whispered_views_;
};

}  // namespace moonclient
