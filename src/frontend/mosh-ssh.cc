/*
    Mosh: the mobile shell
    Copyright 2012 Keith Winstein

    This program is free software: you can redistribute it and/or modify
    it under the terms of the GNU General Public License as published by
    the Free Software Foundation, either version 3 of the License, or
    (at your option) any later version.
*/

#include "src/include/config.h"
#include "src/include/version.h"

#include <cerrno>
#include <csignal>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <deque>
#include <sstream>
#include <stdexcept>
#include <string>
#include <vector>

#include <fcntl.h>
#include <sys/select.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <unistd.h>

#include "src/forward/forwardmanager.h"
#include "src/util/select.h"

namespace {

struct Options
{
  bool no_command;
  bool local;
  std::string ssh_port;
  std::string user;
  std::string host;
  std::vector<std::string> ssh_passthrough;
  std::vector<std::string> forwards;
  std::vector<std::string> command;

  Options( void ) : no_command( false ), local( false ), ssh_port(), user(), host(), ssh_passthrough(), forwards(), command() {}
};

void print_version( FILE* file )
{
  fputs( "mosh-ssh (" PACKAGE_STRING ") [build " BUILD_VERSION "]\n"
         "Copyright 2012 Keith Winstein <mosh-devel@mit.edu>\n"
         "License GPLv3+: GNU GPL version 3 or later <http://gnu.org/licenses/gpl.html>.\n",
         file );
}

void print_usage( FILE* file, const char* argv0 )
{
  print_version( file );
  fprintf( file,
           "\nUsage: %s [-T] [-N] [-D [bind:]port] [-L [bind:]port:host:hostport] [ssh options] [user@]host [command...]\n",
           argv0 );
}

std::string shell_quote( const std::string& text )
{
  std::string ret = "'";
  for ( std::string::const_iterator it = text.begin(); it != text.end(); ++it ) {
    if ( *it == '\'' ) {
      ret += "'\\''";
    } else {
      ret += *it;
    }
  }
  ret += "'";
  return ret;
}

std::string join_command( const std::vector<std::string>& argv )
{
  std::string ret;
  for ( std::vector<std::string>::const_iterator it = argv.begin(); it != argv.end(); ++it ) {
    if ( it != argv.begin() ) {
      ret += " ";
    }
    ret += shell_quote( *it );
  }
  return ret;
}

bool starts_with( const std::string& text, const char* prefix )
{
  return text.compare( 0, strlen( prefix ), prefix ) == 0;
}

void require_value( int& i, int argc, char* argv[], std::string& out, const char* option )
{
  if ( i + 1 >= argc ) {
    throw std::runtime_error( std::string( "missing argument for " ) + option );
  }
  out = argv[++i];
}

Options parse_options( int argc, char* argv[] )
{
  Options options;
  bool end_options = false;

  for ( int i = 1; i < argc; i++ ) {
    std::string arg = argv[i];

    if ( !end_options && arg == "--" ) {
      end_options = true;
      continue;
    }

    if ( !options.host.empty() ) {
      options.command.push_back( arg );
      continue;
    }

    if ( !end_options && arg == "--help" ) {
      print_usage( stdout, argv[0] );
      exit( 0 );
    }
    if ( !end_options && arg == "--version" ) {
      print_version( stdout );
      exit( 0 );
    }
    if ( !end_options && arg == "--local" ) {
      options.local = true;
      continue;
    }

    if ( !end_options && starts_with( arg, "-D" ) && arg != "-D" ) {
      options.forwards.push_back( std::string( "D\t" ) + arg.substr( 2 ) );
      continue;
    }
    if ( !end_options && starts_with( arg, "-L" ) && arg != "-L" ) {
      options.forwards.push_back( std::string( "L\t" ) + arg.substr( 2 ) );
      continue;
    }

    if ( !end_options && !arg.empty() && arg[0] == '-' ) {
      if ( arg == "-T" ) {
        continue;
      } else if ( arg == "-N" ) {
        options.no_command = true;
        continue;
      } else if ( arg == "-D" ) {
        std::string value;
        require_value( i, argc, argv, value, "-D" );
        options.forwards.push_back( std::string( "D\t" ) + value );
        continue;
      } else if ( arg == "-L" ) {
        std::string value;
        require_value( i, argc, argv, value, "-L" );
        options.forwards.push_back( std::string( "L\t" ) + value );
        continue;
      } else if ( arg == "-p" ) {
        require_value( i, argc, argv, options.ssh_port, "-p" );
        options.ssh_passthrough.push_back( "-p" );
        options.ssh_passthrough.push_back( options.ssh_port );
        continue;
      } else if ( arg == "-l" ) {
        require_value( i, argc, argv, options.user, "-l" );
        options.ssh_passthrough.push_back( "-l" );
        options.ssh_passthrough.push_back( options.user );
        continue;
      } else if ( arg == "-F" || arg == "-i" || arg == "-J" || arg == "-o" ) {
        std::string value;
        require_value( i, argc, argv, value, arg.c_str() );
        options.ssh_passthrough.push_back( arg );
        options.ssh_passthrough.push_back( value );
        continue;
      }

      options.ssh_passthrough.push_back( arg );
      continue;
    }

    if ( options.host.empty() ) {
      options.host = arg;
    } else {
      options.command.push_back( arg );
    }
  }

  if ( options.host.empty() ) {
    throw std::runtime_error( "missing host" );
  }

  if ( options.no_command ) {
    options.command.clear();
  } else if ( options.command.empty() ) {
    options.command.push_back( "sh" );
  }

  return options;
}

uint16_t parse_port( const std::string& text )
{
  char* end = NULL;
  errno = 0;
  unsigned long port = strtoul( text.c_str(), &end, 10 );
  if ( errno != 0 || !end || *end != '\0' || port > 65535 ) {
    throw std::runtime_error( "invalid port: " + text );
  }
  return static_cast<uint16_t>( port );
}

std::vector<std::string> split_colon( const std::string& text )
{
  std::vector<std::string> ret;
  std::string::size_type start = 0;
  while ( true ) {
    std::string::size_type pos = text.find( ':', start );
    if ( pos == std::string::npos ) {
      ret.push_back( text.substr( start ) );
      break;
    }
    ret.push_back( text.substr( start, pos - start ) );
    start = pos + 1;
  }
  return ret;
}

void configure_forward( Forward::ForwardManager& manager, const std::string& encoded )
{
  std::string kind = encoded.substr( 0, 1 );
  std::string spec = encoded.substr( 2 );
  std::vector<std::string> parts = split_colon( spec );

  if ( kind == "D" ) {
    std::string bind = "127.0.0.1";
    std::string port;
    if ( parts.size() == 1 ) {
      port = parts[0];
    } else if ( parts.size() == 2 ) {
      bind = parts[0].empty() ? std::string( "127.0.0.1" ) : parts[0];
      port = parts[1];
    } else {
      throw std::runtime_error( "bad -D specification: " + spec );
    }
    manager.add_dynamic_forward( bind, parse_port( port ) );
  } else if ( kind == "L" ) {
    std::string bind = "127.0.0.1";
    std::string local_port, host, host_port;
    if ( parts.size() == 3 ) {
      local_port = parts[0];
      host = parts[1];
      host_port = parts[2];
    } else if ( parts.size() == 4 ) {
      bind = parts[0].empty() ? std::string( "127.0.0.1" ) : parts[0];
      local_port = parts[1];
      host = parts[2];
      host_port = parts[3];
    } else {
      throw std::runtime_error( "bad -L specification: " + spec );
    }
    manager.add_local_forward( bind, parse_port( local_port ), host, parse_port( host_port ) );
  }
}

pid_t spawn_bootstrap( const Options& options, int pipe_fds[2] )
{
  if ( pipe( pipe_fds ) < 0 ) {
    throw std::runtime_error( std::string( "pipe: " ) + strerror( errno ) );
  }

  pid_t pid = fork();
  if ( pid < 0 ) {
    throw std::runtime_error( std::string( "fork: " ) + strerror( errno ) );
  }

  if ( pid == 0 ) {
    close( pipe_fds[0] );
    dup2( pipe_fds[1], STDOUT_FILENO );
    dup2( pipe_fds[1], STDERR_FILENO );
    close( pipe_fds[1] );

    std::string command = "mosh-server";
    const char* env_server = getenv( "MOSH_SERVER" );
    if ( env_server && *env_server ) {
      command = env_server;
    }

    std::vector<std::string> server_argv;
    server_argv.push_back( command );
    server_argv.push_back( "new" );
    server_argv.push_back( "--stdio-session" );
    server_argv.push_back( "--forward=streammux-v1" );
    server_argv.push_back( "--" );
    if ( options.no_command ) {
      server_argv.push_back( "sleep" );
      server_argv.push_back( "2147483647" );
    } else {
      server_argv.push_back( "sh" );
      server_argv.push_back( "-lc" );
      server_argv.push_back( join_command( options.command ) );
    }

    if ( options.local ) {
      std::vector<char*> exec_argv;
      for ( std::vector<std::string>::iterator it = server_argv.begin(); it != server_argv.end(); ++it ) {
        exec_argv.push_back( const_cast<char*>( it->c_str() ) );
      }
      exec_argv.push_back( NULL );
      execvp( exec_argv[0], &exec_argv[0] );
      _exit( 127 );
    }

    std::vector<std::string> ssh_argv;
    ssh_argv.push_back( "ssh" );
    ssh_argv.push_back( "-T" );
    ssh_argv.insert( ssh_argv.end(), options.ssh_passthrough.begin(), options.ssh_passthrough.end() );
    ssh_argv.push_back( options.host );
    ssh_argv.push_back( "--" );
    ssh_argv.push_back( join_command( server_argv ) );

    std::vector<char*> exec_argv;
    for ( std::vector<std::string>::iterator it = ssh_argv.begin(); it != ssh_argv.end(); ++it ) {
      exec_argv.push_back( const_cast<char*>( it->c_str() ) );
    }
    exec_argv.push_back( NULL );
    execvp( exec_argv[0], &exec_argv[0] );
    _exit( 127 );
  }

  close( pipe_fds[1] );
  return pid;
}

void parse_startup( int fd, std::string& port, std::string& key )
{
  std::string buffer;
  char ch;
  while ( read( fd, &ch, 1 ) == 1 ) {
    if ( ch == '\n' ) {
      if ( starts_with( buffer, "MOSH FORWARD " ) ) {
        std::istringstream in( buffer );
        std::string a, b, proto;
        in >> a >> b >> port >> key >> proto;
        if ( proto == "streammux-v1" ) {
          return;
        }
      } else {
        fprintf( stderr, "%s\n", buffer.c_str() );
      }
      buffer.clear();
    } else if ( ch != '\r' ) {
      buffer += ch;
    }
  }
  throw std::runtime_error( "did not receive MOSH FORWARD startup line" );
}

bool fd_in_list( int fd, const std::vector<int>& fds )
{
  return std::find( fds.begin(), fds.end(), fd ) != fds.end();
}

int run_event_loop( Forward::ForwardManager& manager, uint64_t stdio_id, uint64_t stderr_id )
{
  Select& sel = Select::get_instance();
  sel.add_signal( SIGTERM );
  sel.add_signal( SIGINT );
  sel.add_signal( SIGHUP );

  while ( true ) {
    std::vector<int> read_fds = manager.read_fds();
    std::vector<int> write_fds = manager.write_fds();

    sel.clear_fds();
    for ( std::vector<int>::const_iterator it = read_fds.begin(); it != read_fds.end(); ++it ) {
      sel.add_read_fd( *it );
    }
    for ( std::vector<int>::const_iterator it = write_fds.begin(); it != write_fds.end(); ++it ) {
      sel.add_write_fd( *it );
    }

    int active_fds = sel.select( manager.wait_time() );
    if ( active_fds < 0 ) {
      if ( errno == EINTR ) {
        continue;
      }
      perror( "select" );
      return 1;
    }

    for ( std::vector<int>::const_iterator it = read_fds.begin(); it != read_fds.end(); ++it ) {
      if ( fd_in_list( *it, manager.read_fds() ) && sel.read( *it ) ) {
        manager.handle_readable( *it );
      }
    }
    for ( std::vector<int>::const_iterator it = write_fds.begin(); it != write_fds.end(); ++it ) {
      if ( fd_in_list( *it, manager.write_fds() ) && sel.write( *it ) ) {
        manager.handle_writable( *it );
      }
    }

    manager.tick();

    if ( sel.signal( SIGTERM ) || sel.signal( SIGINT ) || sel.signal( SIGHUP ) ) {
      return 255;
    }

    if ( manager.stream_closed( stdio_id ) && manager.stream_closed( stderr_id ) ) {
      return 0;
    }
  }
}

}

int main( int argc, char* argv[] )
{
  try {
    Options options = parse_options( argc, argv );

    int bootstrap_pipe[2];
    pid_t bootstrap_pid = spawn_bootstrap( options, bootstrap_pipe );

    std::string forward_port, forward_key;
    parse_startup( bootstrap_pipe[0], forward_port, forward_key );
    close( bootstrap_pipe[0] );
    waitpid( bootstrap_pid, NULL, WNOHANG );

    const char* connect_host = options.local ? "127.0.0.1" : options.host.c_str();
    std::string numeric_host = connect_host;
    std::string::size_type at = numeric_host.rfind( '@' );
    if ( at != std::string::npos ) {
      numeric_host = numeric_host.substr( at + 1 );
    }

    Forward::ForwardManager manager( Forward::ForwardManager::CLIENT,
                                     forward_key.c_str(),
                                     numeric_host.c_str(),
                                     forward_port.c_str() );

    for ( std::vector<std::string>::const_iterator it = options.forwards.begin(); it != options.forwards.end(); ++it ) {
      configure_forward( manager, *it );
    }

    uint64_t stdio_id = manager.open_stdio_stream( STDIN_FILENO, STDOUT_FILENO );
    uint64_t stderr_id = manager.open_stderr_stream( STDERR_FILENO );

    return run_event_loop( manager, stdio_id, stderr_id );
  } catch ( const std::exception& e ) {
    fprintf( stderr, "mosh-ssh: %s\n", e.what() );
    return 255;
  }
}
