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

    In addition, as a special exception, the copyright holders give
    permission to link the code of portions of this program with the
    OpenSSL library under certain conditions as described in each
    individual source file, and distribute linked combinations including
    the two.

    You must obey the GNU General Public License in all respects for all
    of the code used other than OpenSSL. If you modify file(s) with this
    exception, you may extend this exception to your version of the
    file(s), but you are not obligated to do so. If you do not wish to do
    so, delete this exception statement from your version. If you delete
    this exception statement from all source files in the program, then
    also delete it here.
*/

#include <stdio.h>
#include <iostream>
#include <sstream>

#include "crypto.h"

using namespace Crypto;
using namespace std;

int main( int argc, char *argv[] )
{
  if ( argc != 2 ) {
    fprintf( stderr, "Usage: %s KEY\n", argv[ 0 ] );
    return 1;
  }

  try {
    Base64Key key( argv[ 1 ] );
    Session session( key );

    /* Read input */
    ostringstream input;
    input << cin.rdbuf();

    /* Decrypt message */

    Message message = session.decrypt( input.str() );

    fprintf( stderr, "Nonce = %ld\n",
	     (long)message.nonce.val() );
    cout << message.text;
  } catch ( const CryptoException &e ) {
    cerr << e.what() << endl;
    exit( 1 );
  }

  return 0;
}
