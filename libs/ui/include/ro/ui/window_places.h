#pragma once

#include <cstdint>
#include <map>
#include <string>
#include <unordered_map>

struct ImGuiWindow;

namespace ro::ui {

// ── LA PLACE VOULUE D'UNE FENÊTRE, ET CELLE QU'ON LUI DONNE ────────────────
//
// 🔴 DEUX PLACES ET NON UNE. Celle que le joueur a CHOISIE ne s'écrit que sous
// sa main ; celle qu'on AFFICHE s'en déduit à chaque image, pour l'écran du
// moment, puis se borne dans l'écran. Le bornage ne touche jamais la première :
// passer de 1920 à 1024 rabat les fenêtres, revenir à 1920 les rend où elles
// étaient.
//
// ⚠ LA PLACE S'ANCRE AU BORD LE PLUS PROCHE, sur chaque axe : une fenêtre
// posée dans le tiers droit de l'écran retient sa distance au bord DROIT, et le
// suit quand l'écran change de largeur. Celle du tiers central retient l'écart
// de son centre au centre de l'écran.
//
// ⚠ EN UNITÉS D'INTERFACE, pas en pixels : décalages et tailles sont divisés
// par l'échelle de l'interface. Changer d'échelle n'écrit donc rien ici — la
// place affichée se recalcule, et revenir à l'échelle d'avant rend la place
// d'avant, au pixel près.
//
// ⚠ AUCUN TYPE D'IMGUI DANS CET EN-TÊTE : `app` le lit, et n'inclut pas ImGui.

enum class AnchorSide : std::uint8_t {
  kStart,   // le bord gauche, ou le haut
  kCenter,  // le milieu de l'écran
  kEnd,     // le bord droit, ou le bas
};

struct WindowPlace {
  AnchorSide across = AnchorSide::kStart;
  AnchorSide down = AnchorSide::kStart;
  // Distance au bord d'ancrage, vers l'intérieur de l'écran ; ou, au centre,
  // écart signé du centre de la fenêtre au centre de l'écran.
  float offset_x = 0.0F;
  float offset_y = 0.0F;
  float width = 0.0F;
  float height = 0.0F;
  bool collapsed = false;

  [[nodiscard]] friend bool operator==(const WindowPlace&, const WindowPlace&) = default;
};

// Un rectangle d'écran, en pixels.
struct ScreenRect {
  float x_px = 0.0F;
  float y_px = 0.0F;
  float width_px = 0.0F;
  float height_px = 0.0F;
};

// La place voulue que dit ce rectangle, sur cet écran et à cette échelle.
[[nodiscard]] WindowPlace PlaceFromRect(const ScreenRect& rect, float screen_width_px,
                                        float screen_height_px, float interface_scale);

// Le rectangle que cette place donne sur cet écran, AVANT tout bornage.
[[nodiscard]] ScreenRect RectFromPlace(const WindowPlace& place, float screen_width_px,
                                       float screen_height_px, float interface_scale);

// UNE DISPOSITION ENTIÈRE : la place de chaque fenêtre, et les interrupteurs
// que le client retient — fenêtre ouverte, fenêtre épinglée.
//
// ⚠ LA CLÉ D'UNE PLACE EST CE QU'IMGUI HACHE du nom de la fenêtre : la partie
// après « ### » quand le nom en porte, le nom entier sinon. Un titre traduit
// autrement garde donc sa place.
struct LayoutProfile {
  std::map<std::string, WindowPlace> places;
  std::map<std::string, bool> toggles;

  [[nodiscard]] friend bool operator==(const LayoutProfile&, const LayoutProfile&) = default;
};

// ── LA PASSE QUI POSE LES FENÊTRES ─────────────────────────────────────────
//
// Une fois par image, entre `NewFrame` et le premier `Begin` :
//   • une fenêtre que QUELQU'UN D'AUTRE a déplacée ou redimensionnée depuis
//     l'image précédente — le joueur, ou l'appelant par `SetNextWindowPos` —
//     donne sa nouvelle place voulue, prise une fois bornée dans l'écran ;
//   • toutes les autres reçoivent la place que leur place voulue donne sur cet
//     écran, bornée dans l'écran et à `kMaxScreenFraction` de sa taille.
//
// ⚠ UNE FENÊTRE SANS PLACE VOULUE, vue pour la première fois, adopte celle
// qu'elle a : celle qu'ImGui a relue de son fichier, ou son défaut. C'est ce
// qui fait de la disposition d'avant le premier profil.
//
// ⚠ UNE FENÊTRE QUI NAÎT AILLEURS QUE LÀ OÙ ON L'A PRÉPARÉE a été posée par
// son appelant à l'apparition — sous le curseur, au centre — : sa place gagne.
//
// ⚠ LES FENÊTRES QU'ON NE RETIENT PAS — celles qui demandent à ne pas être
// enregistrées — n'ont pas de place voulue : elles se bornent là où elles sont.
class WindowPlacer {
 public:
  // La passe. `interface_scale` est celle de l'image.
  void Place(float interface_scale);

  [[nodiscard]] const std::map<std::string, WindowPlace>& places() const noexcept {
    return places_;
  }

  // POSE UNE DISPOSITION, à chaud, À LA PLACE de la présente. Les fenêtres
  // qu'elle nomme y vont dès la passe suivante, ouvertes ou non ; celles
  // qu'elle ne nomme pas reprennent la place que leur code leur donne à la
  // première ouverture.
  void Use(const std::map<std::string, WindowPlace>& places);

  // Grandit chaque fois qu'une place voulue change. Un enregistrement compare ce
  // nombre, et n'écrit que quand il a bougé.
  [[nodiscard]] std::uint64_t revision() const noexcept { return revision_; }

 private:
  // Ce que la passe a posé, pour reconnaître à l'image suivante un geste qui
  // n'est pas le sien.
  struct Posed {
    float x_px = 0.0F;
    float y_px = 0.0F;
    float width_px = 0.0F;
    float height_px = 0.0F;
    bool collapsed = false;
  };

  // Recopie dans les réglages d'ImGui la place des fenêtres pas encore nées :
  // elles naîtront à la bonne place, sans une image ailleurs.
  void PrepareUnborn(float interface_scale);
  // La fenêtre n'est plus là où on l'a posée : quelqu'un d'autre l'a bougée.
  // ⚠ `ImGuiWindow` RESTE DÉCLARÉ, NON INCLUS : voir la règle en tête.
  [[nodiscard]] static bool Differs(const Posed& posed, const ImGuiWindow& window);

  std::map<std::string, WindowPlace> places_;
  // Par identifiant ImGui de fenêtre.
  std::unordered_map<std::uint32_t, Posed> posed_;
  // Ce que `PrepareUnborn` a écrit pour chaque fenêtre à naître, par
  // identifiant ImGui : son écart à la naissance dit le geste de l'appelant.
  std::unordered_map<std::uint32_t, Posed> prepared_;
  std::uint64_t revision_ = 0;
  // L'écran et l'échelle de la dernière préparation des fenêtres à naître.
  float prepared_width_px_ = 0.0F;
  float prepared_height_px_ = 0.0F;
  float prepared_scale_ = 0.0F;
  bool unborn_stale_ = true;
};

}  // namespace ro::ui
