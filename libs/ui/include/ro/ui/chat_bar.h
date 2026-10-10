#pragma once

namespace ro::ui {

// LE BATTLE MODE : LA BARRE DE SAISIE SE REPLIE, ET LE CLAVIER REVIENT AU JEU.
//
// C'est le mode du client d'origine — `/bm`, `/battlemode` — et il ne se
// discute pas : sur une carte où l'on se bat, chaque touche doit servir au
// personnage, pas partir dans un champ de texte qu'on n'a pas demandé. La barre
// n'est plus là ; ENTRÉE la déplie, et elle disparaît dès qu'on a fini.
//
// 🔴 CES RÈGLES VALENT QUE LA BARRE AIT LE CLAVIER OU NON, et c'est tout
// l'enjeu. Une barre dépliée qui a perdu le focus — un clic ailleurs suffit —
// deviendrait autrement un piège : ni sortie ni écriture sans aller cliquer
// dedans à la souris. Les touches se traitent donc HORS du champ, dans la
// fenêtre, et non dans le widget qui n'est actif que focalisé.
//
// ⚠ POURQUOI PAS UN CHAMP QUI GARDE LE CLAVIER EN PERMANENCE, comme le fait le
// client d'origine : ImGui l'interdit. Tant qu'un widget détient l'`ActiveId`,
// plus rien n'est survolable ailleurs — le premier clic est mangé partout, et
// un double-clic ne peut jamais aboutir. Le relevé est dans `chat_input.h`, à
// `ChatInputState::typed`. Ce que l'invariant du client protégeait vraiment —
// sortir en une frappe — est tenu par Échap, traité ici.

// LA BARRE EST-ELLE À L'ÉCRAN ? Hors battle mode elle l'est toujours ; dedans,
// seulement dépliée.
[[nodiscard]] constexpr bool ChatBarShown(bool battle_mode, bool unfolded) noexcept {
  return !battle_mode || unfolded;
}

// CE QU'ENTRÉE FAIT DE LA BARRE, quand aucune zone de texte n'écrit.
//
// ⚠ « OUVERTE » NE VEUT PAS DIRE LA MÊME CHOSE DANS LES DEUX MODES. En battle
// mode c'est le dépliement ; hors battle mode la barre est là en permanence, et
// exiger le dépliement renverrait TOUJOURS vers « on l'ouvre » — une barre
// pleine mais sans clavier demanderait alors DEUX Entrée pour envoyer, la
// première ne servant qu'à reprendre le focus. C'est ce qui bloque un lien
// d'objet posé pendant un dialogue : la touche part au dialogue, qui l'attend
// pour sa page suivante.
enum class ChatBarEnter {
  // Elle n'était pas là : on la déplie, avec le clavier.
  kUnfold,
  // Elle est là, et vide : le clavier lui revient.
  kFocus,
  // Elle est là, et porte une phrase : on envoie, et on garde la main.
  kSend,
  // Elle est là, vide, et le battle mode l'avait dépliée : on SORT, en une
  // frappe.
  kFold,
};

[[nodiscard]] constexpr ChatBarEnter ChatBarEnterDoes(bool battle_mode, bool unfolded,
                                                      bool has_text) noexcept {
  if (!ChatBarShown(battle_mode, unfolded)) {
    return ChatBarEnter::kUnfold;
  }
  if (has_text) {
    return ChatBarEnter::kSend;
  }
  return battle_mode ? ChatBarEnter::kFold : ChatBarEnter::kFocus;
}

// ÉCHAP REPLIE-T-IL LA BARRE ?
//
// 🔴 ET LA TOUCHE EST ALORS CONSOMMÉE. Sans cela une seule frappe refermerait
// la barre ET la fenêtre derrière — ou, s'il n'y en avait aucune, ouvrirait le
// menu du jeu. Deux effets pour un geste.
//
// ⚠ HORS BATTLE MODE, ÉCHAP N'APPARTIENT PAS À LA BARRE : elle est permanente,
// il n'y a rien à replier, et la touche doit continuer de fermer ce que le
// joueur regarde.
[[nodiscard]] constexpr bool ChatBarFoldsOnEscape(bool battle_mode, bool unfolded) noexcept {
  return battle_mode && unfolded;
}

// LA BARRE RÉCLAME-T-ELLE LES FRAPPES, sans tenir le clavier ?
//
// 🔴 C'EST CETTE RÈGLE QUI REND LES AUTRES POSSIBLES. Tant qu'Entrée devait
// servir à reprendre le clavier, elle ne pouvait pas refermer. Depuis que TAPER
// rend le clavier tout seul, Entrée retrouve son seul sens : ouvrir, envoyer,
// ou sortir.
//
// ⚠ EN BATTLE MODE SEULEMENT, et la raison tient en une phrase : hors battle
// mode la barre est là en permanence, et confisquer chaque lettre au jeu parce
// qu'un champ traîne à l'écran retirerait au joueur toutes ses touches de
// déplacement, sans qu'il ait rien demandé.
//
// `text_elsewhere` dit qu'une zone de texte écrit déjà — la nôtre une fois
// focalisée, ou celle d'une autre fenêtre : les caractères lui arrivent par le
// chemin ordinaire, et nous n'avons rien à faire.
[[nodiscard]] constexpr bool ChatBarTakesTyping(bool battle_mode, bool unfolded,
                                                bool text_elsewhere) noexcept {
  return battle_mode && unfolded && !text_elsewhere;
}

}  // namespace ro::ui
