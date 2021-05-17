### Connextion UART
- Dans `arch/cpu/cc2538/cc2538-conf.h`: modification de `#define UART1_CONF_BAUD_RATE   57600`

- Dans `arch/platform/zoul/remote-revb/board.h` modification de:
```
#define UART1_RX_PIN             2
#define UART1_TX_PIN             3
```


### NODE_ID

type: `uint16` càd un entier sur __16 bits__.

Il est construit dans `os/sys/node_id.c` à partir de la *link-layer address*.
Sa construction dépend de la définition de `BUILD_WITH_DEPLOYMENT`.
Avec `grep -r 'BUILD_WITH_DEPLOYMENT' .` à la racine de *contiki-ng*, j'ai trouvé que
`BUILD_WITH_DEPLOYMENT` est définis à 1 dans `os/services/deployment/module-macros.h`.

La *link-layer address* est stockée dans `linkaddr_node_addr`. Cette variable (.u8) est remplie dans `arch/platform/zoul/platform.c` via la fonction `ieee_addr_cpy_to` qui est definie dans `arch/cpu/cc2538 ieee-addr.h` et qui va rechercher l'adresse dans la memoire flash du cc2538.

### deployment

*deployment* permet de mapper un adresse MAC avec un NODE_ID.
Ce qui est intéréssant pour ce projet c'est qu'avec ce service, il est possible de récupérer un `node_id ` à partir d'une IPv6 (global ou link-local) et inversemment. Le système d'adresse formé par le `node_id` et un préfixe IP serait alors envisageable.

Malheuresement, ce module dépend d'un tableau qui map les NODE_ID et les adresses MAC. Donc un noeud intermédiaire devrait avoir une entrée dans ce tableau pour chaque noeud son son réseau RPL.

Il existe un exemple de ce service dans `examples/libs/deployment`. A partir de la définition de la table, le code affiche les adresses IPv6 __attendues__ des noeuds du réseau.
Dans le code, il y a également deux commentaires qui indiquent: 'set ipadddr...' et 'set IID' pourtant ce sont des get dont le résultat est stocké dans la même variable.


### Adresse IPv6

Définie sur __128 bits__. Si l'adresse custom n'apporte pas un avantage significatif en terme te taille, l'adresse IPv6 d'un noeud sera utilisée comme adresse source pour les trames LoRa.

La structure d'une IPv6 est définie dans `os/net/ipv6/uip.h` par:
```c
typedef union uip_ip6addr_t {
  uint8_t  u8[16];
  uint16_t u16[8];
} uip_ip6addr_t;

typedef uip_ip6addr_t uip_ipaddr_t;
```

- #### Préfixe IPv6
Dans l'application, la définition du préfixe IPv6 se fait via 
`NETSTACK_ROUTING.root_set_prefix(uip_ipaddr_t *prefix, uip_ipaddr_t *iid)` définie dans `struct routing_driver` de `os/net/routing.h`.

Je fixe l'iid à NULL. Par la doc de `root_set_prefix`, si l'iid est NULL il est construit à partir de `uip_ds6_set_addr_iid`

`void uip_ds6_set_addr_iid(uip_ipaddr_t *ipaddr, uip_lladdr_t *lladdr)` est définie dans `net/ipv6/uip-ds6.h` met les 64 derniers bits d'une IPv6 basé sur une adresse MAC.

`rpl_dag_root_set_prefix(uip_ipaddr_t *prefix, uip_ipaddr_t *iid)` initialise juste une adresse globale (avec `set_global_address`) en fonction des paramètres.

__bizarre__: en cherchant avec `grep`, `root_set_prefix` n'est définie nul part.mais `rpl_dag_root_set_prefix` est bien définie.

`NETSTACK_ROUTING` est definis comme `rpl_lite_driver` (si rpl lite est utilisé)qui est une `struct routing_driver` définie dans `os/net/routing/rpl-lite/rpl.c`. A priori *rpl lite* sera utilisé car c'est une implémentation moins complexe que *rpl classic* néanmoins *rpl lite* ne supporte pas toutes les fonctionnalités du standard. __A approfondir__

Une adresse IPv6 est créée à partir de
`uip_ip6addr` defini dans `os/net/ipv6/uip.h`. Cette macro prend en paramètre l'adresse de destinations et 8 __`uint16_t `__ pour remplir `.u16` de `uip_ip6addr_t`.

On peut également utiliser la macro `uip_ip6addr_u8` pour avoir une IPv6 crée avec des __`uint8_t `__. On pourrait alors avoir un préfixe de 8 bits (deux symboles en hexa). Ce qui réduit le nombre de préfixe possible mais __semble suffisant pour le projet?__. J'ai testé avec un réseau RPL sur cooja et créer le préfixe de cette manière fonctionne.

La taille du préfixe pourrait également être configurée. Ce qui permettrait d'adpter le réseau aau nombre de noeuds intermédiaires.

### Format d'adresse

Avec ce qui a été trouvé ci-dessus, j'ai donc plusieurs possibilités:

- Utiliser l'adresse IPv6 du noeud avec un préfixe associé à chaque réseau RPL<br>
  __<span style="color:green">+</span>__ simple à mettre en place
  <br>
  __<span style="color:green">+</span>__ pas besoin de table de routage ou de processus de conversion d'adresse pour les noeuds intermédiaires.
  <br>
  __<span style="color:red">-</span>__ Occupe 128 bits dans une trame LoRa.<br>
  
  L'adresse de la racine serait alors prédéfinie.

- Utiliser le préfixe et le NODE-ID<br>
  __<span style="color:green">+</span>__ Adresse courte sur 24 bits (8 bits du préfix + 16 bits du NODE-ID)
  <br>
  __<span style="color:red">-</span>__ Besoin d'une table qui mappe un NODE-ID avec une adresse MAC dans les noeuds intermédiaires, pour chaque noeud de son réseau RPL.<br>
  
  L'adresse de la racine serait alors les 24 bits à 0.
  
  On pourrait également modifier l'adresse MAC des noeuds pour que seuls les 6 premiers octets des noeuds du réseaux soient identique. On a donc plus besoin de cette table. Mais je trouve que c'est du chipotage et cela introduit un risque que deux noeuds aient le même NODE-ID.

- Utiliser le préfixe et l'adresse MAC du noeud<br>
Compromis entre les deux premières solutions.
  - On peut reconstruire l'adresse IPv6 du noeud à partir du préfixe et de son adresse MAC. L'opération est peu couteuse en calcul. Donc pas besoin de table de routage.
  - La taille de l'adresse est de 72 bits (64 de l'adresse MAC et 8 du préfixe). Taille qui se trouve entre celle de la première solution et celle de la deuxième.
  
  L'adresse de la racine serait alors 8 bits à 0 (le préfixe) suivi de son adresse MAC.

Pour les trois hypothèses, l'adresse broadcast serait une adresse où tous les bits sont à 1.

### [RPL](https://github.com/contiki-ng/contiki-ng/wiki/Documentation:-RPL)
- Objective function
- [Trickle Algorithm](https://tools.ietf.org/html/rfc6206)
- RPL Classic <br>
Implémentation originale de RPL créée en 2009 alors que le standard était encore en développement.
Il a pleins de fonctionnalités ajoutées au standard. -> L'implémentation est devenue complexe et a donc une grande empreinte ROM.
- RPL Lite <br>
Implémentation par défaut crée en 2017 qui a pour but de garder les fonctionnalités les plus stables et importantes. non-storing mode seulement. -> meilleures performances, une empreinte ROM considérablement réduite mais moins d'interopérabilité.

- storing mode:
  Un noeud a une table de routage pour tous ses enfants.
- non-storing mode:
    Le chemin est contenu dans des paquets envoyés.

- Impact du mode sur Orechstra.
  1. [issue non résolue](https://github.com/contiki-ng/contiki-ng/issues/889): En non-storing mode, le scheduler réveille le système à chaque Slot -> impact négatif sur la consommation d'énergie.

### LoRa
taille max: 255 octets pour LoRa d'après command reference de RN2483
duty cycle lora

set/get
-------
| command | meaning                                       | values                                                        |
|---------|-----------------------------------------------|---------------------------------------------------------------|
| bw      | operaing bandwidth                            | in kHz [125, 250, 500]                                        |
| cr      | coding rate                                   | [4/5, 4/6, 4/7, 4/8]                                          |
| crc     | state of the CRC header                       | [on, off]                                                     |
| freq    | frequency                                     | 433050000 to 434790000 or from 863000000 to 870000000, in Hz. |
| iqi     | state of the invert IQ                        | [on, off]                                                     |
| mod     | /                                             | [lora, fsk]                                                   |
| pwr     | ransceiver output power                       | from -3 to 15 (max +14dBm)                                    |
| sf      | spreading factor                              | [f7, sf8, sf9, sf10, sf11, sf12]                              |
| sync    | configure sync word used during communication | (one byte for LoRa) ex: 12 for 0x12                           |
| wdt     | watchDog timer                                | from 0 to 4294967295 (0 -> disable) in ms                     |

get
---
| command | meaning                                           |
|---------|---------------------------------------------------|
| snr     | signal-to-noise ratio of the last received packet |


### Format des messages LoRa

taille en bit max: 2040
```
|<---24---->|<----24--->|<-1->|<--3--->|<--4--->|<(2040-56=1984)>|
| dest addr |  src addr |  k  |reserved|command | payload |
```

commandes disponibles:
1. JOIN
2. JOIN_RESPONSE
3. DATA
4. ACK
5. PING
6. PONG
7. QUERY
8. CHILD
9. CHILD_RESPONSE
 

commandes sur 4 bits.

### Gestion Adresse IP externe
event tcpip_event -> non car pour tcpip ?


### Protocole mis en place

Le diagramme illustre une première version du protocole mis en place.

NOTE: PAS PRESENT SUR LE DIAGRAMME: pour les données descendantes, les noeuds intermédiaires enverront à un certain intervalle des trames avec la commande QUERY. Si des données sont dispo pour ce noeud, la ROOT va lui transmettre. Sinon, la radio LoRa sera éteinte jusqu'au prochaines données montantes ou l'émission d'un nouveau message QUERY.

Pour raccourcir les messages UDP, deux ports seront utilisé, un pour les données et l'autres pour gérer la connexion.

Pour les JOIN des RPL Nodes, au lieu que le noeud envoie un message udp "JOIN", Je dois regarder s'il est possible de savoir ça via RPL. Cela évite un message.
Si ce n'est pas possible les noeuds intermédiaires peuvent aggréger ces messages join. 
Plus généralement un fonction d'aggrégation peut être réalisée. Cela sera à faire une fois le réseau de base fonctionnel.

Plus généralement, je dois regarder si les noeuds intermédiaires peuvent recevoir des évènements RPL pour par exemple savoir si un noeud quitte le réseau.

Pour les acquitements, si un ACK n'est pas reçu après un temps déterminé, le paquet est retransmis.
Après 3 retransmissions, l'envoi est annulé.
![proto v0](PROTOv0.jpg)