// La fenêtre d'instance. Voir l'en-tête.

#include "ro/ui/memorial_dungeon_window.h"

#include <algorithm>
#include <array>
#include <cstdio>
#include <string>

#include <imgui.h>

#include "ro/core/i18n.h"
#include "ro/ui/modal_box.h"
#include "ro/ui/skin_widgets.h"
#include "ro/ui/skinned_window.h"
#include "ro/ui/window_name.h"

namespace ro::ui {
namespace {

constexpr const char* kTitle = core::TrText("Instance");
constexpr const char* kDestroyQuestionId = "memorial_dungeon_destroy";

constexpr std::int64_t kSecondsPerMinute = 60;
constexpr std::int64_t kSecondsPerHour = 3600;

// LA COULEUR DE LA DIFFICULTÉ glisse du vert au rouge du premier niveau au
// plus haut du menu ; au-delà, elle reste rouge.
constexpr int kHardModeEasiest = 1;
constexpr int kHardModeHardest = 10;
constexpr ImVec4 kHardModeEasyColor{0.35F, 0.80F, 0.35F, 1.0F};
constexpr ImVec4 kHardModeHardColor{0.95F, 0.25F, 0.20F, 1.0F};
constexpr ImVec4 kTurboOnColor{1.00F, 0.70F, 0.15F, 1.0F};
// UNE ÉCHÉANCE PROCHE SE LIT EN ROUGE : sous ce seuil, il reste à peine de
// quoi traverser une carte.
constexpr std::int64_t kUrgentSeconds = 5 * kSecondsPerMinute;
constexpr ImVec4 kUrgentColor{0.95F, 0.30F, 0.25F, 1.0F};
constexpr ImVec4 kQuietColor{0.60F, 0.60F, 0.60F, 1.0F};

// La jauge d'une échéance, en lignes de texte : un filet sous le chiffre.
constexpr float kGaugeLines = 0.5F;
// L'écart entre les libellés et la colonne des valeurs, en espacements.
constexpr float kColumnGaps = 2.0F;

[[nodiscard]] float Scale() { return ImGui::GetStyle().FontScaleMain; }

[[nodiscard]] ImVec4 HardModeColor(int hard_mode) {
  const float span = static_cast<float>(kHardModeHardest - kHardModeEasiest);
  const float ratio =
      std::clamp(static_cast<float>(hard_mode - kHardModeEasiest) / span, 0.0F, 1.0F);
  const auto mix = [ratio](float easy, float hard) { return easy + (hard - easy) * ratio; };
  return ImVec4(mix(kHardModeEasyColor.x, kHardModeHardColor.x),
                mix(kHardModeEasyColor.y, kHardModeHardColor.y),
                mix(kHardModeEasyColor.z, kHardModeHardColor.z), 1.0F);
}

// « Libellé : » à gauche, la valeur dans la colonne commune.
void Label(const char* label, float column) {
  ImGui::TextUnformatted(label);
  ImGui::SameLine(column);
}

[[nodiscard]] const char* OwnerLabel(MemorialOwnerView owner) {
  switch (owner) {
    case MemorialOwnerView::kCharacter:
      return core::Tr("Personnage");
    case MemorialOwnerView::kParty:
      return core::Tr("Groupe");
    case MemorialOwnerView::kGuild:
      return core::Tr("Guilde");
    case MemorialOwnerView::kClan:
      return core::Tr("Clan");
    case MemorialOwnerView::kUnknown:
      break;
  }
  return core::Tr("Propriétaire");
}

// UNE ÉCHÉANCE : le temps restant, puis sa jauge sur la durée totale quand on
// la connaît. La jauge se VIDE à mesure que le temps passe.
void DrawCountdown(std::int64_t seconds_left, std::uint32_t total_s) {
  const std::string left = MemorialDuration(seconds_left);
  if (seconds_left < kUrgentSeconds) {
    ImGui::TextColored(kUrgentColor, "%s", left.c_str());
  } else {
    ImGui::TextUnformatted(left.c_str());
  }
  if (total_s == 0) {
    return;
  }
  const float ratio = std::clamp(
      static_cast<float>(static_cast<double>(seconds_left) / static_cast<double>(total_s)), 0.0F,
      1.0F);
  ImGui::ProgressBar(ratio, ImVec2(-1.0F, ImGui::GetTextLineHeight() * kGaugeLines), "");
}

void DrawDetails(const MemorialDungeonView& view, float column) {
  Label(core::Tr("Difficulté :"), column);
  if (const char* named = MemorialHardModeName(view.hard_mode); named != nullptr) {
    ImGui::TextColored(HardModeColor(view.hard_mode), core::Tr("HM %d · %s"), view.hard_mode,
                       named);
  } else {
    ImGui::TextColored(HardModeColor(view.hard_mode), core::Tr("HM %d"), view.hard_mode);
  }
  SkinTooltip(
      core::Tr("Les monstres de l'instance sont renforcés selon ce niveau, et ses "
               "récompenses croissent avec lui."));

  Label(core::Tr("Mode turbo :"), column);
  if (view.turbo) {
    ImGui::TextColored(kTurboOnColor, "%s", core::Tr("activé"));
  } else {
    ImGui::TextColored(kQuietColor, "%s", core::Tr("désactivé"));
  }
  SkinTooltip(core::Tr("En mode turbo, la plupart des dialogues de l'instance sont sautés."));

  Label(OwnerLabel(view.owner), column);
  if (view.owner_name.empty()) {
    ImGui::TextColored(kQuietColor, "%s", core::Tr("inconnu"));
  } else {
    ImGui::TextUnformatted(view.owner_name.data(), view.owner_name.data() + view.owner_name.size());
  }

  if (!view.waiting) {
    Label(core::Tr("Joueurs présents :"), column);
    ImGui::Text("%d", view.players_inside);
  }
}

void DrawTimers(const MemorialDungeonView& view, float column) {
  Label(core::Tr("Temps restant :"), column);
  if (!view.seconds_left.has_value()) {
    ImGui::TextColored(kQuietColor, "%s", core::Tr("sans limite"));
  } else {
    ImGui::BeginGroup();
    DrawCountdown(*view.seconds_left, view.duration_s);
    ImGui::EndGroup();
    if (!view.end_clock.empty()) {
      Label(core::Tr("Fin prévue :"), column);
      ImGui::TextUnformatted(view.end_clock.data(), view.end_clock.data() + view.end_clock.size());
    }
  }
  if (view.idle_seconds_left.has_value()) {
    Label(core::Tr("Fermeture :"), column);
    ImGui::BeginGroup();
    DrawCountdown(*view.idle_seconds_left, view.idle_timeout_s);
    ImGui::EndGroup();
    SkinTooltip(
        core::Tr("Personne n'est dans l'instance : elle sera détruite si personne "
                 "n'y entre avant la fin de ce délai."));
  }
}

[[nodiscard]] const char* DestroyBlocker(MemorialDestroyGate gate) {
  switch (gate) {
    case MemorialDestroyGate::kNotOwner:
      return core::Tr(
          "Seul le chef du groupe, le maître de la guilde ou le propriétaire peut "
          "détruire l'instance.");
    case MemorialDestroyGate::kOnInstanceMap:
      return core::Tr("Sortez de l'instance pour pouvoir la détruire.");
    case MemorialDestroyGate::kPending:
      return core::Tr("La demande est partie : le serveur n'a pas encore répondu.");
    case MemorialDestroyGate::kHidden:
    case MemorialDestroyGate::kAllowed:
    case MemorialDestroyGate::kUnknownRights:
      break;
  }
  return nullptr;
}

// Le bouton et sa confirmation. Rend vrai au « oui ».
[[nodiscard]] bool DrawDestroy(const MemorialDungeonView& view, const SkinArt& art) {
  if (view.destroy == MemorialDestroyGate::kHidden) {
    return false;
  }
  const char* blocker = DestroyBlocker(view.destroy);
  ImGui::BeginDisabled(blocker != nullptr);
  if (SkinButton(core::Tr("Détruire l'instance"), art)) {
    AskModal(kDestroyQuestionId);
  }
  ImGui::EndDisabled();
  SkinTooltip(blocker != nullptr
                  ? blocker
                  : core::Tr("Détruit l'instance pour tout le groupe. Sa progression est perdue."));
  return DrawModalBox(kDestroyQuestionId,
                      core::Tr("Détruire l'instance ? Toute sa progression sera perdue, pour "
                               "tous ses membres."),
                      core::Tr("Détruire"), core::Tr("Annuler"), art) == ModalAnswer::kYes &&
         DestroyBlocker(view.destroy) == nullptr;
}

}  // namespace

const char* MemorialHardModeName(int hard_mode) {
  // LES NIVEAUX DU MENU DE DIFFICULTÉ ; les autres n'ont pas de nom.
  switch (hard_mode) {
    case 1:
      return core::Tr("Facile");
    case 2:
      return core::Tr("Normal");
    case 3:
      return core::Tr("Moyen");
    case 5:
      return core::Tr("Difficile");
    case 7:
      return core::Tr("Très difficile");
    case 10:
      return core::Tr("Malade");
    default:
      return nullptr;
  }
}

std::string MemorialDuration(std::int64_t seconds) {
  const std::int64_t clamped = std::max<std::int64_t>(0, seconds);
  const int hours = static_cast<int>(clamped / kSecondsPerHour);
  const int minutes = static_cast<int>((clamped % kSecondsPerHour) / kSecondsPerMinute);
  const int rest = static_cast<int>(clamped % kSecondsPerMinute);
  std::array<char, 64> text{};
  if (hours > 0) {
    std::snprintf(text.data(), text.size(), core::Tr("%d h %02d min %02d s"), hours, minutes, rest);
  } else if (minutes > 0) {
    std::snprintf(text.data(), text.size(), core::Tr("%d min %02d s"), minutes, rest);
  } else {
    std::snprintf(text.data(), text.size(), core::Tr("%d s"), rest);
  }
  return text.data();
}

MemorialDungeonWindowAction DrawMemorialDungeonWindow(MemorialDungeonWindowState& state,
                                                      const MemorialDungeonView& view,
                                                      const SkinArt& art) {
  MemorialDungeonWindowAction action;
  if (!state.open) {
    return action;
  }
  const ImGuiViewport* screen = ImGui::GetMainViewport();
  ImGui::SetNextWindowPos(screen->GetCenter(), ImGuiCond_FirstUseEver, ImVec2(0.5F, 0.5F));
  ImGui::SetNextWindowSize(ImVec2(kMemorialDungeonWindowWidth * Scale(), 0.0F), ImGuiCond_Always);
  static WindowName name(kTitle, kMemorialDungeonWindowId);
  const bool begun =
      BeginSkinnedWindow(name.Get(), &state.open, art, SkinnedTitle::kBar,
                         ImGuiWindowFlags_AlwaysAutoResize | ImGuiWindowFlags_NoCollapse);
  if (!begun) {
    EndSkinnedWindow();
    return action;
  }
  if (!view.present) {
    ImGui::TextColored(kQuietColor, "%s", core::Tr("Aucune instance en cours."));
    EndSkinnedWindow();
    return action;
  }
  ImGui::TextUnformatted(view.name.data(), view.name.data() + view.name.size());
  if (view.waiting) {
    ImGui::TextColored(kTurboOnColor, core::Tr("En préparation — place dans la file : %d"),
                       view.wait_position);
  } else {
    ImGui::TextColored(kHardModeEasyColor, "%s", core::Tr("Ouverte"));
  }
  ImGui::Separator();
  // La colonne des valeurs : le plus long des libellés, et un écart.
  const float column = ImGui::CalcTextSize(core::Tr("Joueurs présents :")).x +
                       ImGui::GetStyle().ItemSpacing.x * kColumnGaps;
  if (view.detailed) {
    DrawDetails(view, column);
  }
  if (!view.waiting) {
    DrawTimers(view, column);
  }
  if (view.destroy != MemorialDestroyGate::kHidden) {
    ImGui::Separator();
    action.destroy = DrawDestroy(view, art);
  }
  EndSkinnedWindow();
  return action;
}

}  // namespace ro::ui
