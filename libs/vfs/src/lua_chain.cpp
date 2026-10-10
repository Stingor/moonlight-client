#include "ro/vfs/lua_chain.h"

#include <string>
#include <utility>

namespace ro::vfs {

core::Result<formats::LuaTable> ReadLuaChain(Vfs& vfs,
                                             std::span<const std::string_view> internal_paths) {
  formats::LuaTable globals;
  for (const std::string_view internal_path : internal_paths) {
    auto bytes = vfs.Read(std::string(internal_path));
    if (!bytes) {
      return std::unexpected(core::Error{
          bytes.error().code, std::string(internal_path) + " : " + bytes.error().detail});
    }
    auto read = formats::ReadLuaFile(*bytes, std::move(globals));
    if (!read) {
      return std::unexpected(
          core::Error{read.error().code, std::string(internal_path) + " : " + read.error().detail});
    }
    globals = std::move(*read);
  }
  return globals;
}

}  // namespace ro::vfs
