#pragma once

// LA LISTE DES COMMANDES, BRANCHÉE SUR LE CHAMP DE LA PHRASE.
//
// En-tête PRIVÉ de `libs/ui/src`. Les règles — quelle touche appartient à la
// liste, quand Entrée complète — vivent dans `ro/ui/chat_command_hints.h` ; ici
// seulement la façon de les prendre à ImGui avant que le champ ne les voie.

#include <span>

#include <imgui.h>

#include "ro/ui/chat_command_hints.h"
#include "ro/ui/chat_input.h"
#include "ro/ui/skin_art.h"

namespace ro::ui {

// AVANT LE CHAMP : la liste de l'image, et les touches qu'on prend au champ.
//
// 🔴 ÉCHAP ET ENTRÉE SE PRENNENT AVANT LE WIDGET, ET SE VERROUILLENT pour
// l'image : le champ les lit par `Shortcut`, que le verrou fait taire — sans
// lui, Échap fermerait la liste ET quitterait le champ, Entrée compléterait ET
// enverrait. Une complétion d'Entrée s'écrit ici, et le champ `field` la
// recharge dans la foulée.
//
// ⚠ LA LISTE NE S'OUVRE QU'AU CHAMP ACTIF à l'image précédente : flottant
// au-dessus du fil pendant qu'on joue, elle cacherait ce qu'on lit.
[[nodiscard]] ChatCommandListView TakeCommandKeysBeforeField(ChatInputState& state,
                                                             std::span<const ChatCommandHint> hints,
                                                             ImGuiID field);

// Les drapeaux que la liste ajoute au champ.
//
// 🔴 TAB N'APPARTIENT AU CHAMP QUE LISTE OUVERTE. `CallbackCompletion` lui
// donne la touche, et la navigation d'ImGui ne s'en sert plus pour passer au
// widget suivant. Liste fermée, Tab garde son rôle ordinaire.
[[nodiscard]] ImGuiInputTextFlags CommandFieldFlags(const ChatCommandListView& view);

// Ce que le rappel du champ voit de la liste.
struct CommandFieldHooks {
  ChatInputState* state = nullptr;
  const ChatCommandListView* view = nullptr;
  std::span<const ChatCommandHint> hints;
};

// Dans le rappel du champ : Tab complète, en réécrivant le tampon d'ImGui.
// Rend vrai quand la touche était à la liste.
bool CompleteOnTab(const CommandFieldHooks& hooks, ImGuiInputTextCallbackData* data);

// ↑ ou ↓ : la liste ouverte les prend, et choisit. Rend vrai quand la touche
// était à la liste — l'historique ne doit alors pas la voir.
[[nodiscard]] bool NavigateCommands(const CommandFieldHooks& hooks, bool up);

// APRÈS LE CHAMP : la liste, dessinée sur l'état de la ligne à la fin de
// l'image, au-dessus de `field_corner`. Un clic complète, et rend le clavier
// au champ.
void DrawCommandsOverField(ChatInputState& state, std::span<const ChatCommandHint> hints,
                           bool field_active, ImVec2 field_corner, float width, const SkinArt& art);

}  // namespace ro::ui
