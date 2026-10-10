#include "ro/ui/window_name.h"

#include "ro/core/i18n.h"

namespace ro::ui {

const char* WindowName::Get() {
  const std::size_t serial = core::LanguageSerial();
  if (serial != composed_for_) {
    composed_ = core::Tr(french_);
    composed_ += "###";
    composed_ += id_;
    composed_for_ = serial;
  }
  return composed_.c_str();
}

}  // namespace ro::ui
