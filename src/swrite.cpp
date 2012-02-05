#include <unistd.h>
#include <string.h>
#include <stdio.h>

#include "swrite.hpp"

int swrite( int fd, const char *str, ssize_t len )
{
  ssize_t total_bytes_written = 0;
  ssize_t bytes_to_write = ( len >= 0 ) ? len : strlen( str );
  while ( total_bytes_written < bytes_to_write ) {
    ssize_t bytes_written = write( fd, str + total_bytes_written,
				   bytes_to_write - total_bytes_written );
    if ( bytes_written <= 0 ) {
      perror( "write" );
      return -1;
    } else {
      total_bytes_written += bytes_written;
    }
  }

  return 0;
}
