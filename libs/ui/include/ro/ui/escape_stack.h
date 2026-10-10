#pragma once

#include <vector>

#include <imgui.h>

// ⚠ DÉCLARÉE, PAS INCLUSE. Le type d'une fenêtre vit dans l'en-tête INTERNE
// d'ImGui, que l'on ne veut pas imposer à qui inclut celui-ci : seule la source
// en a besoin.
struct ImGuiWindow;

namespace ro::ui {

// ÉCHAP FERME LA DERNIÈRE FENÊTRE OUVERTE, UNE PAR UNE.
//
// ⚠ ET IL EST AVALÉ TANT QU'IL EN RESTE UNE. C'est le geste que le joueur essaie
// en premier : une fenêtre qui lui résiste passe pour bloquée. Et un Échap qui
// QUITTE LE JEU parce qu'une fenêtre traînait ouverte est pire encore — c'est ce
// que faisait le client jusqu'ici.
//
// ⚠ LA PLUS AU-DESSUS D'ABORD, et non la dernière ouverte : le joueur ferme ce
// qu'il REGARDE. L'ordre de focus d'ImGui dit lequel c'est.
//
// ⚠ ELLE NE POSSÈDE RIEN ET NE DESSINE RIEN. Chaque image, l'étage d'interface
// lui déclare les fenêtres ouvertes ; elle décide, et se vide. Une pile qui
// survivrait à l'image désignerait des fenêtres qui n'existent plus.
class EscapeStack {
 public:
  // Déclare une fenêtre fermable. `open` est l'interrupteur qui la tient
  // ouverte — celui-là même que la fenêtre lit —, et il doit vivre plus
  // longtemps que l'image.
  //
  // Une fenêtre fermée, absente, ou sans interrupteur ne compte pas.
  void Register(const char* window_name, bool* open);
  // Déclare une fenêtre QU'ON LIT, comme une description : Échap la ferme
  // AVANT toute autre, même si le joueur a cliqué ailleurs depuis. On l'ouvre
  // pour un coup d'œil ; la laisser derrière l'inventaire qu'on vient de
  // toucher ferait fermer l'inventaire à sa place.
  void RegisterReading(const char* window_name, bool* open);

  // Sort la PROCHAINE déclaration de la pile : cette fenêtre ne se fermera pas
  // sur Échap, et n'avalera pas la touche non plus.
  //
  // ⚠ À RÉSERVER À CE QU'ON GARDE OUVERT EN TRAVAILLANT — un établi, un banc
  // d'essai. Pour tout le reste, Échap doit fermer.
  void SkipNext() { skip_next_ = true; }

  // Un dialogue modal a pris Échap cette image : rien derrière lui ne se ferme.
  // Sans cela, une seule touche fermerait la boîte ET la fenêtre dessous.
  void Suppress() { suppressed_ = true; }

  // Décide, puis se vide. Une fois par image, après tout le dessin.
  void Process();

  // Vrai quand Échap a de quoi faire. ⚠ C'EST LE VERDICT DE L'IMAGE PRÉCÉDENTE,
  // et c'est ce qu'il faut : la touche arrive avant que l'image ne se dessine, et
  // ce qui était ouvert alors est ce que le joueur avait sous les yeux.
  [[nodiscard]] bool has_work() const noexcept { return has_work_; }

  // Vrai quand Échap a été pressé CETTE image et que personne ne l'a pris : ni
  // une fenêtre à fermer, ni un surgissant. ⚠ VERDICT DU DERNIER `Process` :
  // c'est à l'appelant de dire ce qu'un Échap libre veut dire sur son écran.
  [[nodiscard]] bool escape_unclaimed() const noexcept { return escape_unclaimed_; }

 private:
  struct Closable {
    ImGuiWindow* window = nullptr;
    // Une fenêtre qu'on lit, fermée avant les autres.
    bool reading = false;
    bool* open = nullptr;
  };

  void Add(const char* window_name, bool* open, bool reading);

  std::vector<Closable> open_windows_;
  bool skip_next_ = false;
  bool suppressed_ = false;
  bool has_work_ = false;
  bool escape_unclaimed_ = false;
};

}  // namespace ro::ui
