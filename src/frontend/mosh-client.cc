/*
    Mosh: the mobile shell
    Copyright 2012 Keith Winstein

    This program is free software: you can redistribute it and/or modify
    it under the terms of the GNU General Public License as published by
    the Free Software Foundation, either version 3 of the License, or
    (at your option) any later version.

    This program is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU General Public License for more details.

    You should have received a copy of the GNU General Public License
    along with this program.  If not, see <http://www.gnu.org/licenses/>.
*/

#include <stdlib.h>
#include <string.h>

#include "stmclient.h"
#include "crypto.h"

int main( int argc, char *argv[] )
{
  /* Get arguments */
  char *ip;
  int port;

  if ( argc != 3 ) {
    fprintf( stderr, "Usage: %s IP PORT\n", argv[ 0 ] );
    exit( 1 );
  }

  ip = argv[ 1 ];
  port = myatoi( argv[ 2 ] );

  /* Read key from environment */
  char *env_key = getenv( "MOSH_KEY" );
  if ( env_key == NULL ) {
    fprintf( stderr, "MOSH_KEY environment variable not found.\n" );
    exit( 1 );
  }

  char *key = strdup( env_key );
  if ( key == NULL ) {
    perror( "strdup" );
    exit( 1 );
  }

  if ( unsetenv( "MOSH_KEY" ) < 0 ) {
    perror( "unsetenv" );
    exit( 1 );
  }

  /* Adopt native locale */
  if ( NULL == setlocale( LC_ALL, "" ) ) {
    perror( "setlocale" );
    exit( 1 );
  }

  STMClient client( ip, port, key );

  client.init();

  client.main();

  client.shutdown();

  printf( "\n[mosh is exiting.]\n" );

  free( key );

  return 0;
}
