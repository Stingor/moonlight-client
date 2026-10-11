#pragma once

#include <array>
#include <cstddef>
#include <cstdint>
#include <optional>
#include <span>
#include <string>
#include <string_view>
#include <vector>

#include <imgui.h>

#include "ro/ui/chat_link.h"
#include "ro/ui/skin_art.h"

namespace ro::ui {

// LE CARNET DE CHASSE MVP : la table des créneaux, le groupe de chasse, la
// modale d'invitation, le bandeau d'alerte et les lignes détachées.
//
// 🔴 `ui` NE TIENT AUCUN ÉTAT DE JEU : `app` compose une vue déjà datée et
// traduite à chaque image, et reçoit en retour ce que le joueur a demandé. La
// fenêtre ne garde que de l'état d'INTERFACE — tampons de saisie, tri, glisser.

// L'IDENTITÉ DE LA FENÊTRE : clé de sa place, de son épingle et de la pile
// d'Échap.
inline constexpr const char* kMvpTrackerWindowId = "mvp_tracker";
inline constexpr std::uint16_t kMvpNoSlotView = 0xFFFF;
// La cellule d'une tombe inconnue : toute valeur négative l'est.
inline constexpr std::int16_t kMvpNoTombView = -1;
// La taille initiale, à la première ouverture, en pixels d'interface.
inline constexpr float kMvpFirstWidthPixels = 720.0F;
inline constexpr float kMvpFirstHeightPixels = 460.0F;
// LE CÔTÉ FIGÉ du sprite d'un rang et d'une ligne détachée, en pixels
// d'interface.
inline constexpr float kMvpSpritePixels = 64.0F;
// Combien de temps se montrent le surlignage du rang visé, le bandeau rouge
// d'un refus et le bandeau d'alerte.
inline constexpr std::uint32_t kMvpFocusMs = 6000;
inline constexpr std::uint32_t kMvpResultShownMs = 6000;
inline constexpr std::uint32_t kMvpAlertShownMs = 20000;
// LE NOM DU PERSONNAGE À INVITER, saisi en UTF-8 : jusqu'à 23 caractères du
// fil, chacun au pire 3 octets une fois en UTF-8, plus le zéro final. ⚠ LA
// BORNE RÉELLE EST CELLE DU FIL, après conversion : mesurée en UTF-8, elle
// couperait un nom accentué.
inline constexpr std::size_t kMvpNameMostWireChars = 23;
inline constexpr std::size_t kUtf8MostBytesPerWireChar = 3;
inline constexpr std::size_t kMvpInviteNameUtf8Bytes =
    kMvpNameMostWireChars * kUtf8MostBytesPerWireChar + 1;
// LE NOM D'UN GROUPE DE CHASSE : le serveur en lit 31 octets du fil et coupe le
// reste en silence. Un caractère CP1252 fait un octet du fil.
inline constexpr std::size_t kMvpGroupNameMostWireChars = 31;
inline constexpr std::size_t kMvpGroupNameUtf8Bytes =
    kMvpGroupNameMostWireChars * kUtf8MostBytesPerWireChar + 1;

// LES RÉGLAGES QUE LA FENÊTRE VOIT, recopiés à chaque image. Le défaut est la
// valeur de déclaration.
struct MvpTrackerLook {
  bool show_sprites = true;
  bool animate_sprites = false;
  bool show_filter = false;
  bool line_show_sprite = true;
  ImU32 line_text_color = IM_COL32(245, 237, 209, 255);
  ImU32 line_background_color = IM_COL32(10, 10, 15, 140);
  // La colonne « Délai » : le réglage fait foi, le clic droit de l'en-tête la
  // change aussi.
  bool show_delay = false;
};

// UN RANG DU CATALOGUE, dans l'ordre du serveur. Tout est déjà composé par
// `app`.
struct MvpSlotView {
  std::uint16_t slot_id = kMvpNoSlotView;
  // Zéro : créneau scripté.
  std::uint16_t mob_id = 0;
  // Le sprite PEINT (il tourne sur un créneau scripté) ; zéro : aucun.
  std::uint16_t sprite_mob_id = 0;
  // Le monstre DÉSIGNÉ par le nom ; zéro : le nom est un texte simple.
  std::uint16_t link_mob_id = 0;
  std::uint32_t delay1_ms = 0;
  std::uint32_t delay2_ms = 0;
  std::uint16_t map_cells_x = 0;
  std::uint16_t map_cells_y = 0;
  // Le nom interne de la carte.
  std::string_view map;
  // Le nom lisible de la carte, en UTF-8 ; vide : l'installation ne le connaît
  // pas.
  std::string_view map_label;
  // Le nom affiché, en UTF-8.
  std::string_view label;
  bool favorite = false;
  // La fenêtre de retour, en secondes UNIX du serveur.
  bool window_known = false;
  std::int64_t window_from_s = 0;
  std::int64_t window_to_s = 0;
  bool exact = false;
  // L'observation du groupe.
  bool observed = false;
  // 0 saisi, 1 tombe, 2 tué, 3 miroir ; toute autre valeur se montre « ? ».
  std::uint8_t source_raw = 0;
  std::int64_t kill_time_s = 0;
  std::int64_t exact_respawn_s = 0;
  std::int16_t tomb_x = kMvpNoTombView;
  std::int16_t tomb_y = kMvpNoTombView;
  std::int64_t reported_at_s = 0;
  std::string_view by_name;
};

struct MvpMemberView {
  std::uint32_t user_id = 0;
  int level = 0;
  bool online = false;
  // Vide : compte sans personnage.
  std::string_view name;
  // UNE LIGNE DE NOTRE COMPTE, quelle qu'en soit la session : « (vous) », et
  // rien à exclure. Faux quand le serveur ne dit pas notre compte.
  bool our_account = false;
};

// UN SPRITE DEMANDÉ. Image inutilisable : la case reste vide, mais réservée.
struct MvpSpriteArt {
  std::uint16_t mob_id = 0;
  SkinImage image;
};

// LE PLAN DE LA CARTE SURVOLÉE, composé à l'image qui suit la demande.
struct MvpMapPreview {
  // Vide : rien de composé.
  std::string_view map;
  // Inutilisable : « (pas de miniature) ».
  SkinImage image;
};

// UNE LIGNE DÉTACHÉE RÉSOLUE, c'est-à-dire dont le créneau est au catalogue.
// `line` est son rang dans le livre d'`app`.
struct MvpPinnedLineView {
  int line = -1;
  std::uint16_t slot_id = kMvpNoSlotView;
  std::uint16_t mob_id = 0;
  std::string_view map;
  int x = 0;
  int y = 0;
};

// UNE ALERTE ALLUMÉE, déjà composée par `app`.
struct MvpAlertView {
  std::uint16_t slot_id = kMvpNoSlotView;
  std::string_view label;
  // Le nom de carte montré : lisible s'il est connu, interne sinon.
  std::string_view map;
  // Un instant mérité au Convex Mirror : « de retour à HH:MM ».
  bool exact = false;
  // « HH:MM », l'heure locale de l'instant exact ; vide hors instant exact.
  std::string_view at_clock;
};

struct MvpTrackerView {
  std::int64_t server_now_s = 0;
  std::uint32_t now_ms = 0;
  // L'INTERRUPTEUR MAÎTRE : éteint, rien ne se dessine — ni fenêtre, ni bandeau
  // d'alerte, ni lignes détachées, ni invitation reçue.
  bool enabled = false;
  // 🔴 LA LIAISON EST VÉRIFIÉE : hors d'elle, le serveur jette toute commande
  // sans répondre. Faux, chaque geste qui envoie se grise et dit pourquoi, et
  // ce qu'on a saisi reste dans son champ.
  bool link_ready = false;
  bool catalog_known = false;
  std::span<const MvpSlotView> slots;
  // LE GROUPE.
  std::uint32_t group_id = 0;
  std::uint32_t owner_user_id = 0;
  std::string_view group_name;
  std::span<const MvpMemberView> members;
  // LA PROPRIÉTÉ, dite par le serveur qui nomme notre compte, déduite du nom de
  // notre personnage parmi les membres contre un serveur ancien. `owner_known`
  // faux : rien ne se grise d'avance.
  bool owner_known = false;
  bool i_own = false;
  // LE DERNIER RÉSULTAT QUI N'EST PAS UN SUCCÈS : son code, et s'il se montre
  // encore (`app` a jugé la durée). Un refus se dit en rouge ; une saisie sans
  // effet, qui n'en est pas un, en teinte neutre.
  std::uint8_t result_code = 0;
  bool result_shown = false;
  // L'INVITATION EN ATTENTE ; zéro : aucune.
  std::uint32_t pending_invite = 0;
  std::string_view pending_invite_name;
  // LA RÉPONSE À L'INVITATION : partie et encore attendue (`app` a jugé la
  // patience), refusée (code du serveur), ou l'invitation n'existe plus.
  bool invite_waiting = false;
  std::uint8_t invite_refusal = 0;
  bool invite_gone = false;
  // L'ART.
  std::span<const MvpSpriteArt> sprites;
  MvpMapPreview map_preview;
  // L'étoile des favoris, éteinte et allumée.
  SkinImage favorite_off;
  SkinImage favorite_on;
  // LES LIGNES DÉTACHÉES résolues.
  std::span<const MvpPinnedLineView> lines;
  // LES ALERTES ALLUMÉES, la plus ancienne en tête ; `app` a jugé la durée de
  // chacune.
  std::span<const MvpAlertView> alerts;
};

// UNE CLÉ DE RANG POUR LE TRI : comparée CHAMP À CHAMP, jamais hachée. Le nom et
// la carte en font partie : un créneau renommé refait le tri.
struct MvpRowKey {
  std::uint16_t slot_id = 0;
  std::uint32_t delay1_ms = 0;
  bool favorite = false;
  std::int64_t window_from_s = 0;
  std::int64_t reported_at_s = 0;
  std::string label;
  std::string map;
  std::string map_label;
  bool operator==(const MvpRowKey&) const = default;
};

// CE QUE LA FENÊTRE GARDE d'une image à l'autre : de l'état d'INTERFACE
// seulement.
struct MvpTrackerWindowState {
  // ⚠ JAMAIS MÉMORISÉ.
  bool open = false;
  // Mémorisé, avec les épingles des autres fenêtres.
  bool pinned = false;
  // UNE DEMANDE DE PREMIER PLAN posée par `app` (lien, import, Accepter,
  // alerte, tombe) ; consommée par le dessin.
  bool raise = false;
  // Recopié des réglages à chaque image.
  MvpTrackerLook look;
  bool was_collapsed = false;
  std::array<char, kMvpGroupNameUtf8Bytes> group_name{};
  std::array<char, kMvpInviteNameUtf8Bytes> invite_name{};
  std::array<char, 16> manual_time{};
  // ⚠ JAMAIS MÉMORISÉ : on filtre pour trouver quelque chose maintenant.
  std::array<char, 32> filter{};
  std::uint16_t manual_slot = kMvpNoSlotView;
  bool open_manual_popup = false;
  bool confirm_leave = false;
  bool confirm_dissolve = false;
  // L'INVITATION REMISE À PLUS TARD, hors liaison vérifiée : la modale ne se
  // redemande pas avant que la liaison revienne. Rien n'est parti.
  bool invite_deferred = false;
  std::uint16_t drag_slot = kMvpNoSlotView;
  // Un rang se glissait au début de cette image : son lâcher n'est le clic
  // d'aucun nom.
  bool row_dragged = false;
  // LE RANG VISÉ, posé par `app` : ouverture sur un lien, import.
  std::uint16_t focus_slot = kMvpNoSlotView;
  std::uint32_t focus_ms = 0;
  bool focus_scrolled = false;
  ImVec2 window_pos{0.0F, 0.0F};
  ImVec2 window_size{0.0F, 0.0F};
  // LE GLISSER D'UNE LIGNE DÉTACHÉE, et la taille MESURÉE de chaque ligne, par
  // rang du livre.
  int line_drag = -1;
  ImVec2 line_drag_offset{0.0F, 0.0F};
  std::vector<ImVec2> line_sizes;
  // LE MENU DU NOM : le créneau visé, l'ouverture demandée.
  std::uint16_t menu_slot = kMvpNoSlotView;
  bool menu_requested = false;
  // L'état de la colonne « Délai » que la table a appliqué ; vide : rien
  // encore.
  std::optional<bool> delay_applied;
  // LE TRI MÉMORISÉ. Les places retenues dans le catalogue se gardent avec les
  // empreintes : un ordre conservé ne désigne jamais une place que le
  // catalogue n'a plus.
  int sort_key = 0;
  std::vector<MvpRowKey> sorted_keys;
  std::vector<std::size_t> sorted_rows;
  std::vector<std::size_t> order;
  // LES DEMANDES D'ART DE CETTE IMAGE : les sprites dessinés, la carte survolée.
  std::vector<std::uint16_t> wanted_sprites;
  std::string wanted_map;
  // Le titre composé, identité comprise.
  std::string title;
  // Le bouton « Signaler un bug » de la barre de titre, selon le réglage.
  bool bug_report = false;
};

// CE QUE LE JOUEUR A DEMANDÉ à cette image.
struct MvpTrackerAction {
  // La puce de la barre de titre : la page de réglages du carnet.
  bool open_settings = false;
  // En UTF-8.
  std::string create_group;
  std::string invite_member;
  std::string kick_member;
  bool leave_group = false;
  bool dissolve_group = false;
  bool accept_invite = false;
  bool decline_invite = false;
  // « Fermer » sur une invitation que le serveur dit disparue : rien ne part.
  bool dismiss_invite = false;
  std::uint16_t favorite_slot = kMvpNoSlotView;
  bool favorite_on = false;
  std::uint16_t manual_slot = kMvpNoSlotView;
  std::int64_t manual_kill_time_s = 0;
  // UN GESTE SUR LE NOM D'UN RANG : genre `kMonster`, `item_id` le monstre
  // désigné, `token` le créneau.
  ChatLinkAsk name_link;
  // UN RANG LÂCHÉ HORS DE LA FENÊTRE, et où.
  std::uint16_t pin_slot = kMvpNoSlotView;
  int pin_x = 0;
  int pin_y = 0;
  // UNE LIGNE DÉTACHÉE déplacée à cette image (rang du livre), relâchée, ou
  // retirée.
  int moved_line = -1;
  int moved_x = 0;
  int moved_y = 0;
  bool lines_released = false;
  int removed_line = -1;
  // Le bandeau d'alerte cliqué : ouvrir le carnet sur ce créneau.
  std::uint16_t alert_slot = kMvpNoSlotView;
  // La colonne « Délai » montrée ou masquée au clic droit de l'en-tête.
  std::optional<bool> delay_shown;
  // « Signaler un bug » : `app` joint ce que le carnet montre — le tri, le
  // filtre, le groupe, et le créneau visé.
  bool report_bug = false;
};

// L'APERÇU AU SURVOL D'UN LIEN `<MVPL>`, composé par `app` : les textes sont
// DÉJÀ traduits et datés.
struct MvpLinkTip {
  // Le nom du MVP, ou « MVP ».
  std::string name;
  // Le nom interne de la carte.
  std::string map;
  // « Retour à 21:12 (Convex Mirror) », « Retour entre … », ou vide.
  std::string respawn_line;
  // « Mort à 20:02 », ou vide.
  std::string death_line;
  // « Tombe en (150, 180) », ou vide.
  std::string tomb_line;
  // « Échu : … », ou vide.
  std::string passed_line;
};

// TOUT CE QUE LE CARNET DESSINE, dans cet ordre : le bandeau d'alerte, les
// lignes détachées, le fantôme et le lâcher du glisser, la modale
// d'invitation, puis — ouverte — la fenêtre.
[[nodiscard]] MvpTrackerAction DrawMvpTrackerWindow(MvpTrackerWindowState& state,
                                                    const MvpTrackerView& view, const SkinArt& art);
// L'aperçu d'un `<MVPL>` survolé : une infobulle habillée.
void DrawMvpLinkTip(const MvpLinkTip& tip);

// ── AIDES PUBLIQUES, pour les essais ──

// Une durée lisible : « 2h14 », « 59 min », « 12 s ». Le signe est ignoré.
[[nodiscard]] std::string FormatMvpDuration(std::int64_t seconds);
// L'heure de mort saisie (« 1430 », « 14h30 », « -2350 » pour la veille), en
// secondes UNIX ; zéro si elle est illisible.
[[nodiscard]] std::int64_t ParseMvpKillTime(std::string_view text, std::int64_t server_now_s);
// Le texte d'une ligne détachée.
[[nodiscard]] std::string MvpLineText(const MvpSlotView& slot, std::int64_t server_now_s);
// Le titre de la fenêtre repliée, qui dit la prochaine échéance ; sans
// l'identité « ### ».
[[nodiscard]] std::string MvpCollapsedTitle(std::span<const MvpSlotView> slots,
                                            std::int64_t server_now_s);
// La phrase d'un résultat de groupe : vide pour un succès, nulle pour un code
// inconnu.
[[nodiscard]] const char* MvpResultText(std::uint8_t code);
// Ce résultat est-il un REFUS ? Ni un succès, ni une saisie sans effet.
[[nodiscard]] bool MvpResultRefuses(std::uint8_t code);
// La raison, déjà traduite, d'un geste qui envoie tant que la liaison n'est
// pas vérifiée : la même dans la fenêtre, les menus et le chat.
[[nodiscard]] const char* MvpLinkPendingText();
// La raison, déjà traduite, de tout ce qui passe par le carnet quand son
// interrupteur est éteint : la même dans les menus et le chat.
[[nodiscard]] const char* MvpTrackerOffText();
// Le libellé d'une source d'observation ; « ? » pour une source inconnue.
[[nodiscard]] const char* MvpSourceLabel(std::uint8_t source_raw);
// LE TRI DE LA TABLE que retient `MvpTrackerWindowState::sort_key` : la colonne
// et le sens, « name_asc », « return_desc »… ; « none » sans tri.
[[nodiscard]] std::string MvpSortWord(int sort_key);
// Le filtre de la table : sous-chaîne sans casse du libellé ou de la carte.
[[nodiscard]] bool MvpMatchesFilter(const MvpSlotView& slot, std::string_view filter);
// LA HAUTEUR DONNÉE AU CLIPPER. ⚠ ELLE DOIT ÉGALER CELLE QUE LES CELLULES
// OCCUPENT, sans quoi le défilement dérive.
[[nodiscard]] float MvpTableRowHeight(bool show_sprites, const MvpTrackerView& view,
                                      const SkinArt& art);
// Le nom de carte montré : lisible s'il est connu, interne sinon.
[[nodiscard]] std::string_view MvpMapShown(const MvpSlotView& slot);
// « Baphomet (Labyrinthe de Prontera, 3F) » : le nom d'une ligne détachée.
[[nodiscard]] std::string MvpLineName(const MvpSlotView& slot);
// Le retour d'un créneau, le même dans la table, la ligne et le fantôme.
enum class MvpReturnTone : std::uint8_t { kPlain, kDim, kPossible };
[[nodiscard]] std::string MvpReturnText(const MvpSlotView& slot, std::int64_t server_now_s);
[[nodiscard]] MvpReturnTone MvpReturnToneOf(const MvpSlotView& slot, std::int64_t server_now_s);
// L'en-tête du groupe, singulier compris, identité « ###mvp_group » comprise.
[[nodiscard]] std::string MvpGroupHeader(std::string_view group_name, int accounts);
// « 14:30 aujourd'hui » / « 14:30 hier » : l'heure saisie telle qu'elle a été
// lue.
[[nodiscard]] std::string MvpKillTimeEcho(std::int64_t kill_time_s, std::int64_t server_now_s);
// La plus longue tête de `utf8`, à une frontière de caractère, qui tient en
// `most_wire_bytes` octets CP1252.
[[nodiscard]] std::size_t MvpWireCut(std::string_view utf8, std::size_t most_wire_bytes);
// CE QU'UN CHAMP TROP LONG PERD : les caractères entiers JUSTE AVANT LE
// CURSEUR, là où l'on vient d'écrire — une insertion au début d'un champ plein
// ne doit pas amputer la fin du nom. Curseur trop tôt pour qu'ils suffisent :
// la queue. `bytes` nul : le champ tient.
struct MvpWireExcess {
  std::size_t at = 0;
  std::size_t bytes = 0;
};
[[nodiscard]] MvpWireExcess MvpWireExcessOf(std::string_view utf8, std::size_t cursor,
                                            std::size_t most_wire_bytes);

}  // namespace ro::ui
