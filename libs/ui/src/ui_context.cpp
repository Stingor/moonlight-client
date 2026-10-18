#include "ro/ui/ui_context.h"

#include "ro/ui/modal_box.h"
#include "ro/ui/skin_look.h"
#include "ro/ui/wheel_gate.h"
#include "ro/ui/window_bounds.h"

#include "imgui.h"
// ⚠ L'EN-TÊTE INTERNE, ET POUR UNE SEULE RAISON : `ImGuiSettingsHandler` y vit,
// et c'est le mécanisme par lequel ImGui lui-même range les tables et
// l'accrochage dans son `.ini`. Il n'y a pas d'équivalent public, et écrire nos
// réglages dans un second fichier donnerait deux endroits à tenir à jour — dont
// un que l'utilisateur effacerait sans effacer l'autre.
#include "imgui_internal.h"

namespace ro::ui {
namespace {

// La rubrique du fichier de réglages, et son entrée unique : « [moonclient]
// [Windows] ». ImGui interdit les crochets dans ces noms.
constexpr const char* kSettingsSection = "moonclient";
constexpr const char* kSettingsEntry = "Windows";

// Le nom sous lequel l'échelle s'écrit.
//
// ⚠ AUCUNE FENÊTRE NE PEUT PORTER CE NOM : les deux partagent la même section
// des réglages, et une fenêtre ainsi nommée mangerait l'échelle. Le nom est
// français là où ceux des fenêtres sont des identifiants — c'est précisément ce
// qui les sépare.

// VRAI QUAND LA FORME QU'IMGUI RÉCLAME SE TRADUIT PAR LA MAIN DU JEU : un
// élément qui se clique, ou ce qui se saisit pour redimensionner.
[[nodiscard]] bool PointsLikeAHand(ImGuiMouseCursor asked) {
  switch (asked) {
    case ImGuiMouseCursor_Hand:
    case ImGuiMouseCursor_ResizeAll:
    case ImGuiMouseCursor_ResizeNS:
    case ImGuiMouseCursor_ResizeEW:
    case ImGuiMouseCursor_ResizeNESW:
    case ImGuiMouseCursor_ResizeNWSE:
      return true;
    default:
      return false;
  }
}

}  // namespace

UiContext::UiContext()
    : context_(ImGui::CreateContext()), field_wheel_(std::make_unique<WheelGateState>()) {
  ImGui::SetCurrentContext(context_);
  ImGuiIO& io = ImGui::GetIO();
  AttachFieldWheelGate(*field_wheel_);
  // LE BLEU DES LIENS DU JEU, MÊME HORS D'UNE FENÊTRE HABILLÉE : un lien lit la
  // couleur du style, que l'habillage pose dans ses fenêtres. Ailleurs, sans
  // ceci, ce serait le bleu d'ImGui.
  ImGui::GetStyle().Colors[ImGuiCol_TextLink] = ImGui::ColorConvertU32ToFloat4(kSkinLinkColor);
  // LE STYLE D'ORIGINE, COPIE AVANT TOUTE MISE A L'ECHELLE : voir base_style_.
  base_style_ = std::make_unique<ImGuiStyle>(ImGui::GetStyle());

  // AUCUN FICHIER DE RÉGLAGES PAR DÉFAUT, et c'est un choix et non un oubli.
  // ImGui écrirait sinon un `imgui.ini` dans le répertoire COURANT — celui d'où
  // le programme a été lancé, qui n'est pas le sien — et le relirait au
  // démarrage suivant. Une suite de tests y laisserait un fichier, et deux
  // lancements depuis deux répertoires ne verraient pas le même état de départ.
  //
  // Qui veut retenir la place des fenêtres nomme l'endroit : `UseSettingsFile`.
  io.IniFilename = nullptr;
  io.LogFilename = nullptr;

  // LA POLICE PAR DÉFAUT, POSÉE ICI, pour que le contexte soit utilisable seul.
  // Sans elle, une trame ouverte sans dos n'a rien pour mesurer un texte.
  // Le jeu la remplacera par celle du client ; d'ici là, c'est celle-ci qui
  // permet aux fenêtres d'être testées.
  io.Fonts->AddFontDefault();

  // ⚠ CE DRAPEAU EST CE QUI REND LA TRAME SANS DOS POSSIBLE. Depuis la 1.92,
  // ImGui demande ses textures au dos au fil des trames au lieu de graver un
  // atlas une fois pour toutes. Le poser dit « les demandes de texture seront
  // servies » — ce qui est vrai avec le dos OpenGL, qui le pose lui aussi, et
  // sans conséquence sans lui puisque rien n'est peint.
  io.BackendFlags |= ImGuiBackendFlags_RendererHasTextures;

  // ⚠ IMGUI NE TOUCHE PAS AU CURSEUR DU SYSTÈME, et c'est pour éviter DEUX
  // flèches à l'écran. Le dos de plateforme se croit responsable du curseur : il
  // le RALLUME à chaque image et lui donne la forme qu'ImGui réclame. Or le jeu
  // cache celui du système dès qu'il a chargé le sien — voir `GamePieces` — et
  // les deux décisions se contredisaient une fois par image.
  //
  // Le nôtre gagne, et ce n'est pas un goût : LA FORME DU CURSEUR RO EST
  // PORTEUSE. C'est elle qui dit si un clic frappe, ramasse ou marche, au point
  // qu'un acteur prend le clic exactement quand le curseur change de forme pour
  // lui. Une flèche de système ne sait pas dire cela.
  //
  // Ce qu'on perd : les flèches de redimensionnement au bord d'une fenêtre. Elles
  // deviennent la MAIN du jeu, qui dit « ça se saisit » — voir `pointing`.
  io.ConfigFlags |= ImGuiConfigFlags_NoMouseCursorChange;
}

UiContext::~UiContext() { Release(); }

void UiContext::Release() noexcept {
  if (context_ == nullptr) {
    return;
  }
  // Une trame restée ouverte doit être fermée AVANT la destruction : ImGui
  // garde sinon de l'état qui pointe dans le contexte qu'on détruit.
  if (frame_open_) {
    ImGui::SetCurrentContext(context_);
    ImGui::EndFrame();
    frame_open_ = false;
  }
  ImGui::DestroyContext(context_);
  context_ = nullptr;
  // DestroyContext remet le contexte courant à zéro quand c'était celui-là,
  // mais pas quand un autre était en cours : rien à défaire ici.
}

void UiContext::MakeCurrent() const {
  if (context_ != nullptr) {
    ImGui::SetCurrentContext(context_);
  }
}

void UiContext::BeginFrame(const Viewport& viewport, float elapsed_seconds) {
  MakeCurrent();
  ImGuiIO& io = ImGui::GetIO();
  io.DisplaySize = ImVec2(viewport.width_px, viewport.height_px);
  // ⚠ UN TEMPS NUL FAIT ÉCHOUER UNE ASSERTION D'IMGUI, et la conséquence n'est
  // pas cosmétique : une animation divisée par zéro. Le plancher vaut mieux
  // qu'un refus, parce qu'une image peut légitimement arriver dans la même
  // milliseconde que la précédente sur une machine rapide.
  constexpr float kSmallestFrameSeconds = 1.0F / 1000.0F;
  io.DeltaTime = elapsed_seconds > 0.0F ? elapsed_seconds : kSmallestFrameSeconds;
  // ⚠ AVANT `NewFrame` : c'est lui qui fige l'écran de la trame, et tout ce qui
  // se mesure en part d'écran le lira ensuite.
  KeepLastRealScreen();
  ImGui::NewFrame();
  NotePopupsAtFrameStart();
  // ⚠ LA PASSE ICI, ENTRE `NewFrame` ET LE PREMIER `Begin` : `NewFrame` a déjà
  // appliqué le glisser de la souris, et aucun `Begin` n'a encore lu la place.
  placer_.Place(interface_scale_);
  frame_open_ = true;
}

void UiContext::SetInterfaceScale(float scale) {
  MakeCurrent();
  interface_scale_ = scale < kSmallestInterfaceScale  ? kSmallestInterfaceScale
                     : scale > kLargestInterfaceScale ? kLargestInterfaceScale
                                                      : scale;
  ImGuiStyle& style = ImGui::GetStyle();
  style = *base_style_;
  // ⚠ RIEN À CENT POUR CENT. `ScaleAllSizes(1.0f)` n'est pas un geste vide : il
  // passe chaque mesure par une TRONCATURE et cumule son propre facteur. À
  // l'échelle d'origine, le style doit rester EXACTEMENT celui que tout le reste
  // du code connaît, ce qui évite d'avoir à se demander si un demi-pixel vient
  // de là.
  if (interface_scale_ != 1.0F) {
    style.ScaleAllSizes(interface_scale_);
  }
  // ⚠ LE STYLE SEUL NE REDIMENSIONNE RIEN. `ScaleAllSizes` change les marges,
  // les cadres et les poignées ; la TAILLE et la POSITION de chaque fenêtre,
  // elles, sont en pixels et ne bougent pas. Sans cette passe, doubler l'échelle
  // donnait un texte deux fois plus gros dans des fenêtres restées petites —
  // donc un contenu qui déborde partout, ce qui se lit « le réglage ne marche
  // pas ».
  //
  // ⚠ LES PLACES VOULUES SONT EN UNITÉS D'INTERFACE : la passe recalcule la
  // place affichée de chaque fenêtre à la nouvelle échelle, sans rien écrire
  // dans les places voulues. Revenir à l'ancienne échelle rend donc l'ancienne
  // disposition, au pixel près.
  placer_.Place(interface_scale_);
  // ⚠ LA POLICE NE PASSE PAS PAR `ScaleAllSizes`, qui le dit dans sa propre
  // déclaration : « Do not scale fonts ». Sans cette ligne, les cadres
  // grandiraient autour d'un texte resté petit.
  style.FontScaleMain = interface_scale_;
  // ⚠ RIEN N'EST ENREGISTRÉ ICI. Ce fichier est la mémoire qu'ImGui a de SES
  // fenêtres — où elles sont, lesquelles sont ouvertes — et l'échelle n'en est
  // pas : c'est un choix du JOUEUR, qui vit avec les autres dans son fichier
  // de réglages, au même endroit que le volume et la langue. Deux fichiers
  // qui la porteraient tous les deux se contrediraient au premier désaccord.
}

LayoutProfile UiContext::CurrentLayout() const {
  return LayoutProfile{.places = placer_.places(), .toggles = LayoutToggles()};
}

std::map<std::string, bool> UiContext::LayoutToggles() const {
  std::map<std::string, bool> toggles;
  for (const auto& [name, open] : remembered_) {
    toggles.insert_or_assign(name, *open);
  }
  return toggles;
}

void UiContext::UseLayout(const LayoutProfile& layout) {
  MakeCurrent();
  placer_.Use(layout.places);
  for (const auto& [name, open] : layout.toggles) {
    if (const auto found = remembered_.find(name); found != remembered_.end()) {
      *found->second = open;
    }
  }
  // Le fichier d'ImGui garde les interrupteurs aussi : il doit suivre.
  ImGui::MarkIniSettingsDirty();
}

void UiContext::UseSettingsFile(const std::filesystem::path& path) {
  MakeCurrent();
  settings_path_ = path.string();
  // ⚠ IMGUI NE COPIE PAS CE CHEMIN : il garde le pointeur et le relit à chaque
  // écriture. C'est `settings_path_` qui le fait vivre.
  ImGui::GetIO().IniFilename = settings_path_.c_str();
}

void UiContext::RememberWindow(std::string name, bool* open) {
  if (open == nullptr) {
    return;
  }
  MakeCurrent();
  const bool first = remembered_.empty();
  remembered_.insert_or_assign(std::move(name), open);
  if (!first) {
    return;
  }
  // LE GESTIONNAIRE S'INSCRIT UNE FOIS, à la première fenêtre retenue. ImGui le
  // consultera en lisant le fichier, puis en l'écrivant.
  ImGuiSettingsHandler handler;
  handler.TypeName = kSettingsSection;
  handler.TypeHash = ImHashStr(kSettingsSection);
  handler.UserData = this;
  handler.ReadOpenFn = [](ImGuiContext*, ImGuiSettingsHandler* self, const char*) -> void* {
    return self->UserData;
  };
  handler.ReadLineFn = [](ImGuiContext*, ImGuiSettingsHandler*, void* entry, const char* line) {
    static_cast<UiContext*>(entry)->ReadSetting(line);
  };
  handler.WriteAllFn = [](ImGuiContext*, ImGuiSettingsHandler* self, ImGuiTextBuffer* out) {
    static_cast<UiContext*>(self->UserData)->WriteSettings(out);
  };
  ImGui::AddSettingsHandler(&handler);
}

void UiContext::ReadSetting(const char* line) {
  // « nom=0 » ou « nom=1 ». Une ligne qu'on ne reconnaît pas est IGNORÉE : un
  // fichier de réglages écrit par une version plus récente ne doit pas empêcher
  // celle-ci de démarrer.
  const std::string_view text(line);
  const std::size_t equals = text.find('=');
  if (equals == std::string_view::npos) {
    return;
  }
  const std::string_view name = text.substr(0, equals);
  const std::string_view value = text.substr(equals + 1);
  const auto found = remembered_.find(std::string(name));
  if (found != remembered_.end()) {
    *found->second = value != "0";
  }
}

void UiContext::WriteSettings(ImGuiTextBuffer* out) const {
  out->appendf("[%s][%s]\n", kSettingsSection, kSettingsEntry);
  for (const auto& [name, open] : remembered_) {
    out->appendf("%s=%d\n", name.c_str(), *open ? 1 : 0);
  }
  out->append("\n");
}

void UiContext::SaveSettingsIfDirty() {
  MakeCurrent();
  ImGuiIO& io = ImGui::GetIO();
  if (!io.WantSaveIniSettings || io.IniFilename == nullptr) {
    return;
  }
  ImGui::SaveIniSettingsToDisk(io.IniFilename);
  io.WantSaveIniSettings = false;
}

void UiContext::SetPointer(float x_px, float y_px, bool pressed) {
  MakeCurrent();
  ImGuiIO& io = ImGui::GetIO();
  io.AddMousePosEvent(x_px, y_px);
  io.AddMouseButtonEvent(ImGuiMouseButton_Left, pressed);
}

void UiContext::KeepLastRealScreen() {
  ImGuiIO& io = ImGui::GetIO();
  if (io.DisplaySize.x > 0.0F && io.DisplaySize.y > 0.0F) {
    last_real_width_px_ = io.DisplaySize.x;
    last_real_height_px_ = io.DisplaySize.y;
    return;
  }
  // ⚠ RIEN À RENDRE TANT QU'AUCUNE TRAME N'A EU D'ÉCRAN : un client lancé déjà
  // réduit n'en a jamais vu. Les bornes savent se taire devant un écran nul —
  // c'est leur repli, pas le nôtre.
  if (last_real_width_px_ <= 0.0F || last_real_height_px_ <= 0.0F) {
    return;
  }
  io.DisplaySize = ImVec2(last_real_width_px_, last_real_height_px_);
}

void UiContext::BeginBackedFrame() {
  MakeCurrent();
  // ⚠ AVANT `NewFrame`, et pour la même raison que dans `BeginFrame` : c'est le
  // dos qui a posé l'écran cette fois, mais il le pose à zéro tout pareil.
  KeepLastRealScreen();
  ImGui::NewFrame();
  // ⚠ AVANT TOUT DESSIN : voir `NotePopupsAtFrameStart`.
  NotePopupsAtFrameStart();
  placer_.Place(interface_scale_);
  frame_open_ = true;
}

void UiContext::EndFrame() {
  if (!frame_open_) {
    return;
  }
  MakeCurrent();
  ImGui::Render();
  frame_open_ = false;
}

InputGate UiContext::gate() const {
  if (context_ == nullptr) {
    return {};
  }
  MakeCurrent();
  const ImGuiIO& io = ImGui::GetIO();
  // 🔴 UN ÉLÉMENT TENU N'EST PAS UNE PRISE DE CLAVIER, et pourtant ImGui lève
  // `WantCaptureKeyboard` dès que `ActiveId != 0`. Un objet qu'on GLISSE d'une
  // fenêtre à l'autre éteindrait donc tous les raccourcis du jeu, alors qu'il
  // n'attend aucune touche — et c'est justement en tenant quelque chose qu'on
  // veut ouvrir un contenant.
  const bool widget_held = ImGui::IsAnyItemActive() && !io.WantTextInput;
  // ⚠ LA RÉSERVE SE LIT DANS `OwnerNext`, celle que la dernière image a posée :
  // un widget revendique la molette à chaque image où il est survolé, et ImGui
  // ne la fait passer dans `OwnerCurr` qu'au début de l'image suivante — après
  // les événements qu'on route ici.
  //
  // ⚠ SAUF LA RÉSERVE QU'IMGUI POSE LUI-MÊME pour la fenêtre qu'une salve fait
  // défiler : ce n'est pas un réglage, et Ctrl+molette juste après avoir fait
  // défiler une liste doit encore zoomer.
  const ImGuiContext& imgui = *ImGui::GetCurrentContext();
  const ImGuiID wheel_owner =
      ImGui::GetKeyOwnerData(ImGui::GetCurrentContext(), ImGuiKey_MouseWheelY)->OwnerNext;
  const bool wheel_reserved =
      wheel_owner != ImGuiKeyOwner_NoOwner &&
      (imgui.WheelingWindow == nullptr || wheel_owner != imgui.WheelingWindow->ID);
  return InputGate{.mouse_to_ui = io.WantCaptureMouse,
                   .keyboard_to_ui = io.WantCaptureKeyboard,
                   .text_to_ui = io.WantTextInput,
                   .typing_to_ui = io.WantTextInput || widget_held,
                   // ⚠ C'EST IMGUI QUI L'A DÉCIDÉ, pas nous : chaque widget qui
                   // se clique demande la main quand on le survole, et le
                   // contexte n'en garde que le verdict. Le jeu le traduit dans
                   // SON curseur — celui d'ImGui ne se dessine pas ici. Une
                   // poignée ou un bord qui redimensionne se saisit aussi : la
                   // flèche de redimensionnement devient la même main.
                   .pointing = PointsLikeAHand(ImGui::GetMouseCursor()),
                   .wheel_to_ui = wheel_reserved};
}

}  // namespace ro::ui
