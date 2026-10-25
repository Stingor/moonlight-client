#include "ro/render/gl_context.h"

#include <algorithm>
#include <array>
#include <cstddef>
#include <cstdint>
#include <optional>
#include <string>
#include <utility>
#include <vector>

#include <SDL3/SDL.h>
#include <glad/glad.h>

#include "ro/core/log.h"
#include "ro/render/window_fit.h"

namespace ro::render {
namespace {

using core::ErrorCode;
using core::Fail;

// LA VERSION EXIGÉE : les tableaux de cartes cubiques des ombres ponctuelles, les
// textures-tampons et la copie de texture à texture en ont besoin. Voir
// docs/adr/0123.
constexpr int kGlMajorVersion = 4;
constexpr int kGlMinorVersion = 3;
constexpr int kStencilBits = 8;

// La place utile d'un écran, barre des tâches ôtée ; vide s'il ne répond pas.
[[nodiscard]] Resolution UsableRoom(SDL_DisplayID display) {
  SDL_Rect usable;
  if (display == 0 || !SDL_GetDisplayUsableBounds(display, &usable)) {
    return {};
  }
  return Resolution{.width = usable.w, .height = usable.h};
}

// Les identifiants des écrans branchés, dans l'ordre du système. Vide si la
// vidéo ne répond pas.
[[nodiscard]] std::vector<SDL_DisplayID> DisplayIds() {
  std::vector<SDL_DisplayID> ids;
  int count = 0;
  SDL_DisplayID* found = SDL_GetDisplays(&count);
  if (found == nullptr) {
    return ids;
  }
  ids.assign(found, found + count);
  SDL_free(found);
  return ids;
}

// L'écran de ce rang, ou zéro — l'identifiant que SDL réserve à « aucun ».
[[nodiscard]] SDL_DisplayID DisplayAtRank(int rank) {
  const std::vector<SDL_DisplayID> ids = DisplayIds();
  if (rank < 0 || rank >= static_cast<int>(ids.size())) {
    return 0;
  }
  return ids[static_cast<std::size_t>(rank)];
}

// Le rang de cet écran, ou moins un.
[[nodiscard]] int RankOfDisplay(SDL_DisplayID display) {
  if (display == 0) {
    return -1;
  }
  const std::vector<SDL_DisplayID> ids = DisplayIds();
  const auto found = std::ranges::find(ids, display);
  return found == ids.end() ? -1 : static_cast<int>(found - ids.begin());
}

}  // namespace

std::vector<ScreenInfo> GlContext::ConnectedScreens() {
  std::vector<ScreenInfo> screens;
  if (SDL_WasInit(SDL_INIT_VIDEO) == 0) {
    return screens;
  }
  const SDL_DisplayID primary = SDL_GetPrimaryDisplay();
  for (const SDL_DisplayID display : DisplayIds()) {
    ScreenInfo screen;
    if (const char* name = SDL_GetDisplayName(display); name != nullptr) {
      screen.name = name;
    }
    if (const SDL_DisplayMode* desktop = SDL_GetDesktopDisplayMode(display); desktop != nullptr) {
      screen.desktop = Resolution{.width = desktop->w, .height = desktop->h};
    }
    screen.primary = display == primary;
    screens.push_back(std::move(screen));
  }
  return screens;
}

int GlContext::screen_rank() const { return RankOfDisplay(SDL_GetDisplayForWindow(window_)); }

int GlContext::ScreenRankAt(WindowPlace where) {
  const SDL_Point point{.x = where.x, .y = where.y};
  return RankOfDisplay(SDL_GetDisplayForPoint(&point));
}

void GlContext::MoveToScreen(int screen_rank) const {
  const SDL_DisplayID display = DisplayAtRank(screen_rank);
  if (display == 0) {
    return;
  }
  if ((SDL_GetWindowFlags(window_) & SDL_WINDOW_FULLSCREEN) != 0) {
    SDL_SetWindowFullscreen(window_, false);
    SDL_SyncWindow(window_);
  }
  // ⚠ UNE FENÊTRE AGRANDIE EST CALÉE PAR LE SYSTÈME sur la zone de travail de
  // son écran, et il refuserait de la déplacer.
  if ((SDL_GetWindowFlags(window_) & SDL_WINDOW_MAXIMIZED) != 0) {
    SDL_RestoreWindow(window_);
    SDL_SyncWindow(window_);
  }
  const auto centered = static_cast<int>(SDL_WINDOWPOS_CENTERED_DISPLAY(display));
  SDL_SetWindowPosition(window_, centered, centered);
  SDL_SyncWindow(window_);
}

GlContext::~GlContext() {
  if (context_ != nullptr) {
    SDL_GL_DestroyContext(context_);
  }
  if (window_ != nullptr) {
    SDL_DestroyWindow(window_);
  }
  SDL_QuitSubSystem(SDL_INIT_VIDEO);
}

core::Result<std::unique_ptr<GlContext>> GlContext::Create(const std::string& title, int width,
                                                           int height, bool visible,
                                                           const ScreenPicker& pick) {
  if (!SDL_InitSubSystem(SDL_INIT_VIDEO)) {
    return Fail(ErrorCode::kIoFailure, std::string("SDL_InitSubSystem : ") + SDL_GetError());
  }
  if (visible) {
    const char* driver = SDL_GetCurrentVideoDriver();
    core::LogInfo("vidéo : pilote SDL {}, {} écran(s)", driver == nullptr ? "(aucun)" : driver,
                  DisplayIds().size());
  }

  SDL_GL_SetAttribute(SDL_GL_CONTEXT_MAJOR_VERSION, kGlMajorVersion);
  SDL_GL_SetAttribute(SDL_GL_CONTEXT_MINOR_VERSION, kGlMinorVersion);
  SDL_GL_SetAttribute(SDL_GL_CONTEXT_PROFILE_MASK, SDL_GL_CONTEXT_PROFILE_CORE);
  SDL_GL_SetAttribute(SDL_GL_DEPTH_SIZE, 24);
  // Le stencil porte la silhouette des corps : voir `SpriteRenderer::BodySilhouette`.
  SDL_GL_SetAttribute(SDL_GL_STENCIL_SIZE, kStencilBits);

  // Le constructeur est privé : make_unique ne peut pas l'appeler.
  std::unique_ptr<GlContext> context(new GlContext());

  const SDL_WindowFlags flags =
      SDL_WINDOW_OPENGL | (visible ? SDL_WINDOW_RESIZABLE : SDL_WINDOW_HIDDEN);
  // 🔴 UNE FENÊTRE VISIBLE NAÎT SUR L'ÉCRAN : la taille d'usine ou celle du
  // fichier peut dépasser un petit écran. Les bords ne sont pas encore connus,
  // d'où la part réservée de `FitWindowSize`. Une fenêtre cachée — rendu hors
  // écran — garde la taille demandée au pixel près.
  //
  // ⚠ ET ELLE NAÎT SUR L'ÉCRAN CHOISI, centrée : l'ouvrir sur le principal pour
  // la déplacer ensuite se verrait.
  SDL_DisplayID display = SDL_GetPrimaryDisplay();
  if (visible && pick) {
    if (const SDL_DisplayID wanted = DisplayAtRank(pick(ConnectedScreens())); wanted != 0) {
      display = wanted;
    }
  }
  Resolution asked{.width = width, .height = height};
  if (visible) {
    asked = FitWindowSize(asked, UsableRoom(display));
  }
  const SDL_PropertiesID properties = SDL_CreateProperties();
  const auto centered = static_cast<Sint64>(SDL_WINDOWPOS_CENTERED_DISPLAY(display));
  SDL_SetStringProperty(properties, SDL_PROP_WINDOW_CREATE_TITLE_STRING, title.c_str());
  SDL_SetNumberProperty(properties, SDL_PROP_WINDOW_CREATE_WIDTH_NUMBER, asked.width);
  SDL_SetNumberProperty(properties, SDL_PROP_WINDOW_CREATE_HEIGHT_NUMBER, asked.height);
  SDL_SetNumberProperty(properties, SDL_PROP_WINDOW_CREATE_X_NUMBER, centered);
  SDL_SetNumberProperty(properties, SDL_PROP_WINDOW_CREATE_Y_NUMBER, centered);
  SDL_SetNumberProperty(properties, SDL_PROP_WINDOW_CREATE_FLAGS_NUMBER,
                        static_cast<Sint64>(flags));
  context->window_ = SDL_CreateWindowWithProperties(properties);
  SDL_DestroyProperties(properties);
  if (context->window_ == nullptr) {
    SDL_QuitSubSystem(SDL_INIT_VIDEO);
    return Fail(ErrorCode::kIoFailure, std::string("SDL_CreateWindow : ") + SDL_GetError());
  }

  // ⚠ LA LIGNE AVANT L'APPEL, et non après : c'est dans le pilote graphique que
  // tombent les fermetures sans rapport, et le journal doit alors s'arrêter sur
  // ce qu'on lui demandait.
  if (visible) {
    core::LogInfo("vidéo : fenêtre {}×{} posée, contexte OpenGL {}.{} core demandé", asked.width,
                  asked.height, kGlMajorVersion, kGlMinorVersion);
  }
  context->context_ = SDL_GL_CreateContext(context->window_);
  if (context->context_ == nullptr) {
    return Fail(ErrorCode::kIoFailure,
                std::string("SDL_GL_CreateContext : ") + SDL_GetError() +
                    " — pilote graphique absent, trop ancien, ou sans OpenGL 4.3");
  }

  if (gladLoadGLLoader(reinterpret_cast<GLADloadproc>(SDL_GL_GetProcAddress)) == 0) {
    return Fail(ErrorCode::kIoFailure, "chargement des fonctions OpenGL impossible");
  }
  if (visible) {
    // QUELLE CARTE A RÉPONDU : sur un portable à deux cartes, la puce intégrée
    // répond souvent à la place de l'autre, et c'est d'elle que viennent les
    // pannes.
    const auto text_of = [](GLenum name) {
      const auto* text = reinterpret_cast<const char*>(glGetString(name));
      return std::string(text == nullptr ? "(inconnu)" : text);
    };
    core::LogInfo("vidéo : {} — {} — OpenGL {}", text_of(GL_VENDOR), text_of(GL_RENDERER),
                  text_of(GL_VERSION));
  }
  // Un contexte 4.3 core demandé peut être honoré par un pilote qui rend moins :
  // mieux vaut le constater ici que sur un shader qui refuse de compiler.
  if (GLVersion.major < kGlMajorVersion ||
      (GLVersion.major == kGlMajorVersion && GLVersion.minor < kGlMinorVersion)) {
    return Fail(ErrorCode::kUnsupportedVersion, "OpenGL " + std::to_string(GLVersion.major) + "." +
                                                    std::to_string(GLVersion.minor) +
                                                    " obtenu, 4.3 requis");
  }

  return context;
}

core::Result<std::unique_ptr<GlContext>> GlContext::CreateHeadless(int width, int height) {
  return Create("moonclient", width, height, false, {});
}

core::Result<std::unique_ptr<GlContext>> GlContext::CreateWindowed(const std::string& title,
                                                                   int width, int height,
                                                                   const ScreenPicker& pick) {
  return Create(title, width, height, true, pick);
}

DrawableSize GlContext::drawable_size() const {
  DrawableSize size;
  SDL_GetWindowSizeInPixels(window_, &size.width, &size.height);
  return size;
}

void GlContext::BindForDisplay() const {
  const DrawableSize size = drawable_size();
  glBindFramebuffer(GL_FRAMEBUFFER, 0);
  glViewport(0, 0, size.width, size.height);
}

void GlContext::ClearBoundTarget(const std::array<float, 4>& color) const {
  glClearColor(color[0], color[1], color[2], color[3]);
  glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT | GL_STENCIL_BUFFER_BIT);
}

void GlContext::SetTitle(const std::string& title) const {
  // ⚠ RIEN SI LE TITRE NE CHANGE PAS : la partie le repose à chaque image, et
  // le système redessine la barre de titre à chaque appel.
  if (const char* shown = SDL_GetWindowTitle(window_); shown != nullptr && title == shown) {
    return;
  }
  SDL_SetWindowTitle(window_, title.c_str());
}

void GlContext::SwapBuffers() const { SDL_GL_SwapWindow(window_); }

std::vector<Resolution> GlContext::AvailableResolutions() const {
  std::vector<Resolution> found;
  const SDL_DisplayID display = SDL_GetDisplayForWindow(window_);
  if (display == 0) {
    return found;
  }
  int count = 0;
  SDL_DisplayMode** modes = SDL_GetFullscreenDisplayModes(display, &count);
  if (modes == nullptr) {
    return found;
  }
  for (int rank = 0; rank < count; ++rank) {
    const SDL_DisplayMode* mode = modes[rank];
    if (mode == nullptr || mode->w <= 0 || mode->h <= 0) {
      continue;
    }
    // ⚠ UNE TAILLE, PAS UN MODE. L'écran en déclare un par fréquence et par
    // format de pixel — la même taille y revient cinq ou six fois —, et une
    // liste déroulante qui les montrerait tous serait illisible.
    const Resolution size{.width = mode->w, .height = mode->h};
    if (std::ranges::find(found, size) == found.end()) {
      found.push_back(size);
    }
  }
  SDL_free(modes);
  std::ranges::sort(found, [](const Resolution& left, const Resolution& right) {
    return left.width == right.width ? left.height < right.height : left.width < right.width;
  });
  return found;
}

WindowPlace GlContext::position() const {
  WindowPlace where;
  SDL_GetWindowPosition(window_, &where.x, &where.y);
  return where;
}

void GlContext::SetPosition(WindowPlace where) const {
  SDL_SetWindowPosition(window_, where.x, where.y);
}

void GlContext::KeepOnScreen() const {
  // ⚠ RIEN À RAMENER EN PLEIN ÉCRAN : la fenêtre occupe l'écran par définition,
  // et lui poser une position la ferait ressortir de travers au retour.
  // Ni en fenêtre agrandie : le système la cale lui-même sur la zone de
  // travail, bords de redimensionnement hors de l'écran, et la déplacer la
  // décollerait de ses bords.
  if ((SDL_GetWindowFlags(window_) & (SDL_WINDOW_FULLSCREEN | SDL_WINDOW_MAXIMIZED)) != 0) {
    return;
  }
  const SDL_DisplayID display = SDL_GetDisplayForWindow(window_);
  SDL_Rect usable;
  if (display == 0 || !SDL_GetDisplayUsableBounds(display, &usable)) {
    return;
  }
  const Resolution wide = size();
  WindowPlace where = position();
  // 🔴 LA POSITION ET LA TAILLE SONT CELLES DE LA ZONE CLIENTE : la barre de
  // titre et les bords se tiennent AUTOUR. Borner la zone cliente seule collait
  // son haut en haut de l'écran, et la barre de titre restait au-dessus, hors
  // d'atteinte. Faute de réponse du gestionnaire de fenêtres, les bords valent
  // zéro.
  int top = 0;
  int left = 0;
  int bottom = 0;
  int right = 0;
  if (!SDL_GetWindowBordersSize(window_, &top, &left, &bottom, &right)) {
    top = left = bottom = right = 0;
  }
  // Le bas-droit d'abord, le haut-gauche ENSUITE : dans cet ordre, une fenêtre
  // trop grande finit collée au coin haut-gauche plutôt qu'au bas-droit, donc
  // barre de titre visible.
  where.x = std::min(where.x, usable.x + usable.w - right - wide.width);
  where.y = std::min(where.y, usable.y + usable.h - bottom - wide.height);
  where.x = std::max(where.x, usable.x + left);
  where.y = std::max(where.y, usable.y + top);
  SetPosition(where);
}

Resolution GlContext::desktop_size() const {
  const SDL_DisplayID display = SDL_GetDisplayForWindow(window_);
  if (display == 0) {
    return {};
  }
  const SDL_DisplayMode* desktop = SDL_GetDesktopDisplayMode(display);
  if (desktop == nullptr) {
    return {};
  }
  return Resolution{.width = desktop->w, .height = desktop->h};
}

Resolution GlContext::size() const {
  Resolution wide;
  SDL_GetWindowSize(window_, &wide.width, &wide.height);
  return wide;
}

void GlContext::SetSize(Resolution wanted) const {
  if (wanted.width <= 0 || wanted.height <= 0) {
    return;
  }
  // ⚠ RIEN À FAIRE SI LA FENÊTRE N'EST PAS LIBRE DE SA TAILLE : SDL accepterait
  // l'appel et le garderait pour le retour au mode fenêtré, ce qui ferait
  // ressortir d'un plein écran dans une taille que personne n'a demandée
  // depuis.
  if ((SDL_GetWindowFlags(window_) & SDL_WINDOW_FULLSCREEN) != 0) {
    return;
  }
  // ⚠ BORNÉE À L'ÉCRAN OÙ ELLE EST, bords ôtés : une taille choisie sur un grand
  // écran ne doit pas faire déborder la fenêtre d'un plus petit.
  Resolution room = UsableRoom(SDL_GetDisplayForWindow(window_));
  int top = 0;
  int left = 0;
  int bottom = 0;
  int right = 0;
  if (SDL_GetWindowBordersSize(window_, &top, &left, &bottom, &right)) {
    room.width -= left + right;
    room.height -= top + bottom;
  }
  const Resolution fitted = FitWindowSize(wanted, room);
  SDL_SetWindowSize(window_, fitted.width, fitted.height);
}

void GlContext::SetResizable(bool resizable) const { SDL_SetWindowResizable(window_, resizable); }

core::Result<void> GlContext::SetDisplayMode(DisplayMode mode, Resolution wanted) const {
  // 🔴 LE MODE VIDÉO SE POSE AVANT LE PLEIN ÉCRAN, jamais après : c'est lui qui
  // dit CE QUE le plein écran doit prendre. Nul = « celui du bureau », et c'est
  // exactement ce qui sépare le sans bordure de l'exclusif.
  if (mode == DisplayMode::kFullscreen && wanted.width > 0 && wanted.height > 0) {
    SDL_DisplayMode closest;
    const SDL_DisplayID display = SDL_GetDisplayForWindow(window_);
    if (SDL_GetClosestFullscreenDisplayMode(display, wanted.width, wanted.height, 0.0F, true,
                                            &closest)) {
      SDL_SetWindowFullscreenMode(window_, &closest);
    }
  } else if (mode == DisplayMode::kBorderless) {
    SDL_SetWindowFullscreenMode(window_, nullptr);
  }

  if (!SDL_SetWindowFullscreen(window_, mode != DisplayMode::kWindowed)) {
    return Fail(ErrorCode::kIoFailure, std::string("SDL_SetWindowFullscreen : ") + SDL_GetError());
  }
  // ⚠ ET L'ON ATTEND QUE CE SOIT FAIT. Le gestionnaire de fenêtres répond quand
  // il veut ; sans cette attente, la taille relue juste après est encore
  // l'ancienne, et l'image se dessine cadrée de travers pendant une image ou
  // deux.
  SDL_SyncWindow(window_);
  if (mode == DisplayMode::kWindowed) {
    SetSize(wanted);
    // Même attente pour la taille : bornée avant d'être appliquée, la fenêtre
    // le serait avec ses anciennes mesures.
    SDL_SyncWindow(window_);
    // 🔴 ET ON LA RAMÈNE. `SDL_SetWindowSize` garde le coin haut-gauche : une
    // fenêtre agrandie sort de l'écran par le bas et la droite, barre de titre
    // comprise si elle était déjà basse.
    KeepOnScreen();
  }
  return {};
}

bool GlContext::SetVerticalSync(bool on) const { return SDL_GL_SetSwapInterval(on ? 1 : 0); }

std::optional<std::int64_t> GlContext::FreeVideoMemoryKib() {
  // Les jetons des deux extensions : la mémoire dédiée encore libre chez
  // NVIDIA, et quatre entiers chez AMD dont le premier est la mémoire libre
  // des textures. Les deux comptent en kibioctets.
  constexpr GLenum kNvidiaFreeVideoMemory = 0x9049;
  constexpr GLenum kAmdFreeTextureMemory = 0x87FC;
  constexpr std::size_t kAmdMemoryFields = 4;
  // Ce qui a été demandé à la carte s'achève d'abord : un téléversement encore
  // en file n'a rien pris.
  glFinish();
  if (SDL_GL_ExtensionSupported("GL_NVX_gpu_memory_info")) {
    GLint free_kib = 0;
    glGetIntegerv(kNvidiaFreeVideoMemory, &free_kib);
    return glGetError() == GL_NO_ERROR ? std::optional<std::int64_t>(free_kib) : std::nullopt;
  }
  if (SDL_GL_ExtensionSupported("GL_ATI_meminfo")) {
    std::array<GLint, kAmdMemoryFields> fields{};
    glGetIntegerv(kAmdFreeTextureMemory, fields.data());
    return glGetError() == GL_NO_ERROR ? std::optional<std::int64_t>(fields[0]) : std::nullopt;
  }
  return std::nullopt;
}

}  // namespace ro::render
