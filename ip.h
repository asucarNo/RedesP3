/***************************************************************************
 * ip.h 
 *
 *  Definiciones y cabeceras para el uso de IP en las practicas de 
 *  Redes de Comunicaciones 2 de la EPS-UAM
 *
 *  Autor: Jose Hernandez, Manuel Freire
 *
 *  (C) 2006-07 EPS-UAM 
 ***************************************************************************/

#ifndef __IP_H
#define __IP_H

#include "nivel1.h"

#define IP_ALEN 4

#define IP_HLEN_MIN 20
#define IP_HLEN_MAX 60

#define IP_TTL_DEFECTO 64
#define IP_TOS_DEFECTO 0
#define IP_DF_DEFECTO 1

// variable global con la ip de esta maquina; inicializada en ip.c
extern BYTE dir_ip_local[IP_ALEN];

typedef struct {
    BYTE ttl;
    BYTE df;
    BYTE tos;
    BYTE extra[IP_HLEN_MAX - IP_HLEN_MIN];
    unsigned short elen;
} op_ip;

// funcion de interrupcion desde el nivel IP a un protocolo inferior
typedef int (*pf_notificacion_ip) (BYTE *dir_ip_origen, unsigned int tamano, 
    BYTE *datos, op_ip *opciones);

// prototipos publicos

/****************************************************************************
 * Inicializa el nivel IP, tras inicialiar todos los superiores
 * (incluyendo nivel1 y ARP); debe llamarse antes de usar otras 
 * funciones de 'ip.h'.
 *
 * Lee las variable de entorno 'IPMASK' e 'IPROUTER' (y, ya que inicializa
 * ARP, 'IPLOCAL') para inicializar las direcciones de red.
 *
 * Se puede llamar muchas veces sin efectos adversos
 *
 * entra: nada
 * sale:
 *   0 si todo bien, -1 en caso de error
 ****************************************************************************/
int ip_inicializa();

/****************************************************************************
 * Desinicializa IP; debe llamarse para liberar cualquier recurso que pudiera
 * estar reservado en este nivel.
 *
 * entra: nada
 * sale:
 *    0 si todo bien, -1 en caso de error error
 ****************************************************************************/
int ip_finaliza();

/****************************************************************************
 * Registra un protocolo de nivel inferior. A partir de su correcto registro,
 * el modulo IP llama al callback suministrado cada vez que reciba un
 * datagrama del tipo correspondiente
 *
 * entra:
 *    protocolo - protocolo de nivel inferior (ver RFC 1700)
 *    callback - funcion a llamar con el datagrama recibido
 * sale:
 *    0 si no hay fallos, -1 en caso contrario
 ****************************************************************************/
int ip_registra_protocolo(BYTE protocolo, pf_notificacion_ip callback);

/****************************************************************************
 * Envia un datagrama IP a otro host
 *
 * entra:
 *    protocolo - protocolo de nivel inferior al que corresponde el datagrama
 *    dir_ip_destino - ip del destinatario
 *    tamano - numero de bytes en el datagrama
 *    opciones - opciones a usar/incluir en el datagrama; si se pasa NULL, 
 *          se asumira TTL = 64, TOS = 0, y DF = 1 (ver #defines)
 * sale:
 *    0 si no hay fallos, -1 en caso contrario
 ****************************************************************************/
int ip_envia_datagrama(BYTE protocolo, BYTE *dir_ip_destino, unsigned int tamano, 
    BYTE *datos, op_ip *opciones);

#endif // del #ifndef __IP_H
