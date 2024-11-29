# Projet de Compilation et génération de code (2023-2024)

Projet de compilation et génération de code réalisé dans le cadre de l'UE Compilation et génération de code du semestre 2023-2024 de la 3ème année de Licence Informatique à l'Université Gustave Eiffel.

## Auteur

- Nom: **Baillargeau** Axel

## Introduction

Le but de ce projet est de créer un compilateur de TPC, un sous-ensemble du langage C, à partir du projet d'Analyse Syntaxique du semestre précédent. Le compilateur doit détecter et signaler les erreurs sémantiques dans le code source, prendre en charge les variables globales, les tableaux, les fonctions, les appels de fonctions, les expressions arithmétiques et les instructions de contrôle, ainsi que générer du code nasm pour Unix à partir du code source.

## Build et execution

Toutes les commandes doivent être exécutées dans la racine du projet. Pour build, il faut lancer la commande.

```sh
make
```

Le compilateur se trouvera alors dans le dossier `./bin`.

La commande

```sh
make tests
```

permet de tester que le compilateur fonctionne correctement, en utilisant les fichiers sources se trouvant dans `./test`. Cette commande effectue le build au préalable.

## Difficultés Rencontrées

### Passage de paramètres

J'ai eu du mal à faire fonctionner les passages de paramètres dans les fonctions. En effet, les paramètres ne se retrouvaient pas dans la pile à l'endroit attendu, et les fonctions récursives ne retournaient pas à la bonne adresse. J'ai finalement réussi à résoudre ce problème en ajoutant un offset de la taille des variables locales au début de chaque appel de fonctions.

### Adressage des tableaux

Ayant implémenté les variables globales dans la section `.data` du fichier assembleur, je me suis retrouvé confronté à un problème d'adressage des tableaux.

En effet, les tableaux stockés dans la section `.data` étaient organisés de manière à ce que l'accès à un élément se fasse en ajoutant l'index à l'adresse de base (add). Or, les tableaux stockés dans la pile étaient organisés de manière à ce que l'accès à un élément se fasse en soustrayant l'index à l'adresse de base (sub).

J'ai résolu ce problème en declarant des pointeurs sur les tableaux globaux dans la section `.data` pointant vers la fin du tableau, et en déréférençant les pointeurs des tableaux locaux et globaux afin d'avoir le même comportement pour les deux types de tableaux.

## Conclusion

Ce projet m'a permis de mieux comprendre le fonctionnement d'un compilateur, et de me familiariser avec la génération de code assembleur. J'ai pu mettre en pratique les notions vues en cours, et j'ai appris à résoudre des problèmes de programmation complexes.

## Annexes

### Fonctionnalités non implémentées

- Récupération de toutes les erreurs dans le code source en une seule passe.
- Passage de paramètres par registres.
- Expressions pour l'index des tableaux.
- Inférer la taille minimale requise des tableaux passés en paramètres.

### Ressources externes

- [Docs Bison](https://www.gnu.org/software/bison/manual/html_node/index.html),
- [Docs Flex](https://westes.github.io/flex/manual/index.html#Top),
- [Levine, John. *Flex & Bison*. 2009](https://web.iitd.ac.in/~sumeet/flex__bison.pdf) (parce que les docs ne sont pas complètes),
- [Post SO qui m'a permis de trouver le lien vers le PDF](https://stackoverflow.com/questions/40605350/change-yyin-to-argv1-flex-bison)
