// La règle du double-clic du système. À part de `targeting.cpp` : l'en-tête de
// Windows y apporterait ses macros `min` et `max`.

#include "world/targeting.h"

#if defined(_WIN32)
#include <windows.h>
#endif

namespace moonclient {

DoubleClickRule SystemDoubleClickRule() {
  DoubleClickRule rule;
#if defined(_WIN32)
  rule.delay_ms = GetDoubleClickTime();
  rule.tolerance_x = static_cast<float>(GetSystemMetrics(SM_CXDOUBLECLK));
  rule.tolerance_y = static_cast<float>(GetSystemMetrics(SM_CYDOUBLECLK));
#endif
  return rule;
}

}  // namespace moonclient
