//   Mosh: the mobile shell
//   Copyright 2012 Keith Winstein
//
//   This program is free software: you can redistribute it and/or modify
//   it under the terms of the GNU General Public License as published by
//   the Free Software Foundation, either version 3 of the License, or
//   (at your option) any later version.
//
//   This program is distributed in the hope that it will be useful,
//   but WITHOUT ANY WARRANTY; without even the implied warranty of
//   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
//   GNU General Public License for more details.
//
//   You should have received a copy of the GNU General Public License
//   along with this program.  If not, see <http://www.gnu.org/licenses/>.

#include "config.h"

#include <stdlib.h>
#include <vector>
#include <map>
#include <stdio.h>
#include <string>
#include <sys/socket.h>
#include <boost/algorithm/string.hpp>
#include <boost/format.hpp>
#include <boost/regex.hpp>
#include <getopt.h>
#include <arpa/inet.h>
#include <netdb.h>
#include <signal.h>
#include <errno.h>
#include <sys/ioctl.h>

#if HAVE_PTY_H
#include <pty.h>
#elif HAVE_UTIL_H
#include <util.h>
#endif

#if FORKPTY_IN_LIBUTIL
#include <libutil.h>
#endif

using namespace std;
using namespace boost;

static const char *const MOSH_VERSION = "1.1.3";

inline string shell_quote_string(string x) {
    return "'" + replace_all_copy(x, "'", "'\\''") + "'";
}

template <typename SequenceT>
inline string shell_quote(SequenceT &sequence) {
    vector<string> atoms;
    for (typename SequenceT::iterator i=sequence.begin(); i!=sequence.end(); i++) {
        atoms.push_back(shell_quote_string(*i));
    }
    return join(atoms, " ");
}

void die(string message) {
    fprintf(stderr, "%s\n", message.c_str());
    exit(255);
}

static const string usage_format =
"Usage: %1% [options] [--] [user@]host [command...]\n"
"        --client=PATH        mosh client on local machine\n"
"                                (default: \"mosh-client\")\n"
"        --server=PATH        mosh server on remote machine\n"
"                                (default: \"mosh-server\")\n"
"\n"
"        --predict=adaptive   local echo for slower links [default]\n"
"-a      --predict=always     use local echo even on fast links\n"
"-n      --predict=never      never use local echo\n"
"\n"
"-p NUM  --port=NUM           server-side UDP port\n"
"\n"
"        --help               this message\n"
"        --version            version and copyright information\n"
"\n"
"Please report bugs to mosh-devel@mit.edu.\n"
"Mosh home page: http://mosh.mit.edu";

static string usage;

static const string version_message = str(format(
"mosh %1%\n"
"Copyright 2012 Keith Winstein <mosh-devel@mit.edu>\n"
"License GPLv3+: GNU GPL version 3 or later <http://gnu.org/licenses/gpl.html>.\n"
"This is free software: you are free to change and redistribute it.\n"
"There is NO WARRANTY, to the extent permitted by law.") % MOSH_VERSION);

static char **argv;

void predict_check(string predict, bool env_set) {
    if (predict != "adaptive" &&
        predict != "always" &&
        predict != "never") {
        string explanation;
        if (env_set)
            explanation = " (MOSH_PREDICTION_DISPLAY in environment)";
        fprintf(stderr, "%s: Unknown mode \"%s\"%s.\n", argv[0], predict.c_str(), explanation.c_str());
        die(usage);
    }
}

void cat(int ifd, int ofd) {
    char buf[4096];
    ssize_t n;
    while (1) {
        n = read(ifd, buf, sizeof(buf));
        if (n==-1 && errno == EINTR)
            continue;
        if (n==0)
            break;
        n = write(ofd, buf, n);
        if (n==-1)
            break;
    }
}

int main( int argc, char *_argv[] )
{
    argv = _argv;
    string client = "mosh-client";
    string server = "mosh-server";
    string predict, port_request;
    int help=0, version=0, fake_proxy=0;

    usage = str(format(usage_format) % argv[0]);

    static struct option long_options[] =
    {
        {"client",      required_argument,  0, 'c'},
        {"server",      required_argument,  0, 's'},
        {"predict",     required_argument,  0, 'r'},
        {"port",        required_argument,  0, 'p'},
        {"help",        no_argument,        &help,          1},
        {"version",     no_argument,        &version,       1},
        {"fake-proxy!", no_argument,        &fake_proxy,    1},
        {0, 0, 0, 0}
    };
    while (1) {
        int option_index = 0;
        int c = getopt_long(argc, argv, "anp:",
                            long_options, &option_index);
        if (c==-1)
            break;

        switch (c) {
            case 0:
                // flag has been set
                break;
            case 'c':
                client = optarg;
                break;
            case 's':
                server = optarg;
                break;
            case 'r':
                predict = optarg;
                break;
            case 'p':
                port_request = optarg;
                break;
            case 'a':
                predict = "always";
                break;
            case 'n':
                predict = "never";
                break;
            default:
                die(usage);
        }
    }

    if (help)
        die(usage);
    if (version)
        die(version_message);

    if (predict.size())
        predict_check(predict, 0);
    else if (getenv("MOSH_PREDICTION_DELAY")) {
        predict = getenv("MOSH_PREDICTION_DELAY");
        predict_check(predict, 1);
    } else {
        predict = "adaptive";
        predict_check(predict, 0);
    }

    if (port_request.size()) {
        if (!regex_match(port_request, regex("^[0-9]+$")) ||
            atoi(port_request.c_str()) < 0 ||
            atoi(port_request.c_str()) > 65535)
            die(str(format("%1%: Server-side port (%2%) must be within valid range [0..65535].") % argv[0] % port_request));
    }

    unsetenv("MOSH_PREDICTION_DISPLAY");

    if (fake_proxy) {
        string host = argv[optind++];
        string port = argv[optind++];

        int sockfd;
        struct addrinfo hints, *servinfo, *p;
        int rv;

        memset(&hints, 0, sizeof(hints));
        hints.ai_family = AF_INET;
        hints.ai_socktype = SOCK_STREAM;

        if ((rv = getaddrinfo(host.c_str(), port_request.size() ? port_request.c_str() : "ssh", &hints, &servinfo)) != 0)
            die(str(format("%1%: Could not resolve hostname %2%: getaddrinfo: %3%") % argv[0] % host % gai_strerror(rv)));

        // loop through all the results and connect to the first we can
        for(p = servinfo; p != NULL; p = p->ai_next) {
            if ((sockfd = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP)) == -1)
                continue;

            if (connect(sockfd, p->ai_addr, p->ai_addrlen) == -1) {
                close(sockfd);
                continue;
            }

            char host[NI_MAXHOST], service[NI_MAXSERV];
            if (getnameinfo(p->ai_addr, p->ai_addrlen,
                        host, NI_MAXHOST,
                        service, NI_MAXSERV,
                        NI_NUMERICSERV | NI_NUMERICHOST) == -1)
                die("Couldn't get host name info");

            fprintf(stderr, "MOSH IP %s\n", host);
            break; // if we get here, we must have connected successfully
        }

        if (p == NULL) {
            // looped off the end of the list with no connection
            die(str(format("%1%: failed to connect to host %2% port %3%")
                    % argv[0] % host % port));
        }

        freeaddrinfo(servinfo); // all done with this structure

        int pid = fork();
        if (pid == -1)
            die(str(format("%1%: fork: %2%") % argv[0] % errno));
        if (pid == 0) {
            cat(sockfd, 1);
            shutdown(sockfd, 0);
            exit(0);
        }
        signal(SIGHUP, SIG_IGN);
        cat(0, sockfd);
        shutdown(sockfd, 1);
        waitpid(pid, NULL, 0);
        exit(0);
    }

    if (argc - optind < 1)
        die(usage);

    string userhost = argv[optind++];
    char **command = &argv[optind];
    int commands = argc - optind;

    int pipe_fd[2];
    pipe(pipe_fd);

    FILE *color_file = popen(str(format("%1% -c") % client).c_str(), "r");
    size_t n;
    char *buf = fgetln(color_file, &n);
    if (!buf)
        die(str(format("%1%: Can't count colors: %1%") % argv[0] % errno));
    string colors = string(buf, n);
    pclose(color_file);

    if (!regex_match(colors, regex("^[0-9]+$")) || atoi(colors.c_str()) < 0)
        colors = "0";

    int pty, pty_slave;
    struct winsize ws;
    ioctl(0, TIOCGWINSZ, &ws);
    if (openpty(&pty, &pty_slave, NULL, NULL, &ws)==-1)
        die(str(format("%1%: openpty: %2%") % argv[0] % errno));

    int pid = fork();
    if (pid == -1)
        die(str(format("%1%: fork: %2%") % argv[0] % errno));
    if (pid == 0) {
        close(pty);
        if (-1 == dup2(pty_slave, 1) ||
            -1 == dup2(pty_slave, 2))
            die(str(format("%1%: dup2: %2%") % argv[0] % errno));
        close(pty_slave);

        vector<string> server_args;
        server_args.push_back("new");
        server_args.push_back("-s");
        server_args.push_back("-c");
        server_args.push_back(colors);
        if (port_request.size()) {
            server_args.push_back("-p");
            server_args.push_back(port_request);
        }
        if (commands)
            server_args.insert(server_args.end(), command, command + commands);
        string quoted_self = shell_quote_string(string(argv[0]));
        string quoted_server_args = shell_quote(server_args);
        fflush(stdout);

        string proxy_arg = str(format("ProxyCommand=%1% --fake-proxy -- %%h %%p") % quoted_self);
        string ssh_remote_command = server + " " + quoted_server_args;

        int ret = execlp("ssh", "ssh", "-S", "none", "-o", proxy_arg.c_str(),
                         "-t", userhost.c_str(), "--", ssh_remote_command.c_str(),
                         (char *)NULL);
        if (ret == -1)
            die(str(format("Cannot exec ssh: %1%") % errno));
    }

    close(pty_slave);
    string ip, port, key;

    FILE *pty_file = fdopen(pty, "r");
    string line;
    while (buf = fgetln(pty_file, &n)) {
        line = string(buf, n);
        line = line.erase(line.find_last_not_of("\n"));
        if (regex_match(line, regex("^MOSH IP .*$"))) {
            smatch groups;
            regex_match(line, groups, regex("^MOSH IP (\\S+)\\s*$"));
            ip.assign(groups[1].first, groups[1].second);
        } else if (regex_match(line, regex("^MOSH CONNECT .*$"))) {
            smatch groups;
            regex_match(line, groups, regex("^MOSH CONNECT (\\d+?) ([A-Za-z0-9/+]{22})\\s*$"));
            port.assign(groups[1].first, groups[1].second);
            key.assign(groups[2].first, groups[2].second);
            break;
        } else
            printf("%s\n", line.c_str());
    }
    waitpid(pid, NULL, 0);

    if (!ip.size())
        die(str(format("%1%: Did not find remote IP address (is SSH ProxyCommand disabled?).") % argv[0]));

    if (!key.size() || !port.size())
        die(str(format("%1%: Did not find mosh server startup message.") % argv[0]));

    setenv("MOSH_KEY", key.c_str(), 1);
    setenv("MOSH_PREDICTION_DISPLAY", predict.c_str(), 1);
    execlp(client.c_str(), client.c_str(), ip.c_str(), port.c_str(), (char *)NULL);
}
