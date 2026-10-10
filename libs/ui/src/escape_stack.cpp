#include "ro/ui/escape_stack.h"

#include <imgui_internal.h>

namespace ro::ui {

void EscapeStack::Register(const char* window_name, bool* open) {
  Add(window_name, open, /*reading=*/false);
}

void EscapeStack::RegisterReading(const char* window_name, bool* open) {
  Add(window_name, open, /*reading=*/true);
}

void EscapeStack::Add(const char* window_name, bool* open, bool reading) {
  // ⚠ CONSOMMÉ SANS CONDITION, AVANT TOUT AUTRE TEST. L'étage d'interface
  // déclare toutes ses fenêtres ; si le drapeau ne partait pas ici, il fuirait
  // sur la suivante dès qu'une déclaration sort par un des refus ci-dessous.
  const bool skip = skip_next_;
  skip_next_ = false;
  if (skip || open == nullptr || !*open || window_name == nullptr) {
    return;
  }
  ImGuiWindow* window = ImGui::FindWindowByName(window_name);
  // Jamais dessinée : elle n'a pas d'ordre de focus, donc rien ne dit si elle
  // est au-dessus. Elle en aura un à l'image d'après.
  if (window == nullptr || !window->WasActive) {
    return;
  }
  open_windows_.push_back(Closable{.window = window, .reading = reading, .open = open});
}

void EscapeStack::Process() {
  has_work_ = !open_windows_.empty();
  const bool suppressed = suppressed_;
  suppressed_ = false;

  if (!suppressed && has_work_ && ImGui::IsKeyPressed(ImGuiKey_Escape, /*repeat=*/false)) {
    // UNE FENÊTRE QU'ON LIT D'ABORD, puis LA PLUS AU-DESSUS : le plus grand
    // ordre de focus est la plus récemment passée devant, donc celle que le
    // joueur regarde.
    const Closable* topmost = nullptr;
    for (const Closable& one : open_windows_) {
      if (topmost == nullptr || one.reading > topmost->reading ||
          (one.reading == topmost->reading &&
           one.window->FocusOrder > topmost->window->FocusOrder)) {
        topmost = &one;
      }
    }
    *topmost->open = false;
  }
  open_windows_.clear();
}

}  // namespace ro::ui
