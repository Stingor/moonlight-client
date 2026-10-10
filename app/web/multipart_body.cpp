// Le corps d'un formulaire en plusieurs morceaux : voir `web_transport.h`.

#include <string_view>

#include "web/web_transport.h"

namespace moonclient::web {
namespace {

constexpr std::string_view kBoundarySeed = "----moonclient-part";
constexpr std::string_view kLineEnd = "\r\n";
constexpr std::string_view kDashes = "--";
// Ce qu'on ajoute à la frontière quand une partie la contient déjà.
constexpr char kBoundaryFiller = 'x';

[[nodiscard]] bool Collides(const std::vector<WebPart>& parts, std::string_view boundary) {
  for (const WebPart& part : parts) {
    if (part.content.find(boundary) != std::string::npos) {
      return true;
    }
  }
  return false;
}

}  // namespace

MultipartBody BuildMultipartBody(const std::vector<WebPart>& parts) {
  MultipartBody built;
  built.boundary = std::string(kBoundarySeed);
  while (Collides(parts, built.boundary)) {
    built.boundary.push_back(kBoundaryFiller);
  }
  std::string& body = built.body;
  for (const WebPart& part : parts) {
    body += kDashes;
    body += built.boundary;
    body += kLineEnd;
    body += "Content-Disposition: form-data; name=\"";
    body += part.name;
    body += '"';
    if (!part.file_name.empty()) {
      body += "; filename=\"";
      body += part.file_name;
      body += '"';
    }
    body += kLineEnd;
    if (!part.content_type.empty()) {
      body += "Content-Type: ";
      body += part.content_type;
      body += kLineEnd;
    }
    body += kLineEnd;
    body += part.content;
    body += kLineEnd;
  }
  body += kDashes;
  body += built.boundary;
  body += kDashes;
  body += kLineEnd;
  return built;
}

}  // namespace moonclient::web
