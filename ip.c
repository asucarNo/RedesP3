/*Probando el GitHub*/
#include "nivel1a.h"
#include "arp.h"
#include "ip.h"
#include <netinet/in.h>
BYTE EtherType[ETH_TLEN * 2];
pf_notificacion_ip pnotip
int identificador = 0;
int gestiona_trama_ethernet(int tamano, const BYTE *trama,struct timeval *tv) {

    return 0;
}

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
int ip_inicializa(){

        EtherType[0] = 0x08;
        EtherType[1] = 0x06;
        EtherType[2] = 0x08; 
        EtherType[3] = 0x00;
	if (IniciarNivel1(2, EtherType, gestiona_trama_ethernetgestiona_trama_ethernet, 0) != ETH_OK) {
	    fprintf(stderr, "Error en IniciarNivel1\n");
	    return -1;
	if (arp_inicializa() != 0) {
		fprintf(stderr, "Error en arp_inicializa\n");
		FinalizarNivel1();
		return -1;
	}  
	return 0;
}

/****************************************************************************
 * Desinicializa IP; debe llamarse para liberar cualquier recurso que pudiera
 * estar reservado en este nivel.
 *
 * entra: nada
 * sale:
 *    0 si todo bien, -1 en caso de error error
 ****************************************************************************/
int ip_finaliza(){

	return 0;
}


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
int ip_registra_protocolo(BYTE protocolo, pf_notificacion_ip callback){
	
	return 0;
}


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
int ip_envia_datagrama(BYTE protocolo, BYTE *dir_ip_destino, unsigned int tamano,BYTE *datos, op_ip *opciones){

    /*declaracion variables*/
    BYTE Mensaje[1200]; //nada de numeros magicos
    BYTE vercab;
    BYTE tambyte[2];
    BYTE id[2];
    BYTE cabeceraip[IP_HLEN_MAX];
    BYTE checksum[2];
    int longtotal;
    int version=4;
    int longcabecera=5; //5 es el numero minimo y 15 el maximo, hasta que no sepamos cual escoger 5

    /*formamos el datagrama*/
    longtotal=longcabecera+tamano;
    vercab = 01000101; //version y long cabecera(esto debemos arreglarlo para que sea variable) 
    cabeceraip[0] =vercab;
    cabeceraip[1] =opciones->tos;
    memcpy(tambyte,&htons(longtotal),2); //longitud total del paquete, long trama + long cabecera
    memcpy(id,&htons(identificador),2);
    for (i = 0; i < 2; i++)  /*en cabeceraip long.tot paquete*/
        cabeceraip[2 + i] = tambyte[i]; 
    for (i = 0; i < 2; i++)  /*en cabeceraip identificacion*/
        cabeceraip[4 + i] = id[i]; 
    //como trabajamos con campos de 3 bits?, que hacer con los flags si no tratamos fragmentacion?
    //offset del fragmento
    cabeceraip[8] =opciones->ttl;
    cabezeraip[9]=protocolo;
  /*checksum, creo que se mete un checksum a 0 y luego a partir de ahi se calcula un cheksum de todo*/
    cabezeraip[10]=0;

    //cabeceraip[11] direccion ip origen
    cabeceraip[16]=dir_ip_destino;
    calcula_checksum_ip(longcabecera, cabezeraip,checksum);
    cabezeraip[10]=checksum;

        identificador++;
	return 0;
}

