#pragma once

#include <cstdint>
#include <span>
#include <string_view>
#include <vector>

#include <imgui.h>

#include "ro/ui/skin_art.h"

namespace ro::ui {

// LA CASE D'UN ÉTAT — buff ou altération.
//
// 🔴 UN SEUL ENDROIT QUI SAIT À QUOI RESSEMBLE UN ÉTAT À L'ÉCRAN. Deux
// surfaces qui dessineraient la même icône chacune par sa boucle finiraient par
// diverger — l'une gagnant un grisage ou un compte à rebours que l'autre
// n'aurait pas. Ce foyer existe pour qu'il n'y en ait jamais deux.
//
// Le contrat est : l'APPELANT place, la case peint.

// Ce qu'une case a besoin de savoir. ⚠ AUCUN ÉTAT DE JEU : `ui` reçoit une vue,
// comme partout ailleurs.
struct StatusCellView {
  // Vide quand l'installation ne dessine pas cet état — la case se replie alors
  // sur son abréviation.
  SkinImage icon;

  // Le nom, pour l'infobulle. Vide quand la table ne le nomme pas.
  std::string_view name;

  // CE QUE L'ÉTAT FAIT, ligne à ligne. Vide quand la table ne l'explique pas.
  //
  // 🔴 C'EST CE QUE LE JOUEUR VIENT LIRE. Une infobulle qui n'écrit que le nom
  // répète l'icône ; elle ne dit pas ce qu'« Impositio Manus » ajoute. Six cent
  // trente-trois états en portent une dans cette installation.
  std::span<const std::string_view> description;

  // OÙ LA DURÉE S'INSÈRE dans ces lignes. ⚠ LE CLIENT LA MET AU MILIEU, à la
  // place que sa table lui réserve — pas en tête. Zéro = juste sous le nom.
  int time_line = 0;

  // Ce qu'on écrit DANS la case faute d'image. Vide laisse la case nue.
  std::string_view abbreviation;

  // ⚠ ZÉRO VEUT DIRE « SANS ÉCHÉANCE », et non « fini » : un état permanent n'a
  // ni voile ni compte à rebours. Voir `sim::ActiveStatus`.
  std::uint32_t remaining_ms = 0;
  std::uint32_t total_ms = 0;
};

// Le voile de la part ÉCOULÉE.
//
// ⚠ L'ORDRE DE CES VALEURS PART DANS LES RÉGLAGES : ne pas les intercaler.
enum class StatusSweep : std::uint8_t {
  kNone = 0,
  // Le balayage horaire des jeux de rôle, depuis midi.
  kRadial,
  // Le voile DESCEND depuis le haut : plus lisible en petit.
  kVertical,
};

// Le fond d'icône par défaut : noir et opaque, et les icônes y perdent leur
// transparence.
inline constexpr ImU32 kDefaultStatusIconBackground = IM_COL32(0, 0, 0, 255);

struct StatusCellStyle {
  StatusSweep sweep = StatusSweep::kVertical;
  ImU32 sweep_color = IM_COL32(0, 0, 0, 140);

  // La hauteur du compte à rebours sous l'icône, en pixels. Zéro : pas de texte.
  float countdown_pixels = 0.0F;

  // LE FOND DERRIÈRE CE TEXTE, transparence comprise. Zéro : aucun.
  //
  // ⚠ IL SERT PLUS QU'IL N'ORNE : sous l'icône, les chiffres tombent sur le
  // MONDE — une herbe claire, un mur blanc — et le cerné ne suffit pas toujours.
  ImU32 countdown_background = 0;

  // LE FOND DERRIÈRE L'IMAGE, transparence comprise. Zéro : aucun.
  //
  // ⚠ LES ICÔNES D'ÉTAT SONT TRANSLUCIDES : leur alpha descend sous le quart sur
  // les bords. Sans fond, le décor passe à travers et les éclaircit.
  ImU32 icon_background = 0;

  // 🔴 UN CHAMP, ET NON `ImGuiStyleVar_Alpha`. Le style d'ImGui n'est appliqué
  // que par les widgets qui le LISENT ; les primitives d'un `ImDrawList` —
  // `AddImage`, `AddRectFilled`, `AddText` — l'ignorent totalement. Or cette
  // case ne dessine QUE des primitives : un `PushStyleVar` autour d'elle n'a
  // aucun effet, et cela ne se voit qu'à l'écran, sans un avertissement.
  float alpha = 1.0F;
};

// Peint UNE case dans `top_left..bottom_right`, et pose son infobulle si le
// curseur est dessus.
//
// Rend faux quand rien n'a été peint — ni image, ni abréviation. ⚠ L'APPELANT
// QUI COMPTE SES CASES DOIT EN TENIR COMPTE : une case qui ne se dessine pas ne
// doit pas prendre de place.
bool DrawStatusCell(const StatusCellView& cell, ImVec2 top_left, ImVec2 bottom_right,
                    const StatusCellStyle& style, bool tooltip);

// LE BALAYAGE SEUL : le voile de la part écoulée, du midi de la case.
//
// 🔴 EXPOSÉ PARCE QUE C'EST LA SEULE PART DU DESSIN QUI DÉBORDE DE SA CASE.
// Son rayon passe les coins exprès, et c'est un clip — non sa géométrie — qui le
// retient : un défaut qui ne se voit que sur la case VOISINE, donc pas là où on
// le cherche. Un banc le tient maintenant.
//
// `spent` va de 0 (l'état commence) à 1 (il finit).
void PaintStatusSweep(ImDrawList& into, ImVec2 top_left, ImVec2 bottom_right, float spent,
                      ImU32 color);

// L'INFOBULLE SEULE, pour qui la pose lui-même.
//
// ⚠ EXPOSÉE PARCE QUE LA CASE N'EST PAS LA SEULE SURFACE QUI LA VEUT : un lien de
// chat vers un état la voudra aussi, et deux écritures de la même mise en page
// divergeraient — c'est ce que ce foyer existe pour empêcher.
void DrawStatusTooltip(const StatusCellView& cell);

// ── LA RANGÉE ──────────────────────────────────────────────────────────────

// Le rangement, quand il y a plus d'états que de place.
//
// ⚠ L'ORDRE DE CES VALEURS PART DANS LES RÉGLAGES : ne pas les intercaler.
enum class StatusSort : std::uint8_t {
  // L'ordre où ils sont tombés.
  kArrival = 0,
  // Bientôt fini d'abord — ce qu'il faut relancer.
  kEndingSoon,
  // Plus long d'abord — le fond stable.
  kLongest,
};

struct StatusRowLayout {
  float side = 16.0F;
  float gap = 1.0F;

  // LA PLACE RÉSERVÉE SOUS CHAQUE CASE, en pixels : celle du compte à rebours.
  //
  // 🔴 ELLE APPARTIENT À LA RANGÉE ET NON AU DESSIN, parce que c'est elle qui
  // décide de l'écart entre deux LIGNES. Sans elle, le compte à rebours d'une
  // ligne tomberait sur les icônes de la suivante — et, sur la dernière, hors du
  // cadre, donc invisible.
  float below = 0.0F;
  // Le plafond TOTAL, toutes lignes confondues.
  int most = 8;
  int rows = 1;
  StatusSort sort = StatusSort::kArrival;

  // De droite à gauche : l'origine est alors le coin haut-DROIT.
  //
  // 🔴 LE SENS N'EST PAS COSMÉTIQUE. Une rangée qui grandit vers la gauche garde
  // ses icônes à la même place quand une nouvelle tombe ; empiler par la gauche
  // fait glisser toute la rangée à chaque changement, et l'œil perd ce qu'il
  // suivait.
  bool right_to_left = false;
  // Vers le HAUT : l'origine est alors le coin BAS. Même raison, sur l'autre axe.
  bool upward = false;
  // Le bord à ne pas franchir, zéro pour aucun. Une case qui le passerait part
  // à la ligne suivante, ou n'est pas posée s'il n'en reste plus.
  float limit = 0.0F;
};

// OÙ CHAQUE CASE SE POSE, sans rien dessiner.
//
// ⚠ SÉPARÉ DU DESSIN POUR ÊTRE ESSAYÉ. Le placement est de la géométrie pure —
// le plafond réparti sur les lignes, le retour à la ligne, les deux sens — et
// c'est la seule partie de cette case qu'un banc peut prouver. Mêlé au dessin,
// il ne se vérifierait qu'à l'œil.
[[nodiscard]] std::vector<ImVec2> PlaceStatusRow(std::size_t count, ImVec2 origin,
                                                 const StatusRowLayout& layout);

// L'ordre dans lequel les états se montrent, sous la forme d'une liste de rangs.
//
// ⚠ UNE LISTE DE RANGS PLUTÔT QU'UNE COPIE TRIÉE : la vue appartient à
// l'appelant, et un tri sur place changerait l'ordre d'arrivée, qui EST une
// information.
[[nodiscard]] std::vector<std::size_t> OrderStatusCells(std::span<const StatusCellView> cells,
                                                        StatusSort sort, int most);

// Peint la rangée entière : elle place, ordonne, et dessine.
//
// ⚠ `tooltip` EST UN CHOIX DE L'APPELANT, et non une constante : une case décide
// de son survol par un test de RECTANGLE, qui ignore l'empilement des fenêtres.
// C'est ce qui la rend dessinable dans une fenêtre sourde — et c'est aussi ce
// qui lui ferait poser son infobulle À TRAVERS un panneau ouvert par-dessus.
void DrawStatusRow(std::span<const StatusCellView> cells, ImVec2 origin,
                   const StatusRowLayout& layout, const StatusCellStyle& style, bool tooltip);

}  // namespace ro::ui
