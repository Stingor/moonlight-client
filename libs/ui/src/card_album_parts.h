#pragma once

// LES PIÈCES COMMUNES DE L'ALBUM DE CARTES : la fenêtre (`card_album_window.cpp`)
// et le classeur (`card_album_book.cpp`) les partagent. Interne à `ui`.

#include <array>
#include <cstdint>
#include <string_view>

#include <imgui.h>

#include "ro/ui/card_album_window.h"
#include "ro/ui/skin_art.h"

namespace ro::ui::card_album {

[[nodiscard]] inline float Scale() { return ImGui::GetStyle().FontScaleMain; }
[[nodiscard]] inline float Px(float pixels) { return pixels * Scale(); }

// ── Le classeur, en pixels à l'échelle 100 % ────────────────────────────────
inline constexpr float kSpinePixels = 18.0F;
inline constexpr float kPagePadPixels = 12.0F;
inline constexpr float kPocketGapPixels = 10.0F;
// La pochette dépasse l'illustration de ce pas.
inline constexpr float kPocketMarginPixels = 3.0F;
inline constexpr float kPocketRoundingPixels = 5.0F;
inline constexpr float kArtRoundingPixels = 4.0F;
inline constexpr float kBarPixels = 9.0F;
// La police d'un nom ne descend pas plus bas ; au-delà, on coupe.
inline constexpr float kNameLeastScale = 0.70F;
inline constexpr float kNameIconGapPixels = 3.0F;
// La taille NATIVE d'une icône d'objet : elle passe texel pour pixel.
inline constexpr float kNameIconPixels = 24.0F;
// Le fondu d'une page tournée, en secondes : c'est ce qui fait « tourner »
// plutôt que « clignoter ».
inline constexpr double kFlipFadeSeconds = 0.14;
inline constexpr float kFirstWidthPixels = 960.0F;
inline constexpr float kFirstHeightPixels = 700.0F;
inline constexpr float kLeastWidthPixels = 720.0F;
inline constexpr float kLeastHeightPixels = 480.0F;
inline constexpr int kLeastColumns = 2;
inline constexpr int kLeastRows = 1;

// ── Les teintes propres au classeur ─────────────────────────────────────────
// Une pochette sombre sur un corps clair est le contraire de ce que la palette
// de l'habillage adresse : elles vivent donc ici.
inline constexpr ImU32 kPocketColor = IM_COL32(58, 50, 44, 255);
inline constexpr ImU32 kPocketGlint = IM_COL32(255, 255, 255, 28);
inline constexpr ImU32 kPocketShadow = IM_COL32(0, 0, 0, 55);
inline constexpr ImU32 kFoldShadow = IM_COL32(0, 0, 0, 40);
inline constexpr ImU32 kClear = IM_COL32(0, 0, 0, 0);
// Le liseré de nature : noir ordinaire, bleu mini-boss, orange MVP.
inline constexpr ImU32 kRimOrdinary = IM_COL32(0, 0, 0, 180);
inline constexpr ImU32 kRimMiniBoss = IM_COL32(92, 158, 242, 255);
inline constexpr ImU32 kRimMvp = IM_COL32(240, 150, 48, 255);
inline constexpr float kRimPixels = 2.0F;
inline constexpr ImU32 kArtMissing = IM_COL32(96, 88, 80, 255);
inline constexpr ImU32 kPageEdge = IM_COL32(0, 0, 0, 45);
inline constexpr ImU32 kSpineDark = IM_COL32(0, 0, 0, 80);
inline constexpr ImU32 kBadgeStocked = IM_COL32(222, 178, 54, 255);
inline constexpr ImU32 kBadgeEmpty = IM_COL32(150, 146, 138, 255);
inline constexpr ImU32 kSealFill = IM_COL32(146, 44, 44, 230);
inline constexpr ImU32 kSealText = IM_COL32(250, 240, 235, 255);
inline constexpr ImU32 kHighlight = IM_COL32(40, 150, 60, 255);
inline constexpr ImU32 kDropWelcome = IM_COL32(40, 150, 60, 255);
inline constexpr ImU32 kBarTrack = IM_COL32(0, 0, 0, 30);
inline constexpr ImU32 kBarFill = IM_COL32(222, 178, 54, 255);
inline constexpr ImU32 kBarEdge = IM_COL32(0, 0, 0, 60);
// Le survol ÉCLAIRCIT, il ne colore pas.
inline constexpr ImU32 kHoverTint = IM_COL32(255, 255, 255, 45);
// Une icône d'attente sous une pochette scellée.
inline constexpr ImU32 kSealedIconTint = IM_COL32(110, 110, 110, 255);
// Les macarons de provenance : à quinze pixels, c'est la COULEUR qui se lit.
inline constexpr std::array<ImU32, kCardAlbumSourceCount> kSourceFill = {
    IM_COL32(196, 138, 58, 255),   // Old Card Album
    IM_COL32(124, 100, 196, 255),  // Mystical Card Album
};
inline constexpr ImU32 kSourceText = IM_COL32(255, 248, 238, 255);
inline constexpr ImU32 kSourceEdge = IM_COL32(0, 0, 0, 100);
// Les liserés du staff : vifs et larges, ils disent « travail en attente ».
inline constexpr ImU32 kSourcePendingAdd = IM_COL32(80, 210, 100, 255);
inline constexpr ImU32 kSourcePendingRemove = IM_COL32(226, 80, 74, 255);
inline constexpr float kSourceChipPixels = 15.0F;
inline constexpr float kSourceChipGapPixels = 2.0F;

// Les textes : le gris d'un libellé, et les trois tons d'un compte rendu.
inline constexpr ImVec4 kLabelColor(0.35F, 0.35F, 0.42F, 1.0F);
inline constexpr ImVec4 kDoneColor(0.10F, 0.50F, 0.15F, 1.0F);
inline constexpr ImVec4 kRefusalColor(0.60F, 0.12F, 0.12F, 1.0F);
inline constexpr ImVec4 kWarningColor(0.55F, 0.33F, 0.08F, 1.0F);
inline constexpr ImU32 kBlack = IM_COL32(0, 0, 0, 255);

// LES INTERCALAIRES. Les trois emplacements de tête tiennent dans UN : trois
// onglets pour une famille que les joueurs pensent comme une seule encombraient
// la barre sans rien classer de plus. Les noms sont des termes du jeu, en
// anglais ; seul « Tous » se traduit.
struct SlotFilter {
  const char* label;
  std::uint8_t family;
};
inline constexpr std::array<SlotFilter, 8> kSlotFilters = {{
    {"Tous", 0},
    {"Armor", kCardFamilyArmor},
    {"Weapon", kCardFamilyWeapon},
    {"Shield", kCardFamilyShield},
    {"Garment", kCardFamilyGarment},
    {"Shoes", kCardFamilyShoes},
    {"Accessory", kCardFamilyAccessory},
    {"Head", kCardFamilyHead},
}};

// L'ordre des pochettes.
inline constexpr int kSortCatalogue = 0;
inline constexpr int kSortName = 1;
inline constexpr int kSortReserve = 2;
inline constexpr int kSortSlot = 3;
inline constexpr std::array<const char*, 4> kSortLabels = {
    core::TrText("Catalogue"), core::TrText("Nom"), core::TrText("Réserve"),
    core::TrText("Emplacement")};

// Ce que les pages montrent : tout, les ouvertes, les scellées — puis par
// PROVENANCE. ⚠ LES DEUX ENTRÉES D'ALBUM SUIVENT L'ORDRE DES SOURCES de la vue,
// et le filtre s'y indexe. Un second combo aurait laissé croire qu'on peut
// croiser « scellées » et « Old Card Album », ce que ce filtre ne fait pas.
inline constexpr int kShowAll = 0;
inline constexpr int kShowUnlocked = 1;
inline constexpr int kShowSealed = 2;
inline constexpr int kShowFirstSource = 3;
inline constexpr int kShowNoSource = kShowFirstSource + static_cast<int>(kCardAlbumSourceCount);
inline constexpr std::array<const char*, 6> kShowLabels = {core::TrText("Toutes"),
                                                           core::TrText("Débloquées"),
                                                           core::TrText("Scellées"),
                                                           core::TrText("Old Card Album"),
                                                           core::TrText("Mystical Card Album"),
                                                           core::TrText("Hors des albums")};

// LA NATURE du monstre qui lâche la carte, en boutons à droite des
// intercalaires. Elle se COMBINE avec l'intercalaire et compte comme lui dans
// la complétion : c'est une famille de la collection, pas une vue.
struct NatureFilter {
  const char* label;
  std::uint8_t rank;
};
inline constexpr std::array<NatureFilter, 3> kNatureFilters = {{
    {"Normal", 0},
    {"Boss", 1},
    {"MVP", 2},
}};
inline constexpr std::uint8_t kNatureAll = 0x07;
inline constexpr float kNatureGapPixels = 3.0F;
inline constexpr float kNatureLeastGapPixels = 8.0F;
inline constexpr float kNatureInsetPixels = 1.0F;

// LA GÉOMÉTRIE D'UNE DOUBLE PAGE, recalculée à chaque image. La POCHETTE a une
// taille fixe (l'illustration y est dessinée texel pour pixel) ; ce sont les
// colonnes et les rangées qui suivent la fenêtre.
struct BookLayout {
  float page_width = 0.0F;
  float page_height = 0.0F;
  float art_width = 0.0F;
  float art_height = 0.0F;
  float cell_width = 0.0F;
  float cell_height = 0.0F;
  float name_height = 0.0F;
  int columns = kLeastColumns;
  int rows = kLeastRows;
  int per_page = 2;
  int per_spread = 4;
};

// La taille d'une illustration en pixels ÉCRAN, entière : c'est aussi la taille
// de sa vignette en texels.
void ArtSize(float* width, float* height);

// Une couleur, son alpha multiplié.
[[nodiscard]] ImU32 WithAlpha(ImU32 color, float alpha);

// La pochette passe-t-elle le filtre de nature ?
[[nodiscard]] bool PassesNature(const CardAlbumWindowState& state, const CardAlbumView& view,
                                const CardAlbumPocketView& pocket);

// Tourne les pages de `delta` doubles pages, borné.
void Flip(CardAlbumWindowState& state, int delta);

// Offre une carte du sac à l'album : dépôt si sa pochette est ouverte, sacrifice
// sinon, refus si ce n'est pas une carte du catalogue.
void OfferFromHand(CardAlbumWindowState& state, const CardAlbumView& view, std::uint16_t slot,
                   CardAlbumWindowAction& action);

// Arme une action et, si la pile fait plus d'un exemplaire et qu'on le demande,
// l'invite « combien ? ». Une pile de 1 part directement.
void ArmMove(CardAlbumWindowState& state, CardAlbumVerb verb, std::uint16_t slot,
             std::uint32_t card_id, int most, bool ask_quantity, CardAlbumWindowAction& action);

// LE point d'entrée du sacrifice : la confirmation, ou l'envoi direct si le
// joueur a choisi de s'en passer. Aucun autre chemin ne sacrifie.
void AskSacrifice(CardAlbumWindowState& state, std::uint16_t slot, std::uint32_t card_id, int rest,
                  CardAlbumWindowAction& action);

// La pochette de cette carte, ou nul.
[[nodiscard]] const CardAlbumPocketView* PocketOf(const CardAlbumView& view, std::uint32_t card_id);

// Les trois dialogues de la fenêtre : sacrifice, poids de tirage, quantité.
void DrawDialogs(CardAlbumWindowState& state, const CardAlbumView& view, const SkinArt& art,
                 CardAlbumWindowAction& action);

// Le classeur : les deux pages, la reliure, et tout ce qu'une pochette offre.
void DrawBook(CardAlbumWindowState& state, const CardAlbumView& view, const SkinArt& art,
              float height, CardAlbumWindowAction& action);

}  // namespace ro::ui::card_album
