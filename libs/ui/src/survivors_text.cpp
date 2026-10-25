#include "survivors_text.h"

#include "ro/core/i18n.h"

namespace ro::ui {

const char* SurvivorsText(const char* french) { return french == nullptr ? "" : core::Tr(french); }

}  // namespace ro::ui
