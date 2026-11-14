// LES LIBELLÉS DE L'ONGLET « SERVEUR », traduits à la lecture. Voir
// `ItemSheetServerNumberLabel` dans `ro/ui/item_sheet_editor.h`.

#include "ro/core/i18n.h"
#include "ro/ui/item_sheet_editor.h"

namespace ro::ui {

const char* ItemSheetServerNumberLabel(const ItemSheetServerNumber& number) {
  return core::Tr(number.label);
}

const char* ItemSheetServerScriptLabel(const ItemSheetServerScript& script) {
  return core::Tr(script.label);
}

}  // namespace ro::ui
