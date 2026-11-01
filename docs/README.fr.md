# AS-Trainer 1.1.0

[English](../README.md) · [Русский](README.ru.md) · [Deutsch](README.de.md) · [Français](README.fr.md) · [Español](README.es.md) · [Українська](README.uk.md) · [简体中文](README.zh-CN.md)

Un trainer C++20 / Dear ImGui pour Azeroth Survivors sous Windows x64 : jusqu’à 26 paramètres du personnage, préparation de la prochaine partie, profils INI, choix automatique des récompenses, ensemble légendaire de classe et simulation de 1 à 16×.

## Télécharger et lancer

Téléchargez **`AS-Trainer-1.1.0-windows-x64.zip`** depuis [GitHub Releases](https://github.com/NightlyBlooD/AS-Trainer/releases). Extrayez toute l'archive. Gardez ensemble `azs_overlay.dll`, `load_overlay.exe`, `load_overlay.cmd`, `signature_scan.exe`, `configs`, `signatures`, `fonts`, la documentation et les licences. La DLL, le chargeur et `SHA256SUMS.txt` sont aussi proposés séparément.

1. Lancez la version compatible du jeu en mode fenêtré ou sans bordure.
2. Lancez `load_overlay.cmd` ou `load_overlay.exe` avec les mêmes privilèges que le jeu.
3. **Insert** affiche ou masque le panneau. Réglez les valeurs ou chargez un profil dans **Configurations**. Confirmez l'automatisation avec **Appliquer les règles**.
4. **End** annule les écritures en attente, les blocages et les options Début, désactive l'automatisation et le mode groupe, rétablit la vitesse 1× et masque le panneau. Les valeurs déjà modifiées dans le jeu et les fichiers INI restent en place.

Avec plusieurs processus : `load_overlay.exe --pid 12345`. La DLL reste chargée jusqu'à la fermeture du jeu. **Redémarrez le jeu avant une mise à jour** ; relancer le chargeur ne remplace pas une DLL déjà chargée.

## Compatibilité et langue

Versions prises en charge de `AzerothSurvivors.exe` et leur SHA-256 :

| Version du jeu | SHA-256 |
|---|---|
| `v0.135.1 (90898d5e3)` | `71e5f1767279db4bd428d1cd9c5d50d37759b72295386a69f9e3f6e70bc59249` |
| `v0.132.0 (04e2a2d1c)` | `e60b03aae379c9c3c4c728bba1b3dbaeac161007e34a8c09fc61c061f9cceb9c` |
| `v0.131.0 (e7d9dd47)` | `5dd6819d836748ccf809ab76fe6509c9ee793e087939b5d036c430d835a14e63` |

Les profils v0.131.0 et v0.132.0 sont conservés. Au démarrage, le catalogue adaptatif vérifie une fois 92 règles : cibles natives uniques, tous les décalages de champs nécessaires, taille des enregistrements des participants et preuves explicites de l’ABI. Un hachage inconnu peut être accepté si ce contrôle complet réussit. Les octets du code chargé sont aussi vérifiés avant l’installation des hooks. Les signatures absentes ou ambiguës et les ABI incompatibles sont refusées ; la compatibilité avec toutes les versions futures n’est pas garantie. L’EXE sur disque reste intact.

L'interface et les diagnostics suivent le fichier `language.txt` du client, généralement `%APPDATA%\wenilla-rogue\language.txt`. Les 14 langues du client sont disponibles : anglais (`en`), russe (`ru`), allemand (`de`), français (`fr`), espagnol (`es`), ukrainien (`uk`), chinois simplifié (`zh`), polonais (`pl`), tchèque (`cs`), portugais du Brésil (`pt`), roumain (`ro`), turc (`tr`), japonais (`ja`) et coréen (`ko`). Une valeur inconnue utilise l'anglais. Les changements de langue sont actualisés en une seconde sans redémarrer le trainer. La police fournie couvre aussi les traductions japonaises et coréennes.

## Valeurs et profils

**Appliquer** programme une écriture unique d'une valeur absolue. Depuis un menu, cette action prépare la prochaine partie et active **Début**. **Bloquer** réapplique la valeur après les recalculs du jeu et active aussi Début. **Début** réutilise la valeur à chaque nouvelle partie ; sans cette option, le blocage du champ ne passe pas à la partie suivante. Les valeurs actuelles apparaissent lorsqu'un personnage actif est disponible.

Les 25 champs communs couvrent dégâts, vitesse de course, recharge, rayon de ramassage, zone d'effet, chance, or, coût des coffres, relances, saut, santé actuelle/maximale, chance/dégâts critiques, gains d'expérience/d'or, réduction des dégâts, régénération, cibles supplémentaires, écho, soins par élimination, puissance des buffs, cumuls de DoT, vitesse des ticks DoT et résurrections. Les v0.135.1 et v0.132.0 proposent le vol de vie, soit 26 champs ; la v0.131.0 en conserve 25.

| Valeur | Signification |
|---|---|
| Dégâts / vitesse / zone | `1` = normal ; `2` = multiplicateur doublé |
| Recharge | `0.5` = deux fois plus rapide ; `0.1` = dix fois plus rapide |
| Ramassage | Yards |
| Chance | `0.2` = +20 % ; `100` = +10000 % |
| Chance critique / écho / réduction | Fractions : `0.5` = 50 % |
| Coffre / saut | `0` = coffre gratuit ; le saut est une autorisation |
| Vol de vie | Fraction des dégâts réellement infligés : `0.25` = 25 % de soins ; plage du trainer 0–10, sans limite native démontrée |

Les profils se trouvent dans `configs`, près de la DLL chargée. La sauvegarde retient les valeurs confirmées, Bloquer/Début et les règles appliquées, sans le texte non confirmé des éditeurs. Charger remplace les réglages ; un fichier invalide est rejeté entièrement. **`default.ini`** est chargé automatiquement au prochain démarrage de la DLL. Les exemples se chargent manuellement :

- [normal.ini](../configs/normal.ini) : désactive l'automatisation, vitesse 1×.
- [legendary_16x.ini](../configs/legendary_16x.ini) : 16×, choix automatiques, ensemble légendaire et valeurs bloquées.
- [party_boost.ini](../configs/party_boost.ini) : les 25 champs pour le groupe hébergé, vitesse 1×.

Les nombres sont des **valeurs cibles absolues**, pas des bonus ajoutés : dégâts `20` signifie un multiplicateur de dégâts de 20×.

## Récompenses et temps

Sorts et bijoux ont leurs propres interrupteurs et priorités. Utilisez les clés internes de [reference_keys.txt](../configs/reference_keys.txt), comme `wrath`, `hurricane`, `chosen` ou `legendary:wrath`. L'ordre des clés prime, puis la rareté, puis la première carte. Sans correspondance, la rareté décide. Le délai est de **50 à 5000 ms réelles**. Seules les offres ouvertes sont traitées ; ouvrez d'abord les niveaux stockés avec le bouton du jeu.

L'option du boss final accorde l'ensemble légendaire d'armure et d'armes de la **classe actuelle**, issu de la table Ahn'Qiraj/Naxxramas, après le boss final de **n'importe quelle zone** : 9 objets en général, 10 pour Voleur et Chaman. Le gestionnaire de butin normal les ajoute à la collection d'équipement. Ce mécanisme est distinct des bijoux de la partie et de la rareté des sorts.

L'accélération utilise des sous-pas natifs de simulation. À 16×, 16 minutes de jeu actif prennent environ une minute réelle si les performances le permettent. Les pauses et le choix des récompenses conservent leurs arrêts habituels ; le délai de choix reste réel. La charge CPU augmente, donc la durée exacte varie.

## Coop : uniquement l'hôte de la salle

Dans votre salle, activez **Appliquer les caractéristiques à tout le groupe (hôte uniquement)** et appliquez les règles. Les mêmes valeurs et options Bloquer/Début s'appliquent aux personnages du groupe. Les amis conservent leurs classes, inventaires, équipements, sorts et progressions. Le contenu de l'hôte n'est pas copié ; ce bouton partage les valeurs numériques, pas le choix automatique ni les inventaires.

Un trainer chargé uniquement chez un invité ne peut pas modifier la simulation d'un autre hôte. Le panneau affiche le personnage de l'hôte et les diagnostics du groupe.

## Analyse des signatures hors ligne

Le ZIP contient `signature_scan.exe` et les profils du dossier `signatures` ; ils ne sont pas publiés comme fichiers de téléchargement distincts. Depuis le dossier de l'archive extraite, analysez un EXE sans lancer le jeu ni charger la DLL :

```powershell
.\signature_scan.exe --exe "PATH/AzerothSurvivors.exe" --auto --output compatibility.json
.\signature_scan.exe --exe "PATH/AzerothSurvivors.exe" --profile signatures/azeroth-survivors-adaptive.ini --output scan.json
```

`--auto` vérifie les mêmes contrats hors ligne intégrés à la DLL, sans installer de hooks. `--profile` analyse uniquement le profil indiqué et n’autorise pas son utilisation à l’exécution.

Les profils utilisent des octets hexadécimaux au format IDA et les jokers `?` / `??` pour un octet entier. Chaque règle exige une correspondance unique. La résolution peut renvoyer la correspondance, une cible relative à RIP, le début de la fonction via `.pdata` ou un déplacement de champ signé. Le rapport JSON UTF-8 contient les hachages, l'état de recherche, la position dans le fichier, les RVA/VA et le déplacement de champ éventuel.

Avec `--profile`, un hachage différent est signalé par `hash_matches: false` et ne bloque pas l’analyse diagnostique. Une RVA désigne une instruction ou une fonction, pas une valeur de paramètre ; un décalage de champ indique une position dans un objet, pas une adresse active. Quelques signatures concordantes ne prouvent ni la disposition complète ni l’ABI. `--auto` exige le contrat complet ; le code chargé est vérifié au démarrage de la DLL.

## Compilation et publication

Prérequis : Visual Studio 2022 avec outils C++ x64, Windows SDK et CMake 3.24+. Depuis la racine du dépôt :

```powershell
cmake -S . -B build -G "Visual Studio 17 2022" -A x64 -DBUILD_TESTING=ON
cmake --build build --config Release --parallel 4
ctest --test-dir build -C Release --output-on-failure
cmake --install build --config Release --prefix dist
```

GitHub Actions cible Windows 2022 x64 Release pour les push et pull requests. Les tags `vX.Y.Z` publient les fichiers après réussite de la compilation et des tests.

## Dépendances

[Dear ImGui v1.91.9b](https://github.com/ocornut/imgui/tree/v1.91.9b) : MIT. [MinHook v1.3.4](https://github.com/TsudaKageyu/minhook/tree/v1.3.4) : BSD 2-Clause. Les polices fournies conservent leurs licences d'origine. Gardez les notices lors d'une redistribution. Win32, Direct3D 11 et BCrypt viennent du Windows SDK.
