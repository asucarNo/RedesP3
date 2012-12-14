/*****************************************************************
 *	Redes de comunicaciones I - Práctica 1
 *	Módulo: nivel1a.h
 *	
 *	Descripción: Declaración de todas las funciones que
 *		componen las especificación del nivel1a.
 *
 *	Autores: Víctor Fernández Díaz y Guillermo Carniado Cartas		
 *	Fecha: 15 - Noviembre - 2012
 *	Versión 1.2
 *
 ******************************************************************/
#ifndef _NIVEL1A_H_
#define _NIVEL1A_H_

#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include "nivel1.h"
#include "rc_funcs.h"

typedef int (*tpfNotificacionDesdeNivel1a) (const BYTE *, int , const BYTE *, int , struct timeval *); 
int InicializaNivel1a ( int Tipo1, int Tipo2, tpfNotificacionDesdeNivel1a Funcion , int Timeout );
int FinalizaNivel1a ( void );
int EnviaDatagramaNivel1a ( const BYTE * DireccionDestino , int Tamano , const BYTE *Mensaje, int Tipo);
int RecibeDatagramaNivel1a (BYTE * DireccionRemitente , BYTE *Mensaje, int *Tipo, struct timeval* time);
int NotificacionNivel1(int tam, const BYTE *Trama, struct timeval *timestamp);
int NotificacionNivel1a( const BYTE * Direccion_Remitente , int Tamano , const BYTE *Mensaje, int Tipo, struct timeval * time);
#endif
