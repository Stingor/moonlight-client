#pragma once

#include "ro/ui/cash_shop_window.h"

namespace moonclient {

// LA BOUTIQUE DE VOTE dans la trame de l'interface : l'état de la fenêtre, sa
// vue, et ce qu'elle a demandé. Vue nulle, rien ne se dessine : c'est le
// serveur qui ouvre.
struct CashShopWindows {
  ro::ui::CashShopWindowState state;
  const ro::ui::CashShopView* view = nullptr;
  ro::ui::CashShopWindowAction answer;
};

}  // namespace moonclient
