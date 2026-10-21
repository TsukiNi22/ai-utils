+++
title = "Installation"
summary = "Dépendances, script d'installation, compilation depuis les sources, application Android et premières vérifications."
section = "guide"
order = 10
keywords = "rpm deb dnf apt fedora debian ubuntu setup.sh cmake compilation apk android complétion"
+++

claude-cluster est un programme C++20 (`claude-cluster`) avec un petit wrapper pour les commandes lourdes (`cluster-run`). Claude Code est l'agent par défaut : installez `claude` et connectez-vous d'abord, claude-cluster le pilote.

## Prérequis {#requirements}

| Quoi | Version | Fedora (`dnf`) | Debian / Ubuntu (`apt`) |
|---|---|---|---|
| clang++, CMake | C++20, 3.20 ou plus | `clang cmake` | `clang cmake` |
| [libutils](https://github.com/TsukiNi22/libutils) | 4.0.0 ou plus | `libutils` | `libutils` |
| nlohmann/json | 3.10 ou plus | `json-devel` | `nlohmann-json3-dev` |
| toml++ | 3.0 ou plus | `tomlplusplus-devel` | `libtomlplusplus-dev` |
| OpenSSL (optionnel : https de l'accès web, micro de la page web) | quelconque | `openssl-devel` | `libssl-dev` |
| Qt6 Widgets (optionnel : la fenêtre) | 6.2 ou plus | `qt6-qtbase-devel` | `qt6-base-dev` |
| chafa, ImageMagick (optionnel : images dans le terminal) | quelconque | `chafa ImageMagick` | `chafa imagemagick` |
| un agent | | `claude` (défaut), `ollama`, `qwen`, `opencode`, `codex` | |

FTXUI (l'interface terminal) est téléchargé par CMake dans une version fixe : la première configuration demande un accès réseau. Sans Qt6 la fenêtre n'est pas compilée (`--gui` le dit) et l'interface terminal reste. La voix utilise les outils locaux optionnels `voice-listen` / `voice-say`.

> [!NOTE]
> Les noms des paquets libutils dépendent du dépôt depuis lequel vous l'avez installé ; le skill `libutils-install` du dépôt choisit la bonne méthode pour votre système.

## Fedora et Debian {#distros}

La méthode en une ligne compile et installe pour votre utilisateur (sans root) :

```bash
curl -fsSL https://raw.githubusercontent.com/TsukiNi22/ai-utils/main/setup.sh | bash -s -- install claude-cluster
```

Depuis un clone du dépôt :

```bash
git clone https://github.com/TsukiNi22/ai-utils.git
cd ai-utils
./setup.sh install claude-cluster
./setup.sh remove claude-cluster
```

Le binaire va dans `~/.local/bin`, avec la complétion bash, zsh et fish. Quand le SDK Android est présent, la même commande compile aussi l'application téléphone (voir plus bas).

> [!SOON]
> Les paquets RPM et DEB prêts à installer ne sont pas encore publiés. D'ici là le script ci-dessus est la méthode supportée sur Fedora et sur Debian / Ubuntu.

## Depuis les sources {#source}

```bash
cmake -S ai/claude-cluster -B build -DCMAKE_BUILD_TYPE=Optimized
cmake --build build --parallel
cmake --install build --prefix ~/.local
```

Les modes de compilation sont `Debug`, `Asan` et `Optimized` (défaut). Options : `-DCLUSTER_GUI=OFF` saute la fenêtre Qt6, `-DCLUSTER_TESTS=ON` compile le test unitaire de l'éditeur de ligne. Beaucoup de sessions qui compilent saturent un ordinateur : compilez avec `nice -n 19` et un `--parallel` limité.

## L'application Android {#apk}

L'application est une coque autour de la page web de votre propre ordinateur. Elle est compilée par `android/build.sh` avec le seul SDK Android (sans Gradle) et signée avec une clé créée une fois dans `~/.config/claude-cluster/android/release.jks` : les mises à jour exigent la même clé, gardez-en une copie.

Dès que l'accès web est ouvert, la page propose l'APK à un navigateur Android sur `/app.apk`. Voir [Notifications et application téléphone](page:notifications-apk) pour ce qu'elle fait et [Téléchargements](page:downloads) pour le fichier.

## Vérifier que ça marche {#check}

```bash
claude-cluster --version
claude-cluster            # interface terminal dans un terminal, sinon la fenêtre
claude-cluster serve      # sans interface : piloté par les commandes et le serveur MCP
```

Ensuite : [votre première session](page:first-session). En cas de problème, voir [Dépannage](page:troubleshooting).
