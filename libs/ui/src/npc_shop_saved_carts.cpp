#include "ro/ui/npc_shop_saved_carts.h"

#include <algorithm>
#include <array>
#include <cstring>
#include <string>
#include <string_view>
#include <vector>

#include "ro/core/i18n.h"
#include "ro/core/printed.h"
#include "ro/ui/skin_widgets.h"

#include "npc_shop_saved_carts_menu.h"
#include "ui_scale.h"
#include "ui_text.h"

namespace ro::ui {
namespace {

constexpr const char* kMenuId = "##npc_shop_saved_carts";
constexpr float kNameInputPixels = 160.0F;

// Une saisie qui ne porte que des blancs ne nomme rien : le carnet la
// refuserait.
[[nodiscard]] bool Named(const char* typed) {
  for (; *typed != '\0'; ++typed) {
    if (*typed != ' ' && *typed != '\t' && *typed != '\n' && *typed != '\r') {
      return true;
    }
  }
  return false;
}

// Le nom d'un objet que la boutique ne montre pas : celui de l'enregistrement,
// ou son identifiant s'il n'en avait pas.
[[nodiscard]] std::string ShownName(const NpcShopSavedCartLineView& line) {
  if (!line.item_name.empty()) {
    return std::string(line.item_name);
  }
  return core::Printed("#%u", static_cast<unsigned>(line.item_id));
}

[[nodiscard]] std::string Joined(const std::vector<std::string>& names) {
  std::string joined;
  for (const std::string& name : names) {
    if (!joined.empty()) {
      joined += ", ";
    }
    joined += name;
  }
  return joined;
}

void CopyName(std::array<char, kNpcShopCartNameInputBytes>& into, std::string_view name) {
  into.fill('\0');
  std::memcpy(into.data(), name.data(), std::min(name.size(), into.size() - 1));
}

void DrawSaveRow(NpcShopWindowState& state, const NpcShopView& view, const SkinArt& art,
                 NpcShopWindowAction& action) {
  NpcShopSavedCartsState& saved = state.saved;
  ImGui::TextDisabled("%s", core::Tr("Enregistrer le panier"));
  ImGui::SetNextItemWidth(kNameInputPixels * Scale());
  ImGui::InputTextWithHint("##nom_panier", core::Tr("nom du panier"), saved.name.data(),
                           saved.name.size());
  ImGui::SameLine();
  // ⚠ UN PANIER VIDE NE S'ENREGISTRE PAS : il ne rechargerait rien.
  const bool empty_cart = state.cart.empty();
  ImGui::BeginDisabled(empty_cart || !Named(saved.name.data()));
  if (SkinButton(core::Tr("Enregistrer"), art)) {
    action.cart_book = NpcShopCartBookAsk{.verb = NpcShopCartBookVerb::kSave,
                                          .name = std::string(saved.name.data())};
    saved.name.fill('\0');
  }
  ImGui::EndDisabled();
  if (empty_cart && ImGui::IsItemHovered(ImGuiHoveredFlags_AllowWhenDisabled)) {
    SkinTooltip(core::Tr("Un panier vide ne s'enregistre pas."));
  }
  ImGui::SameLine();
  ImGui::AlignTextToFramePadding();
  ImGui::TextDisabled(core::Tr("%zu / %zu"), view.saved_carts.size(), view.saved_cart_capacity);
  ImGui::TextDisabled("%s", core::Tr("Un nom déjà pris met ce panier à jour."));
}

void DrawRowGestures(NpcShopWindowState& state, const NpcShopView& view, std::size_t index,
                     const SkinArt& art, NpcShopWindowAction& action) {
  NpcShopSavedCartsState& saved = state.saved;
  const int rank = static_cast<int>(index);
  if (saved.renaming == rank) {
    ImGui::SetNextItemWidth(kNameInputPixels * Scale());
    ImGui::InputText("##renommer_panier", saved.rename.data(), saved.rename.size());
    ImGui::SameLine();
    ImGui::BeginDisabled(!Named(saved.rename.data()));
    if (SkinButton(core::Tr("OK"), art)) {
      action.cart_book = NpcShopCartBookAsk{.verb = NpcShopCartBookVerb::kRename,
                                            .index = index,
                                            .name = std::string(saved.rename.data())};
      saved.renaming = -1;
    }
    ImGui::EndDisabled();
    ImGui::SameLine();
    if (SkinButton(core::Tr("Annuler"), art)) {
      saved.renaming = -1;
    }
    return;
  }
  if (saved.removing == rank) {
    ImGui::AlignTextToFramePadding();
    ImGui::TextUnformatted(core::Tr("Supprimer ?"));
    ImGui::SameLine();
    if (SkinButton(core::Tr("Oui"), art)) {
      action.cart_book = NpcShopCartBookAsk{.verb = NpcShopCartBookVerb::kRemove, .index = index};
      saved.removing = -1;
    }
    ImGui::SameLine();
    if (SkinButton(core::Tr("Non"), art)) {
      saved.removing = -1;
    }
    return;
  }
  // Rien ne s'ajoute au panier pendant qu'une transaction attend : c'est la
  // règle des lignes de la liste.
  ImGui::BeginDisabled(view.awaiting);
  if (SkinButton(core::Tr("Charger"), art)) {
    saved.load_note =
        NpcShopCartLoadNote(LoadNpcShopSavedCart(state.cart, view, view.saved_carts[index]));
    ImGui::CloseCurrentPopup();
  }
  ImGui::EndDisabled();
  if (ImGui::IsItemHovered()) {
    SkinTooltip(
        core::Tr("Ajoute ces items au panier courant ; rien ne s'achète avant "
                 "« Acheter »."));
  }
  ImGui::SameLine();
  if (SkinButton(core::Tr("Renommer"), art)) {
    saved.renaming = rank;
    saved.removing = -1;
    CopyName(saved.rename, view.saved_carts[index].name);
  }
  ImGui::SameLine();
  if (SkinButton(core::Tr("Supprimer"), art)) {
    saved.removing = rank;
    saved.renaming = -1;
  }
}

void DrawList(NpcShopWindowState& state, const NpcShopView& view, const SkinArt& art,
              NpcShopWindowAction& action) {
  if (view.saved_carts.empty()) {
    ImGui::TextDisabled("%s", core::Tr("Aucun panier enregistré."));
    return;
  }
  for (std::size_t index = 0; index < view.saved_carts.size(); ++index) {
    const NpcShopSavedCartView& cart = view.saved_carts[index];
    ImGui::PushID(static_cast<int>(index));
    ImGui::AlignTextToFramePadding();
    ImGui::Text(core::Tr("%.*s — %zu item(s)"), static_cast<int>(cart.name.size()),
                cart.name.data(), cart.lines.size());
    ImGui::SameLine();
    DrawRowGestures(state, view, index, art, action);
    ImGui::PopID();
  }
}

}  // namespace

NpcShopCartLoad LoadNpcShopSavedCart(std::vector<NpcShopCartLine>& cart, const NpcShopView& view,
                                     const NpcShopSavedCartView& saved) {
  NpcShopCartLoad load;
  for (const NpcShopSavedCartLineView& line : saved.lines) {
    if (line.amount <= 0) {
      continue;
    }
    const auto offer = std::ranges::find_if(view.offers, [&line](const NpcShopOfferView& on) {
      return on.item.item_id == line.item_id;
    });
    if (offer == view.offers.end()) {
      load.absent.push_back(ShownName(line));
      continue;
    }
    // La ligne de la liste n'offre aucun bouton quand la pile est pleine : le
    // chargement ne prend pas davantage qu'elle.
    if (offer->most < 1) {
      load.full.push_back(std::string(offer->item.name));
      continue;
    }
    const auto wanted = static_cast<int>(std::min<std::int64_t>(line.amount, offer->most));
    AddToNpcShopCart(cart, NpcShopCartLine{.item_id = offer->item.item_id,
                                           .amount = wanted,
                                           .unit_price = offer->price,
                                           .most = offer->most});
  }
  return load;
}

std::string NpcShopCartLoadNote(const NpcShopCartLoad& load) {
  std::string note;
  if (!load.absent.empty()) {
    note = core::Printed(core::Tr("%zu item(s) absent(s) de cette boutique : %s"),
                         load.absent.size(), Joined(load.absent).c_str());
  }
  if (!load.full.empty()) {
    if (!note.empty()) {
      note += '\n';
    }
    note += core::Printed(core::Tr("%zu item(s) non ajouté(s), pile déjà pleine : %s"),
                          load.full.size(), Joined(load.full).c_str());
  }
  return note;
}

void DrawNpcShopSavedCarts(NpcShopWindowState& state, const NpcShopView& view, const SkinArt& art,
                           NpcShopWindowAction& action) {
  // LA FLÈCHE EST CELLE D'UNE LISTE DÉROULANTE : le menu s'ouvre dessous, et
  // ses saisies y restent ouvertes tant qu'on ne clique pas à côté.
  const char* shown = core::Tr("Paniers");
  ImGui::SetNextItemWidth(ImGui::CalcTextSize(shown).x + SkinComboChromeWidth(art));
  const bool open = BeginSkinCombo(kMenuId, shown, art);
  if (!open) {
    if (ImGui::IsItemHovered()) {
      SkinTooltip(core::Tr("Enregistre le panier, ou recharge un panier enregistré."));
    }
  } else {
    DrawSaveRow(state, view, art, action);
    if (!view.saved_cart_report.empty()) {
      ColoredText(kTradeRefusedText, view.saved_cart_report);
    }
    ImGui::Separator();
    DrawList(state, view, art, action);
    EndSkinCombo();
  }
  if (!state.saved.load_note.empty()) {
    DrawTradeHint(state.saved.load_note);
  }
}

}  // namespace ro::ui
