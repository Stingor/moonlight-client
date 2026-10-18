#include "ro/ui/entity_menu.h"

#include <array>
#include <cstdio>

#include <imgui.h>

#include "ro/core/i18n.h"
#include "ro/ui/skin_palette.h"
#include "ro/ui/skin_popup.h"
#include "ro/ui/skin_widgets.h"
#include "ro/ui/skinned_window.h"

#include "ui_scale.h"

namespace ro::ui {
namespace {

constexpr const char* kPopupId = "##menu_entite";

// Serré : un menu contextuel se lit d'un coup d'œil, il ne se contemple pas.
constexpr float kWindowPaddingPixels = 6.0F;
constexpr float kItemSpacingXPixels = 6.0F;
constexpr float kItemSpacingYPixels = 3.0F;

// Les deux boutons de la boîte : l'action, puis l'annulation.
constexpr float kActionButtonPixels = 110.0F;
constexpr float kCancelButtonPixels = 100.0F;

// L'ocre de ce qui n'est pas offert au joueur, le rouge de ce qui retire
// quelque chose au monde, et le gris de ce qui précise sans agir. Définis une
// fois : ils servent aux lignes, au titre du sous-menu et à la boîte.
const ImVec4 kStaffColor(0.62F, 0.28F, 0.10F, 1.0F);
const ImVec4 kDangerColor(0.75F, 0.13F, 0.13F, 1.0F);
const ImVec4 kAsideColor(0.35F, 0.35F, 0.35F, 1.0F);

// Assez pour un nom de personnage et sa phrase.
constexpr std::size_t kLineBytes = 256;

// 🔴 LE TITRE VISIBLE DÉPEND DE LA QUESTION, L'IDENTITÉ NON : seul ce qui suit
// `###` est haché. Une seule boîte sert donc toutes les questions, et les trois
// endroits qui la nomment — ouverture, dessin, test « déjà ouverte » —
// s'accordent sans se passer le même libellé.
[[nodiscard]] const char* QuestionId(EntityMenuQuestion question) {
  switch (question) {
    case EntityMenuQuestion::kAccountBlock:
      return core::Tr("Bannir ce compte###menu_entite_question");
    case EntityMenuQuestion::kEventPoints:
      return core::Tr("Points d'event###menu_entite_question");
    case EntityMenuQuestion::kPartyLeader:
      return core::Tr("Chef de groupe###menu_entite_question");
    case EntityMenuQuestion::kPartyExpel:
      return core::Tr("Expulser###menu_entite_question");
    case EntityMenuQuestion::kFriendRemove:
      return core::Tr("Retirer un ami###menu_entite_question");
    case EntityMenuQuestion::kPetFeed:
      return core::Tr("Nourrir le pet###menu_entite_question");
    case EntityMenuQuestion::kHomunculusFeed:
      return core::Tr("Nourrir l'homoncule###menu_entite_question");
    case EntityMenuQuestion::kMannerGood:
      return core::Tr("Bon point de manière###menu_entite_question");
    case EntityMenuQuestion::kMannerBad:
      return core::Tr("Mauvais point de manière###menu_entite_question");
    case EntityMenuQuestion::kNpcUnload:
    case EntityMenuQuestion::kNone:
      break;
  }
  return core::Tr("Décharger ce NPC###menu_entite_question");
}

// Le nom à écrire dans une question : celui relevé à l'ouverture, ou de quoi le
// désigner malgré tout.
[[nodiscard]] const char* NameOrUnknown(const EntityMenuState& state) {
  return state.question_name.empty() ? core::Tr("(nom inconnu)") : state.question_name.c_str();
}

void AsideText(const char* text) { ImGui::TextColored(kAsideColor, "%s", text); }

// Le couple de boutons d'une boîte. Rend vrai quand l'action est confirmée.
[[nodiscard]] bool ConfirmButtons(const char* action, bool allowed, const SkinArt& art) {
  const float scale = Scale();
  bool confirmed = false;
  ImGui::BeginDisabled(!allowed);
  if (SkinButton(action, art, ImVec2(kActionButtonPixels * scale, 0.0F))) {
    confirmed = true;
    ImGui::CloseCurrentPopup();
  }
  ImGui::EndDisabled();
  ImGui::SameLine();
  if (SkinButton(core::Tr("Annuler"), art, ImVec2(kCancelButtonPixels * scale, 0.0F))) {
    ImGui::CloseCurrentPopup();
  }
  return confirmed;
}

// ⚠ SAUTS DE LIGNE EXPLICITES DANS CE QUI SUIT, ET NON UN TEXTE REPLIÉ : la
// boîte prend la taille de son contenu, et un texte qui se replie sur une
// largeur qu'il fixe lui-même la ferait osciller d'une image à l'autre.

[[nodiscard]] bool AskNpcUnload(EntityMenuState& state, const SkinArt& art) {
  // Le nom relevé à l'ouverture ; faute de mieux, le GID vaut mieux qu'une
  // phrase sans sujet.
  std::array<char, kLineBytes> who{};
  if (state.question_name.empty()) {
    std::snprintf(who.data(), who.size(), core::Tr("Le NPC %u"), state.question_subject);
  } else {
    std::snprintf(who.data(), who.size(), "%s", state.question_name.c_str());
  }
  ImGui::TextColored(kDangerColor, core::Tr("Décharger %s ?"), who.data());
  ImGui::Spacing();
  // Ce que ça coûte VRAIMENT, dit avant le clic : le serveur, lui, ne pose
  // aucune question.
  ImGui::TextUnformatted(
      core::Tr("Il disparaît immédiatement pour TOUS les joueurs\n"
               "connectés, avec ses duplicates. Une conversation en\n"
               "cours avec lui reste ouverte sur un NPC qui n'existe plus."));
  ImGui::Spacing();
  AsideText(
      core::Tr("Pour le faire revenir : clic droit sur un autre NPC du même\n"
               "fichier puis « Recharger son fichier de script », ou\n"
               "@reloadnpcfile. Rien n'est perdu sur le disque."));
  ImGui::Spacing();
  return ConfirmButtons(core::Tr("Décharger"), true, art);
}

// LA SEULE ACTION DU STAFF QUI NE SE DÉFAIT PAS DEPUIS LE JEU : le silence
// expire, la prison s'ouvre, l'expulsion se reconnecte. Celle-ci retire un
// compte entier.
[[nodiscard]] bool AskAccountBlock(EntityMenuState& state, const SkinArt& art) {
  ImGui::TextColored(kDangerColor, core::Tr("Bannir le compte de %s ?"), NameOrUnknown(state));
  ImGui::Spacing();
  ImGui::TextUnformatted(
      core::Tr("C'est le COMPTE qui tombe, pas ce personnage : tous\n"
               "les autres personnages du même compte deviennent\n"
               "inaccessibles, et le bannissement n'a pas de fin."));
  ImGui::Spacing();
  AsideText(
      core::Tr("Pour le lever : @unblock <nom du personnage>.\n"
               "Pour une sanction qui expire, préférez @ban."));
  ImGui::Spacing();
  return ConfirmButtons(core::Tr("Bannir"), true, art);
}

// ⚠ LE SOLDE ACTUEL N'EST PAS AFFICHÉ : le client ne le connaît pas, et
// l'aller-retour qu'il faudrait pour l'obtenir ferait attendre devant une boîte
// vide.
[[nodiscard]] bool AskEventPoints(EntityMenuState& state, const SkinArt& art) {
  ImGui::Text(core::Tr("Points d'event de %s"), NameOrUnknown(state));
  ImGui::Spacing();
  static_cast<void>(SkinSliderInt(core::Tr("à donner###menu_entite_points"), &state.points,
                                  -kEventPointsStep, kEventPointsStep, art, "%+d"));
  ImGui::Spacing();
  AsideText(
      core::Tr("Négatif pour retirer. Ctrl+clic sur la barre pour taper\n"
               "la valeur. Le serveur reborne, et refuse de faire\n"
               "descendre le solde sous zéro."));
  ImGui::Spacing();
  // Zéro ne fait rien : le bouton reste visible et grisé plutôt que de partir
  // en paquet que le serveur refusera.
  return ConfirmButtons(core::Tr("Appliquer"), state.points != 0, art);
}

// LES MINUTES D'UN POINT DE MANIÈRE. ⚠ LE SILENCE EN COURS N'EST PAS AFFICHÉ :
// le client ne le connaît pas, pas plus que le solde des points d'event.
[[nodiscard]] bool AskManner(EntityMenuState& state, const SkinArt& art) {
  const bool good = state.question == EntityMenuQuestion::kMannerGood;
  if (good) {
    ImGui::Text(core::Tr("Bon point de manière pour %s"), NameOrUnknown(state));
  } else {
    ImGui::TextColored(kDangerColor, core::Tr("Mauvais point de manière pour %s"),
                       NameOrUnknown(state));
  }
  ImGui::Spacing();
  static_cast<void>(SkinInputInt(core::Tr("minutes###menu_entite_manieres"), &state.points,
                                 kMannerMinutesFirst, kMannerMinutesMost, kMannerMinutesStep,
                                 kMannerMinutesFastStep, art));
  ImGui::Spacing();
  AsideText(good ? core::Tr("Retire autant de minutes à son silence, et le lève\n"
                            "quand il n'en reste plus.")
                 : core::Tr("Ajoute autant de minutes de silence : ni chat ni\n"
                            "skills. Appliqué deux fois, il prolonge."));
  ImGui::Spacing();
  return ConfirmButtons(core::Tr("Appliquer"), state.points >= kMannerMinutesFirst, art);
}

// Les trois gestes de groupe et d'amitié posent la même question, « celui-ci,
// sûr ? », et ne diffèrent que par la phrase.
[[nodiscard]] bool AskSocial(EntityMenuState& state, const SkinArt& art) {
  // ⚠ UN FORMAT LITTÉRAL PAR PHRASE : un format tenu dans une variable ne se
  // vérifie plus à la compilation.
  const char* button = core::Tr("Retirer l'ami");
  if (state.question == EntityMenuQuestion::kPartyLeader) {
    ImGui::Text(core::Tr("Céder le commandement du groupe à %s ?"), NameOrUnknown(state));
    button = core::Tr("Céder");
  } else if (state.question == EntityMenuQuestion::kPartyExpel) {
    ImGui::Text(core::Tr("Expulser %s du groupe ?"), NameOrUnknown(state));
    button = core::Tr("Expulser");
  } else {
    ImGui::Text(core::Tr("Retirer %s de votre liste d'amis ?"), NameOrUnknown(state));
  }
  if (state.question == EntityMenuQuestion::kPartyLeader) {
    ImGui::Spacing();
    // Le serveur ne le redemandera pas : la direction ne revient que si le
    // nouveau chef la rend.
    AsideText(core::Tr("Vous ne pourrez plus expulser ni nommer de chef."));
  }
  ImGui::Spacing();
  return ConfirmButtons(button, true, art);
}

// La boîte de la question en cours. Dessinée à la RACINE de l'image, pas dans le
// menu : celui-ci est fermé au moment du clic, et une boîte ouverte depuis un
// surgissant mourant avec lui ne s'afficherait jamais.
//
// 🔴 DESSINÉE MÊME SANS QUESTION. Toutes les questions partagent une identité
// (voir `QuestionId`) : une question effacée de l'extérieur — changement de
// carte, nouvelle partie — retrouve donc SA boîte ici, et le cas « aucune » la
// referme. Cessé d'être dessinée, une boîte modale resterait ouverte, invisible,
// et plus rien d'autre ne recevrait la souris.
void DrawQuestion(EntityMenuState& state, const SkinArt& art, EntityMenuAnswer& answer) {
  const char* id = QuestionId(state.question);
  if (state.question_requested) {
    state.question_requested = false;
    ImGui::OpenPopup(id);
  }
  if (!BeginSkinnedModal(id, art)) {
    // Refermée — par un bouton, ou par Échap : la question n'est plus posée.
    if (!ImGui::IsPopupOpen(id)) {
      state.question = EntityMenuQuestion::kNone;
    }
    return;
  }
  bool confirmed = false;
  switch (state.question) {
    case EntityMenuQuestion::kNpcUnload:
      confirmed = AskNpcUnload(state, art);
      break;
    case EntityMenuQuestion::kAccountBlock:
      confirmed = AskAccountBlock(state, art);
      break;
    case EntityMenuQuestion::kEventPoints:
      confirmed = AskEventPoints(state, art);
      break;
    case EntityMenuQuestion::kPartyLeader:
    case EntityMenuQuestion::kPartyExpel:
    case EntityMenuQuestion::kFriendRemove:
      confirmed = AskSocial(state, art);
      break;
    case EntityMenuQuestion::kPetFeed:
      ImGui::TextUnformatted(core::Tr("Nourrir le pet ?"));
      ImGui::Spacing();
      confirmed = ConfirmButtons(core::Tr("Nourrir"), true, art);
      break;
    case EntityMenuQuestion::kHomunculusFeed:
      ImGui::TextUnformatted(core::Tr("Nourrir l'homoncule ?"));
      ImGui::Spacing();
      confirmed = ConfirmButtons(core::Tr("Nourrir"), true, art);
      break;
    case EntityMenuQuestion::kMannerGood:
    case EntityMenuQuestion::kMannerBad:
      confirmed = AskManner(state, art);
      break;
    case EntityMenuQuestion::kNone:
      ImGui::CloseCurrentPopup();
      break;
  }
  if (confirmed) {
    answer.confirmed = state.question_verb;
    answer.confirmed_subject = state.question_subject;
    answer.points = state.points;
  }
  EndSkinnedModal();
}

// UNE LIGNE, partagée par le corps du menu et le sous-menu du staff : deux
// copies auraient divergé au premier ajout. Rend vrai quand le menu doit se
// fermer, c'est-à-dire quand une action a été choisie — une case, elle, ne
// ferme rien. Le trait qui la précède appartient à l'appelant.
[[nodiscard]] bool DrawItem(EntityMenuState& state, const EntityMenuView& view,
                            const EntityMenuItem& item, const SkinArt& art,
                            EntityMenuAnswer& answer) {
  ImGui::PushID(&item);
  if (item.toggle) {
    bool checked = item.checked;
    ImGui::BeginDisabled(item.disabled);
    const bool flipped = SkinCheckBox(item.label.c_str(), &checked, art);
    ImGui::EndDisabled();
    if (flipped) {
      answer.toggled = item.verb;
      answer.toggled_on = checked;
    }
    SkinTooltip(item.tip.c_str());
    ImGui::PopID();
    return false;
  }

  const bool colored = item.danger || item.staff;
  if (colored) {
    ImGui::PushStyleColor(ImGuiCol_Text, item.danger ? kDangerColor : kStaffColor);
  }
  ImGui::BeginDisabled(item.disabled);
  const bool clicked =
      ImGui::Selectable(item.label.c_str(), false, ImGuiSelectableFlags_NoAutoClosePopups);
  ImGui::EndDisabled();
  if (colored) {
    ImGui::PopStyleColor();
  }
  SkinTooltip(item.tip.c_str());
  ImGui::PopID();
  if (!clicked) {
    return false;
  }
  if (item.question != EntityMenuQuestion::kNone) {
    state.question = item.question;
    state.question_requested = true;
    state.question_verb = item.verb;
    state.question_subject = view.subject;
    state.question_name = view.name;
    const bool manner = item.question == EntityMenuQuestion::kMannerGood ||
                        item.question == EntityMenuQuestion::kMannerBad;
    state.points = manner ? kMannerMinutesFirst : 0;
  } else {
    answer.chosen = item.verb;
  }
  // 🔴 DEPUIS LE SOUS-MENU, LA FERMETURE REMONTE LA CHAÎNE et referme aussi le
  // menu parent : le geste est terminé.
  ImGui::CloseCurrentPopup();
  state.open = false;
  return true;
}

void DrawPopup(EntityMenuState& state, const EntityMenuView& view, const SkinArt& art,
               EntityMenuAnswer& answer) {
  const float scale = Scale();
  const bool skinned = art.dresses_windows();
  // ⚠ LA PALETTE DU JEU, POSÉE ICI : ce menu s'ouvre depuis le MONDE, hors de
  // toute fenêtre habillée, et sans elle il retombait sur le thème sombre.
  const SkinStylePushed style = skinned ? PushSkinStyle(art) : SkinStylePushed{};
  ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding,
                      ImVec2(kWindowPaddingPixels * scale, kWindowPaddingPixels * scale));
  ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing,
                      ImVec2(kItemSpacingXPixels * scale, kItemSpacingYPixels * scale));
  const bool visible =
      BeginSkinPopup(kPopupId, ImGuiWindowFlags_NoMove | ImGuiWindowFlags_AlwaysAutoResize |
                                   ImGuiWindowFlags_NoSavedSettings);
  if (visible) {
    // QUI EST VISÉ, en tête : sans cela il faudrait se souvenir de ce qu'on a
    // cliqué.
    if (view.name.empty()) {
      ImGui::TextUnformatted(view.family.c_str());
    } else {
      const bool hangul = view.hangul_name && state.hangul_font != nullptr;
      if (hangul) {
        // Taille nulle : celle du texte courant.
        ImGui::PushFont(state.hangul_font, 0.0F);
      }
      ImGui::TextUnformatted(view.name.c_str());
      if (hangul) {
        ImGui::PopFont();
      }
      ImGui::SameLine();
      ImGui::TextColored(kAsideColor, "· %s", view.family.c_str());
    }
    ImGui::Separator();

    const std::vector<EntityMenuItem>& items = view.items;
    bool close = false;
    for (std::size_t rank = 0; rank < items.size() && !close;) {
      if (!items[rank].submenu) {
        if (items[rank].separator && rank != 0) {
          ImGui::Separator();
        }
        close = DrawItem(state, view, items[rank], art, answer);
        ++rank;
        continue;
      }
      // ── UN SOUS-MENU : « Outils du staff », ou celui qu'une ligne nomme ────
      // 🔴 UN SOUS-MENU D'IMGUI, ET NON UN SECOND SURGISSANT : il s'ouvre au
      // survol, se place à droite — ou à gauche quand l'écran manque — et se
      // referme en chaîne avec son parent.
      const std::string& label = items[rank].submenu_label;
      const bool staff = label.empty();
      const auto inside = [&items, &label](std::size_t at) {
        return at < items.size() && items[at].submenu && items[at].submenu_label == label;
      };
      ImGui::Separator();
      if (staff) {
        ImGui::PushStyleColor(ImGuiCol_Text, kStaffColor);
      }
      // Son identité est son libellé : ceux des sous-menus sont distincts.
      const bool opened = ImGui::BeginMenu(staff ? core::Tr("Outils du staff") : label.c_str());
      if (staff) {
        ImGui::PopStyleColor();
      }
      std::size_t inner = rank;
      if (opened) {
        for (; inside(inner) && !close; ++inner) {
          if (items[inner].separator) {
            ImGui::Separator();
          }
          close = DrawItem(state, view, items[inner], art, answer);
        }
        ImGui::EndMenu();
      }
      // Replié — ou interrompu par un clic — il reste à sauter ce qu'on n'a pas
      // dessiné : sans cela, chaque ligne restante rouvrirait un sous-menu.
      while (inside(inner)) {
        ++inner;
      }
      rank = inner;
    }
    EndSkinPopup();
  } else {
    // Fermé par un clic à côté, ou par Échap : on suit.
    state.open = false;
  }
  ImGui::PopStyleVar(2);
  if (skinned) {
    PopSkinStyle(style);
  }
}

}  // namespace

EntityMenuAnswer DrawEntityMenu(EntityMenuState& state, const EntityMenuView* view,
                                const SkinArt& art) {
  EntityMenuAnswer answer;
  if (state.open_requested) {
    state.open_requested = false;
    // 🔴 RIEN NE S'OUVRE TANT QU'UNE QUESTION EST À L'ÉCRAN. Le menu naît d'un
    // clic dans le monde, qu'une boîte n'arrête pas : ouvert au même niveau de
    // pile, il REMPLACERAIT la boîte, et la question disparaîtrait sans réponse.
    const bool asking = state.question != EntityMenuQuestion::kNone &&
                        ImGui::IsPopupOpen(QuestionId(state.question));
    if (view != nullptr && !view->items.empty() && !asking) {
      state.open = true;
      ImGui::OpenPopup(kPopupId);
      // Le menu s'ouvre au relâchement du bouton : le curseur n'a pas bougé
      // depuis le geste qui l'a demandé.
      ImGui::SetNextWindowPos(ImGui::GetIO().MousePos, ImGuiCond_Always);
    }
  }
  if (state.open && view != nullptr) {
    DrawPopup(state, *view, art, answer);
  } else {
    state.open = false;
    // 🔴 FERMÉ DE L'EXTÉRIEUR, IL SE REFERME POUR DE BON : un surgissant qu'on
    // cesse de dessiner reste ouvert pour ImGui, invisible, et garde la souris.
    if (BeginSkinPopup(kPopupId)) {
      ImGui::CloseCurrentPopup();
      EndSkinPopup();
    }
  }
  DrawQuestion(state, art, answer);
  return answer;
}

}  // namespace ro::ui
