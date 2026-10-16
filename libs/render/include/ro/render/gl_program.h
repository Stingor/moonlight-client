#pragma once

#include <span>
#include <string>
#include <string_view>
#include <vector>

#include <glm/glm.hpp>

#include "ro/core/error.h"

namespace ro::render {

// Programme GLSL compilé et lié.
//
// `unsigned int` plutôt que GLuint : le type est le même, et l'employer ici
// évite d'imposer glad à tout ce qui inclut cet en-tête.
class GlProgram {
 public:
  GlProgram() = default;
  GlProgram(const GlProgram&) = delete;
  GlProgram& operator=(const GlProgram&) = delete;
  GlProgram(GlProgram&& other) noexcept;
  GlProgram& operator=(GlProgram&& other) noexcept;
  ~GlProgram();

  // Le journal de compilation du pilote est repris tel quel dans l'erreur :
  // c'est la seule chose qui dise QUELLE ligne du shader est fautive.
  [[nodiscard]] static core::Result<GlProgram> Link(std::string_view vertex_source,
                                                    std::string_view fragment_source);

  void Use() const;
  void SetMatrix4(std::string_view name, const glm::mat4& value) const;
  void SetVector2(std::string_view name, const glm::vec2& value) const;
  void SetVector3(std::string_view name, const glm::vec3& value) const;
  void SetVector4(std::string_view name, const glm::vec4& value) const;
  void SetInt(std::string_view name, int value) const;
  void SetFloat(std::string_view name, float value) const;
  // Un tableau d'uniformes, depuis son premier élément.
  void SetVector3Array(std::string_view name, std::span<const glm::vec3> values) const;
  void SetFloatArray(std::string_view name, std::span<const float> values) const;

  [[nodiscard]] unsigned int id() const noexcept { return program_; }

 private:
  explicit GlProgram(unsigned int program) noexcept : program_(program) {}

  // L'emplacement d'une variable uniforme, ou −1 : le pilote ignore alors
  // l'écriture, comme il le fait pour une variable que l'optimiseur a retirée.
  [[nodiscard]] int LocationOf(std::string_view name) const;

  // LES EMPLACEMENTS, RELEVÉS UNE FOIS À L'ÉDITION DE LIENS et triés par nom.
  //
  // ⚠ LE PILOTE N'EST PAS INTERROGÉ À CHAQUE ÉCRITURE : ce serait un appel et
  // une chaîne allouée par écriture — des milliers par image, puisque chaque
  // surface de modèle et chaque quad d'acteur posent les leurs.
  struct Uniform {
    std::string name;
    int location = -1;
  };
  std::vector<Uniform> uniforms_;

  unsigned int program_ = 0;
};

}  // namespace ro::render
