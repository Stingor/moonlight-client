#include "game/profile_yaml.h"

#include <fstream>
#include <sstream>
#include <system_error>

namespace moonclient {
namespace {

constexpr const char* kCharactersField = "characters";
constexpr const char* kCharacterField = "character";
constexpr const char* kProfileField = "profile";

}  // namespace

std::optional<std::string> ReadProfileFile(const std::filesystem::path& path) {
  std::error_code missing;
  if (!std::filesystem::exists(path, missing)) {
    return std::string();
  }
  std::ifstream file(path, std::ios::binary);
  if (!file) {
    return std::nullopt;
  }
  std::ostringstream text;
  text << file.rdbuf();
  return text.str();
}

std::optional<YAML::Node> OpenProfileText(const std::string& text, int version) {
  YAML::Node whole;
  try {
    whole = YAML::Load(text);
  } catch (const YAML::Exception&) {
    return std::nullopt;
  }
  if (!whole || whole.IsNull()) {
    return YAML::Node();
  }
  if (!whole.IsMap()) {
    return std::nullopt;
  }
  const YAML::Node read_version = whole[kProfileVersionField];
  try {
    if (!read_version || !read_version.IsScalar() || read_version.as<int>() != version) {
      return std::nullopt;
    }
  } catch (const YAML::Exception&) {
    return std::nullopt;
  }
  return whole;
}

void WriteBindings(YAML::Emitter& out, const std::vector<ProfileBinding>& characters) {
  out << YAML::Key << kCharactersField << YAML::Value << YAML::BeginSeq;
  for (const ProfileBinding& binding : characters) {
    out << YAML::BeginMap;
    out << YAML::Key << kCharacterField << YAML::Value << binding.character;
    out << YAML::Key << kProfileField << YAML::Value << binding.profile;
    out << YAML::EndMap;
  }
  out << YAML::EndSeq;
}

std::vector<ProfileBinding> ReadBindings(const YAML::Node& whole) {
  std::vector<ProfileBinding> read;
  const YAML::Node characters = whole[kCharactersField];
  if (!characters || !characters.IsSequence()) {
    return read;
  }
  for (const YAML::Node& entry : characters) {
    if (!entry.IsMap() || !entry[kCharacterField] || !entry[kProfileField] ||
        !entry[kCharacterField].IsScalar() || !entry[kProfileField].IsScalar()) {
      continue;
    }
    std::erase_if(read, [&entry](const ProfileBinding& binding) {
      return binding.character == entry[kCharacterField].Scalar();
    });
    read.push_back(ProfileBinding{.character = entry[kCharacterField].Scalar(),
                                  .profile = entry[kProfileField].Scalar()});
  }
  return read;
}

}  // namespace moonclient
