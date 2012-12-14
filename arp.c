/***************************************************************************
 * arp.c 
 *
 *  Funciones para implementar ARP en las practicas de 
 *  Redes de Comunicaciones 2 de la EPS-UAM
 *
 *  Autor: Manuel Freire
 *
 *  (C) 2006-07 EPS-UAM 
 ***************************************************************************/

#include <stdio.h>
#include <stdlib.h>
#include <time.h>
#include <string.h>
#include <pthread.h>
#include <unistd.h>
#include "nivel1.h"
#include "nivel1a.h"
#include "arp.h"
#include "rc_funcs.h"

// DEFINEs Y ENUMs INTERNOS

#define ARP_DATA_MIN 28
#define ARP_HLEN 6
#define ARP_TLEN 2
#define ARP_REQ_TIMEOUT_US (1000*500)
#define ARP_REQ_RETRIES 3

enum {
    trama_arp_o_eth = 8,
    trama_arp_o_ip = 14,
    trama_arp_d_eth = 18,
    trama_arp_d_ip = 24
};

// VARIABLES GLOBALES INTERNAS

// dir ip local; exportada en 'ip.h', inicializada en arp_inicializa
BYTE dir_ip_local[IP_ALEN];

// dir ethernet local, inicializada en arp_inicializa
BYTE dir_eth_local[ETH_ALEN];

// dir ethernet para broadcast
BYTE dir_eth_any[ETH_ALEN] = {0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF};

// ethertype para tramas ARP
BYTE ethertype_arp[ETH_TLEN] = {0x08, 0x06};

// parte invariante de una trama ARP (IP sobre Ethernet)
BYTE cabecera_arp[ARP_HLEN] = {0, 1, 8, 0, 6, 4};

// tipo de mensaje ARP para solicitud (REQ)
BYTE arptype_req[ARP_TLEN] = {0x00, 0x01};

// tipo de mensaje ARP para respuesta (REP)
BYTE arptype_rep[ARP_TLEN] = {0x00, 0x02};

// si 1, ya se ha inicializado este nivel (y la dir. IP no estaba tomada)
int arp_inicializado = 0;
int ip_validada; // 1 = nadie ha respondido a un gratitious ARP

// entrada de cache ARP
typedef struct {
   time_t expiry;            // cuando llegue este momento, habra expirado
   BYTE dir_ip[IP_ALEN];        
   BYTE dir_eth[ETH_ALEN];   
} entrada_arp;

// cache ARP (global); todos los accesos deben estar protegidos mediante sem_tabla_arp
struct {
   entrada_arp t[ARP_CACHE_SIZE];
   int n;
} tabla_arp;
// ojo: no se puede incluir dentro de una estructura, porque es mas bien feo...
pthread_mutex_t sem_tabla_arp = PTHREAD_MUTEX_INITIALIZER;

// usada por arp_solicita_direccion_eth / arp_procesa_trama para peticiones en curso: 
// - el que pide pone esperando_respuesta a 1
// - el que recibe lo lee, mete la respuesta en eth_pedida, deja esperando_respuesta a 0
// - el que pide encuentra que esperando_respuesta esta a 0, devuelve eth_pedida
int esperando_respuesta_arp; // 1 = esperando una respuesta
BYTE dir_eth_respuesta_arp[ETH_ALEN];

// PROTOTIPOS INTERNOS

// gestion de cache

void arp_limpia_cache();
int arp_busca_entrada_cache(BYTE *dir_ip, BYTE *dir_ether_buscada);
// NOTA: arp_actualiza_cache es publica: se llama desde IP...

// manejo de tramas ARP

int arp_lee_datos_trama(BYTE *datos_trama, BYTE *tipo, 
    BYTE *dir_eth_origen, BYTE *dir_ip_origen, 
    BYTE *dir_eth_destino, BYTE *dir_ip_destino);

void arp_muestra_datos_trama(BYTE *datos_trama);

void arp_escribe_trama(BYTE *trama, BYTE *tipo, 
    BYTE *dir_eth_destino, BYTE *dir_ip_destino);

int arp_solicita_direccion_eth(BYTE *dir_ip, BYTE *dir_eth_pedida);

// FUNCIONES DE GESTION DE CACHE

/****************************************************************************
 * Limpia correspondencias viejas
 * Basta con buscar la primera entrada no expirada (primeras = mas antiguas),
 * y eliminar todas las anteriores.
 *
 * entra/sale: nada
 ****************************************************************************/
void arp_limpia_cache() {
    int i;
    time_t t = time(NULL);
    
    pthread_mutex_lock(&sem_tabla_arp);    
    {    
        for (i=0; i<tabla_arp.n; i++) {
            if (t < tabla_arp.t[i].expiry) break;
        }
        
        if (i > 0) {
            memmove(tabla_arp.t, tabla_arp.t+i, 
                (tabla_arp.n - i) * sizeof(entrada_arp));
        }
        
        tabla_arp.n -= i;
    }    
    pthread_mutex_unlock(&sem_tabla_arp);    
}

/****************************************************************************
 * Actualiza la cache ARP con una nueva entrada. 
 * Si la entrada ya estaba presente, solo actualiza su 'tiempo de vida'
 * Si no tiene espacio para una nueva entrada, borra la entrada mas antigua.
 *
 * entra:
 *    dir_ip, dir_ether - direcciones a introducir en la nueva entrada
 * sale: nada
 ****************************************************************************/
void arp_actualiza_cache(BYTE *dir_ip, BYTE *dir_ether) {
    int i;
    
    arp_limpia_cache();
    
    pthread_mutex_lock(&sem_tabla_arp);    
    {
        // elimina entrada ya existente (si la hay)
        for (i=0; i<tabla_arp.n; i++) {
            if (memcmp(tabla_arp.t[i].dir_ip, dir_ip, IP_ALEN) == 0) {           
                memmove(tabla_arp.t+i, tabla_arp.t+i+1, 
                    (tabla_arp.n - i - 1) * sizeof(entrada_arp));       
                tabla_arp.n --;
                break;
            }
        }
        
        // necesaria entrada nueva; asegurar espacio
        if (tabla_arp.n == ARP_CACHE_SIZE) {
            memmove(tabla_arp.t, tabla_arp.t+1, 
                    (tabla_arp.n - 1) * sizeof(entrada_arp));       
            tabla_arp.n --;
        }
        
        memcpy(tabla_arp.t[tabla_arp.n].dir_ip, dir_ip, IP_ALEN);
        memcpy(tabla_arp.t[tabla_arp.n].dir_eth, dir_ether, ETH_ALEN);
        tabla_arp.t[tabla_arp.n].expiry = time(NULL) + ARP_CACHE_TTL;
        tabla_arp.n ++;
    }    
    pthread_mutex_unlock(&sem_tabla_arp);    
}

/****************************************************************************
 * Busca una entrada  la cache ARP con una nueva entrada. 
 * Si la entrada ya estaba presente, solo actualiza su 'tiempo de vida'
 * Si no tiene espacio para una nueva entrada, borra la entrada mas antigua.
 *
 * entra:
 *    dir_ip - direccion IP cuya correspondiente dir. ethernet se busca
 *    dir_ether_buscada - direccion ethernet donde escribir el resultado
 * sale:
 *    0 si encontrada, -1 si no encontrada
 ****************************************************************************/
int arp_busca_entrada_cache(BYTE *dir_ip, BYTE *dir_ether_buscada) {
    int i, rc;
    
    arp_limpia_cache();
    
    pthread_mutex_lock(&sem_tabla_arp);    
    {
        rc = -1;
        for (i=0; i<tabla_arp.n; i++) {
            if (memcmp(tabla_arp.t[i].dir_ip, dir_ip, IP_ALEN) == 0) {
                memcpy(dir_ether_buscada, tabla_arp.t[i].dir_eth, ETH_ALEN);
                rc = 0;
                break;
            }
        }
    }
    pthread_mutex_unlock(&sem_tabla_arp);
    return rc;
}

/****************************************************************************
 * Muestra la cache ARP por stdout
 * entra / sale: nada
 ****************************************************************************/
void arp_muestra_cache() {
    int i;
    time_t t = time(NULL);
    
    pthread_mutex_lock(&sem_tabla_arp);
    {
        for (i=0; i<tabla_arp.n; i++) {
            // ignora las entradas que ya han expirado, pero no las limpia todavia
            if (t > tabla_arp.t[i].expiry) continue;
            
            printf("%3.3d.%3.3d.%3.3d.%3.3d    %2.2x.%2.2x.%2.2x.%2.2x.%2.2x.%2.2x    %d\n",
              tabla_arp.t[i].dir_ip[0], tabla_arp.t[i].dir_ip[1], 
              tabla_arp.t[i].dir_ip[2], tabla_arp.t[i].dir_ip[3], 
              tabla_arp.t[i].dir_eth[0], tabla_arp.t[i].dir_eth[1], tabla_arp.t[i].dir_eth[2],
              tabla_arp.t[i].dir_eth[3], tabla_arp.t[i].dir_eth[4], tabla_arp.t[i].dir_eth[5],
              (int)(tabla_arp.t[i].expiry - t));
        }
    }   
    pthread_mutex_unlock(&sem_tabla_arp);
}

// FUNCIONES DE GESTION DE TRAMAS ARP

/****************************************************************************
 * Muestra el contenido de una trama ARP por stdout
 * Si la cabecera ARP es correcta,
 *    Muestra las direcciones origen y destino, y el tipo de trama (REQ o REP)
 * Si la cabecera no es correcta,
 *    Muestra los bytes de la cabecera.
 *
 * entra:
 *    datos_trama - puntero al comienzo de los datos ARP de una trama Ethernet
 * sale:
 *    nada
 ****************************************************************************/
void arp_muestra_datos_trama(BYTE *datos_trama) {

    BYTE dir_eth_origen[ETH_ALEN], dir_ip_origen[IP_ALEN];
    BYTE dir_eth_destino[ETH_ALEN], dir_ip_destino[IP_ALEN];
    BYTE tipo[ARP_TLEN];
    BYTE *ip, *eth;
    int i;
    
    if (arp_lee_datos_trama(datos_trama, tipo, 
        dir_eth_origen, dir_ip_origen, dir_eth_destino, dir_ip_destino) != -1) {
        
        ip = dir_ip_origen;
        eth = dir_eth_origen;
        printf("%3.3d.%3.3d.%3.3d.%3.3d %2.2x.%2.2x.%2.2x.%2.2x.%2.2x.%2.2x -> ",
            ip[0], ip[1], ip[2], ip[3], 
            eth[0], eth[1], eth[2], eth[3], eth[4], eth[5]);
        ip = dir_ip_destino;
        eth = dir_eth_destino;
        printf("%3.3d.%3.3d.%3.3d.%3.3d %2.2x.%2.2x.%2.2x.%2.2x.%2.2x.%2.2x %s\n",
            ip[0], ip[1], ip[2], ip[3], 
            eth[0], eth[1], eth[2], eth[3], eth[4], eth[5], 
            (memcmp(tipo, arptype_req, ARP_TLEN)==0 ? "?" : "!"));
    }
    else {
        printf("Trama fea: ");
        for (i=0; i<ARP_HLEN; i++) {
            printf("%2.2x ", datos_trama[i]);
        }
        printf("\n");
    }                
}

/****************************************************************************
 * Lee los datos de una trama ARP
 *
 * entra:
 *    datos_trama - puntero al comienzo de los datos ARP de una trama Ethernet
 *    tipo, dir_eth_origen, dir_ip_origen, dir_eth_destino, dir_ip_destino - 
 *        punteros a los campos a rellenar con los datos correpondientes
 * sale:
 *    0 si todo bien, -1 si error (no son datos ARP validos)
 ****************************************************************************/
int arp_lee_datos_trama(BYTE *datos_trama, BYTE *tipo, BYTE *dir_eth_origen, BYTE *dir_ip_origen, BYTE *dir_eth_destino, BYTE *dir_ip_destino) {
    /* PRACTICA: implementa la funcion; y devuelve 0 si la trama es valida*/
	int i = 0;
	for(i=0;i<2;i++)
		tipo[i]=datos_trama[20+i];
	for(i=0;i<ETH_ALEN;i++)
		dir_eth_origen[i]=datos_trama[22+i];
	for(i=0;i<IP_ALEN;i++)
		dir_ip_origen[i]=datos_trama[28+i];
	for(i=0;i<ETH_ALEN;i++)
		dir_eth_destino[i]=datos_trama[32+i];
	for(i=0;i<IP_ALEN;i++)
		dir_ip_destino[i]=datos_trama[38+i];

    return 0 ;
}

/****************************************************************************
 * Prepara una trama ARP para ser enviada, con los datos que se soliciten
 *
 * entra:
 *    trama - trama Ethernet a preparar (la parte ARP empieza en trama+ETH_HLEN)
 *    tipo - tipo de peticion
 *    dir_eth_destino, dir_ip_destino - dir. a incluir en los campos de destino
 * sale:
 *    nada
 ****************************************************************************/
void arp_escribe_trama(BYTE *trama, BYTE *tipo, BYTE *dir_eth_destino, BYTE *dir_ip_destino) {
	int i = 0;
	for(i=0;i<ARP_HLEN;i++)
		trama[i]=cabecera_arp[i];
	trama[ARP_HLEN] = tipo[0];
	trama[ARP_HLEN + 1] = tipo[1];
	for(i=0;i<ETH_ALEN;i++)
		trama[ARP_HLEN+2+i] = dir_eth_local[i];
	for(i=0;i<IP_ALEN;i++)
		trama[ARP_HLEN+2+ETH_ALEN+i] = dir_ip_local[i];
	for(i=0;i<ETH_ALEN;i++)
		trama[ARP_HLEN+2+ETH_ALEN+IP_ALEN+i] = dir_eth_destino[i];
	for(i=0;i<IP_ALEN;i++)
		trama[ARP_HLEN+2+ETH_ALEN+IP_ALEN+ETH_ALEN+i] = dir_ip_destino[i];
}

// FUNCIONES PRINCIPALES DE ARP

/****************************************************************************
 * Procesa una trama Ethernet de tipo ARP
 * Si la direccion 'ip' del campo 'destino' es la propia,
 *    Si se trata de una consulta, envia una respuesta
 *    Si se trata de una respuesta, y el origen era la ip de una peticion 
 *       en curso, considera que ha satisfecho la peticion.
 * En cualquier caso, actualiza la cache con la ip/eth origen de la trama
 *
 * entra:
 *    tamano, trama - longitud y datos de la trama ethernet recibida
 *    dir_eth_nivel1 - direccion ethernet origen de la trama recibida
 * sale:
 *    0 si no hay fallos, -1 en caso contrario
 ****************************************************************************/
int arp_procesa_trama(int tamano, BYTE *trama, BYTE *dir_eth_nivel1) {
	int i=0, rc=0;
	BYTE tipo[2];
	BYTE dir_eth_origen[ETH_ALEN];
	BYTE dir_ip_origen[IP_ALEN];
	BYTE dir_eth_destino[ETH_ALEN];
	BYTE dir_ip_destino[ETH_ALEN];
	BYTE tramaARP[28];
	BYTE tramaARPReply[28];
	arp_lee_datos_trama(trama, tipo, dir_eth_origen,dir_ip_origen,dir_eth_destino,dir_ip_destino);  
	if(memcmp(dir_eth_local, dir_eth_nivel1,ETH_ALEN)==0){
		return -1;
	}
	if(memcmp(dir_ip_destino,dir_ip_local,IP_ALEN)==0 && memcmp(tipo,arptype_req,2)==0){ /*Compruebo si soy yo y contesto si lo soy (reply)*/
		arp_escribe_trama(tramaARP, arptype_rep, dir_eth_origen, dir_ip_origen);
		rc=EnviaDatagramaNivel1a(dir_eth_origen, 28 , tramaARP, 1);
		if(rc == -1){
			printf("Error enviando el ARP_REPLY\n");
			return -1;
		}
	}else if(memcmp(dir_eth_destino, dir_eth_local, ETH_ALEN)==0){/*Recibo una respuesta ARP*/
		memcpy(dir_eth_respuesta_arp,dir_eth_origen,ETH_ALEN);
		esperando_respuesta_arp=0;
	}
	arp_actualiza_cache(dir_ip_origen ,dir_eth_origen);
    
    return 0;
}

/****************************************************************************
 * Solicita la direccion Ethernet que corresponde a una direccion IP.
 * Si la direccion pedida ya esta en la cache, la devuelve sin mas.
 * En caso contrario, la solicita enviando una trama REQ, y reintentando
 * varias veces antes de desistir.
 *
 * entra:
 *    dir_ip - direccion IP
 *    dir_ether_pedida - direccion Ethernet a rellenar
 * sale:
 *    0 y dir_ether_pedida a su valor correspondiente, o -1 si error
 ****************************************************************************/
int arp_solicita_direccion(BYTE *dir_ip, BYTE *dir_eth_pedida) {
    
    // comprueba si estan pidiendo la dir. propia
    if (memcmp(dir_ip, dir_ip_local, IP_ALEN) == 0) {
        memcpy(dir_eth_pedida, dir_eth_local, ETH_ALEN);
        return 0;
    }
    
    // busca en la cache
    if (arp_busca_entrada_cache(dir_ip, dir_eth_pedida) == 0) {
        // encontrada en cache
        return 0;
    }
    
    // busca en la red mediante ARP
    return arp_solicita_direccion_eth(dir_ip, dir_eth_pedida);
}
    
/****************************************************************************
 * Solicita una direccion ARP directamente al exterior; no usa cache. Esto
 * es util para implementar ARP gratuito, por ejemplo.
 *
 * entra:
 *    dir_ip - direccion IP
 *    dir_ether_pedida - direccion Ethernet a rellenar
 * sale:
 *    0 y dir_ether_pedida a su valor correspondiente, o -1 si error 
 ****************************************************************************/
int arp_solicita_direccion_eth(BYTE *dir_ip, BYTE *dir_eth_pedida) {    
	BYTE tramaReq[28];
	BYTE tramaRep[28];
	BYTE Direccion_Remitente[ETH_ALEN];
	struct timeval time;
	int rc=0, i=0, tipo=0;
    	arp_escribe_trama(tramaReq, arptype_req , dir_eth_any, dir_ip); /*Escribe trama prepara la trama ARP. 1 Request, dir_eth_any ETH DESTINO broadcast*/
	esperando_respuesta_arp=1; /*Esperamos una respuesta*/
	for(i=0;i<ARP_REQ_RETRIES;i++){
		EnviaDatagramaNivel1a(dir_eth_any, 28 , tramaReq, 1);
		usleep(ARP_REQ_TIMEOUT_US);
		if(esperando_respuesta_arp==0){
			memcpy(dir_eth_pedida,dir_eth_respuesta_arp,ETH_ALEN);
			return 0;
		}
	}
	
    return -1;
}

/****************************************************************************
 * Inicializa ARP; debe llamarse antes de usar otras funciones de 'arp.h'
 * Se puede llamar repetidas veces sin efectos adversos
 *
 * Lee la variable de entorno 'IPLOCAL' para determinar la IP local.
 *
 * entra: nada
 * sale:
 *   0 si todo bien, -1 en caso de error
 ****************************************************************************/
int arp_inicializa() {
	int rc=0;
	BYTE DireccionMacARPG[ETH_ALEN];
  if ( ! arp_inicializado) {
    
    // obtiene dir. ethernet local
    if (ObtenerDirMAC(dir_eth_local) != ETH_OK) {
        fprintf(stderr, "Error obteniendo dir MAC local\n");
        return -1;
    }    

    // obtiene ip local
    if (lee_cadena_ip(getenv("IPLOCAL"), dir_ip_local) != 0) {
        printf("Error obteniendo dir IP local\n");
        return -1;
    }
        
    arp_inicializado = 1;
    rc=arp_solicita_direccion_eth(dir_ip_local, DireccionMacARPG);
    if(rc == -1)
		printf("La ip solo la tienes tú\n");              
    printf("%s/%d: Falta por hacer un ARP gratuito...\n", __FILE__, __LINE__);
    
    fprintf(stderr, "Inicializado: %3.3d.%3.3d.%3.3d.%3.3d / %2.2x.%2.2x.%2.2x.%2.2x.%2.2x.%2.2x\n",
        dir_ip_local[0], dir_ip_local[1], dir_ip_local[2], dir_ip_local[3], 
        dir_eth_local[0], dir_eth_local[1], dir_eth_local[2], 
        dir_eth_local[3], dir_eth_local[4], dir_eth_local[5]);        
  }
  
  return 0;

}

