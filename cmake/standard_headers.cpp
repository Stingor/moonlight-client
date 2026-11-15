// La seule source de `moonclient_std_pch`, la cible qui précompile les en-têtes
// standard pour les autres : voir `cmake/build_speed.cmake`.
//
// Une bibliothèque demande au moins une source, et un objet sans symbole fait
// avertir l'archiveur : cette fonction est ce symbole, et personne ne l'appelle.

namespace ro {

void StandardHeadersArePrecompiled() {}

}  // namespace ro
