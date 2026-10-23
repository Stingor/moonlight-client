#pragma once

namespace ro::ui {

// LE TEXTE D'UN TROPHÉE, traduit à la lecture. ⚠ `french` EST MARQUÉ PAR
// `TrText` dans le catalogue des trophées de `sim` : c'est là que le contrôle
// des traductions le relève.
[[nodiscard]] const char* TrophyText(const char* french);

}  // namespace ro::ui
