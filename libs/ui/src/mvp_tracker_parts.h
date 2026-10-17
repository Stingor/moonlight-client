#pragma once

// LES PIÈCES COMMUNES DU CARNET DE CHASSE MVP : la fenêtre
// (`mvp_tracker_window.cpp`), la table (`mvp_tracker_table.cpp`) et les
// surcouches (`mvp_tracker_overlays.cpp`) les partagent. Interne à `ui`.

#include <cstddef>
#include <cstdint>
#include <span>
#include <string>
#include <string_view>

#include <imgui.h>

#include "ro/ui/mvp_tracker_window.h"
#include "ro/ui/skin_art.h"

namespace ro::ui::mvp_tracker {

// ── LES COULEURS ────────────────────────────────────────────────────────────
// Le bandeau du dernier refus.
inline constexpr ImVec4 kMvpRefusalTextColor(0.60F, 0.12F, 0.12F, 1.0F);
// Le bandeau d'un résultat qui n'est pas un refus — une saisie sans effet :
// gris FONCÉ, sur le même fond clair.
inline constexpr ImVec4 kMvpNeutralResultTextColor(0.25F, 0.27F, 0.32F, 1.0F);
// « possible (encore …) » : FONCÉ dans la table, dont le fond est clair ;
// CLAIR sur une ligne détachée, dont le fond est sombre.
inline constexpr ImVec4 kMvpPossibleTextColor(0.10F, 0.50F, 0.15F, 1.0F);
inline constexpr ImVec4 kMvpPossibleLineColor(0.45F, 0.90F, 0.45F, 1.0F);
// Le rang visé, le temps de `kMvpFocusMs`.
inline constexpr ImVec4 kMvpFocusRowColor(0.85F, 0.72F, 0.30F, 0.22F);
// Le nom d'un MVP qui est un lien, dans la table.
inline constexpr ImU32 kMvpNameLinkColor = IM_COL32(26, 77, 217, 255);
// Le point de la tombe sur le plan : instant mérité, fenêtre, liseré.
inline constexpr ImU32 kMvpTombExactColor = IM_COL32(255, 216, 96, 255);
inline constexpr ImU32 kMvpTombWindowColor = IM_COL32(232, 140, 60, 255);
inline constexpr ImU32 kMvpTombRingColor = IM_COL32(20, 20, 20, 220);
// L'étoile au repos ou allumée, et éteinte survolée.
inline constexpr ImU32 kMvpFavoriteRestTint = IM_COL32(230, 230, 230, 255);
inline constexpr ImU32 kMvpFavoriteHoverTint = IM_COL32(255, 255, 255, 255);
// Le fantôme d'un rang qu'on glisse.
inline constexpr ImU32 kMvpGhostBackColor = IM_COL32(10, 10, 15, 200);
inline constexpr ImU32 kMvpGhostTextColor = IM_COL32(245, 237, 209, 255);
// Le bandeau d'alerte.
inline constexpr ImVec4 kMvpAlertTextColor(1.0F, 0.85F, 0.35F, 1.0F);
// Les lignes secondaires de l'aperçu d'un `<MVPL>` : tout sauf le nom.
inline constexpr ImU32 kMvpTipDimColor = IM_COL32(89, 89, 107, 255);

// ── LA MISE EN PAGE, en pixels d'interface sauf mention « brut » ────────────
inline constexpr float kMvpGroupNameWidthPixels = 200.0F;
inline constexpr float kMvpInviteNameWidthPixels = 180.0F;
inline constexpr float kMvpLevelColumnPixels = 50.0F;
inline constexpr float kMvpStateColumnPixels = 70.0F;
inline constexpr float kMvpActColumnPixels = 70.0F;
// Le retrait des personnages suivants d'un même compte.
inline constexpr float kMvpSecondHeadIndentPixels = 12.0F;
inline constexpr float kMvpFilterWidthPixels = 220.0F;
inline constexpr float kMvpGripColumnPixels = 14.0F;
inline constexpr float kMvpFavoriteColumnPixels = 26.0F;
inline constexpr float kMvpDelayColumnPixels = 100.0F;
inline constexpr float kMvpReturnColumnPixels = 150.0F;
inline constexpr float kMvpAgeColumnPixels = 70.0F;
inline constexpr float kMvpSourceColumnPixels = 70.0F;
// La poignée : sa largeur, ses deux colonnes de points, leur rayon (brut) et
// leur nombre par colonne.
inline constexpr float kMvpGripWidthPixels = 10.0F;
inline constexpr float kMvpGripDotNearPixels = 3.0F;
inline constexpr float kMvpGripDotFarPixels = 7.0F;
inline constexpr float kMvpGripDotRadius = 1.2F;
inline constexpr int kMvpGripDotRows = 3;
// Où le rang visé tombe dans la hauteur de la table quand elle défile vers lui.
inline constexpr float kMvpFocusScrollRatio = 0.4F;
// Le plan de la carte en infobulle, et le point de sa tombe (liseré brut).
inline constexpr float kMvpPreviewSidePixels = 180.0F;
inline constexpr float kMvpTombDotRadiusPixels = 4.0F;
inline constexpr float kMvpTombRingWidth = 1.5F;
// Le côté de l'étoile quand son art ne dit pas sa taille.
inline constexpr float kMvpFavoriteFallbackPixels = 16.0F;
inline constexpr float kMvpManualTimeWidthPixels = 120.0F;
// Le bandeau d'alerte : sous le haut de l'écran, et son fond.
inline constexpr float kMvpAlertTopPixels = 90.0F;
inline constexpr float kMvpAlertBackAlpha = 0.85F;
// Une ligne détachée et le fantôme du glisser (bruts).
inline constexpr ImVec2 kMvpLinePadding(6.0F, 3.0F);
inline constexpr ImVec2 kMvpGhostOffset(12.0F, 8.0F);
inline constexpr ImVec2 kMvpGhostMargin(4.0F, 2.0F);
inline constexpr float kMvpGhostRounding = 3.0F;
// Ce qui écarte une échéance du titre replié : la fin d'une fenêtre plus une
// demi-heure, un instant exact plus un quart d'heure.
inline constexpr std::int64_t kMvpCollapsedWindowFreshS = 1800;
inline constexpr std::int64_t kMvpCollapsedExactFreshS = 900;
// LES COLONNES DE LA TABLE, par index de DÉCLARATION. ⚠ Une colonne masquée
// garde son index : c'est ce qui évite de renuméroter les suivantes selon un
// réglage. Leur nombre doit s'accorder EXACTEMENT avec les déclarations.
inline constexpr int kMvpColumnGrip = 0;
inline constexpr int kMvpColumnFavorite = 1;
inline constexpr int kMvpColumnName = 2;
inline constexpr int kMvpColumnMap = 3;
inline constexpr int kMvpColumnDelay = 4;
inline constexpr int kMvpColumnReturn = 5;
inline constexpr int kMvpColumnAge = 6;
inline constexpr int kMvpColumnSource = 7;
inline constexpr int kMvpColumnCount = 8;
// LA CLÉ DE TRI : `(colonne + 1) × kMvpSortKeysPerColumn`, plus un en croissant ;
// zéro, aucun tri.
inline constexpr int kMvpSortKeysPerColumn = 2;
// La table des membres : nom, niveau, état, action.
inline constexpr int kMvpMemberColumnCount = 4;
// Un texte composé d'une ligne : une phrase, un en-tête, un titre.
inline constexpr std::size_t kMvpPrintedBytes = 256;
// Les unités.
inline constexpr std::int64_t kMvpSecondsPerHour = 3600;
inline constexpr std::int64_t kMvpSecondsPerMinute = 60;
inline constexpr std::uint32_t kMvpMsPerMinute = 60000;

// ── LES AIDES PARTAGÉES ─────────────────────────────────────────────────────

// Un texte à la manière de `printf`, pour une ligne.
//
// ⚠ UNE VRAIE FONCTION VARIADIQUE, MARQUÉE COMME `printf` : c'est ce qui fait
// vérifier ses arguments contre le littéral français que `Tr` a reçu.
#if defined(__GNUC__) || defined(__clang__)
[[nodiscard]] __attribute__((format(printf, 1, 2))) std::string Printed(const char* format, ...);
#else
[[nodiscard]] std::string Printed(const char* format, ...);
#endif

// Un texte de la vue, posé tel quel : il peut porter un pourcent.
void Unformatted(std::string_view text);

// L'INFOBULLE D'UN GESTE GRISÉ faute de liaison vérifiée, posée sur l'élément
// qu'on vient de dessiner ; rien si la liaison l'est.
void TellLinkPending(const MvpTrackerView& view);

// Le rang de ce créneau dans la vue, ou nul.
[[nodiscard]] const MvpSlotView* FindSlotView(std::span<const MvpSlotView> slots,
                                              std::uint16_t slot_id);
// L'EMPREINTE d'un rang pour le tri : ce qui, en changeant, refait le tri.
[[nodiscard]] MvpRowKey RowKeyOf(const MvpSlotView& slot);
// L'ORDRE d'une colonne, `column` étant son index de déclaration. Un créneau
// sans fenêtre (« Retour ») ou sans observation (« Âge ») part en fin de liste
// dans les DEUX sens : il n'a pas d'échéance, pas une échéance infinie.
[[nodiscard]] bool RowBefore(const MvpSlotView& a, const MvpSlotView& b, int column,
                             bool ascending);

// Le sprite d'un rang ou d'une ligne, dans sa case figée — réservée même vide —
// et sa demande pour l'image suivante. (`mvp_tracker_table.cpp`)
void DrawRowSprite(MvpTrackerWindowState& state, const MvpTrackerView& view, std::uint16_t mob_id);
// La table des créneaux, son filtre et son menu de nom. (`mvp_tracker_table.cpp`)
void DrawTable(MvpTrackerWindowState& state, const MvpTrackerView& view, const SkinArt& art,
               MvpTrackerAction& action);

// Le bandeau d'alerte ; son clic demande le carnet sur la ligne.
// (`mvp_tracker_overlays.cpp`)
void DrawAlerts(const MvpTrackerView& view, MvpTrackerAction& action);
// Les lignes détachées. (`mvp_tracker_overlays.cpp`)
void DrawPinnedLines(MvpTrackerWindowState& state, const MvpTrackerView& view,
                     MvpTrackerAction& action);
// Le fantôme d'un rang glissé, et son lâcher. (`mvp_tracker_overlays.cpp`)
void FinishRowDrag(MvpTrackerWindowState& state, const MvpTrackerView& view,
                   MvpTrackerAction& action);

}  // namespace ro::ui::mvp_tracker
