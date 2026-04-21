# EpiRootkit

Projet pédagogique réalisé en groupe dans le cadre du cours SYS_2 à EPITA.

L'objectif était de développer un module kernel Linux capable de s'injecter sur une machine victime via `insmod`, d'établir une connexion distante vers une machine attaquante et de la contrôler à travers un shell distant.


## Ce que fait le rootkit

Une fois chargé sur la machine victime, le module se connecte automatiquement à la machine attaquante et expose un shell distant. Il intègre des mécanismes de dissimulation, de persistance au redémarrage, d'upload/download de fichiers et de chiffrement XOR des communications.

## Architecture

```
epirootkit/
├── rootkit/
│   ├── epirootkit_main.c
│   ├── network.c
│   ├── encrypt.c
│   ├── hide.c
│   ├── load.c
│   ├── persistence.c
│   ├── authentification.c
│   ├── rootkitShell.c
│   ├── rootkit.h
│   └── Makefile
└── attacking_program/
    ├── attack_client.py
    └── decrypt.py
```

## Environnement

Machine victime : Ubuntu 18.04, kernel 5.4.0, choisi pour l'accessibilité de la table des syscalls et l'absence de signature obligatoire des modules.

Machine attaquante : Ubuntu 24.10 avec Python 3.

## Installation

Côté victime :

```bash
sudo apt install linux-headers-$(uname -r) build-essential make gcc
cd rootkit
make all
sudo insmod epirootkit.ko
```

Côté attaquant :

```bash
cd attacking_program
python3 attack_client.py
```

Avant de compiler, modifier l'IP dans rootkit/rootkit.h : IP_ATTACKER = "votre_ip"

## Stack

C (module kernel Linux), Python 3, VirtualBox
