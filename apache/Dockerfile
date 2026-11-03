# Servidor Apache de origen. El enunciado pide que sirva "una pagina HTML
# basica o un sitio web basico"; sitio/ trae ademas una hoja de estilo, un
# script, una imagen y un JSON, para poder demostrar que la regla de URL aplica
# un TTL y una politica de reemplazo distintos segun el tipo de archivo.
#
#   docker build -f apache/Dockerfile -t apache-origen .
#   docker run --rm -p 8081:80 apache-origen
#
# A diferencia de los otros tres, este no necesita el contexto en la raiz, pero
# se construye igual desde ahi para que los cuatro comandos sean uniformes.

FROM httpd:2.4

# httpd:2.4 sirve lo que encuentre en este directorio. Sustituye la pagina
# "It works!" de la imagen base.
COPY apache/sitio/ /usr/local/apache2/htdocs/

# La imagen oficial ya arranca httpd en primer plano como PID 1 y expone el 80,
# asi que no hace falta ni ENTRYPOINT ni EXPOSE.
#
# Tampoco se cambia de usuario: httpd arranca como root a proposito, para poder
# abrir el puerto 80, y deja a los procesos trabajadores corriendo como el
# usuario "daemon". Forzar USER aqui romperia ese arranque.
