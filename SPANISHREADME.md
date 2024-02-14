
[![ci](https://github.com/mobile-shell/mosh/actions/workflows/ci.yml/badge.svg)](https://github.com/mobile-shell/mosh/actions/workflows/ci.yml)

Mosh: la terminal móvil
=======================

Mosh es una aplicación de terminal remota que admite conectividad intermitente, permite el roaming, y proporciona eco local especulativo y edición de línea de las pulsaciones de teclas del usuario.

Su objetivo es admitir los usos interactivos típicos de SSH, además de:

   * Mosh mantiene la sesión activa si el cliente se va a dormir y
     se despierta más tarde, o pierde temporalmente su conexión a Internet.

   * Mosh permite que el cliente y el servidor "roamen" y cambien de IP
     manteniendo la conexión activa. A diferencia de SSH, Mosh
     puede usarse mientras se cambia entre redes Wi-Fi o de Wi-Fi
     a datos celulares o Ethernet cableado.

   * El cliente de Mosh ejecuta un modelo predictivo del comportamiento
     del servidor en segundo plano e intenta adivinar inteligentemente
     cómo afectará cada pulsación de tecla al estado de la pantalla.
     Cuando está seguro de sus predicciones, las mostrará al usuario
     mientras espera la confirmación del servidor. La mayoría de las
     pulsaciones de teclas y usos de las teclas de flecha izquierda
     y derecha pueden ser eco inmediatamente.

     Como resultado, Mosh es utilizable en enlaces de alta latencia,
     por ejemplo, en una conexión de datos celular o Wi-Fi intermitente.
     En contraste con intentos anteriores de modos de eco local en
     otros protocolos, Mosh funciona correctamente con aplicaciones
     de pantalla completa como emacs, vi, alpine, e irssi, y se
     recupera automáticamente de errores de predicción ocasionales
     dentro de un RTT. En enlaces de alta latencia, Mosh subraya
     sus predicciones mientras están pendientes y elimina el subrayado
     cuando son confirmadas por el servidor.

Mosh no admite el reenvío de X ni los usos no interactivos de SSH,
incluido el reenvío de puertos.

Otras características
--------------

   * Mosh ajusta su velocidad de fotogramas para no llenar las colas
     de red en enlaces lentos, por lo que "Control-C" siempre funciona
     dentro de un RTT para detener un proceso desbocado.

   * Mosh advierte al usuario cuando no ha recibido noticias del servidor
     durante un tiempo.

   * Mosh soporta enlaces con pérdida que pierden una fracción
     significativa de sus paquetes.

   * Mosh maneja algunos casos especiales de Unicode mejor que SSH
     y los emuladores de terminal existentes por sí solos, pero
     requiere un entorno UTF-8 para funcionar.

   * Mosh aprovecha SSH para establecer la conexión y autenticar
     usuarios. Mosh no contiene ningún código privilegiado (root).

Obtención de Mosh
------------

  [El sitio web de Mosh](https://mosh.org/#getting) tiene información sobre
  paquetes para muchos sistemas operativos, así como instrucciones para construir
  desde la fuente.

  Tenga en cuenta que `mosh-client` recibe una clave de sesión AES como
  variable de entorno. Si está portando Mosh a un nuevo sistema operativo,
  asegúrese de que las variables de entorno de un proceso en ejecución no
  sean legibles por otros usuarios. Hemos confirmado que este es el caso en
  GNU/Linux, OS X y FreeBSD.

Uso
-----

  El binario `mosh-client` debe existir en la máquina del usuario, y el
  binario `mosh-server` en el host remoto.

  El usuario ejecuta:

    $ mosh [usuario@]host

  Si los binarios `mosh-client` o `mosh-server` viven fuera de la
  `$PATH` del usuario, `mosh` acepta los argumentos `--client=PATH` y
  `--server=PATH` para seleccionar ubicaciones alternativas. Se documentan
  más opciones en la página del manual mosh(1).

  Hay [más ejemplos](https://mosh.org/#usage) y una
  [FAQ](https://mosh.org/#faq) en el sitio web de Mosh.

Cómo funciona
------------

  El programa `mosh` se conectará por SSH a `usuario@host` para establecer
  la conexión. SSH puede solicitar al usuario una contraseña o usar
  autenticación de clave pública para iniciar sesión.

  A partir de este punto, `mosh` ejecuta el proceso `mosh-server` (como el
  usuario) en la máquina del servidor. El proceso del servidor escucha en
  un puerto UDP alto y envía su número de puerto y una clave secreta AES-128
  de vuelta al cliente a través de SSH. La conexión SSH se cierra y la
  sesión de terminal comienza sobre UDP.

  Si el cliente cambia de direcciones IP, el servidor comenzará a enviar
  al cliente en la nueva dirección IP en unos pocos segundos.

  Para funcionar, Mosh requiere que se pasen datagramas UDP entre cliente
  y servidor. Por defecto, `mosh` usa un número de puerto entre 60000 y
  61000, pero el usuario puede seleccionar un puerto específico con la opción
  -p. Tenga en cuenta que la opción -p no tiene efecto en el puerto utilizado
  por SSH.

Consejos para distribuidores
----------------------

Una nota sobre las banderas del compilador: Mosh es código sensible a la
seguridad. Al hacer compilaciones automatizadas para un paquete binario,
recomendamos pasar la opción `--enable-compile-warnings=error` a `./configure`.
En GNU/Linux con `g++` o `clang++`, el paquete debe compilar limpiamente con
`-Werror`. Por favor, informe un error si no lo hace.

Cuando estén disponibles, Mosh se compila con una variedad de banderas de
endurecimiento binario como `-fstack-protector-all`, `-D_FORTIFY_SOURCE=2`, etc.
Estos proporcionan seguridad proactiva contra la posibilidad de un error de
corrupción de memoria en Mosh o una de las bibliotecas que utiliza. Para una
lista completa de banderas, busque `HARDEN` en `configure.ac`. El script
`configure` detecta qué banderas son compatibles con su compilador, y las
habilita automáticamente. Para desactivar esta detección, pase
`--disable-hardening` a `./configure

`. Por favor, informe un error si tiene
problemas con la configuración predeterminada; nos gustaría que la mayor
cantidad posible de usuarios estuviera ejecutando una configuración lo más
segura posible.

Mosh se envía con un ajuste de optimización predeterminado de `-O2`. Algunos
distribuidores han preguntado sobre cambiar esto a `-Os` (que hace que un
compilador prefiera las optimizaciones de espacio a las optimizaciones de
tiempo). Hemos probado con el programa `src/examples/benchmark` incluido para
esto. Los resultados son que `-O2` es un 40% más rápido que `-Os` con g++ 4.6
en GNU/Linux, y un 16% más rápido que `-Os` con clang++ 3.1 en Mac OS X. En
ambos casos, `-Os` produjo un binario más pequeño (hasta un 40%, ahorrando
casi 200 kilobytes en disco). Si bien Mosh no es especialmente intensivo en
CPU y en su mayoría está inactivo cuando el usuario no está escribiendo,
creemos que los resultados sugieren que `-O2` (el predeterminado) es
preferible.

Nuestros paquetes de Debian y Fedora presentan Mosh como un paquete único.
Mosh tiene una dependencia de Perl que solo es necesaria para el uso del
cliente. Para algunas plataformas, puede tener sentido tener paquetes
separados de mosh-server y mosh-client para permitir el uso de mosh-server
sin Perl.

Notas para desarrolladores
--------------------------
