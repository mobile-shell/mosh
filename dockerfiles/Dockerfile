FROM alpine:latest
MAINTAINER Brett Randall <javabrett@gmail.com>

# NOTE: openssh is only required for testing connection to built docker using the built mosh-server
RUN apk update \
	&& apk add alpine-sdk autoconf automake protobuf-dev ncurses ncurses-dev ncurses-static openssl-dev openssh

RUN git clone https://github.com/mobile-shell/mosh.git \
	&& cd mosh \
	&& ./autogen.sh \
	&& LDFLAGS="-static" ./configure --prefix=/usr \
	&& make \
	&& make install \
	&& ssh-keygen -f /etc/ssh/ssh_host_rsa_key -N '' -t rsa \
	&& ssh-keygen -f /etc/ssh/ssh_host_dsa_key -N '' -t dsa \
	&& adduser -D mosh \
	&& echo 'mosh:m0sh-st@t1c' | chpasswd

EXPOSE 22

ENTRYPOINT /usr/sbin/sshd \
	&& sh
