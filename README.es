	Sylpheed - cliente de correo electrónico ligero y amigable

   Copyright(C) 1999-2006 Hiroyuki Yamamoto <hiro-y@kcn.ne.jp>

   Este programa es software libre; puede redistribuirlo y/o modificarlo 
   bajo los términos de la GNU General Public License publicada por la 
   Free Software Foundation; tanto la versión 2, como (opcionalmente) 
   cualquier versión posterior.

   Este programa se distribuye con la esperanza de que sea útil, pero 
   SIN NINGUNA GARANTÍA; ni siquiera la garantía implícita de 
   COMERCIALIDAD o ADECUACIÓN PARA ALGÚN PROPÓSITO PARTICULAR. Vea la 
   GNU General Public License para más detalles.

   Usted debería haber recibido una copia de la GNU General Public License 
   junto con este programa; en caso contrario, escriba a la Free Software 
   Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA  02110-1301  USA
   
   Para más detalles vea el fichero COPYING.

Qué es Sylpheed
===============

Sylpheed es un cliente de correo electrónico basado en la librería gráfica
GTK+. Corre bajo el X Window System y también en Microsoft Windows.

Sylpheed es un software libre distribuido bajo la GPL de GNU.

Sylpheed tiene las siguientes características:

    * Interfaz de usuario simple, elegante, y pulido
    * Manejo confortable construido al detalle
    * Disponibilidad inmediata con mínima configuración
    * Funcionamiento ligero
    * Alta fiabilidad
    * Soporte de internacionalización y múltiples idiomas
    * Alto nivel de procesamiento del Japonés
    * Soporte de varios protocolos
    * Gran capacidad de filtrado y búsquedas 
    * Control del correo basura
    * Cooperación flexible con programas externos

La apariencia e interfaz son similares a algunos clientes de correo populares
para Windows, como Outlook Express o Becky!. Muchas órdenes son accesibles
con el teclado, como en los clientes Mew y Wanderlust basados en Emacs.
Por ello podrá ser capaz de migrar a Sylpheed con relativa comodidad en caso 
de que estuviera acostumbrado a otros clientes.

Los mensajes se gestionan en formato MH, y podrá usarlos junto con otros
clientes basados en el formato MH (tal como Mew). Tiene menos posibilidades
de perder correos ante falles ya que cada fichero se corresponde a un correo.
Puede importa o exportar mensajes en formato mbox. También puede utilizar
fetchmail y/o procmail, y programas externos para recibir (como inc o imget).

Características principales implementadas actualmente
=====================================================

Protocolos soportados

	o POP3
	o IMAP4rev1
	o SMTP
	o NNTP
	o SSL/TLSv1 (POP3, SMTP, IMAP4rev1, NNTP)
	o IPv6

Características

	o múltiples cuentas
	o filtrado de gran capacidad
	o búsquedas (petición de búsqueda, búsqueda rápida, carpeta de búsqueda)
	o control del correo no deseado (correo basura)
	o vista jerárquica
	o presentación y transferencia de adjuntos por MIME
	o vista de imágenes incrustadas
	o lector de noticias de internet (news)
	o soporte de SMTP AUTH (PLAIN / LOGIN / CRAM-MD5)
	o autentificación CRAM-MD5 (SMTP AUTH / IMAP4rev1)
	o autentificación APOP (POP3)
	o firmas y cifrado PGP (necesita GPGME)
	o comprobación ortográfica (necesita GtkSpell)
	o vista de X-Face
	o cabeceras definidas por el usuario
	o etiquetas de marca y color
	o atajos de teclado compatibles con Mew/Wanderlust
	o soporte de múltiples carpetas MH
	o exportación/importación de mbox
	o acciones para trabajar con programas externos
	o editor externo
	o almacenamiento de mensajes en cola
	o comprobación automática de correo
	o borradores de mensaje
	o plantillas
	o recorte de líneas
	o auto-guardado
	o URI en las que se puede hacer clic
	o libro de direcciones
	o gestión de mensajes nuevos y no leídos
	o impresión
	o modo sin conexión
	o control remoto a través de la línea de órdenes
	o configuración por cada carpeta
	o soporte de LDAP, vCard, y JPilot
	o arrastrar y soltar
	o soporte de autoconf y automake
	o internacionalización de mensajes con gettext
	o soporte de m17n (múltiples idiomas)

y más.

Instalación
===========

Vea INSTALL para las instrucciones de instalación.

Uso
===

Preparación antes de la ejecución
---------------------------------

Si esta usando una codificación de caracteres distinta de UTF-8 para
los nombres de fichero, debe establecer la variable de entorno siguiente
(no funcionará si no se especifica):

(usar la codificación específica de la localización)
% export G_FILENAME_ENCODING=@locale

o

(especificación manual de la codificación)
% export G_FILENAME_ENCODING=ISO-8859-1

Si quiere que se muestren los mensajes traducidos en su idioma,
debe especificar algunas variables de entorno relativas a la localización.
Por ejemplo:

% export LANG=de_DE

(sustituir de_DE con el nombre de la localización actual)

Si no quiere mensajes traducidos, establezca LC_MESSAGES a "C"
(y no establezca LC_ALL si esta especificada). 

Cómo ejecutar
-------------

Escriba «sylpheed» en la línea de órdenes, o haga doble clic en el icono
en un gestor de ficheros para ejecutar.

Arranque inicial
----------------

Cuando se ejecuta Sylpheed por primera vez crea automáticamente los ficheros
de configuración bajo ~/.sylpheed-2.0/, y le pregunta la ubicación del buzón.
Por omisión es ~/Mail. Si existe algún fichero en el directorio que no se
corresponda al formato MH tendrá que especificar otra ubicación.

Si no existe ~/.sylpheed-2.0/ pero la configuración de una versión anterior
existe en ~/.sylpheed/, se realizará la migración automáticamente después de
la confirmación.

Si la codificación de la localización no es UTF-8 y la variable de entorno
G_FILENAME_ENCODING no está establecida se mostrará una ventana de aviso.

Configuración necesaria
-----------------------

Inicialmente deberá crear al menos una cuenta para enviar o recibir mensajes
(puede leer los mensajes ya existentes sin crear ninguna cuenta). El diálogo
de configuración se mostrará al hacer clic en el menú «Configuración -> Crear
nueva cuenta...» o «Cuenta» en la barra de herramientas. Después se rellene
los valores necesarios.

Vea el manual proporcionado con este programa para el uso general.

Configuraciones ocultas
-----------------------

Se pueden configurar la mayoría de las características de Sylpheed a través
de la ventana de preferencias, pero hay algunos parámetros que carecen de
interfaz de usuario (no tiene que modificarlos para el uso normal). Debe
editar el fichero ~/.sylpheed-2.0/sylpheedrc con un editor de texto cuando
Sylpheed no se este ejecutando para cambiarlos.

allow_jisx0201_kana		permite JIS X 0201 Kana (kana de media anchura)
                                al enviar
                                0: desactivado 1: activado   [por omisión: 0]
translate_header		traducir cabeceras como «Desde:», «Para:» y
                                «Asunto:».
                                0: desactivado 1: activado   [por omisión: 1]
enable_rules_hint		habilita colores de fila alternativos en la
                                vista resumen
                                0: desactivado 1: activado   [por omisión: 1]
bold_unread			muestra en la vista resumen los mensajes no 
                                leídos con una tipografía en negrita
                                0: desactivado 1: activado   [por omisión: 1]
textview_cursor_visible		mostrar el cursor en la vista de texto
                                0: desactivado 1: activado   [por omisión: 0]
logwindow_line_limit		especificar el número de líneas máximo en la
                                ventana de traza
				0: ilimitado  n (> 0): n líneas 
				[por omisión: 1000]

Al contrario que la 1.0.x, esta versión no permite por omisión la modificación
directa de los atajos de menú. Puede usar alguno de los métodos siguientes
para ello:

1. Usando GNOME 2.8 o posterior
   Ejecute gconf-editor («Aplicaciones - Herramientas del sistema - Editor de
   configuración».
   Seleccione «desktop - gnome - interface» y marque «can-change-accels» en él.

2. Usando versiones anteriores a GNOME 2.8 u otros entornos
   Añada (o cree una nueva) gtk-can-change-accels = 1 al fichero ~/.gtkrc-2.0

3. Cuando Sylpheed no este ejecutándose, edite directamente el fichero 
   ~/.sylpheed-2.0/menurc usando un editor de texto.

Información
===========

Puede comprobar la versión más reciente e información sobre Sylpheed en:

	http://sylpheed.sraoss.jp/

Existe también un manual de Sylpheed escrito por
Yoichi Imai <yoichi@silver-forest.com> en:

	http://y-imai.good-day.net/sylpheed/

Infórmenos
==========

Comentarios, ideas y (la mayoría de) informes de errores (y especialmente 
parches) son muy bienvenidos.

Subversion
==========

Puede obtener el código fuente más reciente del repositorio Subversion.

Vaya a un directorio apropiado y con el comando:

	svn checkout svn://sylpheed.sraoss.jp/sylpheed/trunk

se creará el árbol de las fuentes con nombre «sylpheed» bajo el directorio
actual.

El subdirectorio de sylpheed está dividido como sigue:

    * trunk/     Árbol principal
    * branches/  Ramas experimentales varias
    * tags/      Ramas etiquetadas de las versiones liberadas

Para actualizarse a los cambios más recientes, ejecute la orden:

	svn update

en el directorio correspondiente.

-- 
Hiroyuki Yamamoto <hiro-y@kcn.ne.jp>
