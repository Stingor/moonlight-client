#pragma once

// Les pièces que les deux fichiers de la fiche de monstre se partagent : sa
// palette, ses libellés, et les onglets du second fichier.

#include <cstddef>
#include <cstdint>
#include <string>

#include <imgui.h>

#include "ro/ui/monster_sheet_window.h"
#include "ro/ui/skin_art.h"

namespace ro::ui::monster_sheet {

// ── LA PALETTE ──────────────────────────────────────────────────────────────
//
// 🔴 LE CORPS EST CLAIR : les teintes vives et pâles y sont illisibles, et le
// texte grisé d'ImGui — réglé pour un thème sombre — s'y efface presque. Ces
// valeurs sont assez sombres pour se lire sur le papier.
// Le libellé d'un couple « libellé : valeur », et toute phrase secondaire.
inline constexpr ImU32 kLabelColor = IM_COL32(89, 89, 107, 255);
// Le nom du monstre et les titres de section.
inline constexpr ImU32 kTitleColor = IM_COL32(115, 61, 5, 255);
// Le mini-boss, la récompense de MVP, l'homonymie.
inline constexpr ImU32 kAmberColor = IM_COL32(153, 102, 13, 255);
inline constexpr ImU32 kGreenColor = IM_COL32(26, 128, 38, 255);
inline constexpr ImU32 kRedColor = IM_COL32(153, 31, 31, 255);
inline constexpr ImU32 kBlueColor = IM_COL32(38, 64, 153, 255);
// Une résistance faible, entre l'immunité et la normale.
inline constexpr ImU32 kLowResistColor = IM_COL32(51, 115, 38, 255);
// LA COULEUR DES LIENS DE L'HABILLAGE, sans l'opacité de la fenêtre :
// `Colored` la pose en couleur de texte, et ImGui l'applique en écrivant.
[[nodiscard]] ImU32 LinkColor();

// Les deux rangs de boss que le serveur envoie.
inline constexpr std::uint8_t kBossRankMini = 1;
inline constexpr std::uint8_t kBossRankMvp = 2;

// « Mini-boss » ou « MVP », traduit, ou nul pour un monstre ordinaire.
[[nodiscard]] const char* BossBadge(std::uint8_t rank);
// Le badge sur la ligne en cours, dans sa couleur : rouge pour un MVP.
void DrawBossBadge(std::uint8_t rank);

// Le relevé de Sense qui diffère de la base.
inline constexpr ImU32 kSenseNoteColor = IM_COL32(179, 140, 51, 255);

// Un texte à la manière de `printf`, pour une ligne.
//
// ⚠ UNE VRAIE FONCTION VARIADIQUE, MARQUÉE COMME `printf` : c'est ce qui fait
// vérifier ses arguments contre le littéral français que `Tr` a reçu. Un
// gabarit de fonction perdrait ce contrôle.
#if defined(__GNUC__) || defined(__clang__)
[[nodiscard]] __attribute__((format(printf, 1, 2))) std::string Printed(const char* format, ...);
#else
[[nodiscard]] std::string Printed(const char* format, ...);
#endif

// Un libellé, dans la couleur des libellés.
void Label(const char* text);
// Une PHRASE, qui s'enroule au bord de la fenêtre au lieu d'y être coupée.
void LabelWrapped(const char* text);
void Colored(ImU32 color, const char* text);

// Le « (?) » de cette fiche : sur la ligne du libellé qu'il commente, dans la
// couleur des libellés, et sa bulle au survol.
void Help(const char* text);

// Le soulignement et la main d'un lien survolé.
void DecorateLink(bool hovered);

// Les onglets du second fichier.
//
// ⚠ LES TROIS TABLES QUI DÉFILENT REÇOIVENT L'HABILLAGE : chacune défile dans sa
// propre fenêtre, dont la barre d'ImGui est transparente. La barre du client
// s'y peint avant la fermeture de la table, sans quoi rien ne dit qu'une
// liste en cache la suite.
void DrawResistTab(const MonsterSheetView& view);
void DrawDropsTab(MonsterSheetWindowState& state, const MonsterSheetView& view, const SkinArt& art,
                  MonsterSheetAnswer& answer);
void DrawSpawnsTab(MonsterSheetWindowState& state, const MonsterSheetView& view, const SkinArt& art,
                   MonsterSheetAnswer& answer);
void DrawSkillsTab(MonsterSheetWindowState& state, const MonsterSheetView& view, const SkinArt& art,
                   MonsterSheetAnswer& answer);

}  // namespace ro::ui::monster_sheet
