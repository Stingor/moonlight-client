#pragma once

#include <cstddef>
#include <optional>
#include <span>
#include <string_view>

#include <imgui.h>

#include "ro/ui/skin_art.h"

namespace ro::ui {

// LA RÉPARATION — « Repairable items » —, la fenêtre où Repair Weapon fait
// choisir la pièce à réparer.
//
// Celle du client : 280 sur 150, une liste des noms sur fond gris clair, « OK »
// et « Annuler » en bas à droite.
//
// ⚠ SON OUVERTURE N'APPARTIENT PAS AU JOUEUR : c'est la liste du serveur qui
// l'ouvre. « OK » sans ligne choisie ne fait rien ; « Annuler » et la croix
// envoient le refus.

inline constexpr const char* kRepairWindowId = "item_repair";

// ── LA GÉOMÉTRIE DU CLIENT, en pixels d'art, depuis le coin de la fenêtre ───
inline constexpr float kRepairWindowWidth = 280.0F;
inline constexpr float kRepairWindowHeight = 150.0F;
// La liste : à (12, 22), vingt-quatre pixels moins large que la fenêtre et
// cinquante-cinq moins haute.
inline constexpr ImVec2 kRepairListOrigin{12.0F, 22.0F};
inline constexpr float kRepairListWidthMargin = 24.0F;
inline constexpr float kRepairListHeightMargin = 55.0F;
inline constexpr ImU32 kRepairListColor = IM_COL32(240, 240, 240, 255);
// Les deux boutons, de la taille des bitmaps du client, comptés depuis le coin
// bas-droit : « OK » à 91 pixels du bord, « Annuler » à 46, tous deux à 24 du
// bas. Le mot est dans l'image : un libellé qui s'élargirait déborderait du
// cadre.
inline constexpr ImVec2 kRepairButtonSize{42.0F, 20.0F};
inline constexpr float kRepairOkFromRight = 91.0F;
inline constexpr float kRepairCancelFromRight = 46.0F;
inline constexpr float kRepairButtonsFromBottom = 24.0F;

// LA HAUTEUR D'UNE LIGNE, celle d'une icône d'objet.
inline constexpr float kRepairRowHeight = 24.0F;
// La quantité d'un matériau qui manque se lit en rouge.
inline constexpr ImU32 kRepairMissingColor = IM_COL32(200, 0, 0, 255);

// UNE PIÈCE DE LA LISTE : son nom composé — raffinement et cartes compris —, son
// icône, et le matériau que la réparation consommera, avec ce qu'on en porte.
struct RepairRowView {
  std::string_view name;
  SkinImage icon;
  // Vide quand la pièce n'en demande aucun que l'on sache nommer.
  std::string_view material;
  SkinImage material_icon;
  int material_owned = 0;
};

// CE QUE LA FENÊTRE MONTRE : les pièces, dans l'ordre du serveur.
struct RepairView {
  std::span<const RepairRowView> rows;
};

struct RepairWindowState {
  // ⚠ CE N'EST PAS LE JOUEUR QUI LE POSE : il recopie la liste du serveur.
  bool open = false;
  bool was_open = false;
  std::optional<std::size_t> selected;
};

struct RepairWindowAction {
  // Le joueur a tranché : une ligne, ou rien pour le refus. Faux tant qu'il
  // n'a rien décidé.
  bool decided = false;
  std::optional<std::size_t> chosen;
};

[[nodiscard]] RepairWindowAction DrawRepairWindow(RepairWindowState& state, const RepairView& view,
                                                  const SkinArt& art);

}  // namespace ro::ui
