#pragma once

#include <array>
#include <cstddef>
#include <cstdint>
#include <filesystem>
#include <string>
#include <string_view>
#include <vector>

#include "ro/vfs/vfs.h"

// LE JUGE DES EFFETS : combien des identifiants que le client peut montrer
// montrent quelque chose CHEZ NOUS, et lesquels ne montrent rien.
//
// POURQUOI IL EXISTE. Sans lui, « ça ne marche pas » reste une impression, et
// le compte de ce qui se voit reste un commentaire qui périme. Nos tables
// engendrées disent ce que nous SAVONS montrer ; aucune ne peut donc rendre un
// manque. Le juge part de l'autre bout — des tables du CLIENT — et essaie chacun.
//
// ⚠ SON DÉNOMINATEUR EST UNE UNION, et la mesure a défait l'hypothèse contraire.
// Le répartiteur procédural traite 1 112 identifiants ; les deux tables de
// fichiers du client en nomment 1 157 ; et 227 seulement sont dans les deux. Un
// juge assis sur les seuls cas procéduraux déclarerait introuvables 930 effets
// que le client montre très bien.
//
// ⚠ IL ESSAIE VRAIMENT. Il ne consulte pas des tables pour dire qu'un effet est
// connu : il joue chaque identifiant par le MÊME chemin que le jeu, fait battre
// son horloge jusqu'au bout de sa vie, et compte les quads composés. Un effet
// dont toutes les tables parlent mais dont aucune texture ne se lit compte pour
// zéro, ce qui est exactement ce qu'il vaut à l'écran.
//
// ⚠ ET LES DEUX FAMILLES, parce que le banc a déjà payé de n'en montrer qu'une :
// chercher Firewall parmi les seuls effets procéduraux était vain, son effet
// ayant un fichier. Un juge qui ne voit qu'une moitié fait conclure qu'une chose
// manque alors qu'elle est là.
//
// SON RAPPORT EST REPRODUCTIBLE, et cela ne tient pas au hasard : les figures
// qui tirent au sort tirent d'une graine faite de la taille de la troupe et du
// battement, l'une et l'autre remises à zéro entre deux essais. Deux exécutions
// sur la même installation rendent le même fichier, octet pour octet — c'est ce
// qui permet de le comparer et d'y lire ce qui a bougé.
namespace moonclient {

// POURQUOI un effet est resté muet.
//
// ⚠ SANS CELA, LA LISTE DES MUETS N'EST QU'UNE LISTE. Chacune de ces causes
// envoie à un endroit différent : une ressource absente n'est pas notre défaut
// et se déclare, une figure qui ne compose rien envoie au dessinateur, un semis
// qui n'a rien posé envoie relire le patron. Les confondre revient à rouvrir
// cent dix-sept cas à la main pour trier ce qu'une mesure sait dire.
enum class SilentBecause : std::uint8_t {
  // Il se voit, ou aucune de nos tables n'en parle : il n'y a rien à expliquer.
  kNotSilent,
  // Le `.str` que nos tables lui donnent n'est pas dans cette installation. Le
  // client natif ne montre rien non plus : c'est un FAUX TROU.
  kFileMissing,
  // Le fichier est là et se lit, et ne compose pourtant aucun quad visible.
  kFileShowsNothing,
  // Un patron le nomme, et le semis n'a posé aucun nœud.
  kNothingSown,
  // Des nœuds vivent, mais leur image ne se lit pas.
  kNoTexture,
  // Le nœud est MORT avant qu'une seule de ses pistes ait commencé.
  //
  // ⚠ C'EST UN FAUX TROU. Sans cette cause, un tel effet se range sous la
  // suivante et envoie relire un dessinateur irréprochable. Une piste peut
  // naître avec un RETARD — son horloge est négative et rien ne bouge tant
  // qu'elle l'est — et le nœud, lui, a sa propre vie. Quand la seconde s'épuise
  // avant que la première n'atteigne zéro, la figure n'a jamais eu sa chance :
  // elle n'est pas en cause.
  //
  // ⚠ CE N'EST PAS FORCÉMENT UN FAUX TROU, et le seul cas qui l'a fait naître
  // n'en était PAS un. EF_POKJUK (297) semblait mourir avant de peindre ; la
  // table des durées du client lui donne 99999999 — l'infini. Ce qui le tuait
  // était NOTRE valeur, pas celle du client. Voir ADR 0042.
  //
  // La cause reste juste et reste utile : elle dit qu'une figure n'a jamais été
  // appelée, donc qu'il ne sert à rien de relire son dessinateur. Mais elle
  // accuse alors la DURÉE — la nôtre d'abord, celle du client ensuite.
  kDiedBeforeStart,
  // Des nœuds vivent, leur image se lit, et la figure ne rend aucun quad.
  kNoQuad,
};

[[nodiscard]] std::string_view DescribeSilence(SilentBecause cause);

// CE QUE LA TROUPE A ATTEINT DE MIEUX sur toute la vie de l'effet.
//
// ⚠ « LA FIGURE NE REND AUCUN QUAD » EST UN CONSTAT, PAS UN DIAGNOSTIC, et le
// tas qui reste après les deux premières corrections est justement celui-là :
// trente-quatre identifiants répartis sur seize fonctions, sans tête.
//
// ⚠ ET LES TROIS CRITÈRES DE PISTE N'ONT RIEN REFENDU, ce qui est la mesure la
// plus utile de cette fiche. « Piste jamais allumée », « alpha jamais monté »
// et « rayon resté nul » paraissaient accuser le semis, le mouvement et la
// graine ; le contrôle les a tous trois abattus — 254, 57 et 314 effets qui SE
// VOIENT les portent aussi. La raison se voit après coup : la moitié des formes
// ne tire rien de ses pistes, si bien qu'un critère pris sur elles ne sépare
// pas. Les nombres restent ici parce que le CONTRÔLE en a besoin ; aucun ne
// sert à accuser. Voir `EffectJudgement::showing_with_signature` et ADR 0028.
//
// ⚠ LE MAXIMUM SUR LA VIE, et non l'état final : un nœud naît alpha nul et
// rayon nul — c'est la règle de la famille —, si bien qu'un instantané pris à
// la fin accuserait tout le monde.
struct FigureTrace {
  float best_node_alpha = 0.0F;
  std::int16_t best_track_alpha = 0;
  float best_radius = 0.0F;
  bool any_track_active = false;
  // Vrai dès qu'une piste allumée a vu son horloge atteindre zéro, c'est-à-dire
  // dès qu'une seule a COMMENCÉ.
  //
  // ⚠ SANS LUI, « LA FIGURE NE REND RIEN » NE SE DISTINGUE PAS DE « LA FIGURE
  // N'A JAMAIS ÉTÉ APPELÉE ». Une piste retardée garde une horloge négative et
  // ne bouge pas ; si le nœud meurt avant, aucun dessinateur n'a été consulté,
  // et l'accuser envoie lire le mauvais code.
  bool any_track_started = false;
  // Combien de nœuds ont vécu au plus. Zéro dit que la troupe s'est vidée avant
  // le premier dessin, ce qui n'accuse ni la figure ni le mouvement.
  std::size_t most_nodes = 0;

  // LES FORMES que la troupe a portées, par leur numéro d'énumération.
  //
  // ⚠ C'EST UN FAIT, LÀ OÙ LE RESTE DE CETTE FICHE EST UNE INFÉRENCE, et c'est
  // ce qui le rend utile : une figure muette se lit dans SON dessinateur, et la
  // forme dit lequel. Grouper les muets par forme range trente-quatre cas
  // dispersés sur seize bâtisseurs en quelques dessinateurs à relire.
  //
  // Le numéro plutôt qu'un nom : les nommer demanderait un catalogue de
  // quatre-vingt-huit entrées, que `PooledNodeShape` porte déjà.
  std::array<std::uint8_t, 4> shapes = {};
  std::uint8_t shape_count = 0;
};

// Ce que la trace donne à lire, en une ligne : les FORMES portées, donc les
// dessinateurs à relire.
//
// ⚠ ELLE N'ACCUSE PLUS PERSONNE, et c'est une mesure. Les trois signatures de
// piste qu'elle nommait sont portées par des centaines d'effets qui se voient ;
// les garder aurait envoyé vingt-quatre lectures au mauvais endroit. Ce qui
// reste est un FAIT — la forme du nœud — et non une inférence.
[[nodiscard]] std::string DescribeFigure(const FigureTrace& trace);

// Les trois signatures d'une trace, dans l'ordre de `showing_with_signature` :
// piste jamais allumée, alpha jamais monté, rayon resté nul.
[[nodiscard]] std::array<bool, 3> FigureSignatures(const FigureTrace& trace);

// Ce qu'un identifiant a donné à l'essai.
struct EffectVerdict {
  std::uint16_t effect_id = 0;
  // L'étage du répartiteur procédural qui porte ce cas, ou ZÉRO pour un
  // identifiant qui n'en a pas — c'est la majorité des effets en fichier.
  std::uint8_t stage = 0;
  // La fonction du client qui traite ce cas. VIDE quand il n'y en a pas de cas,
  // et vide aussi pour un cas traité inline sans bâtisseur nommé.
  std::string_view client_function;

  // Le `.str` que nos tables donnent à cet identifiant, ou vide. Le chemin est
  // relatif au dossier des effets.
  std::string file;
  // Combien de quads le fichier compose, toutes ses images additionnées. Zéro
  // avec un chemin non vide accuse une lecture, pas une table.
  std::size_t file_quads = 0;

  // Vrai quand une de nos tables procédurales connaît cet identifiant.
  bool knows_pattern = false;
  // Ce que la troupe a composé sur toute sa vie : les sprites posés, les quads
  // du monde, les cases de terrain peintes, les quads d'écran.
  std::size_t node_sprites = 0;
  std::size_t node_quads = 0;
  std::size_t ground_cells = 0;
  std::size_t screen_quads = 0;
  // Combien de nœuds la composition a écartés, faute d'image ou faute de quad.
  // Ce sont les deux comptes que la troupe tient d'elle-même.
  std::size_t nodes_without_texture = 0;
  std::size_t nodes_without_quad = 0;
  // Vrai quand nos tables nomment un fichier que l'installation n'a pas.
  bool file_missing = false;
  SilentBecause cause = SilentBecause::kNotSilent;
  // Ce que la troupe a atteint de mieux. Ne sert qu'aux muets de la dernière
  // cause, mais se relève pour tous : le coût est de quatre nombres.
  FigureTrace trace;

  // Ce que l'essai a produit, tout confondu. C'est le seul chiffre qui réponde
  // à « est-ce que cela se voit ».
  [[nodiscard]] std::size_t shown() const noexcept {
    return file_quads + node_sprites + node_quads + ground_cells + screen_quads;
  }
  [[nodiscard]] bool shows() const noexcept { return shown() != 0; }

  // Vrai quand une de nos tables le nomme et qu'il ne sort RIEN.
  //
  // ⚠ C'EST LE SEUL TAS QUI ACCUSE UN DÉFAUT. Un identifiant dont aucune table
  // ne parle attend un portage ; celui-ci, nous le nommons déjà, et le silence
  // vient d'une ressource absente ou d'une figure qui ne compose pas. Les
  // confondre dans un même « ne se voit pas » mêle ce qu'il reste à écrire et
  // ce qui est cassé.
  [[nodiscard]] bool silent() const noexcept {
    return !shows() && (!file.empty() || knows_pattern);
  }
};

// Le verdict sur la famille entière.
struct EffectJudgement {
  // Un par identifiant que le client peut montrer, par identifiant croissant.
  std::vector<EffectVerdict> verdicts;
  std::size_t showing = 0;
  // Combien tiennent leur image d'un fichier, d'un patron, ou des deux. Les
  // trois tas se recouvrent : un effet peut montrer par les deux chemins.
  std::size_t by_file = 0;
  std::size_t by_pattern = 0;
  // Ceux dont une table parle mais qui n'ont rien composé. Ce tas-là accuse une
  // RESSOURCE ou une FIGURE, et non un portage manquant : c'est le plus
  // rentable à lire. `silent_because` le répartit par cause.
  std::size_t named_but_silent = 0;
  // Combien de muets pour chacune des causes, dans l'ordre de l'énumération.
  std::array<std::size_t, 7> silent_because = {};
  // LE CONTRÔLE DU DIAGNOSTIC, et il est là pour l'INVALIDER.
  //
  // ⚠ UNE SIGNATURE QUI ACCUSE DOIT ÊTRE ABSENTE DE CE QUI MARCHE, et c'est le
  // corollaire d'ADR 0028 : une signature floue interdit de trancher. « L'alpha
  // n'est jamais monté » ne veut dire quelque chose que si les effets qui SE
  // VOIENT, eux, le font monter. Ils ne le font pas — cinquante-sept s'en
  // passent —, et ce verdict aurait envoyé vingt-quatre lectures au mauvais
  // endroit s'il n'avait pas été mesuré.
  //
  // Un par signature, dans l'ordre de `FigureTrace` : piste jamais allumée,
  // alpha jamais monté, rayon resté nul. Une signature dont ce compte n'est pas
  // NUL n'a pas le droit d'accuser, et `DescribeFigure` ne s'en sert pas.
  std::array<std::size_t, 3> showing_with_signature = {};
  std::uint32_t elapsed_ms = 0;
};

// Essaie tous les identifiants à cas réel, un par un, et rend leur verdict.
//
// Le VFS est le seul contexte dont le juge ait besoin : ni carte, ni monde, ni
// fenêtre. C'est ce qui rend son résultat indépendant de l'endroit où l'on se
// trouve en jeu quand on le demande.
[[nodiscard]] EffectJudgement JudgeEffects(ro::vfs::Vfs& vfs);

// Écrit le rapport. Faux si le fichier ne s'ouvre pas.
[[nodiscard]] bool WriteEffectJudgement(const EffectJudgement& judgement,
                                        const std::filesystem::path& to);

// Les quelques lignes qu'on lit dans le chat, le rapport restant sur le disque.
[[nodiscard]] std::vector<std::string> DescribedJudgement(const EffectJudgement& judgement);

// CE QUE LA TROUPE FAIT DE SON HORLOGE, un tic après l'autre, pour UN effet.
//
// ⚠ LE VERDICT DIT « LA FIGURE NE REND AUCUN QUAD », ET CE N'EST PAS UN
// DIAGNOSTIC. Il porte un maximum pris sur toute la vie, ce qui range un effet
// dans le bon tas sans dire à quel MOMENT il a manqué. Or cette famille-ci naît
// muette et s'ouvre plus tard : le mouvement du sous-type 52 laisse tomber sa
// piste cent cinquante-cinq tics avant d'écrire le compteur dont le dessinateur
// fait sa garde. Un effet qui n'a pas atteint ce tic-là dans la fenêtre du juge
// est INDISCERNABLE d'une figure cassée, et c'est ce que cette trace sépare.
struct EffectTickSample {
  std::int32_t tick = 0;
  // Combien de nœuds vivent, et combien de leurs pistes sont ALLUMÉES. Une
  // troupe pleine de pistes éteintes accuse le semis ; une troupe vide, le
  // patron.
  std::size_t nodes = 0;
  std::size_t active_tracks = 0;
  // L'horloge de la PREMIÈRE piste allumée. Négative tant que son retard court,
  // et c'est elle qu'il faut lire pour savoir si la fenêtre suffit.
  std::int32_t first_track_ticks = 0;
  std::int16_t best_track_alpha = 0;
  float best_radius = 0.0F;
  // Ce que ce tic-ci a composé, tout confondu. Le premier tic non nul est la
  // réponse cherchée.
  std::size_t quads = 0;
  // 🔴 OÙ LA FIGURE SE TROUVE, en ordonnée du monde et par rapport au sol de la
  // scène du juge, qui est à zéro. Une trace qui ne dit que « combien » est
  // à moitié aveugle : une pointe qui jaillit trop haut, un anneau qui se noie
  // dans le terrain et une figure parfaitement posée rendent tous le même
  // compte de quads. Seuls les quads du MONDE y entrent — ceux d'écran et de
  // sprite n'ont pas d'ordonnée de monde à donner.
  //
  // Les deux valent zéro quand aucun quad de monde n'est composé.
  float world_y_low = 0.0F;
  float world_y_high = 0.0F;
  // 🔴 ET JUSQU'OÙ ELLE S'ÉTEND DANS LE PLAN DU SOL — la diagonale de sa boîte
  // en abscisse et en profondeur.
  //
  // C'est la seconde moitié de la même leçon que l'ordonnée : un semeur qui
  // MARCHE de la source à la cible et un semeur cloué à son point rendent le
  // même compte de nœuds et la même hauteur. Seule l'étendue au sol les sépare,
  // et c'est elle qui dit si le trait de Frost Diver a couvert ses dix cases ou
  // s'est empilé sur place.
  //
  // Zéro quand aucun quad de monde n'est composé.
  float world_ground_span = 0.0F;

  // 🔴 ET OÙ LES NŒUDS EUX-MÊMES SE TIENNENT, figure ou pas.
  //
  // Les deux mesures ci-dessus lisent les quads du MONDE, et sont donc muettes
  // pour une troupe de SPRITES : un sprite se compose en pixels d'écran, et n'a
  // aucun coin dans le monde à donner. Une boule de feu qui part trop bas, ou
  // qui ne part pas du tout, rend exactement la même trace qu'une boule juste —
  // c'est la troisième fois que « combien » ne suffit pas.
  //
  // Celles-ci lisent la PLACE des nœuds vivants, que toutes les familles ont.
  // Les trois valent zéro quand aucun nœud ne vit.
  float node_y_low = 0.0F;
  float node_y_high = 0.0F;
  float node_ground_span = 0.0F;
};

// Joue un seul identifiant par le chemin du juge et rend sa chronologie.
//
// VIDE quand le semis n'a posé aucun nœud : il n'y a alors pas de chronologie à
// lire, et c'est une autre cause.
[[nodiscard]] std::vector<EffectTickSample> TraceEffectTicks(ro::vfs::Vfs& vfs,
                                                             std::uint16_t effect_id);

}  // namespace moonclient
