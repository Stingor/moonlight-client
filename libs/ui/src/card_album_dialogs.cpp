// L'ALBUM DE CARTES : ses trois dialogues — la confirmation du sacrifice, le
// poids de tirage d'une demande du staff, la quantité d'un déplacement.
//
// ⚠ TOUS DÉCLARÉS AU NIVEAU DE LA FENÊTRE, jamais dans la boucle de pochettes :
// ouvert depuis une pochette, un surgissant se replacerait au curseur de
// l'élément courant et sauterait d'une image à l'autre.

#include <algorithm>
#include <string>

#include <imgui.h>

#include "ro/ui/modal_box.h"
#include "ro/ui/skin_widgets.h"
#include "ro/ui/skinned_window.h"

#include "card_album_parts.h"
#include "item_grid.h"

namespace ro::ui::card_album {
namespace {

// Place une boîte au milieu de la fenêtre, à son apparition.
void CenterNextBox(float width_pixels) {
  const ImVec2 origin = ImGui::GetWindowPos();
  const ImVec2 size = ImGui::GetWindowSize();
  ImGui::SetNextWindowPos(ImVec2(origin.x + size.x * 0.5F, origin.y + size.y * 0.5F),
                          ImGuiCond_Appearing, ImVec2(0.5F, 0.5F));
  ImGui::SetNextWindowSize(ImVec2(Px(width_pixels), 0.0F), ImGuiCond_Appearing);
}

[[nodiscard]] bool EnterPressed() {
  return ImGui::IsKeyPressed(ImGuiKey_Enter, false) ||
         ImGui::IsKeyPressed(ImGuiKey_KeypadEnter, false);
}

[[nodiscard]] std::string NameOf(const CardAlbumView& view, std::uint32_t card_id) {
  const CardAlbumPocketView* pocket = PocketOf(view, card_id);
  return pocket != nullptr && !pocket->name.empty() ? CardAlbumShortName(pocket->name)
                                                    : std::string(core::Tr("Cette carte"));
}

[[nodiscard]] const CardAlbumArt* ArtOf(const CardAlbumView& view, std::uint32_t card_id,
                                        bool silhouette) {
  for (const CardAlbumArt& art : view.art) {
    if (art.card_id == card_id && art.silhouette == silhouette) {
      return &art;
    }
  }
  return nullptr;
}

// LA CONFIRMATION DU SACRIFICE.
void DrawConfirmBox(CardAlbumWindowState& state, const CardAlbumView& view, const SkinArt& art,
                    CardAlbumWindowAction& action) {
  // ⚠ `###` : l'identité survit à la traduction du titre, et l'ouverture et le
  // dessin doivent porter EXACTEMENT la même chaîne.
  const std::string title = std::string(core::Tr("Sacrifier cette carte ?")) + "###album_sacrifice";
  constexpr float kBoxPixels = 420.0F;
  constexpr float kPicturePixels = 96.0F;
  constexpr float kWrapPixels = 300.0F;
  if (state.confirm_asked) {
    AskModal(title.c_str());
    state.confirm_asked = false;
  }
  CenterNextBox(kBoxPixels);
  if (!BeginSkinnedModal(title.c_str(), art)) {
    // Fermée autrement que par nos boutons (Échap) : c'est un abandon.
    state.confirm_slot = 0;
    return;
  }
  // La carte qu'on s'apprête à donner, en silhouette : c'est la pochette qui
  // s'ouvrira.
  if (const CardAlbumArt* picture = ArtOf(view, state.confirm_card, true);
      picture != nullptr && picture->picture.usable()) {
    const float height = Px(kPicturePixels);
    ImGui::Image(picture->picture.id,
                 ImVec2(height * picture->picture.width / picture->picture.height, height));
    ImGui::SameLine();
  }
  ImGui::BeginGroup();
  if (const CardAlbumPocketView* pocket = PocketOf(view, state.confirm_card);
      pocket != nullptr && !pocket->name.empty()) {
    ImGui::TextUnformatted(pocket->name.c_str());
  }
  if (state.confirm_rest > 0) {
    ImGui::TextColored(kLabelColor, core::Tr("Les %d autres copies seront rangées ensuite."),
                       state.confirm_rest);
  }
  ImGui::Separator();
  ImGui::PushTextWrapPos(ImGui::GetCursorPosX() + Px(kWrapPixels));
  ImGui::TextWrapped(
      "%s", core::Tr("Elle sera CONSOMMÉE et débloquera définitivement sa pochette dans "
                     "l'album. Les copies suivantes pourront alors y être rangées et reprises "
                     "librement. Ce sacrifice ne peut pas être annulé."));
  ImGui::PopTextWrapPos();
  ImGui::EndGroup();
  ImGui::Spacing();
  // Entrée vaut « Sacrifier » : la question a déjà été posée, le geste
  // irréversible est le clic qui l'a ouverte.
  if (SkinButton(core::Tr("Sacrifier"), art) || EnterPressed()) {
    if (state.confirm_slot != 0) {
      action.verb = CardAlbumVerb::kSacrifice;
      action.slot = state.confirm_slot;
      action.card_id = state.confirm_card;
      action.amount = 1;
      action.rest = state.confirm_rest;
    }
    state.confirm_slot = 0;
    ImGui::CloseCurrentPopup();
  }
  ImGui::SameLine();
  if (SkinButton(core::Tr("Annuler"), art)) {
    state.confirm_slot = 0;
    ImGui::CloseCurrentPopup();
  }
  EndSkinnedModal();
}

// LE POIDS DE TIRAGE d'une carte que le staff ajoute à un album d'objets. Il se
// demande parce que c'est la seule chose que le geste ne peut pas deviner : une
// carte commune et une carte de MVP ne se posent pas au même tarif.
void DrawRateBox(CardAlbumWindowState& state, const CardAlbumView& view, const SkinArt& art,
                 CardAlbumWindowAction& action) {
  const std::string title = std::string(core::Tr("Poids de tirage")) + "###album_poids";
  constexpr float kBoxPixels = 400.0F;
  constexpr float kWrapPixels = 360.0F;
  constexpr float kFieldPixels = 110.0F;
  if (state.rate_asked) {
    AskModal(title.c_str());
    state.rate_asked = false;
  }
  CenterNextBox(kBoxPixels);
  if (!BeginSkinnedModal(title.c_str(), art)) {
    return;
  }
  const CardAlbumSourceView* source = nullptr;
  for (const CardAlbumSourceView& s : view.sources) {
    if (s.item_id == state.rate_album) {
      source = &s;
    }
  }
  const std::string card = NameOf(view, state.rate_card);
  const std::string album =
      source != nullptr ? std::string(source->name) : std::string(core::Tr("l'album"));
  ImGui::Text(core::Tr("Ajouter %s à %s"), card.c_str(), album.c_str());
  ImGui::Separator();
  ImGui::PushTextWrapPos(ImGui::GetCursorPosX() + Px(kWrapPixels));
  ImGui::TextWrapped("%s",
                     core::Tr("Le poids est le « Rate » du groupe serveur : une part sur le total "
                              "du tirage, pas un pourcentage. Plus il est haut, plus la carte sort "
                              "souvent."));
  ImGui::PopTextWrapPos();
  if (source != nullptr && source->typical_rate > 0) {
    ImGui::TextColored(kLabelColor, core::Tr("Poids dominant de cet album : %d"),
                       source->typical_rate);
  }
  ImGui::Spacing();
  if (ImGui::IsWindowAppearing()) {
    ImGui::SetKeyboardFocusHere();
  }
  ImGui::SetNextItemWidth(Px(kFieldPixels));
  ImGui::InputInt("##album_poids_valeur", &state.rate_value, 0, 0);
  state.rate_value = std::clamp(state.rate_value, kCardAlbumRateLeast, kCardAlbumRateMost);
  ImGui::Spacing();
  if (SkinButton(core::Tr("Demander l'ajout"), art) || EnterPressed()) {
    action.staff = CardAlbumStaffAsk{.album_id = state.rate_album,
                                     .card_id = state.rate_card,
                                     .intent = CardAlbumIntent::kAdd,
                                     .rate = state.rate_value};
    ImGui::CloseCurrentPopup();
  }
  ImGui::SameLine();
  if (SkinButton(core::Tr("Annuler"), art)) {
    ImGui::CloseCurrentPopup();
  }
  EndSkinnedModal();
}

void DrawQuantityBox(CardAlbumWindowState& state, const SkinArt& art,
                     CardAlbumWindowAction& action) {
  if (!state.quantity_asked) {
    return;
  }
  // Destination NOMMÉE : « Retirer » seul ne dirait pas où part la carte.
  const char* verb = state.quantity.verb == CardAlbumVerb::kWithdraw ? core::Tr("Vers l'inventory")
                                                                     : core::Tr("Vers l'album");
  const AmountAnswer given = DrawCountPrompt(state.quantity.amount, state.quantity.most, verb, art);
  if (given.chosen) {
    action.verb = state.quantity.verb;
    action.slot = state.quantity.slot;
    action.card_id = state.quantity.card_id;
    action.amount = given.amount;
  }
  if (given.closed) {
    state.quantity_asked = false;
  }
}

}  // namespace

void DrawDialogs(CardAlbumWindowState& state, const CardAlbumView& view, const SkinArt& art,
                 CardAlbumWindowAction& action) {
  DrawConfirmBox(state, view, art, action);
  DrawRateBox(state, view, art, action);
  DrawQuantityBox(state, art, action);
}

}  // namespace ro::ui::card_album
