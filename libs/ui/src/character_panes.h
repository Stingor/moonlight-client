#pragma once

#include <optional>
#include <span>

#include "ro/ui/character_staff.h"
#include "ro/ui/character_window.h"

// LES VOLETS DE LA FICHE, chacun dans son fichier.
//
// En-tête PRIVÉ : il vit dans `src/` et non dans `include/`, parce que personne
// hors de cette bibliothèque n'a à dessiner un demi-panneau. La coque décide de
// la place, les volets décident du contenu.
namespace ro::ui {

// LE MANNEQUIN. `costume` change les cases montrées — quatre au lieu de dix, et
// la rangée des mains disparaît — mais ni la hauteur de l'avatar ni le reste.
//
// ⚠ L'AVATAR EST EN ENTRÉE ET EN SORTIE : il reçoit l'image que `app` a
// composée, et repart avec la boîte mesurée, la direction que la molette a
// tournée et la pose que la liste a choisie.
//
// `equipment_shown` et `costumes_shown` : ce que le serveur dit des deux
// réglages que la case sous les poses montre — « Montrer mon équipement » sur
// l'onglet Équipement, « Voir les costumes » sur l'onglet Costume.
void DrawCharacterDoll(const CharacterHeaderView& header, std::span<const EquipSlotView> slots,
                       std::span<const CompanionCaseView> companions, CharacterAvatarState& avatar,
                       const SkinImage& avatar_image, const std::optional<bool>& equipment_shown,
                       const std::optional<bool>& costumes_shown, bool costume, bool preview,
                       float pane_width, const SkinArt& art, CharacterWindowAction& action);

// La largeur de la plus large des deux cases sous les poses, libellé compris :
// le volet du mannequin ne descend pas sous elle, et ne change donc pas de
// largeur d'un onglet à l'autre.
[[nodiscard]] float DollSettingWidth(const SkinArt& art);

// LES NOMBRES. Rend la largeur qu'il lui faudrait, mesurée à la police courante
// — c'est la coque qui décide ensuite s'il tient.
[[nodiscard]] float CharacterStatsPaneWidth(const SkinArt& art);

// ⚠ IL REND OÙ SES RANGÉES SE SONT POSÉES : c'est là-dessus que le volet du
// staff aligne ses commandes. Voir `CharacterStatRows`.
void DrawCharacterStats(const CharacterStatsView& stats, const SkinArt& art,
                        CharacterWindowAction& action, CharacterStatRows& rows);

// LE VOLET DU STAFF, le troisième. ⚠ IL NE TESTE AUCUN DROIT : c'est la coque
// qui décide de le dessiner ou non, parce que c'est elle qui compte les crans de
// largeur — et le cran de trop ne doit pas exister sans le volet.
[[nodiscard]] float CharacterStaffPaneWidth(const SkinArt& art);

void DrawCharacterStaff(StaffPaneState& state, const StaffPaneView& view,
                        const CharacterStatRows& rows, const SkinArt& art, StaffAction& action);

// L'ONGLET DES TITRES, pleine largeur. ⚠ IL N'ÉMET RIEN LUI-MÊME : le titre
// choisi repart dans `action.equip_title`, et c'est `app` qui le demande.
void DrawCharacterTitles(std::span<char> filter, const CharacterTitlesView& titles,
                         CharacterWindowAction& action);

}  // namespace ro::ui
