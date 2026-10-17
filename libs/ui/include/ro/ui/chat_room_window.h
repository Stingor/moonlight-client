#pragma once

#include <array>
#include <cstddef>
#include <cstdint>
#include <optional>
#include <span>
#include <string>
#include <string_view>
#include <vector>

#include "ro/ui/chat_input.h"
#include "ro/ui/chat_link.h"
#include "ro/ui/chat_log.h"
#include "ro/ui/skin_art.h"

namespace ro::ui {

// LES SALONS DE DISCUSSION : le formulaire de création, la salle, et l'invite
// de mot de passe d'un salon privé.
//
// 🔴 LE FORMULAIRE NE SE FERME PAS EN ENVOYANT. Il reste à l'écran jusqu'à la
// réponse du serveur, qui peut refuser — « un salon du même nom existe déjà »
// est le refus le plus courant. La saisie survit alors, et l'erreur se lit
// dans la fenêtre, le champ fautif surligné.
//
// 🔴 LA SALLE N'A PAS DE SAISIE À ELLE : c'est la barre de la conversation qui
// s'y dessine, tant que le salon dure. Dans un salon, tout ce qu'on dit y part
// déjà ; une seconde saisie ferait la même chose, sans l'historique, les
// destinataires ni les liens de la première.
//
// ⚠ FERMER LA SALLE, C'EST EN SORTIR, et elle ne se range pas dans la pile
// d'Échap : on la garde ouverte en jouant, et un Échap destiné au menu du jeu
// ne doit pas faire quitter le salon sans un mot.

inline constexpr const char* kChatRoomMakeWindowId = "chatroom_make";
inline constexpr const char* kChatRoomWindowId = "chatroom_room";
inline constexpr const char* kChatRoomPasswordWindowId = "chatroom_password";

// LES CHAMPS SONT PLUS LARGES QUE CE QUI PART : on laisse taper trop long pour
// le MONTRER, compteur au rouge, plutôt que de couper sans rien dire.
inline constexpr std::size_t kChatRoomTitleFieldBytes = 64;
inline constexpr std::size_t kChatRoomPasswordFieldBytes = 24;

// La limite d'un salon neuf : le plafond du serveur, et le choix par défaut du
// formulaire du jeu.
inline constexpr int kChatRoomFirstLimit = 20;

// Le champ qu'une erreur désigne.
enum class ChatRoomField : std::uint8_t { kNone, kTitle, kPassword };

// LES PLAFONDS DU SERVEUR, que `app` recopie depuis `sim` : cette couche ne
// le connaît pas, et les écrire une seconde fois ici les laisserait diverger.
struct ChatRoomLimits {
  std::size_t title_bytes = 0;
  std::size_t password_bytes = 0;
  std::size_t fewest_password_bytes = 0;
  int fewest_seats = 0;
  int most_seats = 0;
};

// LES MOTS DU CLIENT D'ORIGINE, déjà dans la langue du joueur. Un mot vide ne
// se dessine pas.
struct ChatRoomWordsView {
  std::string_view making_room;
  std::string_view room_setup;
  std::string_view title_label;
  std::string_view limit_label;
  std::string_view password_label;
  std::string_view people_unit;
  std::string_view public_word;
  std::string_view private_word;
  std::string_view kick;
  std::string_view give_owner;
  std::string_view enter_password;
  std::string_view english_only;
};

// ── LE FORMULAIRE ───────────────────────────────────────────────────────────

// UN SALON VISIBLE ALENTOUR, dans l'annuaire du formulaire.
struct ChatRoomNearbyView {
  std::uint32_t room_id = 0;
  std::string_view title;
  std::uint16_t users = 0;
  std::uint16_t limit = 0;
  bool locked = false;
  // Le double-clic y entrerait-il ? Ni plein, ni salle de PNJ fermée.
  bool joinable = true;
  // À combien de cases de nous, sur l'axe le plus long.
  int distance_cells = 0;
};

struct ChatRoomMakeView {
  ChatRoomLimits limits;
  ChatRoomWordsView words;
  // Une création est en route : le bouton attend la réponse.
  bool waiting = false;
  // Le refus à montrer dans la fenêtre, et le champ qu'il désigne.
  std::string_view error;
  ChatRoomField error_field = ChatRoomField::kNone;
  // CE QUI EMPÊCHE DE CRÉER ICI, dit AVANT le clic — vide quand rien ne s'y
  // oppose. Le serveur refuserait, et souvent sans un mot.
  std::string_view blocker;
  // UN SALON VISIBLE PORTE DÉJÀ CE TITRE : l'avertissement, ou vide.
  std::string_view title_taken;
  // Le serveur n'accepte qu'un titre en ASCII.
  bool ascii_required = true;
  std::span<const ChatRoomNearbyView> nearby;
  // Les derniers titres, du plus récent au plus ancien.
  std::span<const std::string> recent_titles;
};

struct ChatRoomMakeState {
  bool open = false;
  // Demandé par qui l'ouvre : la fenêtre passe devant, le titre prend le clavier.
  bool raise = false;
  // ⚠ LA SAISIE SURVIT À LA FERMETURE : rouvrir après un refus, ou après
  // l'avoir fermée par mégarde, la retrouve.
  std::array<char, kChatRoomTitleFieldBytes> title{};
  std::array<char, kChatRoomPasswordFieldBytes> password{};
  int limit = kChatRoomFirstLimit;
  bool is_public = true;
  // L'œil : le client d'origine masque sans jamais laisser relire.
  bool show_password = false;
};

struct ChatRoomMakeAction {
  // « Créer », ou Entrée dans le titre.
  bool create = false;
  // Le joueur a retouché le champ que l'erreur désignait : elle s'efface.
  bool edited_faulty_field = false;
  // Un salon de l'annuaire, à rejoindre.
  std::optional<std::uint32_t> join_room;
  // La fenêtre vient d'être fermée : croix ou « Annuler ».
  bool closed = false;
};

[[nodiscard]] ChatRoomMakeAction DrawChatRoomMakeWindow(ChatRoomMakeState& state,
                                                        const ChatRoomMakeView& view,
                                                        const SkinArt& art);

// ── LA SALLE ────────────────────────────────────────────────────────────────

struct ChatRoomMemberView {
  std::string_view name;
  bool owner = false;
  // Le joueur lui-même : on ne se chuchote pas, on ne s'expulse pas, on ne
  // s'ajoute pas en ami.
  bool me = false;
  // Déjà dans la liste d'amis : « Ajouter en ami » se grise.
  bool befriended = false;
};

// CE QUE LA SALLE MONTRE, recomposé à chaque image.
struct ChatRoomView {
  ChatRoomLimits limits;
  ChatRoomWordsView words;
  // « Publ. : titre (2/20) », déjà composée.
  std::string_view caption;
  std::span<const ChatRoomMemberView> members;
  // Le joueur porte-t-il les droits de chef ?
  bool owned = false;
  // La liste d'amis est pleine : « Ajouter en ami » se grise.
  bool friends_full = false;
  // Le refus des derniers réglages envoyés, dit dans leur volet, ou vide.
  std::string_view settings_error;
  // Les réglages courants, pour pré-remplir le volet à son ouverture.
  std::string_view title;
  std::string_view password;
  int limit = kChatRoomFirstLimit;
  bool is_public = true;
  // Le fil, déjà trié et décoré comme celui de la conversation.
  std::span<const ChatLineView> lines;
  ChatLogStyle log_style;
  ChatLogLayout* layout = nullptr;
  // ── LA BARRE DE LA CONVERSATION, prêtée ────────────────────────────────────
  // Nulle : la salle dit qu'on ne peut pas écrire ici.
  ChatInputState* input = nullptr;
  const ChatLinePreview* preview = nullptr;
  std::span<const ChatInputLink> input_links;
  ItemLinkFacts link_facts;
};

struct ChatRoomWindowState {
  // Demandé par qui veut la salle devant.
  bool raise = false;
  // Le volet « Réglages », demandé, puis montré une image plus tard : la
  // fenêtre grandit D'ABORD, le volet paraît ENSUITE, sans quoi le contenu
  // déborde le temps d'une image et l'ascenseur clignote.
  bool settings_wanted = false;
  bool settings_shown = false;
  float last_width = 0.0F;
  float last_height = 0.0F;
  std::array<char, kChatRoomTitleFieldBytes> settings_title{};
  std::array<char, kChatRoomPasswordFieldBytes> settings_password{};
  bool settings_show_password = false;
  int settings_limit = kChatRoomFirstLimit;
  bool settings_public = true;
  // Le membre visé par le menu d'un membre.
  std::string menu_member;
};

struct ChatRoomWindowAction {
  // La croix ou « Quitter » : on sort du salon.
  bool leave = false;
  std::optional<std::string> expel;
  std::optional<std::string> give_owner;
  // La demande d'amitié, par le nom, comme celle du menu d'entité.
  std::optional<std::string> befriend;
  // « Enregistrer le chat » : le fil du salon, dans son fichier.
  bool save_chat = false;
  // Les réglages à appliquer, tels que le volet les tient.
  bool apply_settings = false;
  // CE QUE LA BARRE A RENDU, à router comme celle de la conversation.
  std::optional<std::string> said;
  ChatInputAsk input;
  // Un geste sur un lien du fil.
  ChatLinkAsk link;
};

[[nodiscard]] ChatRoomWindowAction DrawChatRoomWindow(ChatRoomWindowState& state,
                                                      const ChatRoomView& view, const SkinArt& art);

// ── LE MENU D'UN MEMBRE, au clic droit sur son nom ───────────────────────────
//
// Tout membre l'a, soi-même compris : « Enregistrer le chat » ne vise personne.
// Le chef y trouve en plus l'expulsion et la cession de ses droits.

enum class ChatRoomMemberVerb : std::uint8_t { kExpel, kGiveOwner, kBefriend, kSaveChat };

struct ChatRoomMemberEntry {
  ChatRoomMemberVerb verb = ChatRoomMemberVerb::kSaveChat;
  // Pourquoi la ligne est grisée, déjà traduit ; nul quand elle est permise.
  const char* refusal = nullptr;
};

// Les lignes du menu de ce membre, dans leur ordre.
[[nodiscard]] std::vector<ChatRoomMemberEntry> ChatRoomMemberMenu(const ChatRoomMemberView& member,
                                                                  const ChatRoomView& view);

// L'intention qu'une ligne choisie émet pour ce membre.
void ChooseChatRoomMemberVerb(ChatRoomMemberVerb verb, std::string_view member,
                              ChatRoomWindowAction& action);

// ── L'INVITE DE MOT DE PASSE ────────────────────────────────────────────────

struct ChatRoomPasswordState {
  bool open = false;
  // Le salon visé.
  std::uint32_t room_id = 0;
  std::array<char, kChatRoomPasswordFieldBytes> input{};
  bool reveal = false;
  bool was_open = false;
};

struct ChatRoomPasswordView {
  ChatRoomLimits limits;
  ChatRoomWordsView words;
  // Le dernier refus du serveur, DANS l'invite : elle reste ouverte pour qu'on
  // corrige sans recliquer l'enseigne.
  std::string_view error;
  // Une demande attend sa réponse.
  bool waiting = false;
};

struct ChatRoomPasswordAction {
  bool enter = false;
  bool cancelled = false;
  bool edited = false;
};

[[nodiscard]] ChatRoomPasswordAction DrawChatRoomPasswordWindow(ChatRoomPasswordState& state,
                                                                const ChatRoomPasswordView& view,
                                                                const SkinArt& art);

}  // namespace ro::ui
