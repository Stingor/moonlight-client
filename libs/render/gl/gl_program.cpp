#include "ro/render/gl_program.h"

#include <algorithm>
#include <string>
#include <utility>
#include <vector>

#include <glad/glad.h>

namespace ro::render {
namespace {

using core::ErrorCode;
using core::Fail;

core::Result<GLuint> CompileStage(GLenum stage, std::string_view source, const char* label) {
  const GLuint shader = glCreateShader(stage);
  const auto* text = source.data();
  const auto length = static_cast<GLint>(source.size());
  glShaderSource(shader, 1, &text, &length);
  glCompileShader(shader);

  GLint compiled = GL_FALSE;
  glGetShaderiv(shader, GL_COMPILE_STATUS, &compiled);
  if (compiled == GL_TRUE) {
    return shader;
  }

  GLint log_length = 0;
  glGetShaderiv(shader, GL_INFO_LOG_LENGTH, &log_length);
  std::vector<char> log(static_cast<std::size_t>(log_length > 0 ? log_length : 1));
  glGetShaderInfoLog(shader, static_cast<GLsizei>(log.size()), nullptr, log.data());
  glDeleteShader(shader);

  return Fail(ErrorCode::kCorruptData,
              std::string("shader ") + label + " : " + std::string(log.data()));
}

// Le suffixe qu'un tableau uniforme porte dans la liste du pilote.
constexpr std::string_view kFirstElementSuffix = "[0]";

}  // namespace

GlProgram::GlProgram(GlProgram&& other) noexcept
    : uniforms_(std::move(other.uniforms_)), program_(other.program_) {
  other.program_ = 0;
}

GlProgram& GlProgram::operator=(GlProgram&& other) noexcept {
  if (this != &other) {
    if (program_ != 0) {
      glDeleteProgram(program_);
    }
    program_ = other.program_;
    uniforms_ = std::move(other.uniforms_);
    other.program_ = 0;
  }
  return *this;
}

GlProgram::~GlProgram() {
  if (program_ != 0) {
    glDeleteProgram(program_);
  }
}

core::Result<GlProgram> GlProgram::Link(std::string_view vertex_source,
                                        std::string_view fragment_source) {
  auto vertex = CompileStage(GL_VERTEX_SHADER, vertex_source, "de sommets");
  if (!vertex) {
    return std::unexpected(vertex.error());
  }
  auto fragment = CompileStage(GL_FRAGMENT_SHADER, fragment_source, "de fragments");
  if (!fragment) {
    glDeleteShader(*vertex);
    return std::unexpected(fragment.error());
  }

  const GLuint program = glCreateProgram();
  glAttachShader(program, *vertex);
  glAttachShader(program, *fragment);
  glLinkProgram(program);
  // Les shaders ne servent plus une fois liés ; le programme en garde une
  // référence jusqu'à sa propre destruction.
  glDeleteShader(*vertex);
  glDeleteShader(*fragment);

  GLint linked = GL_FALSE;
  glGetProgramiv(program, GL_LINK_STATUS, &linked);
  if (linked != GL_TRUE) {
    GLint log_length = 0;
    glGetProgramiv(program, GL_INFO_LOG_LENGTH, &log_length);
    std::vector<char> log(static_cast<std::size_t>(log_length > 0 ? log_length : 1));
    glGetProgramInfoLog(program, static_cast<GLsizei>(log.size()), nullptr, log.data());
    glDeleteProgram(program);
    return Fail(ErrorCode::kCorruptData, std::string("édition de liens GLSL : ") + log.data());
  }

  GlProgram linked_program(program);
  GLint count = 0;
  glGetProgramiv(program, GL_ACTIVE_UNIFORMS, &count);
  GLint longest = 0;
  glGetProgramiv(program, GL_ACTIVE_UNIFORM_MAX_LENGTH, &longest);
  std::vector<char> name(static_cast<std::size_t>(longest > 0 ? longest : 1));
  for (GLint index = 0; index < count; ++index) {
    GLsizei length = 0;
    GLint size = 0;
    GLenum type = 0;
    glGetActiveUniform(program, static_cast<GLuint>(index), static_cast<GLsizei>(name.size()),
                       &length, &size, &type, name.data());
    std::string uniform(name.data(), static_cast<std::size_t>(length));
    const GLint location = glGetUniformLocation(program, uniform.c_str());
    // Un tableau se nomme « u_x[0] » dans la liste ; on l'écrit « u_x ».
    if (uniform.ends_with(kFirstElementSuffix)) {
      uniform.resize(uniform.size() - kFirstElementSuffix.size());
    }
    linked_program.uniforms_.push_back(Uniform{.name = std::move(uniform), .location = location});
  }
  std::ranges::sort(linked_program.uniforms_, {}, &Uniform::name);
  return linked_program;
}

int GlProgram::LocationOf(std::string_view name) const {
  const auto found = std::ranges::lower_bound(
      uniforms_, name, {}, [](const Uniform& uniform) { return std::string_view(uniform.name); });
  return found != uniforms_.end() && found->name == name ? found->location : -1;
}

void GlProgram::Use() const { glUseProgram(program_); }

void GlProgram::SetMatrix4(std::string_view name, const glm::mat4& value) const {
  glUniformMatrix4fv(LocationOf(name), 1, GL_FALSE, &value[0][0]);
}

void GlProgram::SetVector2(std::string_view name, const glm::vec2& value) const {
  glUniform2fv(LocationOf(name), 1, &value[0]);
}

void GlProgram::SetVector3(std::string_view name, const glm::vec3& value) const {
  glUniform3fv(LocationOf(name), 1, &value[0]);
}

void GlProgram::SetVector4(std::string_view name, const glm::vec4& value) const {
  glUniform4fv(LocationOf(name), 1, &value[0]);
}

void GlProgram::SetInt(std::string_view name, int value) const {
  glUniform1i(LocationOf(name), value);
}

void GlProgram::SetFloat(std::string_view name, float value) const {
  glUniform1f(LocationOf(name), value);
}

}  // namespace ro::render
