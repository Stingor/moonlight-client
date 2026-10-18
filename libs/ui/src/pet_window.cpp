// LA FICHE DU FAMILIER : l'en-tête (sprite, nom-bouton, espèce, accessoire),
// les deux jauges, les ordres, l'auto-alimentation, les évolutions et leur
// panneau déplié. Voir l'en-tête.

#include "ro/ui/pet_window.h"

#include <algorithm>
#include <array>
#include <cfloat>
#include <cstdio>
#include <string>
#include <string_view>

#include <imgui.h>

#include "ro/core/i18n.h"
#include "ro/core/text.h"
#include "ro/ui/skin_art.h"
#include "ro/ui/skin_text.h"
#include "ro/ui/skin_tooltip.h"
#include "ro/ui/skin_widgets.h"
#include "ro/ui/skinned_window.h"

#include "pet_intimacy_text.h"
#include "ui_scale.h"
#include "ui_strings.h"

namespace ro::ui {
namespace {

// ── LA PALETTE, réglée pour le corps clair ──────────────────────────────────
//
// Ses textes se posent sur le corps de l'habillage par `InkOn` : tels sur un
// corps clair, retournés sur un sombre.
constexpr ImU32 kLabelColor = IM_COL32(89, 89, 107, 255);
constexpr ImU32 kValueColor = IM_COL32(26, 26, 33, 255);
constexpr ImU32 kRedColor = IM_COL32(153, 31, 31, 255);
// Nulle : la couleur des liens de l'habillage, que la cellule lit au dessin.
constexpr ImU32 kLinkInk = 0;
constexpr ImU32 kGaugeTextColor = IM_COL32(255, 255, 255, 255);
constexpr ImU32 kGaugeShadowColor = IM_COL32(0, 0, 0, 200);
constexpr ImU32 kFedGaugeColor = IM_COL32(89, 153, 64, 255);
constexpr ImU32 kStarvingGaugeColor = IM_COL32(191, 51, 51, 255);
constexpr ImU32 kIntimacyGaugeColor = IM_COL32(140, 89, 140, 255);

// ── LES MESURES, en pixels d'interface ──────────────────────────────────────
// La largeur la plus étroite : sans elle, la première image se dessine à la
// largeur par défaut, chaque texte enroulé y prend dix lignes et la hauteur
// ajustée explose.
constexpr float kLeastWidthPixels = 330.0F;
// La place de la première ouverture, rabattue dans l'écran.
constexpr float kSpawnXPixels = 1054.0F;
constexpr float kSpawnYPixels = 369.0F;
constexpr float kSpawnHeightPixels = 260.0F;
constexpr float kSpriteBoxPixels = 64.0F;
constexpr float kEvolutionBoxPixels = 72.0F;
constexpr float kGaugePixels = 15.0F;
// Le texte d'une jauge remonte un peu : le jambage descendant de la police le
// fait paraître trop bas une fois centré.
constexpr float kGaugeTextLiftPixels = 2.0F;
constexpr float kGaugeShadowPixels = 1.0F;
// Un champ de largeur FIXE : sous l'ajustement automatique, une largeur
// relative ferait grandir la fenêtre d'une image à l'autre.
constexpr float kRenameFieldPixels = 160.0F;
constexpr float kRenameButtonPixels = 62.0F;
constexpr float kOrderButtonPixels = 96.0F;
constexpr int kReturnButtonOrders = 2;
constexpr float kConfirmButtonPixels = 80.0F;
constexpr float kEvolveButtonPixels = 110.0F;
constexpr float kMaterialIndentPixels = 8.0F;
// La flèche entre les deux espèces : le trait, puis la pointe.
constexpr float kArrowBoxPixels = 36.0F;
constexpr float kArrowStartPixels = 4.0F;
constexpr float kArrowShaftEndPixels = 26.0F;
constexpr float kArrowTipPixels = 32.0F;
constexpr float kArrowBasePixels = 22.0F;
constexpr float kArrowHalfPixels = 6.0F;
constexpr float kArrowThicknessPixels = 2.0F;

constexpr const char* kWindowId = "###pet_window";
constexpr const char* kLinkMenuId = "##pet_link_menu";

void TextColored(ImU32 color, const char* text) {
  ImGui::PushStyleColor(ImGuiCol_Text, InkOn(color, PaintingLook().body));
  ImGui::TextUnformatted(text);
  ImGui::PopStyleColor();
}

void WrappedColored(ImU32 color, const char* text) {
  ImGui::PushStyleColor(ImGuiCol_Text, InkOn(color, PaintingLook().body));
  ImGui::TextWrapped("%s", text);
  ImGui::PopStyleColor();
}

// La jauge : le libellé dessiné à la main, centré, en blanc ombré — celui
// d'ImGui suit le bord du remplissage et devient illisible à mi-course.
void Gauge(const char* label, int current, int most, ImU32 color) {
  const float ratio =
      most > 0 ? std::clamp(static_cast<float>(current) / static_cast<float>(most), 0.0F, 1.0F)
               : 0.0F;
  std::string text = label;
  text += "  " + std::to_string(current) + " / " + std::to_string(most);
  ImGui::PushStyleColor(ImGuiCol_PlotHistogram, color);
  ImGui::ProgressBar(ratio, ImVec2(-1.0F, Px(kGaugePixels)), "");
  ImGui::PopStyleColor();
  const ImVec2 low = ImGui::GetItemRectMin();
  const ImVec2 high = ImGui::GetItemRectMax();
  const ImVec2 size = ImGui::CalcTextSize(text.c_str());
  const ImVec2 at((low.x + high.x - size.x) * 0.5F,
                  (low.y + high.y - size.y) * 0.5F - Px(kGaugeTextLiftPixels));
  AddTextRelief(ImGui::GetWindowDrawList(), at, kGaugeTextColor, text.c_str(), kGaugeShadowColor,
                ImVec2(kGaugeShadowPixels, kGaugeShadowPixels));
}

// Une image dans un cadre carré, ou le cadre vide.
void SpriteBox(const SkinImage& image, float side) {
  const ImVec2 low = ImGui::GetCursorScreenPos();
  if (image.usable()) {
    ImGui::GetWindowDrawList()->AddImage(image.id, low, ImVec2(low.x + side, low.y + side));
  }
  ImGui::Dummy(ImVec2(side, side));
}

// UNE CELLULE D'ITEM CLIQUABLE : icône facultative et libellé, avec les gestes
// de tout lien d'objet (`ro::ui::ItemLinkCell`).
//
// `item_id` est ce que la cellule DÉSIGNE, `shown` ce qu'on LIT : une espèce
// montre le nom du monstre et mène à son ŒUF. Avec `icon`, sa place se garde
// même quand l'image manque.
void PetItemLink(PetWindowState& state, PetWindowAction& action, std::uint32_t item_id,
                 const std::string& shown, ImU32 color, const SkinImage* icon) {
  DrawItemLinkCell(ItemLinkCell{.item_id = item_id,
                                .text = shown,
                                .icon = icon != nullptr ? *icon : SkinImage{},
                                .keep_icon_place = icon != nullptr,
                                .color = color},
                   state.link_menu, action.link);
}

void DrawRenameRow(PetWindowState& state, const PetWindowView& view, const SkinArt& art,
                   PetWindowAction& action) {
  if (state.renaming) {
    ImGui::SetNextItemWidth(Px(kRenameFieldPixels));
    ImGui::InputText("##pet_name", state.rename_buffer.data(), state.rename_buffer.size());
    const std::string_view typed(state.rename_buffer.data());
    ImGui::SameLine();
    if (SkinSmallButton(core::Tr("Valider"), art, ImVec2(Px(kRenameButtonPixels), 0.0F)) &&
        !typed.empty()) {
      // La borne porte sur la forme ENVOYÉE, en octets : elle se dit après
      // coup, sans tronquer la saisie au milieu d'un caractère.
      if (core::Cp1252FromUtf8(typed).size() > view.name_max_bytes) {
        std::array<char, 128> refusal = {};
        std::snprintf(refusal.data(), refusal.size(),
                      core::Tr("Le nom du pet doit faire %d octets au plus."),
                      static_cast<int>(view.name_max_bytes));
        state.status = refusal.data();
        state.status_refused = true;
      } else {
        action.rename = std::string(typed);
        state.status = core::Tr("Renommage envoyé.");
        state.status_refused = false;
        state.renaming = false;
      }
    }
    ImGui::SameLine();
    if (SkinSmallButton(SuffixedLabel(core::Tr("Annuler"), "##pet_rename_cancel").c_str(), art,
                        ImVec2(Px(kRenameButtonPixels), 0.0F))) {
      state.renaming = false;
    }
    // L'AVERTISSEMENT EST PERMANENT sous le champ, et suit la saisie : le
    // renommage est définitif.
    std::array<char, 192> warning = {};
    std::snprintf(warning.data(), warning.size(),
                  core::Tr("Le nom du pet deviendra %s, voulez-vous continuer ?"),
                  state.rename_buffer.data());
    WrappedColored(kRedColor, warning.data());
    return;
  }
  const std::string shown = view.name.empty() ? std::string("?") : view.name;
  if (view.renameable) {
    // LE NOM EST LE BOUTON, tant que le serveur accepte encore.
    if (SkinSmallButton(SuffixedLabel(shown.c_str(), "##pet_rename").c_str(), art)) {
      std::fill(state.rename_buffer.begin(), state.rename_buffer.end(), '\0');
      std::copy_n(shown.begin(), std::min(shown.size(), state.rename_buffer.size() - 1),
                  state.rename_buffer.begin());
      state.renaming = true;
    }
    SkinTooltip(core::Tr("Cliquer pour renommer — une seule fois, définitivement."));
  } else {
    ImGui::AlignTextToFramePadding();
    TextColored(kValueColor, shown.c_str());
    SkinTooltip(core::Tr("Ce pet a déjà été renommé : le serveur n'accepte qu'une fois."));
  }
}

void DrawHeader(PetWindowState& state, const PetWindowView& view, const SkinArt& art,
                PetWindowAction& action) {
  const float side = Px(kSpriteBoxPixels);
  state.sprite_box_px = side;
  SpriteBox(view.sprite, side);
  ImGui::SameLine();
  ImGui::BeginGroup();
  DrawRenameRow(state, view, art, action);
  std::string line = view.species.empty() ? std::string("?") : view.species;
  line += "  ·  ";
  line += core::Tr("niv.");
  line += " " + std::to_string(view.level);
  TextColored(kLabelColor, line.c_str());
  if (view.has_accessory) {
    std::string worn = core::Tr("Accessoire :");
    worn += " ";
    worn += view.accessory.empty() ? std::string("?") : view.accessory;
    TextColored(kValueColor, worn.c_str());
  } else {
    TextColored(kLabelColor, core::Tr("Aucun accessoire."));
  }
  ImGui::EndGroup();
}

void DrawGauges(const PetWindowView& view) {
  Gauge(core::Tr("Faim"), view.hunger, view.hunger_most,
        view.starving ? kStarvingGaugeColor : kFedGaugeColor);
  // LE SENS DE LA VARIATION : c'est lui qui dit si le familier descend.
  if (view.previous_hunger != view.hunger) {
    const bool falling = view.hunger < view.previous_hunger;
    std::string trend =
        falling ? core::Tr("en baisse, précédemment") : core::Tr("en hausse, précédemment");
    trend += " " + std::to_string(view.previous_hunger);
    TextColored(falling ? kRedColor : kLabelColor, trend.c_str());
  }
  Gauge(core::Tr("Intimité"), view.intimacy, view.intimacy_most, kIntimacyGaugeColor);
  TextColored(kValueColor, PetIntimacyText(view.intimacy_label));
}

void DrawFeed(PetWindowState& state, const PetWindowView& view, const SkinArt& art,
              PetWindowAction& action) {
  if (SkinSmallButton(core::Tr("Nourrir"), art, ImVec2(Px(kOrderButtonPixels), 0.0F))) {
    // Première garde, AVANT la confirmation.
    if (view.mail_writing) {
      action.feed_refused_mail = true;
    } else {
      state.feed_confirming = true;
    }
  }
  SkinTooltip(
      core::Tr("Nourrir un pet qui n'a pas faim lui fait PERDRE de l'intimité. Sans "
               "nourriture dans l'inventory, le client refuse sans rien envoyer."));
}

void DrawFeedConfirmation(PetWindowState& state, const PetWindowView& view, const SkinArt& art,
                          PetWindowAction& action) {
  WrappedColored(kValueColor, core::Tr("Voulez-vous vraiment nourrir votre pet ?"));
  if (SkinSmallButton(SuffixedLabel(core::Tr("Confirmer"), "##pet_feed_ok").c_str(), art,
                      ImVec2(Px(kConfirmButtonPixels), 0.0F))) {
    state.feed_confirming = false;
    // Seconde garde, à la CONFIRMATION.
    if (view.trading) {
      action.feed_refused_trade = true;
    } else {
      action.order = PetOrder::kFeed;
    }
  }
  ImGui::SameLine();
  if (SkinSmallButton(SuffixedLabel(core::Tr("Annuler"), "##pet_feed_cancel").c_str(), art,
                      ImVec2(Px(kConfirmButtonPixels), 0.0F))) {
    state.feed_confirming = false;
  }
}

void DrawReturnToEgg(PetWindowState& state, const SkinArt& art, PetWindowAction& action) {
  ImGui::Spacing();
  if (SkinSmallButton(core::Tr("Remettre dans l'œuf"), art,
                      ImVec2(Px(kOrderButtonPixels) * kReturnButtonOrders, 0.0F))) {
    if (state.confirm_return_to_egg) {
      state.return_confirming = true;
    } else {
      action.order = PetOrder::kReturnToEgg;
    }
  }
  if (!state.return_confirming) {
    return;
  }
  TextColored(kValueColor, core::Tr("Ranger le pet dans son œuf ?"));
  if (SkinSmallButton(core::Tr("Ranger"), art, ImVec2(Px(kConfirmButtonPixels), 0.0F))) {
    state.return_confirming = false;
    action.order = PetOrder::kReturnToEgg;
  }
  ImGui::SameLine();
  if (SkinSmallButton(SuffixedLabel(core::Tr("Annuler"), "##pet_egg_cancel").c_str(), art,
                      ImVec2(Px(kConfirmButtonPixels), 0.0F))) {
    state.return_confirming = false;
  }
}

void DrawArrow(float box) {
  ImDrawList* const into = ImGui::GetWindowDrawList();
  const ImVec2 at = ImGui::GetCursorScreenPos();
  const float middle = at.y + box * 0.5F;
  const ImU32 ink = InkOn(kValueColor, PaintingLook().body);
  into->AddLine(ImVec2(at.x + Px(kArrowStartPixels), middle),
                ImVec2(at.x + Px(kArrowShaftEndPixels), middle), ink, Px(kArrowThicknessPixels));
  into->AddTriangleFilled(ImVec2(at.x + Px(kArrowTipPixels), middle),
                          ImVec2(at.x + Px(kArrowBasePixels), middle - Px(kArrowHalfPixels)),
                          ImVec2(at.x + Px(kArrowBasePixels), middle + Px(kArrowHalfPixels)), ink);
  ImGui::Dummy(ImVec2(Px(kArrowBoxPixels), box));
}

// Le refus du bouton, vide quand rien ne s'y oppose.
std::string BlockText(PetEvolutionBlock block, const PetWindowView& view) {
  switch (block) {
    case PetEvolutionBlock::kIntimacy: {
      std::array<char, 128> text = {};
      std::snprintf(text.data(), text.size(), core::Tr("Le serveur exige %d d'intimité, pas %d."),
                    view.evolution_server_minimum, view.evolution_offered_above + 1);
      return text.data();
    }
    case PetEvolutionBlock::kAccessory:
      return core::Tr("Le pet doit retirer son accessoire avant d'évoluer.");
    case PetEvolutionBlock::kMaterials:
      return core::Tr("Il manque des matériaux.");
    case PetEvolutionBlock::kNone:
      break;
  }
  return {};
}

void DrawEvolutionPanel(PetWindowState& state, const PetWindowView& view, const SkinArt& art,
                        PetWindowAction& action) {
  const PetEvolutionPanelView& panel = view.panel;
  ImGui::Separator();
  const float box = Px(kEvolutionBoxPixels);
  state.evolution_box_px = box;
  SpriteBox(panel.source_sprite, box);
  ImGui::SameLine();
  DrawArrow(box);
  ImGui::SameLine();
  SpriteBox(panel.target_sprite, box);

  // Les deux espèces mènent chacune à son ŒUF.
  PetItemLink(state, action, panel.source_egg_id,
              panel.source_species.empty() ? std::string("?") : panel.source_species, kLinkInk,
              nullptr);
  ImGui::SameLine();
  TextColored(kValueColor, "  →  ");
  ImGui::SameLine();
  PetItemLink(state, action, panel.target_egg_id,
              panel.target_species.empty() ? std::string("?") : panel.target_species, kLinkInk,
              nullptr);

  // CE QU'ON POSSÈDE en regard de ce qu'il faut, le manque en rouge.
  if (panel.materials.empty()) {
    TextColored(kLabelColor, core::Tr("Aucun matériau connu du client."));
  } else {
    TextColored(kValueColor, core::Tr("Matériaux requis :"));
    for (const PetEvolutionMaterialView& material : panel.materials) {
      std::string label = material.name.empty() ? std::string("?") : material.name;
      label += "  " + std::to_string(material.owned) + " / " + std::to_string(material.needed);
      ImGui::Indent(Px(kMaterialIndentPixels));
      PetItemLink(
          state, action, material.item_id, label,
          material.owned >= material.needed ? kLinkInk : InkOn(kRedColor, PaintingLook().body),
          &material.icon);
      ImGui::Unindent(Px(kMaterialIndentPixels));
    }
  }

  // LE REFUS, DIT AVANT LE CLIC, au seuil du serveur.
  const std::string refusal = BlockText(panel.block, view);
  if (!refusal.empty()) {
    WrappedColored(kRedColor, refusal.c_str());
  }
  ImGui::BeginDisabled(!refusal.empty());
  if (SkinSmallButton(SuffixedLabel(core::Tr("Faire évoluer"), "##pet_evo_ok").c_str(), art,
                      ImVec2(Px(kEvolveButtonPixels), 0.0F))) {
    action.evolve_to = panel.target_egg_id;
    state.evolution_target = 0;
  }
  ImGui::EndDisabled();
  ImGui::SameLine();
  if (SkinSmallButton(SuffixedLabel(core::Tr("Annuler"), "##pet_evo_cancel").c_str(), art,
                      ImVec2(Px(kConfirmButtonPixels), 0.0F))) {
    state.evolution_target = 0;
  }
}

void DrawEvolutions(PetWindowState& state, const PetWindowView& view, const SkinArt& art,
                    PetWindowAction& action) {
  ImGui::Spacing();
  switch (view.evolution) {
    case PetEvolutionShown::kIntimacyTooLow:
      WrappedColored(kLabelColor,
                     core::Tr("Le pet ne peut évoluer que lorsque son intimité est Loyal."));
      return;
    case PetEvolutionShown::kEggUnknown:
      TextColored(kLabelColor, core::Tr("Évolution : le client ne connaît pas l'œuf de ce pet."));
      return;
    case PetEvolutionShown::kNoRecipe:
      TextColored(kLabelColor, core::Tr("Aucune évolution pour ce pet."));
      return;
    case PetEvolutionShown::kTargets:
      break;
  }
  int rank = 0;
  for (const PetEvolutionTargetView& target : view.evolution_targets) {
    std::array<char, 128> text = {};
    std::snprintf(text.data(), text.size(), core::Tr("Évolution - %s"),
                  target.species.empty() ? "?" : target.species.c_str());
    const std::string id = "##pet_evo" + std::to_string(rank++);
    // Recliquer la même cible replie le panneau.
    if (SkinSmallButton(SuffixedLabel(text.data(), id.c_str()).c_str(), art)) {
      state.evolution_target = state.evolution_target == target.egg_id ? 0 : target.egg_id;
    }
  }
  if (state.evolution_target != 0 && view.panel.target_egg_id == state.evolution_target) {
    DrawEvolutionPanel(state, view, art, action);
  }
}

void DrawActions(PetWindowState& state, const PetWindowView& view, const SkinArt& art,
                 PetWindowAction& action) {
  DrawFeed(state, view, art, action);
  ImGui::SameLine();
  if (SkinSmallButton(core::Tr("Spectacle"), art, ImVec2(Px(kOrderButtonPixels), 0.0F))) {
    action.order = PetOrder::kPerform;
  }
  ImGui::SameLine();
  ImGui::BeginDisabled(!view.has_accessory);
  if (SkinSmallButton(core::Tr("Retirer l'accessoire"), art,
                      ImVec2(Px(kOrderButtonPixels), 0.0F))) {
    action.order = PetOrder::kUnequipAccessory;
  }
  ImGui::EndDisabled();
  if (!view.has_accessory) {
    SkinTooltip(core::Tr("Ce pet ne porte aucun accessoire."));
  }
  // CE QU'IL MANGE, ET COMBIEN IL EN RESTE, sous le bouton qui le nourrit : en
  // rouge quand l'inventory n'en a plus, puisque le repas sera refusé.
  if (view.food_known) {
    const bool missing = view.food_in_bag <= 0;
    TextColored(missing ? kRedColor : kLabelColor, core::Tr("Nourriture :"));
    ImGui::SameLine();
    PetItemLink(state, action, view.food_item_id,
                view.food_name + " ×" + std::to_string(view.food_in_bag),
                missing ? InkOn(kRedColor, PaintingLook().body) : kLinkInk, &view.food_icon);
  }
  if (state.feed_confirming) {
    DrawFeedConfirmation(state, view, art, action);
  }
  DrawReturnToEgg(state, art, action);

  // LA CASE N'EXISTE QUE POUR UN ŒUF QUI Y A DROIT.
  if (view.auto_feed_listed) {
    bool enabled = view.auto_feed;
    if (SkinCheckBox(core::Tr("Alimentation automatique"), &enabled, art)) {
      action.auto_feed = enabled;
    }
    SkinTooltip(
        core::Tr("Le serveur nourrit le pet tout seul sous un certain seuil de faim. L'état "
                 "affiché est celui que le SERVEUR confirme, pas la case cochée."));
  }
  DrawEvolutions(state, view, art, action);
}

void DrawLinkMenu(PetWindowState& state, PetWindowAction& action) {
  DrawItemLinkMenu(kLinkMenuId, state.link_menu, action.link);
}

}  // namespace

void OpenPetWindow(PetWindowState& state) {
  state.open = true;
  state.focus_asked = true;
  state.status.clear();
  state.status_refused = false;
  state.feed_confirming = false;
  state.return_confirming = false;
  state.renaming = false;
  // Rouvrir la fiche ne la rend pas dépliée sur un choix ancien.
  state.evolution_target = 0;
}

PetWindowAction DrawPetWindow(PetWindowState& state, const PetWindowView& view,
                              const SkinArt& art) {
  PetWindowAction action;
  // UN ACTEUR ABSENT NE SE DESSINE PAS, mais la fiche reste ouverte : un
  // changement de carte le fait disparaître le temps d'un aller-retour.
  if (!state.open || !view.present) {
    return action;
  }
  if (state.focus_asked) {
    state.focus_asked = false;
    ImGui::SetNextWindowFocus();
  }
  const ImVec2 screen = ImGui::GetIO().DisplaySize;
  ImGui::SetNextWindowPos(
      ImVec2(std::min(Px(kSpawnXPixels), std::max(0.0F, screen.x - Px(kLeastWidthPixels))),
             std::min(Px(kSpawnYPixels), std::max(0.0F, screen.y - Px(kSpawnHeightPixels)))),
      ImGuiCond_FirstUseEver);
  ImGui::SetNextWindowSizeConstraints(ImVec2(Px(kLeastWidthPixels), 0.0F),
                                      ImVec2(FLT_MAX, FLT_MAX));
  // LE TITRE PORTE LE NOM ; l'identifiant fixe garde place et taille quand il
  // change.
  std::string title = view.name.empty() ? std::string(core::Tr("Pet")) : view.name;
  title += kWindowId;
  SetNextWindowBulletClickable(core::Tr("Réglages de cette fenêtre"));
  const bool begun = BeginSkinnedWindow(
      title.c_str(), &state.open, art, SkinnedTitle::kBar,
      ImGuiWindowFlags_NoCollapse | ImGuiWindowFlags_NoResize | ImGuiWindowFlags_AlwaysAutoResize);
  action.settings_asked = TitleBulletClicked();
  if (begun) {
    DrawHeader(state, view, art, action);
    ImGui::Separator();
    DrawGauges(view);
    ImGui::Separator();
    DrawActions(state, view, art, action);
    if (!state.status.empty()) {
      WrappedColored(state.status_refused ? kRedColor : kLabelColor, state.status.c_str());
    }
    DrawLinkMenu(state, action);
  }
  EndSkinnedWindow();
  return action;
}

}  // namespace ro::ui
