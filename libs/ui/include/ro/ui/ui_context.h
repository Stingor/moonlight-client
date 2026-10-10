#pragma once

#include <array>
#include <cstddef>
#include <cstdint>
#include <filesystem>
#include <map>
#include <memory>
#include <string>
#include <string_view>
#include <vector>

struct ImGuiContext;
struct ImGuiStyle;
struct ImGuiTextBuffer;

#include "ro/ui/font_setup.h"
#include "ro/ui/window_places.h"

namespace ro::ui {

// Le contexte d'interface : sa durée de vie, sa trame, et QUI reçoit le geste.
//
// POURQUOI UN OBJET PLUTÔT QUE LES APPELS NUS D'IMGUI
// ImGui porte son contexte dans un pointeur global, qu'on pose et qu'on lit
// sans le passer. C'est le patron que CLAUDE.md refuse — un `Instance()`
// global, mesuré ailleurs à 789 sites d'appel — et on ne peut pas l'enlever
// d'ImGui. Ce qu'on peut faire, et qui suffit, c'est n'écrire ce pointeur
// QU'ICI : au-dessus, un contexte se reçoit par référence comme tout le reste,
// et deux contextes peuvent coexister — celui du jeu, et celui qu'un test
// construit et détruit.
//
// La destruction est celle d'un objet C++ ordinaire, et c'est l'autre gain :
// un test qui oublie de fermer son contexte le ferait fuir dans le suivant, et
// ASan le dirait trop tard pour qu'on sache lequel.

// La surface où l'interface se dessine, en pixels de TAMPON.
//
// ⚠ Pixels de tampon, pas de fenêtre : sur un écran à facteur d'échelle les
// deux diffèrent, et confondre l'un pour l'autre place chaque fenêtre et chaque
// clic à côté d'autant. C'est la même distinction que la boucle fait déjà entre
// `SDL_GetWindowSize` et la taille dessinable.
struct Viewport {
  float width_px = 0.0F;
  float height_px = 0.0F;
};

// QUI reçoit le geste : l'interface, ou le jeu.
//
// ⚠ PAS DE CHAÎNE DE PRIORITÉS ÉCRITE À LA MAIN. Un ordre du genre « le banc
// d'abord : il est dessiné par-dessus, donc il reçoit la molette avant la
// caméra et avant le grimoire » est juste tant que personne n'ajoute de
// fenêtre, c'est-à-dire jamais longtemps, et son défaut ne se voit pas : un
// clic destiné à un panneau fait aussi marcher le personnage dessous.
//
// ImGui, lui, SAIT quelle fenêtre est sous le curseur, parce qu'il les a toutes
// dessinées. On lui demande, au lieu de le redire.
// Les bornes de l'échelle d'interface.
//
// En deçà du plancher les cadres deviennent plus fins qu'un pixel et
// disparaissent ; au-delà du plafond une seule fenêtre couvre l'écran, et la
// borne des 90 % la réduirait à chaque image sans que le joueur comprenne
// pourquoi.
inline constexpr float kSmallestInterfaceScale = 0.5F;
inline constexpr float kLargestInterfaceScale = 2.0F;

struct InputGate {
  // Vrai quand une fenêtre est sous le curseur : le monde ne doit alors ni se
  // faire désigner, ni tourner, ni défiler.
  bool mouse_to_ui = false;
  // Vrai quand l'interface a le clavier, pour quelque raison que ce soit.
  //
  // ⚠ TROP LARGE POUR ÉTEINDRE LES RACCOURCIS À LUI SEUL, et c'est un piège :
  // ImGui le lève dès qu'un élément est ACTIF — un bouton tenu, une poignée
  // tirée, un objet qu'on GLISSE. Aucun de ces gestes n'attend de touche. Ce
  // qui doit vraiment faire taire le jeu est plus étroit : voir `typing_to_ui`,
  // et la prise TOTALE qu'est une modale.
  bool keyboard_to_ui = false;
  // Vrai quand un champ de texte attend des caractères. Distinct du précédent :
  // une fenêtre peut avoir le clavier pour ses flèches sans rien saisir.
  bool text_to_ui = false;
  // 🔴 VRAI QUAND LA PRISE EST CELLE D'UNE SAISIE — ou d'un élément qu'on tient,
  // qu'ImGui compte pareil sans qu'il attende la moindre touche.
  //
  // C'est ce qui sépare les deux avalages. Une saisie ne réclame que les touches
  // qui ÉCRIVENT : le reste — les touches de fonction, Inser, les combinaisons
  // avec Alt — revient au jeu, parce qu'aucun champ ne sait qu'en faire. Une
  // modale, elle, prend TOUT : le joueur doit lui répondre avant quoi que ce
  // soit d'autre.
  bool typing_to_ui = false;
  // Vrai quand la souris survole quelque chose qui se CLIQUE. Le jeu en fait
  // une main qui pointe : au-dessus d'un panneau, c'est le seul indice qu'une
  // chose répondra au clic.
  bool pointing = false;
  // Vrai quand un widget survolé s'est réservé la molette : un curseur, une
  // case de raccourci qu'on règle à Maj+molette. La caméra n'en prend alors
  // aucun cran, même avec Ctrl ou Maj tenus, qui sont justement les
  // modificateurs de ces réglages.
  bool wheel_to_ui = false;
};

class UiContext {
 public:
  // Construit un contexte, sa police par défaut posée.
  //
  // ⚠ RIEN ICI NE REND DE `Result`, et c'est mesuré plutôt que supposé :
  // `ImGui::CreateContext` alloue par `IM_NEW`, qui abandonne le programme
  // faute de mémoire et ne rend jamais rien de nul. Un `Result` que personne ne
  // peut faire échouer ferait croire à un chemin d'erreur qui n'existe pas, et
  // chaque appelant paierait un `if` mort.
  UiContext();

  UiContext(const UiContext&) = delete;
  UiContext& operator=(const UiContext&) = delete;

  // ⚠ NI COPIABLE NI DÉPLAÇABLE, ET CE N'EST PAS UNE PRÉCAUTION.
  //
  // Ce contexte SE DÉSIGNE LUI-MÊME, deux fois : `io.IniFilename` pointe dans sa
  // propre chaîne, et le gestionnaire de réglages qu'il inscrit auprès d'ImGui
  // porte son adresse. Un objet ainsi fait ne survit pas à un déplacement — les
  // deux références resteraient sur la dépouille.
  //
  // ÇA A PLANTÉ : `UiStage::Open` construisait un contexte local, y inscrivait
  // ses fenêtres, puis le déplaçait dans l'étage. Le client se fermait SANS UN
  // MOT, et le journal s'arrêtait sur une ligne parfaitement normale.
  //
  // Un déplacement rafistolé — recopier les champs, repointer les références —
  // marcherait, et se casserait au champ suivant qu'on oublierait. Le supprimer
  // fait porter la règle par le COMPILATEUR : qui veut déplacer ce contexte
  // déplace un pointeur vers lui.
  UiContext(UiContext&&) = delete;
  UiContext& operator=(UiContext&&) = delete;

  ~UiContext();

  // Ouvre une trame SANS DOS : on pose soi-même la surface et le temps écoulé.
  //
  // C'est la voie des tests, et elle n'est pas un pis-aller : une fenêtre s'y
  // construit, se mesure et rend ses intentions sans fenêtre ni carte
  // graphique. C'est ce qui met les fenêtres au premier étage des tests — celui
  // qui n'a aucune I/O — au lieu de l'œil.
  void BeginFrame(const Viewport& viewport, float elapsed_seconds);

  // ── LES POLICES ─────────────────────────────────────────────
  // Bâtit l'atlas à partir des familles qu'on nomme. ⚠ LES CHEMINS VIENNENT
  // D'`app` : `C:\\Windows\\Fonts\\…` est un fait de plateforme, et cet étage-ci
  // ne sait qu'assembler — ce qui le rend essayable avec des fichiers inventés.
  //
  // ⚠ REMPLACE TOUT L'ATLAS, police intégrée comprise. À appeler AVANT la
  // première trame : ImGui grave ses textures au fil des trames, et refaire
  // l'atlas en cours de route lui reprendrait ce qu'il vient de poser.
  void UseFonts(const FontSetup& setup);

  // Change de famille. Moins de zéro = la police intégrée d'ImGui.
  //
  // ⚠ `family` EST UN RANG DE L'ATLAS, qui ne tient que les familles trouvées
  // sur la machine. Un rang pris dans une table plus longue — celle du réglage,
  // qui les nomme toutes — désigne une autre famille dès qu'une manque : c'est
  // `ChooseFontNamed` qu'il faut alors.
  void ChooseFont(int family);
  // Change de famille, par son nom. Un nom que l'atlas ne porte pas — la
  // famille manque sur cette machine — rend la première, comme au démarrage.
  void ChooseFontNamed(std::string_view label);
  [[nodiscard]] int chosen_font() const;

  // La police de ce dessin-là, dans la famille choisie. Une variante absente
  // retombe sur le normal : Tahoma ne livre pas d'italique, et un texte non
  // penché vaut mieux qu'un texte manquant.
  [[nodiscard]] ImFont* ChosenFont(FontStyle style) const;

  // Cette famille a-t-elle été trouvée ? Le menu masque les autres plutôt que
  // d'offrir un choix sans effet.
  [[nodiscard]] bool FontFamilyUsable(int family) const;

  // LA POLICE QUI PORTE CE NOM, dans ce dessin : une famille de l'atlas, puis
  // une police du dossier du jeu — qui n'a qu'un dessin. Nulle si aucune ne le
  // porte : l'appelant garde alors la sienne.
  //
  // ⚠ PAR LE NOM ET NON PAR LE RANG : une famille absente de la machine décale
  // les rangs de l'atlas, pas les noms.
  [[nodiscard]] ImFont* FontNamed(std::string_view label, FontStyle style) const;

  // Les noms des polices du dossier du jeu que l'atlas a su lire, dans l'ordre
  // où on les a données.
  [[nodiscard]] std::vector<std::string> LooseFontNames() const;

  // RETIENT LA PLACE DES FENÊTRES d'une partie à l'autre, dans ce fichier.
  //
  // ⚠ LE CHEMIN SE DONNE, IL NE SE DEVINE PAS. Sans cet appel, ImGui écrirait
  // un `imgui.ini` dans le répertoire COURANT — celui d'où le programme a été
  // lancé, qui n'est pas le sien — et deux lancements depuis deux répertoires ne
  // verraient pas le même état de départ. Une suite de tests y laisserait un
  // fichier. C'est donc au client de nommer l'endroit.
  //
  // À appeler AVANT la première trame : ImGui lit ce fichier à ce moment-là.
  void UseSettingsFile(const std::filesystem::path& path);

  // À retenir AUSSI : qu'une fenêtre soit OUVERTE ou fermée.
  //
  // ⚠ IMGUI NE LE SAIT PAS. Il retient la position, la taille et le repli — ce
  // sont ses affaires — mais l'ouverture est un booléen qui appartient à
  // l'appelant, celui qu'on passe à `Begin`. Il voyage donc dans le même
  // fichier, sous notre propre rubrique, par le mécanisme qu'ImGui offre pour
  // ça.
  //
  // ⚠ LE BOOLÉEN DOIT SURVIVRE AU CONTEXTE : on en garde l'adresse, et les
  // réglages s'écrivent tant que le contexte vit.
  void RememberWindow(std::string name, bool* open);

  // L'ÉCHELLE DE L'INTERFACE : un facteur, et tout suit — marges, cadres,
  // coins, et le TEXTE.
  //
  // ⚠ ELLE SE FAIT ICI ET NON DANS OPENGL, et la différence se voit à l'œil.
  // Agrandir le rendu reviendrait à étirer une image déjà gravée : le texte
  // deviendrait flou, ce qui est précisément ce qu'on cherche à éviter en
  // agrandissant. Depuis ImGui 1.92 les glyphes se REGRAVENT à la taille
  // demandée, et le texte reste net à n'importe quel facteur.
  //
  // Le facteur est borné : voir kSmallestInterfaceScale.
  void SetInterfaceScale(float scale);
  [[nodiscard]] float interface_scale() const noexcept { return interface_scale_; }

  // LA DISPOSITION TELLE QU'ELLE EST : la place voulue de chaque fenêtre, et
  // les interrupteurs retenus. Voir `WindowPlacer`.
  [[nodiscard]] LayoutProfile CurrentLayout() const;
  // Les interrupteurs seuls : de quoi comparer à chaque image sans recopier
  // toutes les places.
  [[nodiscard]] std::map<std::string, bool> LayoutToggles() const;
  // POSE UNE DISPOSITION, à chaud : les places dès la passe suivante, les
  // interrupteurs tout de suite. Une fenêtre qu'elle ne nomme pas reprend sa
  // place d'origine ; un interrupteur qu'elle ne nomme pas reste tel quel.
  void UseLayout(const LayoutProfile& layout);
  // Grandit chaque fois qu'une place voulue change. ⚠ LES INTERRUPTEURS N'Y
  // SONT PAS : qui enregistre les compare lui-même, ils sont peu nombreux.
  [[nodiscard]] std::uint64_t layout_revision() const noexcept { return placer_.revision(); }

  // Écrit les réglages sur disque s'ils ont changé. La boucle l'appelle ; sans
  // elle, ce qu'on vient de déplacer serait perdu à la fermeture.
  void SaveSettingsIfDirty();

  // Pose le curseur SANS DOS, en pixels de tampon.
  //
  // Le pendant de `BeginFrame` pour l'autre moitié de ce qu'un dos fournit. Ce
  // qu'elle permet est le test qui compte : dessiner une fenêtre, poser le
  // curseur dessus, et vérifier que le LAISSEZ-PASSER se ferme — donc que le
  // monde en dessous ne recevra pas le clic.
  //
  // ⚠ LE VERDICT NE VIENT QU'À LA TRAME SUIVANTE. ImGui ne sait ce qu'il y a
  // sous le curseur qu'une fois les fenêtres dessinées, et il les dessine avec
  // la position posée AVANT. C'est le fonctionnement normal d'une interface en
  // mode immédiat, et non un retard à corriger.
  void SetPointer(float x_px, float y_px, bool pressed);

  // Ouvre une trame dont LE DOS a déjà posé la surface et le temps.
  //
  // Deux entrées plutôt qu'une parce qu'il y a deux propriétaires possibles de
  // ces valeurs, et qu'un seul les tient à la fois. Les écraser après le dos
  // perdrait le facteur d'échelle qu'il est seul à connaître.
  void BeginBackedFrame();

  // Ferme la trame et compose la liste de dessin. À appeler même sans dos :
  // c'est elle qui arrête l'état interne d'ImGui, et l'oublier laisse la trame
  // suivante s'ouvrir sur la précédente.
  void EndFrame();

  // Ce qu'ImGui a retenu du geste, tel qu'il est APRÈS la trame qui vient de se
  // fermer. Lisible à tout moment ; la boucle le lit avant de traiter ses
  // événements, donc sur le verdict de l'image précédente — ce qui est le
  // fonctionnement normal d'une interface en mode immédiat.
  [[nodiscard]] InputGate gate() const;

  // Vrai entre `BeginFrame` et `EndFrame`. Ce que la boucle dessine hors de
  // cette fenêtre-là n'irait nulle part.
  [[nodiscard]] bool frame_open() const noexcept { return frame_open_; }

  // Le contexte nu, pour le dos SEUL. Personne d'autre n'en a l'usage.
  [[nodiscard]] ImGuiContext* handle() const noexcept { return context_; }

 private:
  void Release() noexcept;
  // Pose ce contexte comme celui qu'ImGui emploie. Toutes les méthodes
  // publiques passent par là : deux contextes vivants ne se marchent pas
  // dessus, et un test n'a rien à ordonner.
  void MakeCurrent() const;
  // LA DISPOSITION SE CALCULE CONTRE LE DERNIER ÉCRAN CONNU, JAMAIS CONTRE UN
  // ÉCRAN NUL.
  //
  // 🔴 UNE FENÊTRE RÉDUITE DANS LA BARRE DES TÂCHES A UN ÉCRAN DE TAILLE NULLE,
  // et ce n'est pas une supposition : le pont SDL3 pose lui-même `w = h = 0`
  // dès que la fenêtre porte `SDL_WINDOW_MINIMIZED`. Tout ce qui se mesure en
  // part d'écran vaut alors zéro.
  //
  // 🔴 ET LE DÉGÂT SURVIT AU RETOUR, parce qu'un plafond de taille n'est pas un
  // rognage à l'affichage : il ÉCRIT dans la fenêtre, et part ensuite dans les
  // réglages. La chatbox, dont la hauteur est une part d'écran, revenait au
  // plancher — le joueur retrouvait une conversation de quatre lignes, et rien
  // dans le client ne disait d'où cela venait.
  //
  // ⚠ C'EST ICI QUE LA RÈGLE VIT, et non dans chaque fenêtre qui divise par
  // l'écran : elles sont une douzaine, et la prochaine ne saura pas.
  void KeepLastRealScreen();
  // Les deux moitiés du gestionnaire de réglages : lire une ligne, tout écrire.
  void ReadSetting(const char* line);
  void WriteSettings(ImGuiTextBuffer* out) const;

  ImGuiContext* context_ = nullptr;
  bool frame_open_ = false;
  // Gardée vivante : ImGui ne copie pas le chemin qu'on lui donne.
  std::string settings_path_;
  // Les interrupteurs à retenir, par nom de fenêtre. Les adresses ne sont pas
  // possédées.
  std::map<std::string, bool*> remembered_;
  // ⚠ LE STYLE D'ORIGINE, GARDÉ INTACT. `ScaleAllSizes` MULTIPLIE ce qui est
  // déjà là : l'appliquer deux fois donne le carré, et aucune division ne revient
  // en arrière — les valeurs sont tronquées au pixel au passage. On redonne donc
  // le style d'origine avant chaque mise à l'échelle.
  //
  // Par pointeur opaque : `ImGuiStyle` est une structure d'ImGui, et cet
  // en-tête ne l'inclut pas — il est lu par `app`, qui n'a pas à connaître
  // ImGui.
  std::unique_ptr<ImGuiStyle> base_style_;
  float interface_scale_ = 1.0F;
  // La place voulue de chaque fenêtre, et ce que la passe a posé. Voir
  // `WindowPlacer`.
  WindowPlacer placer_;
  // LE DERNIER ÉCRAN QUI EXISTAIT VRAIMENT, en pixels de tampon. Voir
  // `KeepLastRealScreen`. ⚠ DEUX FLOTTANTS ET NON UN `ImVec2` : cet en-tête est
  // lu par `app`, qui n'inclut pas ImGui.
  float last_real_width_px_ = 0.0F;
  float last_real_height_px_ = 0.0F;
  // CE QUE L'ATLAS A BÂTI, une entrée par famille et par dessin, et le choix.
  //
  // 🔴 À CE CONTEXTE, ET NON EN GLOBALES DE FICHIER : deux contextes vivants se
  // les partageaient, et détruire le second laissait au premier des `ImFont*`
  // pointant dans un atlas libéré.
  std::array<std::array<ImFont*, static_cast<std::size_t>(FontStyle::kCount)>, kMostFontFamilies>
      fonts_{};
  ImFont* builtin_font_ = nullptr;
  int chosen_font_ = 0;
  std::size_t family_count_ = 0;
  // Le nom de chaque famille bâtie, au rang de `fonts_`.
  std::array<std::string, kMostFontFamilies> family_labels_{};
  // LES POLICES DU DOSSIER DU JEU, avec leurs octets : l'atlas lit dedans sans
  // les posséder, et ils doivent lui survivre — même ordre de destruction que
  // le fichier d'emoji.
  struct LooseFont {
    std::string label;
    std::vector<char> bytes;
    ImFont* font = nullptr;
  };
  std::vector<LooseFont> loose_fonts_;
  // Le fichier d'emoji en mémoire, et le chemin d'où il vient. Voir `UseFonts`.
  std::vector<char> emoji_bytes_;
  std::string emoji_path_;
  bool emoji_read_ = false;
};

}  // namespace ro::ui
