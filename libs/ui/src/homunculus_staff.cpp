// LA SECTION « STAFF » DE L'ONGLET HOMONCULE, et la liste habillée des
// homoncules que le grimoire partage. Voir `homunculus_staff.h` et
// `ro/ui/staff_homunculus.h`.
//
// ⚠ RIEN N'Y EST PERMIS OU REFUSÉ : le droit ne décide que de ce qui se montre,
// et le serveur revalide chaque commande. Un bouton n'est grisé que quand
// l'onglet SAIT que le serveur refuserait, ou qu'un nouvel envoi serait faux
// tant que le précédent attend sa réponse (le niveau visé).

#include "homunculus_staff.h"

#include <algorithm>
#include <optional>
#include <string>
#include <string_view>

#include <imgui.h>

#include "ro/core/i18n.h"
#include "ro/ui/list_keys.h"
#include "ro/ui/skin_widgets.h"

namespace ro::ui {
namespace {

// UN CURSEUR QUI N'ENVOIE QU'AU TERME DU GESTE — voir `HomunculusStaffSettle`.
// `shown` est ce qu'il montre hors geste.
//
// ⚠ IL NE PREND PAS LA MOLETTE : un cran effleuré en faisant défiler la fiche
// enverrait une commande, et `@homlevel` ne se défait pas.
//
// Rend la valeur à envoyer, rien tant qu'il n'y a rien à envoyer.
[[nodiscard]] std::optional<int> ReleasedSlider(const char* label, int shown, int lowest,
                                                int highest, std::optional<int>& draft,
                                                const SkinArt& art) {
  int value = std::clamp(draft.value_or(shown), lowest, highest);
  const bool changed =
      SkinSliderInt(label, &value, lowest, highest, art, "%d", 0, SkinSliderWheel::kLeaves);
  return HomunculusStaffSettle(value,
                               HomunculusStaffGesture{
                                   .changed = changed,
                                   .active = ImGui::IsItemActive(),
                                   .released_after_edit = ImGui::IsItemDeactivatedAfterEdit(),
                                   .mouse_held = ImGui::IsMouseDown(ImGuiMouseButton_Left),
                               },
                               draft);
}

// La valeur que montre un curseur hors geste : l'envoyée tant qu'elle est
// attendue, la servie sinon. Une attente échue s'efface ici.
[[nodiscard]] int Shown(std::optional<HomunculusStaffPending>& pending, int served, double now_s) {
  if (!HomunculusStaffAwaited(pending, served, now_s)) {
    pending.reset();
    return served;
  }
  return pending->value;
}

void Order(HomunculusTabAction& action, HomunculusStaffOrder order, int amount = 0) {
  action.verb = HomunculusVerb::kStaff;
  action.staff_order = order;
  action.staff_amount = amount;
}

void DrawSliders(HomunculusTabState& state, const HomunculusTabView& view, const SkinArt& art,
                 HomunculusTabAction& action) {
  const double now_s = ImGui::GetTime();
  if (const std::optional<int> hunger =
          ReleasedSlider(core::Tr("Faim"), Shown(state.hunger_pending, view.hunger, now_s), 0,
                         kHomunculusStaffHungerMost, state.hunger_draft, art)) {
    Order(action, HomunculusStaffOrder::kSetHunger, *hunger);
    state.hunger_pending = HomunculusStaffPending{.value = *hunger, .sent_at_s = now_s};
  }
  SkinTooltip(core::Tr("@homhungry : pose la faim de l'homoncule (0 à 100)."));

  if (const std::optional<int> intimacy =
          ReleasedSlider(core::Tr("Intimité"), Shown(state.intimacy_pending, view.intimacy, now_s),
                         0, kHomunculusStaffIntimacyMost, state.intimacy_draft, art)) {
    Order(action, HomunculusStaffOrder::kSetIntimacy, *intimacy);
    state.intimacy_pending = HomunculusStaffPending{.value = *intimacy, .sent_at_s = now_s};
  }
  SkinTooltip(core::Tr("@homfriendly : pose l'intimité de l'homoncule (0 à 1000)."));

  // ⚠ LE CURSEUR DIT LE NIVEAU VISÉ, LA COMMANDE AJOUTE DES NIVEAUX : on envoie
  // la différence, et rien quand elle est nulle. Il ne descend pas sous le
  // niveau actuel, puisque le serveur ne retire pas de niveau.
  //
  // 🔴 GRISÉ TANT QU'UN NIVEAU EST ATTENDU : il montre le niveau demandé, et
  // aucune différence ne se calcule sur le niveau servi d'avant la réponse.
  const int level_shown = Shown(state.level_pending, view.level, now_s);
  const bool level_awaited = state.level_pending.has_value();
  if (level_awaited) {
    state.level_draft.reset();
  }
  const int level_most = std::max({view.level, level_shown, view.staff_level_most});
  ImGui::BeginDisabled(level_awaited);
  const std::optional<int> level = ReleasedSlider(core::Tr("Niveau visé"), level_shown, view.level,
                                                  level_most, state.level_draft, art);
  ImGui::EndDisabled();
  if (level.has_value()) {
    if (const std::optional<int> added =
            HomunculusStaffLevelsToAdd(*level, view.level, level_awaited)) {
      Order(action, HomunculusStaffOrder::kAddLevels, *added);
      state.level_pending = HomunculusStaffPending{.value = *level, .sent_at_s = now_s};
    }
  }
  SkinTooltip(level_awaited
                  ? core::Tr("Un niveau est déjà demandé : le curseur attend que le serveur "
                             "renvoie le nouveau niveau.")
                  : core::Tr("@homlevel : ajoute à l'homoncule les niveaux qui le mènent au "
                             "niveau visé."));
}

void DrawButtons(const SkinArt& art, HomunculusTabAction& action) {
  if (SkinSmallButton(core::Tr("Rebrasser"), art)) {
    Order(action, HomunculusStaffOrder::kShuffle);
  }
  SkinTooltip(
      core::Tr("@homshuffle : retire au sort les statistiques de croissance de l'homoncule."));
  ImGui::SameLine();
  if (SkinSmallButton(core::Tr("Croissance"), art)) {
    Order(action, HomunculusStaffOrder::kStats);
  }
  SkinTooltip(core::Tr("@homstats : écrit ses statistiques et les bornes de leur croissance."));
  ImGui::SameLine();
  if (SkinSmallButton(core::Tr("Fiche"), art)) {
    Order(action, HomunculusStaffOrder::kInfo);
  }
  SkinTooltip(core::Tr("@hominfo : écrit sa fiche résumée dans la conversation."));
  ImGui::SameLine();
  if (SkinSmallButton(core::Tr("Évoluer"), art)) {
    Order(action, HomunculusStaffOrder::kEvolve);
  }
  SkinTooltip(core::Tr("@homevolution : le fait évoluer, si le serveur l'accepte."));
}

void DrawMutation(HomunculusTabState& state, const HomunculusTabView& view, const SkinArt& art,
                  HomunculusTabAction& action) {
  if (view.mutations.empty()) {
    return;
  }
  ImGui::SetNextItemWidth(ImGui::GetFontSize() * kStaffHomunculusComboEms);
  static_cast<void>(StaffHomunculusCombo("##mutation", &state.mutation_rank, view.mutations, art));
  ImGui::SameLine();
  // ⚠ GRISÉ SEULEMENT QUAND L'ONGLET SAIT : une classe inconnue laisse le
  // serveur juger.
  const bool refused =
      view.level < kHomunculusMutateLevelLeast || (view.evolved.has_value() && !*view.evolved);
  ImGui::BeginDisabled(refused);
  if (SkinSmallButton(core::Tr("Muter"), art)) {
    Order(action, HomunculusStaffOrder::kMutate);
    action.staff_job = StaffHomunculusJobAt(view.mutations, state.mutation_rank);
  }
  ImGui::EndDisabled();
  SkinTooltip(core::Tr(
      "@hommutate : mute vers la forme choisie un homoncule évolué de niveau 99 ou plus."));
}

void DrawTalk(HomunculusTabState& state, const SkinArt& art, HomunculusTabAction& action) {
  const float button = SkinSmallButtonWidth(core::Tr("Dire"), art);
  ImGui::SetNextItemWidth(
      std::max(1.0F, ImGui::GetContentRegionAvail().x - button - ImGui::GetStyle().ItemSpacing.x));
  const bool entered = ImGui::InputText("##homtalk", state.talk.data(), state.talk.size(),
                                        ImGuiInputTextFlags_EnterReturnsTrue);
  ImGui::SameLine();
  const std::string_view typed(state.talk.data());
  ImGui::BeginDisabled(typed.empty());
  const bool clicked = SkinSmallButton(core::Tr("Dire"), art);
  ImGui::EndDisabled();
  SkinTooltip(core::Tr("@homtalk : fait dire la phrase tapée à l'homoncule."));
  if ((entered || clicked) && !typed.empty()) {
    Order(action, HomunculusStaffOrder::kTalk);
    action.staff_text = std::string(typed);
    state.talk = {};
  }
}

}  // namespace

bool HomunculusStaffAwaited(const std::optional<HomunculusStaffPending>& pending, int served,
                            double now_s) noexcept {
  return pending.has_value() && served != pending->value &&
         now_s - pending->sent_at_s < kHomunculusStaffAwaitSeconds;
}

std::optional<int> HomunculusStaffSettle(int value, const HomunculusStaffGesture& gesture,
                                         std::optional<int>& draft) {
  if (gesture.released_after_edit) {
    draft.reset();
    return value;
  }
  if (gesture.active) {
    // ⚠ UN CLIC QUI SAISIT SANS RIEN CHANGER NE FAIT PAS DE BROUILLON : il
    // renverrait la valeur montrée au relâchement.
    if (gesture.changed || draft.has_value()) {
      draft = value;
    }
    return std::nullopt;
  }
  if (gesture.changed) {
    draft = value;
  }
  if (draft.has_value() && !gesture.mouse_held) {
    const int settled = *draft;
    draft.reset();
    return settled;
  }
  return std::nullopt;
}

std::optional<int> HomunculusStaffLevelsToAdd(int target, int served, bool awaited) noexcept {
  if (awaited || target <= served) {
    return std::nullopt;
  }
  return target - served;
}

bool StaffHomunculusCombo(const char* id, int* rank, std::span<const StaffHomunculusChoice> choices,
                          const SkinArt& art) {
  if (choices.empty()) {
    return false;
  }
  *rank = std::clamp(*rank, 0, static_cast<int>(choices.size()) - 1);
  bool changed = false;
  if (BeginSkinCombo(id, choices[static_cast<std::size_t>(*rank)].label.c_str(), art)) {
    ComboKeys keys(*rank, static_cast<int>(choices.size()));
    for (std::size_t index = 0; index < choices.size(); ++index) {
      const bool chosen = static_cast<int>(index) == *rank;
      ImGui::PushID(static_cast<int>(index));
      if (keys.Took(static_cast<int>(index),
                    ImGui::Selectable(choices[index].label.c_str(), chosen) && !chosen)) {
        *rank = static_cast<int>(index);
        changed = true;
      }
      ImGui::PopID();
    }
    EndSkinCombo();
  }
  return changed;
}

std::uint16_t StaffHomunculusJobAt(std::span<const StaffHomunculusChoice> choices, int rank) {
  if (rank < 0 || static_cast<std::size_t>(rank) >= choices.size()) {
    return 0;
  }
  return choices[static_cast<std::size_t>(rank)].job;
}

void DrawHomunculusStaff(HomunculusTabState& state, const HomunculusTabView& view,
                         const SkinArt& art, HomunculusTabAction& action) {
  if (!view.staff_allowed) {
    return;
  }
  // ⚠ À CHAQUE IMAGE, sinon ImGui garde son propre état et le nôtre ne pilote
  // rien. Le retour fait foi.
  ImGui::SetNextItemOpen(state.staff_open);
  state.staff_open = ImGui::CollapsingHeader(core::Tr("Staff###homun_staff"));
  if (!state.staff_open) {
    return;
  }
  ImGui::PushID("homun_staff");
  DrawSliders(state, view, art, action);
  DrawButtons(art, action);
  DrawMutation(state, view, art, action);
  DrawTalk(state, art, action);
  ImGui::PopID();
}

}  // namespace ro::ui
