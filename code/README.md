# Manuel d'utilisation LoRaMAC

# Installation

-   #### Noeud Contiki

    Une fois Contiki-ng installé (cf. [https://github.com/contiki-ng/contiki-ng/wiki#setting-up-contiki-ng](https://github.com/contiki-ng/contiki-ng/wiki#setting-up-contiki-ng) ), les modifications suivantes doivent y être apportées:

    1.   `arch/cpu/cc2538/cc2538-conf.h`

         -` #define UART1_CONF_BAUD_RATE   115200 /**< Default UART1 baud rate */`
         +`#define UART1_CONF_BAUD_RATE   57600 /**< Default UART1 baud rate */`

    2.   `arch/platform/zoul/remote-revb/board.h`
         -`#define UART1_RX_PIN             1`

         +`#define UART1_RX_PIN             2` 

         et 
         -`#define UART1_TX_PIN             0`
         +`#define UART1_TX_PIN             3`

    3.   `contiki-default-conf.h`
         -`#define UIP_CONF_BUFFER_SIZE 1280`
         +`#define UIP_CONF_BUFFER_SIZE 279`

    Ensuite copier le dossier `prototype/rpl_root/loramacv2` dans `contiki-ng/os/services`

-   #### Racine LoRa

    -   Installer les dépendances python contenues dans le fichier `prototype/lora_root/py_lora_mac/requirements.txt`
    -   Installer le package *pyloramac* en se positionnant dans le répertoire `prototype/lora_root/py_lora_mac` puis: `pip install .`

## Utilisation de l'exemple

`example-udp-rpl-lora` contient le code d'exemple permettant de tester le trafic montant, descendant et bidirectionnel (ping-pong)

-   `lora-root-node` contient le script de l'application pour la racine LoRa. Ce script prend deux paramètres: le port du RN2483 et l'entier représentant le mode de fonctionnement (exemple: `python lora-root.py /dev/ttyUSB0 2`)
-   `rpl-node` contient l'application Contiki pour un noeud RPL ou racine RPL. Dans le Makefile, veillez à bien adapter le chemin de Contiki vers le répertoire où il installé.  La ligne `MODULES += os/services/loramacv2` doit être commentée si l'application est utilisée pour un noeud RPL.