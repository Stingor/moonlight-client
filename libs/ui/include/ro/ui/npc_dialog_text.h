#pragma once

#include <cstdint>
#include <optional>
#include <span>
#include <string>
#include <string_view>
#include <vector>

#include <imgui.h>

#include "ro/ui/chat_link.h"
#include "ro/ui/rich_text.h"

namespace ro::ui {

// LE TEXTE D'UN SCRIPT DE PNJ : ses couleurs, ses balises, et sa mise en page.
//
// ⚠ POURQUOI PAS `rich_text.h`, QUI LIT DÉJÀ DU TEXTE BALISÉ. Les deux ne
// disposent pas la même chose. Une description est un bloc de texte à HAUTEUR DE
// LIGNE CONSTANTE, qu'on veut pouvoir SÉLECTIONNER : il y pose une boîte par
// glyphe. Une page de dialogue porte des icônes, des sprites et des images au
// milieu des mots — donc des lignes dont la hauteur DÉPEND de ce qu'elles
// contiennent —, et jusqu'à quelques milliers de lignes qu'il faut replacer sans
// saccade quand la fenêtre change de largeur : elle pose une boîte par MOT et la
// garde en cache. Fondre les deux demanderait à la géométrie de l'une de porter
// le cas de l'autre, pour un gain nul : le balisage, lui, est partagé — c'est le
// même vocabulaire, lu par le même client.
//
// ── LE VOCABULAIRE DU CLIENT ────────────────────────────────────────────────
//
// `^RRGGBB`  change la couleur. ⚠ `^000000` N'EST PAS DU NOIR : c'est le retour
//            à la couleur par défaut.
// `^i[id]`   l'icône d'un objet, au fil du texte.
// `<B>`/`<I>`               gras, italique.
// `<FONT …>`                ignoré : une seule police est chargée.
// `<ITEM>nom<INFO>id</INFO></ITEM>`   un objet ; le clic ouvre sa description.
// `<URL>texte<INFO>adresse</INFO></URL>`   le dehors.
// `<NAVI>texte<INFO>carte,x,y</INFO></NAVI>`   un lieu.
// `<QUEST>`, `<TIPBOX>`, `<MSG>`      reconnus, non suivis.
//
// 🔴 C'EST UNE LISTE BLANCHE, ET ELLE DÉCIDE SEULE DE CE QUI EST UNE BALISE.
// Tout autre mot entre chevrons est du TEXTE. Un parseur qui avale ce qui tient
// entre `<` et le premier `>` fait disparaître la moitié d'une phrase : « Enter
// ^6B9900<Wave Mode - Forest>^000000. » ne montrait que « Enter . », là où le
// client officiel rend la phrase entière.
//
// ── LE VOCABULAIRE DE LA MAISON ─────────────────────────────────────────────
//
// Les balises que le client officiel ne connaît PAS, et qui sont donc à nous.
// Champs séparés par ':' et le champ libre EN DERNIER — un nom de monstre porte
// des espaces, une adresse porte des ':'.
//
//   `<MOBL>id:rang:nom</MOBL>`   lien monstre (rang 0 normal / 1 boss / 2 MVP)
//   `<ITMR>id:nom</ITMR>`        lien objet de base, sans raffinement ni cartes
//   `<CRAF>id:nom</CRAF>`        lien recette
//   `<SETL>clé:libellé</SETL>`   lien vers un réglage
//   `<STAL>efst:libellé</STAL>`  lien vers un ÉTAT, par index
//   `<MOBS>id</MOBS>`            sprite de monstre, au fil du texte
//   `<MOBP>id</MOBP>`            portrait de PAGE, dans sa colonne
//   `<IMG>source</IMG>`          image : ressource du jeu, ou adresse
//   `<IMG>l:h:source</IMG>`      la même, à la taille imposée
//
// 🔴 CE QU'ELLES COÛTENT À QUI NE LES REND PAS : le dialogue du client officiel
// efface les balises qu'il connaît et laisse passer les autres. Un joueur resté
// sur le client d'origine lirait donc `<MOBL>1002:0:Poring</MOBL>` en toutes
// lettres — d'où le nom en clair que la balise transporte, second filet qui rend
// la ligne lisible même brute.
//
// ⚠ LE NOM VOYAGE, et ce n'est pas une facilité : le client ne sait pas nommer
// un monstre. Une balise qui ne porterait que l'identifiant afficherait un
// numéro.

// CE QU'UN LIEN DU CLIENT OUVRE.
enum class DialogLink : std::uint8_t {
  kNone,
  // Un objet : sa description. La charge est son identifiant, en chiffres.
  kItem,
  // Le dehors. ⚠ `ui` NE L'OUVRE PAS — elle rend l'intention, et `app` décide.
  kAddress,
  // Un lieu : « carte,x,y ».
  kPlace,
  // Reconnus pour ne pas s'afficher, mais sans destination : le client officiel
  // les mène à des fenêtres que nous n'avons pas.
  kQuest,
  kNotice,
};

// CE QU'UNE BALISE DE LA MAISON DÉSIGNE.
enum class DialogTargetKind : std::uint8_t {
  kNone,
  kItem,
  kMonster,
  kRecipe,
  kSetting,
  kStatus,
};

// LE RANG D'UN MONSTRE, tel que `<MOBL>` le transporte.
enum class MonsterRank : std::uint8_t { kOrdinary, kBoss, kChampion };

struct DialogTarget {
  DialogTargetKind kind = DialogTargetKind::kNone;
  // L'identifiant : objet, monstre, recette, index d'état. Une clé de réglage
  // n'est pas un nombre, et vit dans `key`.
  std::uint32_t id = 0;
  MonsterRank rank = MonsterRank::kOrdinary;
  std::string key;
  // Le libellé TRANSPORTÉ par le script. ⚠ Il ne fait pas foi partout : un état
  // et un réglage se nomment chez le LECTEUR, dans sa langue, et le texte du
  // script n'est alors qu'un repli.
  std::string label;

  [[nodiscard]] bool valid() const noexcept { return kind != DialogTargetKind::kNone; }
};

// UN MÉDIA : ni texte ni lien, un rectangle à placer.
enum class DialogMedia : std::uint8_t { kNone, kImage, kMonsterSprite };

// UN MORCEAU DE LIGNE HOMOGÈNE, tel que le parseur le rend.
struct DialogRun {
  // Le texte affiché, en UTF-8. Vide pour une icône ou un média.
  std::string text;
  // La couleur demandée, ou zéro pour celle du thème.
  ImU32 color = 0;
  bool bold = false;
  bool italic = false;

  DialogLink link = DialogLink::kNone;
  // Ce que `<INFO>` portait : un identifiant d'objet, une adresse, un lieu.
  std::string payload;

  // L'objet dont l'icône s'affiche ici (`^i[id]`), ou zéro.
  std::uint32_t icon_item = 0;

  DialogTarget target;

  DialogMedia media = DialogMedia::kNone;
  // `kImage` : le chemin d'une ressource du jeu, ou une adresse.
  std::string source;
  // `kMonsterSprite` : la classe du monstre.
  std::uint32_t monster_id = 0;
  // Ce que le script impose, ou zéro : à nous de choisir.
  float wanted_width = 0.0F;
  float wanted_height = 0.0F;
};

// CE QUE LE LECTEUR SAIT NOMMER, et que cette couche ignore.
//
// 🔴 DEUX BALISES NE PORTENT PAS LEUR NOM, ET C'EST VOULU. Un réglage et un état
// se nomment CHEZ CELUI QUI REGARDE : chaque client les écrit dans sa langue,
// depuis ses propres tables, et le libellé que le script transporte n'est qu'un
// repli pour les clients qui ne connaissent pas la destination. Un PNJ écrit
// donc une clé ou un index, et chacun lit le nom qu'il connaît.
//
// ⚠ ET C'EST AUSSI CE QUI DÉCIDE QU'IL Y A UN LIEN : une destination que ce
// client n'a pas — une section qui n'existe pas ici, un état absent de son
// installation — ne forme AUCUN lien. Le libellé du script reste alors, en texte
// simple : il dit encore de quoi on parle, il ne prétend plus mener quelque
// part.
struct DialogVocabulary {
  // Le libellé AFFICHÉ d'un lien de réglage, enveloppe comprise et déjà
  // traduit, ou une chaîne vide.
  std::string (*setting_label)(std::string_view key, void* owner) = nullptr;
  // Le nom d'un état, ou une chaîne vide.
  std::string (*status_label)(std::uint32_t effect_id, void* owner) = nullptr;
  void* owner = nullptr;
};

// Découpe une ligne. Le texte rendu est déjà celui qui s'affiche : les balises
// en sont retirées, et celles de la maison y ont laissé leur libellé décoré.
//
// Sans vocabulaire, `<SETL>` et `<STAL>` retombent sur le libellé que le script
// transporte — ce qui reste lisible, et ne mène nulle part.
[[nodiscard]] std::vector<DialogRun> ParseDialogLine(std::string_view line,
                                                     const DialogVocabulary& words = {});

// LE MENU DU CLIC DROIT D'UNE CIBLE, par le genre de lien dont il reprend les
// entrées ; rien pour une cible qui n'a pas de menu à elle — son clic droit ne
// fait rien plutôt qu'ouvrir un menu qui parlerait d'autre chose.
[[nodiscard]] std::optional<ChatLinkKind> DialogTargetMenuKind(DialogTargetKind kind);

// LE PORTRAIT DE LA PAGE (`<MOBP>`), ou zéro.
//
// 🔴 IL NE PEUT PAS ÊTRE RELEVÉ PAR LA MISE EN PAGE, alors que c'est là que
// vivent toutes les autres balises : la mise en page se refait à chaque
// changement de largeur du texte, or c'est le portrait qui DÉCIDE de cette
// largeur — il occupe une colonne. L'y lire ferait dépendre la largeur d'un
// calcul qui dépend de la largeur.
//
// Le DERNIER de la page gagne : un script qui en pose deux a changé d'avis, et
// c'est sa dernière phrase qui décrit l'écran affiché.
[[nodiscard]] std::uint32_t PageMonsterPortrait(std::span<const std::string> lines);

// UN FRAGMENT DÉJÀ PLACÉ : un mot, une icône, un média.
//
// Les distances partent du coin haut gauche du contenu, et non de l'écran : le
// placement survit ainsi au défilement, et ne se recalcule qu'au changement de
// page ou de largeur.
struct DialogFragment {
  float left = 0.0F;
  float top = 0.0F;
  float width = 0.0F;
  float height = 0.0F;
  // Où commence le soulignement d'un lien : au début des espaces qui le
  // précèdent, pour que tout son libellé se souligne d'un trait — « <[Mob]
  // Poring> » fait deux mots.
  float underline_left = 0.0F;

  // Le texte, en UTF-8. Vide pour une icône ou un média.
  std::string text;
  ImU32 color = 0;
  bool bold = false;

  DialogLink link = DialogLink::kNone;
  std::string payload;
  std::uint32_t icon_item = 0;

  // 🔴 LA CIBLE ET LA SOURCE SONT DÉSIGNÉES PAR INDEX, PAS PORTÉES. Une
  // `DialogTarget` pèse ses deux chaînes, et une page en compte des dizaines de
  // milliers de fragments — un agent de téléportation en produit autant qu'il y
  // a de mots. Les porter coûterait des mégaoctets de champs vides. Moins un :
  // aucune.
  int target = -1;
  int source = -1;
  DialogMedia media = DialogMedia::kNone;
  std::uint32_t monster_id = 0;
};

// CE QUE LA MISE EN PAGE A DEMANDÉ HORS D'ELLE, et ce qu'on lui a répondu.
//
// 🔴 UNE IMAGE ARRIVE SOUVENT APRÈS SA PAGE : téléchargée, ou décodée à la
// première demande. Mesurée nulle, elle n'a pas de place ; revenue, elle en
// veut une. Garder la question et la réponse permet de savoir, à chaque image,
// si la page doit être refaite — sans la refaire pour le savoir.
struct DialogMediaProbe {
  DialogRun run;
  float width = 0.0F;
  float height = 0.0F;
};
struct DialogIconProbe {
  std::uint32_t item_id = 0;
  float aspect = 0.0F;
};

// LA PAGE DISPOSÉE.
struct DialogLayout {
  std::vector<DialogFragment> fragments;
  // Ce que les fragments désignent par index. Reconstruits avec la mise en page,
  // donc jamais désaccordés d'elle.
  std::vector<DialogTarget> targets;
  std::vector<std::string> sources;
  float height = 0.0F;
  // La largeur et la taille de police pour lesquelles tout ceci vaut.
  float wrap_width = 0.0F;
  float font_size = 0.0F;
  // Les mesures venues d'ailleurs : voir `DialogMeasuresChanged`.
  std::vector<DialogMediaProbe> media_probes;
  std::vector<DialogIconProbe> icon_probes;
};

// LA PLACE QU'UN MÉDIA OCCUPE, décidée hors de cette couche.
//
// ⚠ ELLE DOIT ÊTRE DÉTERMINISTE POUR UN ÉTAT DONNÉ, et c'est ce qui la rend
// sûre : le dessin la redemande et la compare à la réponse notée pour savoir
// si une image est arrivée entre-temps. Deux appels dans le même état doivent
// rendre la même chose, sinon la page se remet en page à chaque image.
struct DialogMediaSize {
  float width = 0.0F;
  float height = 0.0F;
};
using DialogMediaMeasure = DialogMediaSize (*)(const DialogRun& media, float available_width,
                                               void* owner);

// LE RAPPORT LARGEUR / HAUTEUR D'UNE ICÔNE D'OBJET, ou zéro si elle manque.
//
// ⚠ UN RAPPORT, PAS DES PIXELS : une icône au fil du texte prend la hauteur
// d'une ligne, et sa largeur en découle. Passer sa taille brute la ferait
// changer de taille avec le thème.
using DialogIconAspect = float (*)(std::uint32_t item_id, void* owner);

// De quoi mesurer et placer une page.
struct DialogMetrics {
  // La largeur de repli, et le pas d'une ligne de texte.
  float wrap_width = 0.0F;
  float line_height = 0.0F;
  // La hauteur des glyphes, qui n'est pas celle de la ligne.
  float font_size = 0.0F;
  // ⚠ LA MESURE EST PASSÉE PLUTÔT QUE PRISE À IMGUI, et c'est ce qui rend la
  // disposition ESSAYABLE : le repli et la coupure d'un mot sont exactement là
  // où les fautes se logent, et un banc d'essai n'a ni police ni fenêtre.
  RichMeasure measure = nullptr;
  void* measure_owner = nullptr;
  DialogIconAspect icon_aspect = nullptr;
  void* icon_owner = nullptr;
  DialogMediaMeasure media_measure = nullptr;
  void* media_owner = nullptr;
  // Ce que le lecteur sait nommer : voir `DialogVocabulary`.
  DialogVocabulary words;
};

// Place toutes les lignes d'une page.
[[nodiscard]] DialogLayout LayOutDialog(std::span<const std::string> lines,
                                        const DialogMetrics& metrics);

// UNE MESURE VENUE D'AILLEURS A-T-ELLE CHANGÉ depuis la mise en page ? Une
// image arrivée, une icône chargée : la page est alors à refaire. Elle repose
// les mêmes questions, sans rien placer.
[[nodiscard]] bool DialogMeasuresChanged(const DialogLayout& laid, const DialogMetrics& metrics);

}  // namespace ro::ui
