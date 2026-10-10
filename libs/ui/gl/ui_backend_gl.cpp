#include "ui_backend_gl.h"

#include <utility>

#include <SDL3/SDL.h>

#include "imgui.h"
#include "imgui_impl_opengl3.h"
#include "imgui_impl_sdl3.h"

namespace ro::ui {
namespace {

// La version de GLSL que le dos émet, dans la forme que réclame son
// initialisation. Elle suit le contexte de `GlContext`, qui est un OpenGL 3.3
// core : une version plus haute ferait échouer la compilation des shaders sur
// le pilote, une plus basse refuserait les entrées/sorties du profil core.
constexpr const char* kGlslVersion = "#version 330";

}  // namespace

core::Result<UiBackendGl> UiBackendGl::Attach(UiContext& ui, SDL_Window* window) {
  if (window == nullptr) {
    return core::Fail(core::ErrorCode::kInvalidArgument, "fenêtre nulle : aucun dos à attacher");
  }
  // Le dos travaille sur le contexte COURANT d'ImGui : le poser est la première
  // chose à faire, sans quoi il s'installerait dans celui d'un autre.
  ImGui::SetCurrentContext(ui.handle());

  UiBackendGl backend;
  // ⚠ LE CONTEXTE OPENGL EST PASSÉ NUL, ET C'EST MESURÉ : le dos SDL3 ne s'en
  // sert que pour les fenêtres SECONDAIRES, celles qu'on détache hors de la
  // fenêtre principale. Ce mode n'est pas activé — `ImGuiConfigFlags_
  // ViewportsEnable` n'est pas posé —, donc la valeur n'est jamais lue. Lui
  // donner le vrai contexte demanderait à `GlContext` de l'exposer, c'est-à-dire
  // d'ouvrir une poignée qu'aucun besoin réel ne réclame.
  if (!ImGui_ImplSDL3_InitForOpenGL(window, nullptr)) {
    return core::Fail(core::ErrorCode::kIoFailure, "le dos SDL3 d'ImGui a refusé de s'ouvrir");
  }
  backend.platform_open_ = true;

  if (!ImGui_ImplOpenGL3_Init(kGlslVersion)) {
    return core::Fail(core::ErrorCode::kIoFailure, "le dos OpenGL d'ImGui a refusé de s'ouvrir");
  }
  backend.renderer_open_ = true;
  return backend;
}

UiBackendGl::UiBackendGl(UiBackendGl&& other) noexcept
    : platform_open_(std::exchange(other.platform_open_, false)),
      renderer_open_(std::exchange(other.renderer_open_, false)) {}

UiBackendGl& UiBackendGl::operator=(UiBackendGl&& other) noexcept {
  if (this != &other) {
    Release();
    platform_open_ = std::exchange(other.platform_open_, false);
    renderer_open_ = std::exchange(other.renderer_open_, false);
  }
  return *this;
}

UiBackendGl::~UiBackendGl() { Release(); }

void UiBackendGl::Release() noexcept {
  // Dans l'ORDRE INVERSE de l'ouverture : le dos de rendu tient des objets
  // OpenGL, et le dos de plateforme la fenêtre qui porte leur contexte.
  if (renderer_open_) {
    ImGui_ImplOpenGL3_Shutdown();
    renderer_open_ = false;
  }
  if (platform_open_) {
    ImGui_ImplSDL3_Shutdown();
    platform_open_ = false;
  }
}

bool UiBackendGl::ProcessEvent(const SDL_Event& event) const {
  return ImGui_ImplSDL3_ProcessEvent(&event);
}

void UiBackendGl::BeginFrame(UiContext& ui) const {
  ImGui_ImplOpenGL3_NewFrame();
  ImGui_ImplSDL3_NewFrame();
  ui.BeginBackedFrame();
}

void UiBackendGl::Draw() const { ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData()); }

}  // namespace ro::ui
