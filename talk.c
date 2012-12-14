/***************************************************************************
 * talk.c
 *
 *  Programa principal para probar IP a nivel basico en las practicas de 
 *  Redes de Comunicaciones 2 de la EPS-UAM
 *
 *  Autor: Manuel Freire
 *  Madificado: K.Koroutchev 2010/02/25
 *
 *  (C) 2006-07 EPS-UAM 
 ***************************************************************************/

#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <signal.h>
#include <sys/types.h>
#include <unistd.h>
#include <sys/wait.h>

#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>

#include "ip.h"
#include "rc_funcs.h"

#define IP_PROTOCOL_TALK 207
#define MAX_LINEA 32000

/****************************************************************************
 * Procesa paquetes IP de tipo 'talk' recibidos desde la red
 * (se usara el numero de protocolo 207, no asignado segun rfc 1700)
 *
 * entra:
 *    dir_ip_origen - direccion IP que lo envia
 *    tamano - numero de bytes en el campo datos
 *    datos - los datos del paquete
 *    opciones - opciones usadas al enviar
 * sale:
 *    0, -1 si error
 ****************************************************************************/
int gestiona_paquete_talk(BYTE *dir_ip_origen, unsigned int tamano, 
    BYTE *datos, op_ip *opciones) {
        
    if (tamano != 0 && datos[tamano-1] != 0) {
        datos[tamano-1] = 0;
    }
    
    printf("%3.3d.%3.3d.%3.3d.%3.3d>> '%s'\n--\n",
        dir_ip_origen[0], dir_ip_origen[1], dir_ip_origen[2], dir_ip_origen[3], 
        (tamano == 0 ? "" : (char *)datos) );
        
    return 0;
}

/*

Modificaciones 2010/02/25:

1) MAX_LINEA de 256 a 32000 para permitir comprobacion de desfragmentacion.
2) No utiliza lee_cadena_ip(), ya que la funcion repite la funcion de la libreria inet_aton().
3) modo de dar novel de trazas.

 */
/***************************************************************************
 * "main"
 ***************************************************************************/

int main(int argc, char **argv) {
  char buffer[MAX_LINEA];
  BYTE dir_ip_destino[IP_ALEN];  
  char *ayuda =
    "Uso: talk [<nivel_trazas>] <dir_ip_destino>\n"
    "  <nivel_trazas> \n"
    "                 -0 = sin trazas, \n"
    "                 -3 = maximo detalle\n"
    ;

  // nivel trazas
  if (argc>1 && argv[1][0]=='-') {
    if (!argv[1][2] && argv[1][1]>='0' && argv[1][1]<='3') {
      int debug=argv[1][1]-'0';
      ActivarTrazas(debug, "stdout");
    }
    argc--; argv++;
  }
  if (argc<=1) {
    fprintf(stderr, ayuda);    
    return 0;
  }
  
  // dir ip destino
  if (!inet_aton(argv[1],(struct in_addr *)dir_ip_destino)) {
    // lee_cadena_ip(argv[1], dir_ip_destino) != 0) {
    fprintf(stderr,"Error obteniendo dir con la que hablar\n");
    fprintf(stderr,ayuda);
    return -1;
  }
  printf("La ip es: %d.%d.%d.%d\n",
	 dir_ip_destino[0],
	 dir_ip_destino[1],
	 dir_ip_destino[2],
	 dir_ip_destino[3]);
  // inicia ip
  if (ip_inicializa() != 0 || 
      ip_registra_protocolo(IP_PROTOCOL_TALK, gestiona_paquete_talk) != 0) {
    fprintf(stderr, "Error en inicializando IP o registrando protocolo\n");
    return -1;
  }
  // bucle principal
  printf("Talk iniciado; usa EOF (ctrl-D) en linea vacia para salir.\n");
  while(fgets(buffer, MAX_LINEA, stdin)) {
    if (ip_envia_datagrama(IP_PROTOCOL_TALK, 
			   dir_ip_destino, strlen(buffer)+1, (BYTE *)buffer, NULL) != 0) {
      printf("Error enviando paquete.\n");
    }
  }
  if (ip_finaliza() != 0) {
    printf("Error finalizando IP\n");
    return -1;
  }
  return 0;
}
