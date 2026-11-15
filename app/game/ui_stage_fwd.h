#pragma once

// L'INTERFACE DU JEU, NOMMÉE SANS ÊTRE LUE.
//
// Un en-tête qui ne fait que passer l'étage, ses fenêtres ou ses réponses — par
// pointeur, par référence, en paramètre — inclut celui-ci, et non
// `game/ui_stage.h`, `game/ui_windows.h` ou `game/ui_answers.h` : ses lecteurs
// ne se recompilent pas quand une fenêtre change. `UiStage::Windows` ne se
// nomme pas d'une classe seulement déclarée : on écrit `UiWindows`, qui est le
// même type.

namespace moonclient {

class UiStage;
struct UiWindows;
struct UiAnswers;
struct UiNpcTalk;
struct UiCharacterSheetView;
struct UiChatWindowAsk;
struct UiLobbySeatsAnswers;
struct UiLobbySeatsFrame;

}  // namespace moonclient
