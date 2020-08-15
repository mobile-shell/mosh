FROM alpine:3.12 AS build-env

WORKDIR /app
COPY . .
RUN apk update && apk --no-cache add autoconf automake libtool git \
    build-base gcc abuild binutils cmake linux-headers \
    perl perl-doc protoc protobuf-dev ncurses-dev openssl-dev libutempter zlib-dev ;\
    ./autogen.sh ;\
    ./configure ;\
    make && make install

# final stage
FROM alpine:3.12
COPY --from=build-env /usr/local /usr/local
RUN apk update && apk --no-cache add perl perl-doc


CMD ["/usr/local/bin/mosh"]
