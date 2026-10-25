#pragma once

namespace ro::ui {

// LA FENÊTRE `window_id` A-T-ELLE LE FOCUS ? Elle, ou l'une de ses enfants
// (une liste défilante, un surgissant qu'elle a ouvert). `window_id` est
// l'identité qui suit « ### » dans son nom, celle que reçoit la pile d'Échap.
//
// ⚠ C'EST LE FOCUS DE L'IMAGE PRÉCÉDENTE, et c'est ce qu'il faut à qui route
// les touches : elles arrivent avant que l'image ne se dessine, et la fenêtre
// qui avait le focus est celle à qui le joueur parlait. Une fenêtre jamais
// dessinée, ou absente de la dernière image, ne l'a pas.
[[nodiscard]] bool WindowHasFocus(const char* window_id);

}  // namespace ro::ui
