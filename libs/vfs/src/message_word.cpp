#include "message_word.h"

#include <functional>
#include <mutex>
#include <set>
#include <string>

#include "ro/core/log.h"

namespace ro::vfs {
namespace {

// Les gabarits déjà signalés : chacun se dit une fois.
std::mutex reported_names_mutex;
std::set<std::string, std::less<>> reported_names;

}  // namespace

std::string Edged(std::string_view lead, const char* text, std::string_view trail) {
  std::string edged(lead);
  edged += text;
  edged += trail;
  return edged;
}

std::string_view NoteMissingWord(std::string_view name) {
  bool first_time = false;
  {
    const std::lock_guard<std::mutex> lock(reported_names_mutex);
    first_time = reported_names.emplace(name).second;
  }
  if (first_time) {
    core::LogWarning("mots du client : {} ne s'imprime pas, son texte restera vide", name);
  }
  return {};
}

}  // namespace ro::vfs
