// LE MODÈLE DE LA TABLE DES NOMS DU GARDE — voir `KnownSignatures`.
//
// La vraie table s'écrit dans `guard_signatures.local.inc`, à côté de ce
// fichier, et git l'ignore. Une entrée par ligne :
//
//   {sonde, "outil tel que le serveur le journalise", "motif en minuscules"},
//
// `kProcess` compare le nom entier d'un exécutable ; `kProcessSubstring`,
// `kWindowClass` et `kModule` cherchent le motif DANS le nom observé.
//
// ⚠ LA BÂTISSE NE VOIT LE FICHIER QU'À LA CONFIGURATION : après l'avoir créé,
// relancer `cmake --preset`. Ses retouches suivantes se recompilent seules.

{GuardProbe::kProcess, "Outil d'exemple", "outil-exemple.exe"},
{GuardProbe::kProcessSubstring, "Outil d'exemple", "outil-exemple"},
{GuardProbe::kWindowClass, "Outil d'exemple", "outilexemplewindow"},
{GuardProbe::kModule, "Outil d'exemple", "outil-exemple-hook"},
