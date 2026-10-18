#include "ro/ui/card_drop.h"

#include <cstring>

#include "ro/ui/shortcut_bar.h"

namespace ro::ui {
namespace {

// Les trois cadres, en `IM_COL32` : le vert et le rouge d'un verdict, et un
// blanc voilé tant que le serveur n'a pas répondu.
constexpr ImU32 kAcceptedFrame = IM_COL32(60, 200, 80, 255);
constexpr ImU32 kRefusedFrame = IM_COL32(220, 50, 50, 255);
constexpr ImU32 kUnknownFrame = IM_COL32(255, 255, 255, 120);
// L'épaisseur du cadre, en pixels : assez pour se voir autour d'une icône,
// assez peu pour ne pas la manger.
constexpr float kFrameThicknessPixels = 2.0F;

}  // namespace

CardDropTaken TakeCardDrop(std::uint16_t equip_slot, const CardDropHint& hint, CardDropSeen& seen) {
  CardDropTaken taken;
  if (equip_slot == 0 || !ImGui::BeginDragDropTarget()) {
    return taken;
  }
  // ⚠ LA CHARGE SE LIT AVANT D'ÊTRE ACCEPTÉE : l'accepter rendrait cette cible
  // seule destinataire du lâcher, même pour un objet qui n'est pas une carte.
  const ImGuiPayload* const held = ImGui::GetDragDropPayload();
  ItemDrag carried;
  const bool item = held != nullptr && held->IsDataType(kItemDragPayload) &&
                    held->DataSize == static_cast<int>(sizeof(ItemDrag));
  if (item) {
    std::memcpy(&carried, held->Data, sizeof(carried));
  }
  if (!item || !carried.card || carried.from != ItemHolder::kInventory ||
      carried.client_slot == equip_slot) {
    ImGui::EndDragDropTarget();
    return taken;
  }
  const CardDropAim aim{.card_slot = carried.client_slot, .equip_slot = equip_slot};
  seen.hovering = aim;
  // ⚠ ACCEPTÉE AVANT LE LÂCHER, et sans le cadre d'ImGui : c'est le verdict de
  // `app` qui colore la case, pas le simple fait d'être survolée.
  if (const ImGuiPayload* const dropped = ImGui::AcceptDragDropPayload(
          kItemDragPayload,
          ImGuiDragDropFlags_AcceptBeforeDelivery | ImGuiDragDropFlags_AcceptNoDrawDefaultRect);
      dropped != nullptr && dropped->IsDelivery()) {
    seen.dropped = aim;
  }
  ImGui::EndDragDropTarget();
  taken.taken = true;
  taken.look = hint.Describes(aim) ? hint.look : CardDropLook::kUnknown;
  return taken;
}

void PaintCardDropFrame(ImDrawList* into, ImVec2 top_left, ImVec2 bottom_right, CardDropLook look) {
  ImU32 color = kUnknownFrame;
  switch (look) {
    case CardDropLook::kUnknown:
      break;
    case CardDropLook::kAccepted:
      color = kAcceptedFrame;
      break;
    case CardDropLook::kRefused:
      color = kRefusedFrame;
      break;
  }
  into->AddRect(top_left, bottom_right, color, 0.0F, ImDrawFlags_None, kFrameThicknessPixels);
}

}  // namespace ro::ui
