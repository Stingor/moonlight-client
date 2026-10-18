// LA SECTION « SPRITES » DU PANNEAU DE L'ÉDITEUR DE CARTE : la saisie d'un
// sprite de décor par son nom ou son numéro, la liste, et le sprite désigné —
// sa place, son action et son animation, son orientation, son roulis, son
// échelle, son opacité, sa raison.

#include "ro/ui/map_editor_sprites_panel.h"

#include <algorithm>
#include <array>
#include <cstring>
#include <format>
#include <optional>
#include <span>
#include <string>
#include <string_view>

#include "ro/core/i18n.h"
#include "ro/ui/map_editor_effects_panel.h"
#include "ro/ui/map_editor_row_menu.h"
#include "ro/ui/skin_widgets.h"
#include "ro/ui/triple_clipboard.h"
#include "ro/ui/wheel_drag.h"

#include "imgui.h"

namespace ro::ui {
namespace {

// LES RANGS D'ANIMATION D'UNE CRÉATURE, dans l'ordre de son .act.
constexpr std::array<const char*, 5> kCreatureActions = {
    core::TrText("Attente"), core::TrText("Marche"), core::TrText("Attaque"),
    core::TrText("Blessure"), core::TrText("Mort")};
// LES HUIT DIRECTIONS DU .act, sous une caméra qui n'a pas pivoté.
constexpr std::array<const char*, 8> kDirections = {
    core::TrText("De face"),    core::TrText("Sud-ouest"), core::TrText("Ouest"),
    core::TrText("Nord-ouest"), core::TrText("De dos"),    core::TrText("Nord-est"),
    core::TrText("Est"),        core::TrText("Sud-est")};

// Ce que vaut un pixel glissé et un cran de molette sur chaque réglage, et ses
// bornes : celles du document pour la cadence et l'échelle.
constexpr float kLargestFrameSpeed = 16.0F;
constexpr float kSmallestFrameSpeed = 0.05F;
constexpr WheelDrag kFrameSpeedDrag{.drag_step = 0.01F,
                                    .wheel_step = 0.1F,
                                    .lowest = kSmallestFrameSpeed,
                                    .highest = kLargestFrameSpeed,
                                    .format = "%.2f x"};
constexpr float kLargestTurn = 1440.0F;
constexpr WheelDrag kTurnDrag{.drag_step = 1.0F,
                              .wheel_step = 5.0F,
                              .lowest = -kLargestTurn,
                              .highest = kLargestTurn,
                              .format = "%.0f °/s"};
constexpr float kFullTurn = 360.0F;
constexpr WheelDrag kRollDrag{.drag_step = 1.0F,
                              .wheel_step = 5.0F,
                              .lowest = 0.0F,
                              .highest = kFullTurn,
                              .format = "%.0f °"};
constexpr float kLargestScale = 16.0F;
constexpr float kSmallestScale = 0.05F;
constexpr WheelDrag kScaleDrag{.drag_step = 0.01F,
                               .wheel_step = 0.1F,
                               .lowest = kSmallestScale,
                               .highest = kLargestScale,
                               .format = "%.2f"};
constexpr WheelDrag kOpacityDrag{
    .drag_step = 0.005F, .wheel_step = 0.05F, .lowest = 0.0F, .highest = 1.0F, .format = "%.2f"};

// LE TEXTE D'UN CHAMP SAISI que le document recharge, sauf sous les doigts :
// `buffer` reprend `text` quand l'objet change ou que l'on ne tape pas.
void Reload(std::span<char> buffer, std::optional<std::size_t>& of, bool typing,
            const std::optional<std::size_t>& slot, const std::string& text) {
  const std::string_view typed(buffer.data(), strnlen(buffer.data(), buffer.size()));
  if (of == slot && (typing || typed == text)) {
    return;
  }
  std::fill(buffer.begin(), buffer.end(), '\0');
  const std::size_t kept = std::min(text.size(), buffer.size() - 1);
  std::copy_n(text.begin(), kept, buffer.begin());
  of = slot;
}

[[nodiscard]] std::string Typed(std::span<const char> buffer) {
  return std::string(buffer.data(), strnlen(buffer.data(), buffer.size()));
}

// L'ACTION ET L'ANIMATION du sprite : le rang, figée ou en boucle.
[[nodiscard]] bool AnimationFields(MapEditorSprite& sprite, const SkinArt& art,
                                   WheelDragResult& moved) {
  bool toggled = false;
  const std::size_t named = sprite.action_count == 0
                                ? kCreatureActions.size()
                                : std::min(sprite.action_count, kCreatureActions.size());
  int action = static_cast<int>(sprite.action);
  if (sprite.action < named) {
    if (SkinCombo(core::Tr("Action"), &action,
                  std::span<const char* const>(kCreatureActions.data(), named), art)) {
      sprite.action = static_cast<std::uint32_t>(action);
      toggled = true;
    }
  }
  // UN .act QUI PORTE D'AUTRES RANGS les donne au chiffre.
  if (sprite.action_count > kCreatureActions.size() || sprite.action >= named) {
    const int highest = std::max(static_cast<int>(sprite.action_count) - 1, action);
    if (SkinInputInt(core::Tr("Rang d'animation"), &action, 0, highest, 1, 1, art)) {
      sprite.action = static_cast<std::uint32_t>(action);
      toggled = true;
    }
  }
  toggled |= SkinCheckBox(core::Tr("Figée"), &sprite.frozen, art);
  if (sprite.frozen) {
    int frame = static_cast<int>(sprite.held_frame);
    const int last = std::max(static_cast<int>(sprite.frame_count) - 1, 0);
    if (SkinInputInt(core::Tr("Image"), &frame, 0, last, 1, 1, art)) {
      sprite.held_frame = static_cast<std::uint32_t>(frame);
      toggled = true;
    }
  } else {
    const WheelDragResult paced =
        WheelDragFloat(core::Tr("Vitesse de l'animation"), sprite.frame_speed, kFrameSpeedDrag);
    moved.changed |= paced.changed;
    moved.released |= paced.released;
  }
  return toggled;
}

// L'ORIENTATION, LE ROULIS, L'ÉCHELLE ET L'OPACITÉ.
[[nodiscard]] bool PoseFields(MapEditorSprite& sprite, const SkinArt& art, WheelDragResult& moved) {
  int direction = static_cast<int>(sprite.direction);
  const bool turned = SkinCombo(core::Tr("Orientation"), &direction, kDirections, art);
  if (turned) {
    sprite.direction = static_cast<std::uint32_t>(direction);
  }
  for (const WheelDragResult field :
       {WheelDragFloat(core::Tr("Rotation (°/s)"), sprite.turn_degrees_per_second, kTurnDrag),
        WheelDragFloat(core::Tr("Roulis"), sprite.roll_degrees, kRollDrag),
        WheelDragFloat(core::Tr("Roulis (°/s)"), sprite.roll_degrees_per_second, kTurnDrag),
        WheelDragFloat(core::Tr("Échelle"), sprite.scale, kScaleDrag),
        WheelDragFloat(core::Tr("Opacité"), sprite.opacity, kOpacityDrag)}) {
    moved.changed |= field.changed;
    moved.released |= field.released;
  }
  return turned | SkinCheckBox(core::Tr("Masquer"), &sprite.hidden, art);
}

// Le sprite désigné : tous ses champs, puis ses boutons.
void DrawChosenSprite(const MapEditorView& view, MapEditorPanelState& panel,
                      std::vector<MapEditorIntent>& intents, const SkinArt& art) {
  const std::size_t slot = *view.selected_sprite;
  ImGui::PushID("sprite");
  map_editor::SetSprite set{.slot = slot, .sprite = view.sprites[slot], .released = false};
  WheelDragResult moved;
  Reload(panel.sprite_name, panel.sprite_name_of, panel.sprite_name_typing, view.selected_sprite,
         set.sprite.sprite);
  static_cast<void>(ImGui::InputText(core::Tr("Sprite (nom ou numéro)"), panel.sprite_name.data(),
                                     panel.sprite_name.size()));
  panel.sprite_name_typing = ImGui::IsItemActive();
  bool toggled = false;
  if (ImGui::IsItemDeactivatedAfterEdit() && !Typed(panel.sprite_name).empty()) {
    set.sprite.sprite = Typed(panel.sprite_name);
    toggled = true;
  }
  if (set.sprite.action_count == 0) {
    ImGui::TextDisabled("%s", core::Tr("Ce sprite ne se lit pas dans l'installation."));
  }
  const WheelDragResult placed =
      PositionInCells(core::Tr("Position (cases, hauteur)"), set.sprite.position, view.map_cubes);
  moved.changed |= placed.changed;
  moved.released |= placed.released;
  toggled |= PositionClipboardMenu("position", set.sprite.position, view.map_cubes);
  toggled |= AnimationFields(set.sprite, art, moved);
  toggled |= PoseFields(set.sprite, art, moved);
  Reload(panel.sprite_why, panel.sprite_why_of, panel.sprite_why_typing, view.selected_sprite,
         set.sprite.why);
  static_cast<void>(
      ImGui::InputText(core::Tr("Pourquoi"), panel.sprite_why.data(), panel.sprite_why.size()));
  panel.sprite_why_typing = ImGui::IsItemActive();
  if (ImGui::IsItemDeactivatedAfterEdit()) {
    set.sprite.why = Typed(panel.sprite_why);
    toggled = true;
  }
  if (moved.changed || moved.released || toggled) {
    set.released = moved.released || toggled || !ImGui::IsAnyItemActive();
    intents.emplace_back(std::move(set));
  }
  if (SkinButton(core::Tr("Retirer le sprite"), art)) {
    intents.emplace_back(map_editor::RemoveSprite{slot});
  }
  ImGui::SameLine();
  if (SkinButton(core::Tr("Centrer"), art)) {
    intents.emplace_back(map_editor::Focus{});
  }
  ImGui::SameLine();
  if (SkinButton(core::Tr("Dupliquer"), art)) {
    intents.emplace_back(map_editor::Duplicate{});
  }
  ImGui::PopID();
}

}  // namespace

void DrawSprites(const MapEditorView& view, MapEditorPanelState& panel,
                 std::vector<MapEditorIntent>& intents, const SkinArt& art) {
  static_cast<void>(ImGui::InputTextWithHint(core::Tr("Sprite à ajouter"),
                                             core::Tr("nom ou numéro de monstre"),
                                             panel.sprite_input.data(), panel.sprite_input.size()));
  const std::string wanted = Typed(panel.sprite_input);
  ImGui::BeginDisabled(wanted.empty());
  if (SkinButton(core::Tr("Ajouter un sprite"), art)) {
    intents.emplace_back(map_editor::AddSprite{wanted});
  }
  ImGui::EndDisabled();
  for (std::size_t slot = 0; slot < view.sprites.size(); ++slot) {
    const MapEditorSprite& sprite = view.sprites[slot];
    // Il se reconnaît à sa case, comme sur la minicarte.
    const std::array<float, 3> cells = CellsOfNative(sprite.position, view.map_cubes);
    const std::string label =
        std::format("{} {} — {} — {:.1f} {:.1f} {:.1f}{}{}##sprite{}", core::Tr("Sprite"), slot,
                    sprite.sprite, cells[0], cells[1], cells[2],
                    sprite.hidden ? std::format(" ({})", core::Tr("masqué")) : std::string(),
                    WhyTag(sprite.why), slot);
    const bool chosen = view.selected_sprite == slot;
    if (ImGui::Selectable(label.c_str(), chosen)) {
      intents.emplace_back(map_editor::SelectSprite{chosen ? std::nullopt : std::optional(slot)});
    }
    DrawMapEditorRowMenu(
        MapEditorRowMenu{.select = map_editor::SelectSprite{slot},
                         .remove_label = core::Tr("Supprimer"),
                         .cells = cells,
                         .move_to = [&sprite, slot, cubes = view.map_cubes](
                                        const std::array<float, 3>& pasted) -> MapEditorIntent {
                           map_editor::SetSprite set{
                               .slot = slot, .sprite = sprite, .released = true};
                           set.sprite.position = NativeOfCells(pasted, cubes);
                           return set;
                         }},
        intents);
  }
  if (view.selected_sprite.has_value() && *view.selected_sprite < view.sprites.size()) {
    DrawChosenSprite(view, panel, intents, art);
  }
}

}  // namespace ro::ui
