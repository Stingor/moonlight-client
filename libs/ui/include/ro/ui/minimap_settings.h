#pragma once

#include <cstdint>

namespace ro::ui {

// ── CE QUE LE JOUEUR A RÉGLÉ DE LA MINIMAP ──────────────────────────────────
//
// Des scalaires nus, pour que l'appelant les écrive sans rien savoir de la
// fenêtre. ⚠ AUCUN TYPE D'IMGUI DANS CET EN-TÊTE : le profil de disposition le
// porte, et `app` lit ce profil sans inclure ImGui.

// Bornes et pas du zoom, ceux des boutons + et − du radar du jeu : le joueur
// retrouve l'amplitude qu'il connaît.
inline constexpr float kMinimapZoomLeast = 1.0F;
inline constexpr float kMinimapZoomMost = 4.0F;
inline constexpr float kMinimapZoomStep = 4.0F / 3.0F;

// La position d'une minimap que le joueur n'a jamais glissée.
inline constexpr int kMinimapUnplaced = -1;

// La teinte qui laisse l'art de la flèche tel quel : blanc opaque, ses quatre
// canaux à 255 — la même valeur dans tout ordre d'octets.
inline constexpr std::uint32_t kMinimapArrowUntinted = 0xFFFF'FFFFU;

struct MinimapSettings {
  // La position de la fenêtre. Elle n'a pas de réglage numérique : on la glisse,
  // et le glissement se retient ici.
  //
  // ⚠ `kMinimapUnplaced` TANT QUE LE JOUEUR NE L'A PAS GLISSÉE : elle se tient alors
  // au coin haut-droit, calculé à chaque image contre l'écran du moment — le coin
  // haut-gauche est celui des icônes de menu. Un coin écrit dans le fichier
  // serait celui d'UNE taille d'écran. Une place qui ne tient plus dans l'écran
  // s'y montre ramenée, sans être réécrite.
  int pos_x = kMinimapUnplaced;
  int pos_y = kMinimapUnplaced;
  // Le côté de la ZONE DE CARTE, en pixels — pas la taille de la fenêtre.
  // Réglé à la poignée, jamais par un curseur. Un écran trop petit le montre
  // rabattu, sans le réécrire.
  int side_px = 220;
  // La souris traverse la fenêtre ; elle cesse alors aussi d'être déplaçable et
  // redimensionnable — trois faces du même choix.
  bool click_through = false;
  // 1 = la carte entière ; au-delà on ne voit que 1/zoom, centré sur le joueur.
  float zoom = kMinimapZoomLeast;
  int backdrop_percent = 70;
  int map_percent = 100;
  // Filtrage LINÉAIRE de l'image de carte, qui est très agrandie.
  bool smooth = true;
  // La demi-étendue de la flèche du joueur, en pixels, et sa teinte, une
  // couleur d'ImGui — blanc laisse l'art tel quel.
  int arrow_half_px = 9;
  std::uint32_t arrow_tint = kMinimapArrowUntinted;
  bool show_coords = true;
  // Le nom LISIBLE du lieu, replié sur la largeur de la carte.
  bool show_map_name = true;
  bool show_party = true;
  bool show_guild = true;
  bool show_quests = true;
  bool show_town = true;
  // Les sorties de la carte, à part des PNJ : on ne cherche pas l'une comme
  // on cherche l'autre.
  bool show_portals = true;
  bool show_viewpoints = true;
  bool show_boss = true;
  bool show_route = true;

  [[nodiscard]] friend bool operator==(const MinimapSettings&, const MinimapSettings&) = default;
};

// LA PRÉSENTATION DE LA MINIMAP, telle qu'un profil de disposition la range :
// ses réglages, et si elle est affichée — un raccourci la masque et la rend.
//
// ⚠ PAS SES DONNÉES : les marqueurs du joueur sont ceux de la carte, communs à
// tous les profils.
struct MinimapLayout {
  MinimapSettings settings;
  bool shown = true;

  [[nodiscard]] friend bool operator==(const MinimapLayout&, const MinimapLayout&) = default;
};

}  // namespace ro::ui
