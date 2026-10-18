#pragma once

#include "ro/ui/entry_queue_window.h"

namespace ro::ui {

// LES PIÈCES DE L'INSCRIPTION, partagées entre la liste
// (`entry_queue_window.cpp`) et les fenêtres de la file
// (`entry_queue_prompts.cpp`). Chacune rend le verbe choisi, ou `kNothing`.

// « En attente » : l'effectif, le temps écoulé, annuler ou fermer sans annuler.
[[nodiscard]] EntryQueueAction DrawEntryQueueStandby(EntryQueueWindowState& state,
                                                     const EntryQueueView& view,
                                                     const SkinArt& art);
// « Terrain prêt » : la modale, son compte à rebours, entrer ou refuser.
[[nodiscard]] EntryQueueAction DrawEntryQueueReady(EntryQueueWindowState& state,
                                                   const EntryQueueView& view, const SkinArt& art);
// La boîte qui dit un résultat.
[[nodiscard]] EntryQueueAction DrawEntryQueueNotice(EntryQueueWindowState& state,
                                                    const EntryQueueView& view, const SkinArt& art);

// Le rang du bouton de rapport de bug parmi les actions de titre : c'est le
// seul que ces fenêtres posent.
inline constexpr int kEntryQueueBugReportRank = 0;

// LA LARGEUR DE REPLI DES TEXTES, en hauteurs de police : une fenêtre qui
// s'ajuste à son contenu s'élargirait sinon à la phrase la plus longue.
inline constexpr float kEntryQueueWrapFonts = 24.0F;

// Pourquoi un bouton qui envoie est éteint hors liaison. Traduit.
[[nodiscard]] const char* EntryQueueLinkPendingText();

// LA CONFIRMATION D'UN VERBE, en deux temps : le bouton la DEMANDE, puis la
// fenêtre la DESSINE à chaque image. Les deux appels se font au niveau de la
// fenêtre, hors de tout enfant : la boîte et sa demande doivent partager la
// pile d'identités, sans quoi elle ne paraît jamais.
//
// ⚠ DEMANDÉE AU CLIC SEULEMENT, jamais redemandée d'elle-même : une boîte
// fermée par un autre surgissant ne revient pas fermer celui-ci à son tour.
void AskEntryQueueVerb(EntryQueueWindowState& state, EntryQueueVerb verb);
// Rend vrai quand le joueur dit « oui » ; `confirming` revient à `kNothing` dès
// que la boîte se ferme, quelle qu'en soit la raison.
[[nodiscard]] bool ConfirmEntryQueueVerb(EntryQueueWindowState& state, EntryQueueVerb verb,
                                         const char* question, const char* yes, const char* no,
                                         const SkinArt& art);

}  // namespace ro::ui
