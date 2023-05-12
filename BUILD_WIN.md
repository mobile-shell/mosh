# Build Windows

## Steps


1. Download and install msys2 following the [instruction](https://www.msys2.org/#installation).
2. Install dependencies.
    ```
    pacman -S mingw-w64-ucrt-x86_64-gcc
    pacman -S mingw-w64-ucrt-x86_64-protobuf
    pacman -S mingw-w64-ucrt-x86_64-ncurses
    pacman -S mingw-w64-ucrt-x86_64-crypto++
    pacman -S mingw-w64-ucrt-x86_64-openssl
    pacman -S mingw-w64-ucrt-x86_64-autotools
    ```
3. Clone the repo and checkout the target branch.
4. Run `./autogen.sh`.
5. Run `./configure --enable-static-libraries --disable-server  --disable-examples`.
6. Open `src/include/config.h`and find the following macro and make them defined.
    ```
    /* Define to 1 if you have the `cfmakeraw' function. */
    #define HAVE_CFMAKERAW
    
    /* Define if you have forkpty(). */
    #define HAVE_FORKPTY
    ```
7. Open `src/frontend/Makefile` and find the following libs definition and add libraries `-lws2_32 -lncursew`.
    ```
    LIBS = -Wl,-Bstatic -lz -Wl,-Bdynamic -lws2_32 -lncursesw
    ```
8. Open `Makefile` and find var`SUBDIRS`and remove unnecessary directories.
    ```
    SUBDIRS = scripts src man conf
    👇
    SUBDIRS = src
    ```
9. Run `make` and build successfully.