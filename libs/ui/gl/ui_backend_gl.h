#pragma once

#include "ro/core/error.h"
#include "ro/ui/ui_context.h"

// Déclarations anticipées, comme pour `GlContext` : SDL n'apparaît pas dans
// cette interface, seulement dans son implémentation.
struct SDL_Window;
union SDL_Event;

namespace ro::ui {

// LE DOS D'IMGUI : les entrées par SDL3, le dessin par OpenGL 3.
//
// Tout ce qui touche à la plateforme tient ici, et rien d'autre n'y tient. Ce
// que cette frontière achète est concret : `ro_ui` — donc toutes les fenêtres —
// est bâtie et testée sur le runner Linux sous ASan, où SDL n'est même pas
// téléchargé, et une fenêtre s'y vérifie sans carte graphique.
//
// ⚠ CE DOS N'EST PAS DÉDUCTIBLE DE L'ÉDITEUR DE LIENS : vcpkg n'installe qu'UNE
// bibliothèque ImGui, dos compris. C'est `scripts/check_hygiene.py` qui interdit
// à `ro_ui` d'inclure la plateforme — voir `rule_ui_stays_platform_free`.
class UiBackendGl {
 public:
  UiBackendGl(const UiBackendGl&) = delete;
  UiBackendGl& operator=(const UiBackendGl&) = delete;
  UiBackendGl(UiBackendGl&& other) noexcept;
  UiBackendGl& operator=(UiBackendGl&& other) noexcept;
  ~UiBackendGl();

  // Attache le dos au contexte donné.
  //
  // ⚠ LE CONTEXTE DOIT VIVRE PLUS LONGTEMPS QUE LE DOS : ImGui range l'état du
  // dos DANS le contexte, et le détruire d'abord laisserait la fermeture écrire
  // dans de la mémoire libérée.
  //
  // Échoue si l'un des deux dos refuse de s'initialiser. Le jeu se passe alors
  // d'interface plutôt que de ne pas démarrer.
  [[nodiscard]] static core::Result<UiBackendGl> Attach(UiContext& ui, SDL_Window* window);

  // Donne un événement à l'interface. Rend VRAI quand elle l'a pris.
  //
  // ⚠ CE VRAI NE SUFFIT PAS À DÉCIDER : ImGui prend tous les événements de
  // souris et de clavier, y compris ceux qui ne visent aucune fenêtre. Ce qui
  // tranche est `UiContext::gate()`, qui sait ce qu'il y a SOUS le curseur.
  bool ProcessEvent(const SDL_Event& event) const;

  // Ouvre la trame. Le dos pose lui-même la taille et le temps écoulé — il est
  // le seul à connaître le facteur d'échelle de l'écran.
  void BeginFrame(UiContext& ui) const;

  // Peint ce que la trame fermée a composé.
  //
  // ⚠ À APPELER APRÈS `UiContext::EndFrame`, et EN DERNIER dans l'image : rien
  // de ce que l'interface dessine ne doit être masqué par le monde.
  void Draw() const;

 private:
  UiBackendGl() = default;
  void Release() noexcept;

  // Ce qui reste à fermer. Les deux dos s'initialisent l'un après l'autre, et
  // le second peut échouer : sans ces deux témoins, la fermeture fermerait un
  // dos qui n'a jamais été ouvert.
  bool platform_open_ = false;
  bool renderer_open_ = false;
};

}  // namespace ro::ui
