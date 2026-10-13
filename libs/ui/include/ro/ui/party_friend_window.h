#pragma once

#include <array>
#include <cstdint>
#include <optional>
#include <span>
#include <string>
#include <string_view>

#include <imgui.h>

#include "ro/core/i18n.h"
#include "ro/ui/skin_art.h"
#include "ro/ui/status_bar.h"
#include "ro/ui/status_cell.h"

namespace ro::ui {

// LA FENÊTRE AMIS / GROUPE : deux onglets, la liste du groupe et celle des amis,
// les demandes reçues, et les réglages du groupe.
//
// ⚠ ELLE NE DÉCIDE D'AUCUN DROIT ET N'ENVOIE RIEN. `app` compose la vue — qui
// est chef, où est chacun, ce que le serveur a dit du partage — et route
// l'intention rendue. Un geste que le serveur refuserait n'est pas offert.
//
// ⚠ TOUT TEXTE ARRIVE EN UTF-8, et tout ce que le joueur tape repart en UTF-8 :
// la conversion vers le fil est l'affaire de `net`.

inline constexpr const char* kPartyFriendWindowTitle = core::TrText("Groupe / Amis");
inline constexpr const char* kPartyFriendWindowId = "party_friends";

// Les plafonds du serveur : membres d'un groupe, et entrées d'une liste d'amis.
inline constexpr int kPartyMembersMost = 24;
inline constexpr int kFriendsMost = 40;

// POURQUOI UN MEMBRE NE REÇOIT PAS SA PART D'EXP, en bits.
//
// Les trois premiers viennent du serveur, qui seul tient l'inactivité, la mort
// et l'échoppe. Le dernier est calculé ici : le serveur compare la carte du
// membre à celle du MONSTRE, ce qu'il ne peut pas annoncer d'avance ; la fenêtre
// la compare à la carte du joueur — « touche-t-il quelque chose quand JE tue ? ».
// Il est pris au-dessus des bits du serveur pour qu'un ajout de ce côté ne le
// percute pas.
inline constexpr std::uint8_t kPartyShareIdle = 0x01;
inline constexpr std::uint8_t kPartyShareDead = 0x02;
inline constexpr std::uint8_t kPartyShareBusy = 0x04;
inline constexpr std::uint8_t kPartyShareOtherMap = 0x80;

// La couleur de l'annonce des réglages du groupe dans le chat.
inline constexpr ImU32 kPartyOptionsChatColor = IM_COL32(255, 204, 136, 255);

// ⚠ L'ORDRE DE CES VALEURS PART DANS LES RÉGLAGES : ne pas les intercaler.
enum class PartyFriendTab : std::uint8_t { kFriends = 0, kParty = 1 };

// Le texte d'une jauge de PV ou de SP.
enum class PartyGaugeText : int { kNone = 0, kNumbers, kPercent, kBoth };

// Le nom de carte sur la ligne : complet, court (avant la première virgule), ou
// absent — l'infobulle le donne toujours en entier.
enum class PartyMapMode : int { kFull = 0, kShort, kNone };

// La tête du personnage à la place de l'icône de classe, par onglet.
inline constexpr int kPartyHeadInParty = 1;
inline constexpr int kPartyHeadInFriends = 2;

// ── LES RÉGLAGES PERSISTANTS (section « Groupe / Amis » du panneau) ─────────
//
// Les entiers se gardent en entiers pour le fichier : un mode relu hors bornes
// retombe sur le défaut au dessin.
struct PartyFriendLook {
  bool show_job_icon = true;
  bool show_level = true;
  bool show_hp_bar = true;
  int hp_text_mode = static_cast<int>(PartyGaugeText::kNumbers);
  // ⚠ LE SP D'UN AUTRE JOUEUR SE DEMANDE AU SERVEUR : `app` ne le réclame que
  // si ce réglage est allumé.
  bool show_sp = false;
  int sp_text_mode = static_cast<int>(PartyGaugeText::kNumbers);
  int map_mode = static_cast<int>(PartyMapMode::kFull);
  bool show_tooltip = true;

  bool show_buffs = false;
  int buff_px = 16;
  int buff_max = 8;
  int buff_rows = 1;
  bool buff_time = true;
  int buff_sweep = static_cast<int>(StatusSweep::kRadial);
  // L'OPACITÉ DES ICÔNES, en pour cent, et le TEXTE DU TEMPS RESTANT : les
  // réglages de la barre d'états du joueur. Taille automatique, le texte fait
  // la moitié de l'icône, avec un plancher ; sinon, la taille réglée.
  int buff_opacity_percent = kFullStatusOpacityPercent;
  bool buff_time_auto = true;
  int buff_time_px = kDefaultStatusCountdownPixels;
  ImU32 buff_time_background = kDefaultStatusCountdownBackground;
  ImU32 buff_icon_background = kDefaultStatusIconBackground;
  ImU32 buff_sweep_color = kDefaultStatusSweepColor;

  bool click_targets = false;
  // Plus de redimensionnement ; la fenêtre se déplace toujours.
  bool lock_size = false;
  // Somme de `kPartyHeadInParty` et `kPartyHeadInFriends`.
  int head_mode = 0;

  int icon_px = 40;
  int bar_w = 96;
  int bar_h = 7;
  int row_spacing = 4;
  // Zéro : la taille de l'interface.
  int text_px = 0;
  bool bars_stacked = false;
  bool text_in_bars = false;
};

// Les trois réglages du groupe tels que le serveur les tient. Chaque champ vaut
// 0 (chacun pour soi) ou 1 (partagé).
struct PartyOptionsView {
  bool known = false;
  int exp = 0;
  int pickup = 0;
  int share = 0;
};

struct PartyMemberView {
  std::uint32_t account_id = 0;
  std::uint32_t char_id = 0;
  std::string_view name;
  // La carte telle que le serveur l'envoie, pour comparer ; et son nom affiché,
  // déjà résolu par `app` — vide quand la table ne le connaît pas.
  std::string_view map_raw;
  std::string_view map_name;
  bool leader = false;
  bool offline = false;
  bool is_me = false;
  std::string_view job_name;
  int base_level = 0;
  SkinImage job_icon;
  // Vide quand l'apparence n'est pas connue, ou le membre hors ligne.
  SkinImage head;
  // Un sprite le représente dans la scène : il se cible, et son clic droit
  // ouvre le menu d'entité.
  bool actor_present = false;
  // Déjà dans la liste d'amis du joueur.
  bool is_friend = false;
  // Pourquoi « Inviter dans mon carnet MVP » est grisé, DÉJÀ TRADUIT et vivant
  // le temps de l'image ; nul quand l'invitation est offerte.
  const char* mvp_refusal = nullptr;

  // La position en cellules, et la mini-carte de SA carte avec ses dimensions en
  // cellules. ⚠ LA CELLULE NE SE DIVISE PAS PAR LES PIXELS DU BITMAP.
  bool position_known = false;
  int x = 0;
  int y = 0;
  SkinImage minimap;
  int map_width_cells = 0;
  int map_height_cells = 0;

  bool hp_known = false;
  int hp = 0;
  int max_hp = 0;
  bool sp_known = false;
  int sp = 0;
  int max_sp = 0;

  // Les bits du serveur (`kPartyShareIdle`…), 0 quand il reçoit sa part.
  std::uint8_t share_flags = 0;

  // Ses états, dans l'ORDRE D'ARRIVÉE. ⚠ UNE LISTE VIDE NE DIT PAS « AUCUN » : le
  // serveur ne diffuse ces états qu'à qui voit le personnage.
  std::span<const StatusCellView> statuses;
};

struct FriendView {
  std::uint32_t account_id = 0;
  std::uint32_t char_id = 0;
  std::string_view name;
  bool online = false;
  SkinImage head;
  bool actor_present = false;
  // Déjà membre du groupe du joueur.
  bool in_my_party = false;
  // Comme pour un membre du groupe : la raison du grisage, ou nul.
  const char* mvp_refusal = nullptr;
};

// LA DEMANDE REÇUE, une seule à la fois : une nouvelle remplace l'ancienne.
struct PartyInviteView {
  bool active = false;
  // Faux : une invitation de groupe, et `name` est celui du GROUPE.
  bool is_friend = false;
  std::uint32_t party_id = 0;
  std::uint32_t account_id = 0;
  std::uint32_t char_id = 0;
  std::string_view name;
};

struct PartyFriendView {
  bool in_party = false;
  bool i_am_leader = false;
  std::string_view party_name;
  PartyOptionsView options;
  // L'état du partage n'est su qu'après le premier paquet du serveur. Le seuil
  // d'inactivité vient avec lui ; zéro dit la règle éteinte.
  bool share_known = false;
  int share_idle_seconds = 0;

  std::span<const PartyMemberView> members;
  std::span<const FriendView> friends;
  // Le carnet de chasse MVP est éteint : « Inviter dans mon carnet MVP » n'est
  // dans le menu d'aucune ligne.
  bool mvp_invites_withdrawn = false;

  SkinImage crown;
  // La cible courante du jeu, pour le liseré de la ligne.
  std::uint32_t target_account_id = 0;

  // La grille de groupe, allumée depuis cette fenêtre. Faux : pas de case.
  bool grid_offered = false;
  bool grid_enabled = false;

  PartyInviteView invite;
};

// ── CE QUE LA FENÊTRE GARDE D'UNE IMAGE À L'AUTRE ──────────────────────────

enum class PartyFriendVerb : std::uint8_t {
  kNothing,
  kWhisper,
  // La conversation 1:1 à part, ouverte sur ce membre ou cet ami.
  kWhisperWindow,
  kMakeLeader,
  kExpel,
  // ⚠ UN CHEF QUI QUITTE PASSE D'ABORD LA MAIN à un membre en ligne sur sa
  // carte, s'il y en a un : c'est à `app` de l'envoyer avant le départ.
  kLeave,
  kRemoveFriend,
  kCreateParty,
  kInvite,
  kAddFriend,
  kAnswerParty,
  kAnswerFriend,
  kPartyOptions,
  kTarget,
  kEntityMenu,
  // L'invitation au carnet de chasse MVP, adressée par nom.
  kInviteMvp,
};

struct PartyFriendWindowState {
  bool open = false;
  bool pinned = false;
  PartyFriendTab tab = PartyFriendTab::kParty;

  std::array<char, 64> new_party_name = {};
  std::array<char, 64> invite_name = {};
  std::array<char, 64> friend_name = {};

  // Les réglages du groupe en cours d'édition, et les derniers vus chez le
  // serveur : un changement de sa part recale l'édition et s'annonce au chat.
  int opt_exp = 0;
  int opt_pickup = 0;
  int opt_share = 0;
  PartyOptionsView seen;

  // La confirmation d'un geste sans retour, ouverte à l'image suivante au
  // niveau de la fenêtre.
  bool ask_confirm = false;
  PartyFriendVerb confirm = PartyFriendVerb::kNothing;
  std::uint32_t confirm_account = 0;
  std::uint32_t confirm_char = 0;
  std::string confirm_name;

  // De faux états, le temps de régler l'affichage. ⚠ NON PERSISTÉ : retrouvé
  // allumé, il ferait croire à des buffs qu'on n'a pas.
  bool buff_preview = false;
};

struct PartyFriendAction {
  PartyFriendVerb verb = PartyFriendVerb::kNothing;
  std::uint32_t account_id = 0;
  std::uint32_t char_id = 0;
  std::uint32_t party_id = 0;
  // Le nom du personnage ou du groupe, selon le verbe.
  std::string name;
  bool accept = false;
  int exp = 0;
  int pickup = 0;
  int share = 0;

  // Ce qui peut arriver dans la même image qu'un verbe.
  //
  // LE NOM CLIQUÉ : un appui gauche sur la ligne d'un membre du groupe ou d'un
  // ami EN LIGNE, soi compris, que le joueur soit en vue ou non. `app` en fait
  // ce qu'un clic sur son sprite ferait — viser, frapper — selon la compétence
  // armée et les touches tenues ; zéro, aucun.
  std::uint32_t named_account_id = 0;
  bool settings_asked = false;
  // La fenêtre de recherche de groupe est demandée.
  bool booking_asked = false;
  bool grid_toggled = false;
  bool grid_enabled = false;
  bool closed = false;
  // Une ligne à poster dans le chat, en `kPartyOptionsChatColor`. Vide : rien.
  std::string chat_line;
  // L'ONGLET dont il veut poser le lien dans le chat — Maj + clic sur lui.
  std::optional<PartyFriendTab> link_tab;
};

// ── LES RÈGLES, séparées du dessin pour être éprouvées ──────────────────────

// Ce qui précède la première virgule, ou la première parenthèse ouvrante
// précédée d'une espace.
[[nodiscard]] std::string_view ShortMapName(std::string_view full);

// Les raisons qui écartent ce membre du partage : celles du serveur, plus la
// carte. Zéro hors ligne ou tant que l'état n'est pas connu.
[[nodiscard]] std::uint8_t PartyShareFlags(const PartyFriendView& view,
                                           const PartyMemberView& member);

// UNE raison, la plus actionnable d'abord. Vide s'il n'y en a aucune.
[[nodiscard]] std::string PartyShareReason(std::uint8_t flags, int idle_seconds);

// Le texte d'une jauge dans le mode donné. Le pourcentage reste à un tant qu'il
// reste un point.
[[nodiscard]] std::string PartyGaugeCaption(int current, int most, int mode);

// La ligne de chat qui annonce les trois réglages.
[[nodiscard]] std::string PartyOptionsAnnouncement(const PartyOptionsView& options);

// Pourquoi « Nommer chef » est refusé sur ce membre, déjà traduit ; nul quand
// le geste est permis. Le serveur l'exige en ligne et sur la carte du chef.
[[nodiscard]] const char* PartyLeaderRefusal(const PartyFriendView& view,
                                             const PartyMemberView& member);

// ⚠ À APPELER À CHAQUE IMAGE, MÊME FENÊTRE FERMÉE : la demande reçue et
// l'annonce des réglages du groupe vivent hors de la fenêtre.
[[nodiscard]] PartyFriendAction DrawPartyFriendWindow(PartyFriendWindowState& state,
                                                      const PartyFriendView& view,
                                                      const PartyFriendLook& look,
                                                      const SkinArt& art);

}  // namespace ro::ui
