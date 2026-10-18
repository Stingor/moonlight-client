#pragma once

#include "imgui.h"

namespace ro::ui {

// LES NOMS DE FICHIERS DANS LEUR POLICE, le temps d'un bloc : voir
// `MapEditorPanelState::names_font`.
class NamesFont {
 public:
  explicit NamesFont(ImFont* font) : pushed_(font != nullptr) {
    if (pushed_) {
      // Taille nulle : celle du texte courant.
      ImGui::PushFont(font, 0.0F);
    }
  }
  NamesFont(const NamesFont&) = delete;
  NamesFont& operator=(const NamesFont&) = delete;
  ~NamesFont() {
    if (pushed_) {
      ImGui::PopFont();
    }
  }

 private:
  bool pushed_;
};

}  // namespace ro::ui
