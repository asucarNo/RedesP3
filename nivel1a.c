/*****************************************************************
 *	Redes de comunicaciones I - Práctica 1
 *	Módulo: nivel1a.c
 *	
 *	Descripción: Implementación de todas las funciones que
 *		componen las especificación del nivel1a.
 *
 *	Autores: Víctor Fernández Díaz y Guillermo Carniado Cartas		
 *	Fecha: 15 - Noviembre - 2012
 *	Versión 2.0 - ARP
 *
 ******************************************************************/
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include "nivel1a.h"

BYTE broadcast[ETH_ALEN] = {0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF};
BYTE DireccionOrigen[ETH_ALEN];
BYTE EtherType[ETH_TLEN * 2];
int Nivel1aIniciado = 0;

/***************************************************************************
 * Funci�n : InicializaNivel1a
 * Fecha: 14 - Octubre - 2012
 *
 * Descripci�n : Inicializa el nivel1a y sucesivos para permitir
 * 	 la transmisi�n de informaci�n empleando las funciones 
 *	 de este protocolo.
 *
 * Par�metros:
 *   Tipo1: Flag de tipo entero para comprobaci�n de tipo.
 *   Tipo2: Flag de tipo entero para comprobaci�n de tipo.
 *   Funcion: Puntero a funci�n de notificaci�n que ser� empleada para
 *	validar los mensajes recibidos.
 *   Timeout: Valor del tiempo que se esperar� por una trama antes de rechazarla
 *
 * C�digo de retorno:
 *   0  :Todo ha ido bien.
 *  -1  :En caso de error.
 ***************************************************************************/
int InicializaNivel1a(int Tipo1, int Tipo2, tpfNotificacionDesdeNivel1a Funcion, int Timeout) {
    int tipon1 = 0;
    /*Se prepara el Ethertype en funcion de los argumentos*/
    if (Tipo1 == 1 && Tipo2 == 1) {
        EtherType[0] = 0x20;
        EtherType[1] = 0x98;
        EtherType[2] = 0x84;
        EtherType[3] = 0x3F;
        tipon1 = 2;
    } else if (Tipo1 == 1 && Tipo2 == 0) {
        tipon1 = 1;
        EtherType[0] = 0x20;
        EtherType[1] = 0x98;
    } else if (Tipo1 == 0 && Tipo2 == 1) {
        tipon1 = 1;
        EtherType[0] = 0x84;
        EtherType[1] = 0x3F;
    } else {
        tipon1 = 0;
    }
  
    /*Se obtiene la direcci�n remitente*/
    if (ObtenerDirMAC(DireccionOrigen) != ETH_OK) {
        printf("Error obteniendo la direcci�n origen");
        FinalizarNivel1();
        return (ETH_ERROR);
    }
    Nivel1aIniciado = 1;
    return (ETH_OK);
}

/***************************************************************************
 * Funci�n : FinalizaNivel1a
 * Fecha: 14 - Octubre - 2012
 *
 * Descripci�n : Finaliza el nivel1a y sucesivos para terminar el uso del
 *	protocolo de transmisi�n de datos.
 *
 * C�digo de retorno:
 *   0  :Todo ha ido bien.
 *  -1  :En caso de error.
 ***************************************************************************/
int FinalizaNivel1a() {
    if (Nivel1aIniciado == 0)
        return (ETH_ERROR);

    if (FinalizarNivel1() != ETH_OK) {
        return (ETH_ERROR);
    }
    Nivel1aIniciado = 0;
    return (ETH_OK);
}

/***************************************************************************
 * Funci�n : EnviaDatagramaNivel1a
 * Fecha: 14 - Octubre - 2012
 *
 * Descripci�n : Funci�n para env�o de tramas mediante el uso del nivel1a.
 * 	      Prepara la trama para ser enviada incluyendo cabeceras y CRC.
 *
 * Par�metros:
 *	Direccion_Destino: Indica la MAC del usuario que recibir� el mensaje.
 *	Tamano: Indica el tama�o del mensaje pasado por argumentos que se desea enviar
 *	Mensaje: Mensaje que se desea env�ar.
 *	Tipo: Flag de tipo entero para la elecci�n del Ethertype.
 *
 * C�digo de retorno:
 *   0  :Todo ha ido bien.
 *  -1  :En caso de error.
 ***************************************************************************/
int EnviaDatagramaNivel1a(const BYTE * Direccion_Destino, int Tamano, const BYTE *Mensaje, int Tipo) {
    int rc = 0, i = 0;
    BYTE TramaNivel1Env[ETH_FRAME_MAX];
    if (Tipo == 1) {
        EtherType[0] = 0x08;
        EtherType[1] = 0x06;
    } else if (Tipo == 2) {
        EtherType[0] = 0x08;
        EtherType[1] = 0x00;
    } else
        return ETH_ERROR;
	ObtenerDirMAC(DireccionOrigen);
    /* Construir la trama: Campos de cabecera. */
    for (i = 0; i < ETH_ALEN; i++)
        TramaNivel1Env[i] = Direccion_Destino[i]; /* Direcci�n destino.   */
    for (i = 0; i < ETH_ALEN; i++)
        TramaNivel1Env[ETH_ALEN + i] = DireccionOrigen[i]; /* Direcci�n origen.    */
    for (i = 0; i < 2; i++)
        TramaNivel1Env[2 * ETH_ALEN + i] = EtherType[i]; /* Ethertype.          */
    /* A�adir campo de datos */
    if (Tamano > ETH_DATA_MAX) {
        /* Truncar el mensaje si excede la m�xima longitud de datos permitida*/
        Tamano = ETH_DATA_MAX;
    }
    for (i = 0; i < Tamano; i++)
        TramaNivel1Env[ETH_HLEN + i] = Mensaje[i];
    /* Comprobar si los datos a transmitir no llegan al m�nimo requerido */
    /*    por el protocolo. */
    if (Tamano < ETH_DATA_MIN) {
        /* Se completa la trama hasta alcanzar el m�nimo */
        for (i = 0; i < ETH_DATA_MIN - Tamano; i++)
            TramaNivel1Env[ETH_HLEN + Tamano + i] = 0;
        Tamano = ETH_DATA_MIN;
    }
/*	for(i=0;i<Tamano;i++)
		printf("%X",TramaNivel1Env[i]);
	printf("\n");*/
    rc = EnviarTramaNivel1(Tamano+ETH_HLEN, TramaNivel1Env);
    /* Comprobar si ha habido error de transmisi�n */

    if (rc != ETH_OK) {
        rc=FinalizarNivel1();
        return (ETH_ERROR);
    }


    return (ETH_OK);
}

/***************************************************************************
 * Funci�n : RecibeDatagramaNivel1a
 * Fecha: 14 - Octubre - 2012
 *
 * Descripci�n : Recibe una trama desde el nivel1 y se encarga de trocearla
 *	para devolver �nicamente el mensaje y el remitente.
 *
 * Par�metros:
 *	Direccion_Remitente: Indica la MAC del usuario que envi� el mensaje
 *	Mensaje: Mensaje que se desea env�ar.
 *	Tipo: Flag de tipo entero para la elecci�n del Ethertype.
 *	time: Timeout.
 *
 * C�digo de retorno:
 *   0  :Todo ha ido bien.
 *  -1  :En caso de error.
 ***************************************************************************/
int RecibeDatagramaNivel1a(BYTE * Direccion_Remitente, BYTE *Mensaje, int *Tipo, struct timeval* time) {
    BYTE TramaNivel1Rec[ETH_FRAME_MAX];
    int rc = 0, i = 0;

    if (Nivel1aIniciado == 0)
        return ETH_ERROR;

    rc = RecibirTramaNivel1(TramaNivel1Rec, time);
    if (rc == ETH_ERROR)
        return ETH_ERROR;
    if (rc == 0)
        return rc;
	ObtenerDirMAC(DireccionOrigen);
    /*Se separan los datos que se devolver�n*/
    for (i = 0; i < ETH_ALEN; i++) {
        Direccion_Remitente[i] = TramaNivel1Rec[i + ETH_ALEN];
    }

    EtherType[0] = TramaNivel1Rec[ETH_ALEN * 2];
    EtherType[1] = TramaNivel1Rec[1 + ETH_ALEN * 2];

    if (EtherType[0] == 0x20 && EtherType[1] == 0x98) {
        *Tipo = 1;
    } else if (EtherType[0] == 0x84 && EtherType[1] == 0x3F) {
        *Tipo = 2;

    }
    for (i = 0; i < rc - ETH_HLEN - ETH_CRCLEN; i++) {
        Mensaje[i] = TramaNivel1Rec[ETH_HLEN + i];
    }

    return i;
}

/***************************************************************************
 * Funci�n : NotificacionNivel1
 * Fecha: 14 - Octubre - 2012
 *
 * Descripci�n : Funci�n llamada al recibir una trama. Comprueba CRC y
 *		si el destinatario es el correcto. De lo contrario, la descarta.
 *
 * Par�metros:
 *	tam: Indica el tama�o de la trama pasada por argumentos.
 *	Trama: Cadena de bytes que contiene la trama recibida.
 *	timestamp: 
 *
 * C�digo de retorno:
 *   0  :Todo ha ido bien.
 *  -1  :En caso de error.
 ***************************************************************************/
int NotificacionNivel1(int tam, const BYTE *Trama, struct timeval *timestamp) {
    BYTE EtherType[ETH_TLEN];
	BYTE DireccionDestino[ETH_ALEN];
	BYTE DireccionMAC[ETH_ALEN];
    int i = 0;

    if (tam == 0) {
        return (ETH_ERROR);
    }
    if (Trama == NULL) {
        return (ETH_ERROR);
    }

    for (i = 0; i < ETH_ALEN; i++) {
        DireccionDestino[i] = Trama[i];
    }

    ObtenerDirMAC(DireccionMAC);

    /*Comprobamos que el receptor sea el destinatario o si la emisi�n es en broadcast*/
    if ((memcmp(DireccionDestino, DireccionMAC, 6) != 0) && (memcmp(DireccionDestino, broadcast, 6) != 0)) {
        return (ETH_ERROR);
    }
    if (EtherType[0] == 0x08 && EtherType[1] == 0x06) {
       
    } else if (EtherType[0] == 0x08 && EtherType[1] == 0x00) {
       
    }else{
	return ETH_ERROR;
	}
    return (ETH_OK);
}
