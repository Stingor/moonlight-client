#include "ro/ui/chat_url_prompt.h"

#include <algorithm>
#include <array>
#include <cstddef>
#include <cstdio>
#include <string>
#include <string_view>

#include <imgui.h>
#include <imgui_internal.h>

#include "ro/core/i18n.h"
#include "ro/core/text.h"
#include "ro/core/web_address.h"
#include "ro/ui/modal_box.h"
#include "ro/ui/skin_tooltip.h"
#include "ro/ui/skin_widgets.h"
#include "ro/ui/skinned_window.h"
#include "ro/ui/window_name.h"

#include "ui_scale.h"
#include "ui_text.h"

namespace ro::ui {
namespace {

// Où la phrase se replie, en pixels d'interface : une adresse de deux cents
// caractères ne doit pas élargir la boîte jusqu'au bord de l'écran.
constexpr float kPromptWrapPixels = 440.0F;
// Une phrase traduite, avec un nom d'hôte dedans.
constexpr std::size_t kPromptLineBytes = 512;
// Le plus long nom d'hôte qu'une phrase reprend : la borne d'un nom de domaine.
// Au-delà, c'est une adresse fabriquée, et la phrase doit rester entière.
constexpr std::size_t kMostHostBytes = 253;

// L'hôte tel qu'une phrase le reprend : borné, et coupé ENTRE deux caractères
// — `snprintf` couperait sinon au milieu d'un, et la phrase finirait en losange.
[[nodiscard]] std::string ShownHost(std::string_view host) {
  return std::string(core::WithoutDanglingUtf8(host.substr(0, kMostHostBytes)));
}

// 🔴 SANS VOILE : assombrir tout l'écran pour une confirmation d'un clic serait
// disproportionné, et le chat doit rester lisible pendant qu'on décide. La
// boîte reste modale — un clic à côté ne déclenche rien ailleurs.
//
// ⚠ LA COULEUR SE LIT À L'OUVERTURE ET SE RETIRE APRÈS LA FERMETURE : la boîte
// habillée pousse ses propres couleurs par-dessus, qu'il ne faut pas dépiler à
// sa place.
constexpr ImU32 kNoVeil = IM_COL32(0, 0, 0, 0);

// UN TEXTE EN GRAS : le faux gras du client, le même texte repassé un pixel
// plus loin. La police n'a qu'une graisse chargée.
//
// ⚠ LA PREMIÈRE PASSE EST UN TEXTE D'IMGUI, pas un dessin : il prend sa place,
// se replie comme les autres, et se relit dans le journal d'ImGui. La seconde
// suit les MÊMES coupures.
void BoldText(std::string_view text, ImU32 color) {
  if (text.empty()) {
    return;
  }
  const char* const begin = text.data();
  const char* const end = begin + text.size();
  const ImVec2 at = ImGui::GetCursorScreenPos();
  const float wrap = ImGui::CalcWrapWidthForPos(at, ImGui::GetCurrentWindow()->DC.TextWrapPos);
  ColoredText(color, text);
  ImGui::GetWindowDrawList()->AddText(ImGui::GetFont(), ImGui::GetFontSize(),
                                      ImVec2(at.x + kFakeBoldPixels * Scale(), at.y), color, begin,
                                      end, wrap);
}

// UNE LIGNE D'AVERTISSEMENT : le signe peint, puis la phrase repliée à sa
// droite.
void WarningLine(const char* text, const SkinArt& art) {
  const float side = ImGui::GetFontSize();
  const ImVec2 at = ImGui::GetCursorScreenPos();
  PaintWarningMark(*ImGui::GetWindowDrawList(), at, side);
  ImGui::SetCursorScreenPos(ImVec2(at.x + side + ImGui::GetStyle().ItemSpacing.x, at.y));
  ColoredText(art.look().warn_text, text);
}

// 🔴 SOUS LA SOURIS, LÀ OÙ LE JOUEUR VIENT DE CLIQUER, et toujours dans
// l'écran. La boîte habillée naît au centre ; on la déplace donc à son
// apparition, puis on la garde dans l'écran une fois sa taille connue — elle ne
// l'est qu'à l'image suivante, la boîte se mesurant d'abord sans se montrer.
void KeepUnderMouse() {
  const ImVec2 now = ImGui::GetWindowPos();
  ImVec2 wanted = ImGui::IsWindowAppearing() ? ImGui::GetMousePos() : now;
  const ImVec2 size = ImGui::GetWindowSize();
  const ImVec2 screen = ImGui::GetIO().DisplaySize;
  wanted.x = std::clamp(wanted.x, 0.0F, std::max(0.0F, screen.x - size.x));
  wanted.y = std::clamp(wanted.y, 0.0F, std::max(0.0F, screen.y - size.y));
  if (wanted.x != now.x || wanted.y != now.y) {
    ImGui::SetWindowPos(wanted);
  }
}

// LA BOÎTE ÉTAIT-ELLE MONTRÉE à l'image précédente ?
//
// ⚠ C'EST CE QUI DISTINGUE UNE BOÎTE CONGÉDIÉE — Échap, clic au dehors — d'une
// boîte fermée par ses boutons : la première rend « Annuler », la seconde a
// déjà rendu sa réponse.
//
// 🔴 LE MAGASIN EST CELUI DE L'APPELANT, relevé AVANT d'ouvrir la boîte : une
// fois dedans, « la fenêtre courante » est la boîte elle-même, et la marque
// posée là ne se relirait pas de dehors à l'image où elle s'est fermée.
class ShownMark {
 public:
  explicit ShownMark(const char* name)
      : kept_(ImGui::GetStateStorage()), key_(ImGui::GetID(name)) {}
  [[nodiscard]] bool was() const { return kept_->GetBool(key_, false); }
  void Note(bool shown) { kept_->SetBool(key_, shown); }

 private:
  ImGuiStorage* kept_;
  ImGuiID key_;
};

// Ferme la boîte depuis un de ses boutons : sa réponse est déjà rendue.
void CloseByButton(ShownMark& mark) {
  ImGui::CloseCurrentPopup();
  mark.Note(false);
}

// LES LIGNES D'AVERTISSEMENT, une par drapeau levé, dans l'ordre du plus
// courant au plus perfide.
void Warnings(const ChatUrlPrompt& prompt, const SkinArt& art) {
  if (prompt.unreadable) {
    WarningLine(core::Tr("Adresse illisible : elle peut cacher un autre site."), art);
  }
  if ((prompt.warnings & core::kWebWarnPlainHttp) != 0) {
    WarningLine(core::Tr("Connexion non chiffrée (http) : ce qui transite peut être lu ou "
                         "modifié en chemin."),
                art);
  }
  if ((prompt.warnings & core::kWebWarnUserInfo) != 0) {
    std::array<char, kPromptLineBytes> line{};
    std::snprintf(line.data(), line.size(),
                  core::Tr("L'adresse contient « @ » : ce qui précède n'est pas le site, c'est "
                           "%s qui s'ouvrira."),
                  ShownHost(prompt.host).c_str());
    WarningLine(line.data(), art);
  }
  if ((prompt.warnings & core::kWebWarnPunycode) != 0) {
    WarningLine(core::Tr("Nom de domaine internationalisé (xn--) : il peut imiter un autre site "
                         "lettre pour lettre."),
                art);
  }
  if ((prompt.warnings & core::kWebWarnNonAsciiHost) != 0) {
    WarningLine(
        core::Tr(
            "Le nom du site contient des caractères non latins : il peut imiter un autre site."),
        art);
  }
}

}  // namespace

void DrawChatUrlPrompt(ChatUrlPrompt& prompt, const SkinArt& art) {
  // ⚠ L'IDENTITÉ NE BOUGE PAS AVEC LA LANGUE : c'est elle qu'on ouvre, elle
  // qu'on dessine, et un titre retraduit entre les deux ouvrirait une boîte et
  // en chercherait une autre.
  static WindowName name(core::TrText("Ouvrir cette adresse ?"), "chat_url_prompt");
  const char* const id = name.Get();
  if (prompt.asked) {
    prompt.asked = false;
    prompt.always = false;
    prompt.answer = ChatUrlAnswer::kNone;
    AskModal(id);
  }
  // 🔴 DESSINÉE MÊME SANS QUESTION : c'est ici qu'une boîte congédiée sans ses
  // boutons rend sa réponse. Elle n'ouvre rien, et c'est le bon défaut pour un
  // garde-fou.
  ShownMark mark(id);
  ImGui::PushStyleColor(ImGuiCol_ModalWindowDimBg, kNoVeil);
  if (!BeginSkinnedModal(id, art)) {
    ImGui::PopStyleColor();
    if (mark.was() && prompt.answer == ChatUrlAnswer::kNone) {
      prompt.answer = ChatUrlAnswer::kCancel;
      prompt.always = false;
    }
    mark.Note(false);
    return;
  }
  mark.Note(true);
  KeepUnderMouse();
  ImGui::PushTextWrapPos(ImGui::GetCursorPosX() + kPromptWrapPixels * Scale());

  ImGui::TextUnformatted(core::Tr("Ce lien vient d'un autre joueur."));
  ImGui::TextUnformatted(core::Tr("Il ouvrira votre navigateur sur :"));
  ImGui::Spacing();
  // 🔴 L'ADRESSE QUI PARTIRA VRAIMENT, schéma compris, et non celle qui est
  // écrite dans la ligne : le texte montré et la destination peuvent n'avoir
  // aucun rapport, et c'est tout l'objet de la boîte.
  ColoredText(art.look().link, prompt.openable);
  ImGui::Spacing();
  // 🔴 L'HÔTE À PART, EN GRAS : c'est la seule partie de l'adresse qui dit OÙ
  // l'on va, et la plus facile à maquiller au milieu d'un long chemin.
  BoldText(core::Tr("Site :"), art.look().text);
  ImGui::SameLine();
  BoldText(prompt.host.empty() ? std::string_view(core::Tr("ce site")) : prompt.host,
           art.look().text);
  if (prompt.warnings != 0 || prompt.unreadable) {
    ImGui::Spacing();
    Warnings(prompt, art);
  }
  // ⚠ « TOUJOURS » AFFAIBLIT LE GARDE-FOU : il n'est offert qu'à une adresse
  // sans aucun avertissement — donc en https — et dont le site est connu.
  // L'appelant le propose ; la boîte tient la règle quand même, parce que c'est
  // elle qui la montre.
  const bool offers_always =
      prompt.offer_always && prompt.warnings == 0 && !prompt.unreadable && !prompt.host.empty();
  if (!offers_always) {
    prompt.always = false;
  } else {
    ImGui::Spacing();
    std::array<char, kPromptLineBytes> always{};
    std::snprintf(always.data(), always.size(), core::Tr("Toujours ouvrir les liens de %s"),
                  ShownHost(prompt.host).c_str());
    static_cast<void>(SkinCheckBox(always.data(), &prompt.always, art));
  }
  ImGui::PopTextWrapPos();
  ImGui::Spacing();

  if (SkinButton(core::Tr("Ouvrir"), art)) {
    prompt.answer = ChatUrlAnswer::kOpen;
    CloseByButton(mark);
  }
  ImGui::SameLine();
  if (SkinButton(core::Tr("Annuler"), art)) {
    prompt.answer = ChatUrlAnswer::kCancel;
    CloseByButton(mark);
  }
  ImGui::SameLine();
  // COPIER PLUTÔT QU'OUVRIR : le geste prudent doit être à portée ici aussi, pas
  // seulement dans le menu qu'on vient de quitter.
  if (SkinButton(core::Tr("Copier"), art)) {
    prompt.answer = ChatUrlAnswer::kCopy;
    CloseByButton(mark);
  }
  // 🔴 « TOUJOURS » NE VAUT QU'AVEC « OUVRIR » : cocher puis renoncer n'admet
  // rien. Sans cela, une case cochée par mégarde survivrait à un « Annuler ».
  if (prompt.answer != ChatUrlAnswer::kNone && prompt.answer != ChatUrlAnswer::kOpen) {
    prompt.always = false;
  }
  EndSkinnedModal();
  ImGui::PopStyleColor();
}

// 🔴 CETTE CONFIRMATION-CI EST LA PLUS INSISTANTE DES DEUX. Ouvrir un lien
// révèle l'adresse du joueur une fois, sur un geste délibéré ; admettre un site
// la lui révèle à CHAQUE SURVOL, jusqu'à ce qu'il revienne dessus. Le texte dit
// donc ce qui est accordé, pas ce qu'on y gagne.
void DrawChatHostPrompt(ChatHostPrompt& prompt, const SkinArt& art) {
  static WindowName name(core::TrText("Autoriser ce site ?"), "chat_host_prompt");
  const char* const id = name.Get();
  if (prompt.asked) {
    prompt.asked = false;
    prompt.granted = false;
    AskModal(id);
  }
  ImGui::PushStyleColor(ImGuiCol_ModalWindowDimBg, kNoVeil);
  if (!BeginSkinnedModal(id, art)) {
    ImGui::PopStyleColor();
    return;
  }
  KeepUnderMouse();
  ImGui::TextUnformatted(
      core::Tr("Les images de ce site se chargeront\nautomatiquement, au simple survol :"));
  ImGui::Spacing();
  BoldText(prompt.host, art.look().link);
  ImGui::Spacing();
  ImGui::TextUnformatted(
      core::Tr("Ce serveur pourra alors voir que vous êtes\nen ligne, et depuis quelle adresse."));
  ImGui::Spacing();
  ColoredText(art.look().faint_text, core::Tr("Réglages du chat pour revenir dessus."));
  ImGui::Spacing();
  if (SkinButton(core::Tr("Autoriser"), art)) {
    prompt.granted = true;
    ImGui::CloseCurrentPopup();
  }
  ImGui::SameLine();
  if (SkinButton(core::Tr("Annuler"), art)) {
    ImGui::CloseCurrentPopup();
  }
  EndSkinnedModal();
  ImGui::PopStyleColor();
}

}  // namespace ro::ui
