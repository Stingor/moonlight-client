#pragma once

#include <fstream>
#include <map>
#include <string>
#include <vector>

#include "patcher/patch_fetcher.h"

namespace moonclient::test_support {

// Un réseau qui sert ce qu'on lui a confié, et compte ce qu'on lui demande.
//
// ⚠ IL RESPECTE LA BORNE comme le vrai : un contenu plus long que
// `limit_bytes` échoue sans rien écrire.
class FakePatchFetcher final : public patcher::PatchFetcher {
 public:
  std::map<std::string, std::string> served;
  std::vector<std::string> requested;

  ro::core::Result<std::string> FetchText(const std::string& url, std::uint64_t limit_bytes,
                                          std::stop_token /*stop*/) override {
    requested.push_back(url);
    const auto found = served.find(url);
    if (found == served.end()) {
      return ro::core::Fail(ro::core::ErrorCode::kIoFailure, url + " : 404");
    }
    if (found->second.size() > limit_bytes) {
      return ro::core::Fail(ro::core::ErrorCode::kIoFailure, url + " : au-delà de la borne");
    }
    return found->second;
  }

  ro::core::Result<void> FetchToFile(const std::string& url,
                                     const std::filesystem::path& destination,
                                     std::uint64_t limit_bytes,
                                     const patcher::FetchProgressSink& on_progress,
                                     std::stop_token stop) override {
    auto text = FetchText(url, limit_bytes, stop);
    if (!text) {
      return std::unexpected(text.error());
    }
    std::ofstream(destination, std::ios::binary) << *text;
    if (on_progress) {
      on_progress(patcher::FetchProgress{text->size(), text->size()});
    }
    return {};
  }
};

}  // namespace moonclient::test_support
