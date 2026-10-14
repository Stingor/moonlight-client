// Les lignes « Équipement mis/retiré ». Voir l'en-tête.

#include "character/equip_change_line.h"

#include <utility>

#include "ro/core/i18n.h"
#include "ro/core/printed.h"
#include "ro/sim/equip_location.h"

#include "character/equip_attribute_lines.h"

namespace moonclient {
namespace {

// Le rouge d'un refus, celui des refus de pose.
constexpr std::uint32_t kEquipSwitchRefusalColor = 0xFF0000;

[[nodiscard]] const char* RefusalText(ro::sim::EquipSwitchRefusal refusal) {
  switch (refusal) {
    case ro::sim::EquipSwitchRefusal::kNothingToReplace:
      return ro::core::Tr("Il n'y a aucun équipement à échanger.");
    case ro::sim::EquipSwitchRefusal::kCannotRegister:
      return ro::core::Tr("Impossible de ranger cet item dans la fenêtre d'échange.");
    case ro::sim::EquipSwitchRefusal::kCannotSwap:
      return ro::core::Tr("Impossible d'échanger l'équipement.");
    case ro::sim::EquipSwitchRefusal::kNone:
      break;
  }
  return nullptr;
}

void AddRefusal(std::vector<EquipChangeLine>& lines, ro::sim::EquipSwitchRefusal refusal) {
  if (const char* text = RefusalText(refusal); text != nullptr) {
    lines.push_back(EquipChangeLine{
        .text = text, .color = kEquipSwitchRefusalColor, .type = ro::sim::ChatType::kPublic});
  }
}

// ⚠ DES MUNITIONS SORTIES DU JEU DE RECHANGE DISENT AUSSI LEUR RETRAIT : le
// client d'origine passe par le même chemin que pour les munitions portées.
void AddAmmoTakenOff(std::vector<EquipChangeLine>& lines, const ro::sim::EquipSwitchNews& news,
                     std::string_view item_name) {
  if (news.location == ro::sim::kEquipAmmo && !item_name.empty()) {
    lines.push_back(EquipChangeLineOf(item_name, false));
  }
}

// Une ligne verte de la case 7.
[[nodiscard]] EquipChangeLine Placed(std::string text) {
  return EquipChangeLine{.text = std::move(text), .color = EquipAttributesColor(true)};
}

}  // namespace

EquipChangeLine EquipChangeLineOf(std::string_view item_name, bool equipped) {
  const std::string name(item_name);
  // ⚠ LA PAIRE DE COULEURS DES ATTRIBUTS : le client d'origine écrit les deux
  // cases avec les mêmes, vert à la pose et rouge au retrait.
  return EquipChangeLine{
      .text = equipped ? ro::core::Printed(ro::core::Tr("%s a été équipé."), name.c_str())
                       : ro::core::Printed(ro::core::Tr("%s a été retiré."), name.c_str()),
      .color = EquipAttributesColor(equipped)};
}

std::vector<EquipChangeLine> EquipSwitchLines(const ro::sim::EquipSwitchNews& news,
                                              std::string_view item_name) {
  std::vector<EquipChangeLine> lines;
  const std::string name(item_name);
  switch (news.event) {
    case ro::sim::EquipSwitchEvent::kPlaced:
      if (!name.empty()) {
        lines.push_back(Placed(ro::core::Printed(
            ro::core::Tr("%s a été placé dans la fenêtre d'échange d'équipement."), name.c_str())));
      }
      break;
    case ro::sim::EquipSwitchEvent::kRemoved:
      AddAmmoTakenOff(lines, news, item_name);
      if (!name.empty()) {
        lines.push_back(Placed(ro::core::Printed(
            ro::core::Tr("%s a été retiré de la fenêtre d'échange d'équipement."), name.c_str())));
      }
      break;
    case ro::sim::EquipSwitchEvent::kRemoveRefused:
      AddRefusal(lines, news.refusal);
      AddAmmoTakenOff(lines, news, item_name);
      break;
    case ro::sim::EquipSwitchEvent::kPlaceRefused:
    case ro::sim::EquipSwitchEvent::kSwapRefused:
      AddRefusal(lines, news.refusal);
      break;
  }
  return lines;
}

}  // namespace moonclient
