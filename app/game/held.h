#pragma once

#include <memory>

namespace moonclient {

// UN OBJET TENU À PART, que l'en-tête de son porteur n'a besoin que de NOMMER.
//
// Un membre tenu par valeur oblige l'en-tête de la structure à inclure celui de
// son type, et chaque source qui lit la structure le relit alors. Tenu par
// `Held`, il n'y est que déclaré : seules les sources qui s'en servent incluent
// son en-tête, et elles seules se recompilent quand il change.
//
// ⚠ IL N'EST JAMAIS NUL : l'objet naît avec son porteur et meurt avec lui. Ce
// n'est pas un `std::unique_ptr`, qu'on remet à zéro ou qu'on teste ; c'est une
// valeur qui vit ailleurs.
//
// ⚠ LE PORTEUR DÉFINIT SON CONSTRUCTEUR ET SON DESTRUCTEUR LÀ OÙ `T` EST
// COMPLET, dans sa source et non dans son en-tête : c'est là, et seulement là,
// que sa construction et sa destruction s'instancient.
template <typename T>
class Held {
 public:
  Held() : held_(std::make_unique<T>()) {}
  ~Held() = default;

  Held(const Held&) = delete;
  Held& operator=(const Held&) = delete;
  // ⚠ IL SE DÉPLACE, pour qu'un porteur se rende par valeur — les réponses
  // d'une image. Déplacé, il ne sert plus qu'à être détruit.
  Held(Held&&) noexcept = default;
  Held& operator=(Held&&) = delete;

  [[nodiscard]] T& operator*() noexcept { return *held_; }
  [[nodiscard]] const T& operator*() const noexcept { return *held_; }
  [[nodiscard]] T* operator->() noexcept { return held_.get(); }
  [[nodiscard]] const T* operator->() const noexcept { return held_.get(); }

  // ⚠ IMPLICITES, et c'est voulu : un membre tenu à part se passe à une fonction
  // qui attend `T&` comme s'il était tenu par valeur. Ce qui ne se convertit pas
  // — une déduction de modèle, une affectation — s'écrit avec `*`.
  operator T&() noexcept { return *held_; }
  operator const T&() const noexcept { return *held_; }

 private:
  std::unique_ptr<T> held_;
};

}  // namespace moonclient
