# Zonal Cache. Se construye desde la RAIZ del repositorio, porque el Makefile
# y comun/ viven ahi:
#
#   docker build -f zonal_cache/Dockerfile -t zonal-cache .
#   docker run --rm -p 9777:9777 \
#       -e NODE_API_HOST=... -e EXTRA_HEADERS=$'x-config-key: ...\r\n' \
#       zonal-cache

FROM debian:bookworm-slim AS compilacion

# apt-get update y install en un solo RUN: separados, Docker podria reutilizar
# una capa de update vieja junto a un install nuevo y resolver contra indices
# desactualizados. El rm va en el mismo RUN porque borrar en una capa posterior
# no achica la imagen, solo tapa los archivos.
RUN apt-get update && apt-get install -y --no-install-recommends \
        build-essential \
        libssl-dev \
    && rm -rf /var/lib/apt/lists/*

WORKDIR /obra

# Solo lo que hace falta para compilar este ejecutable. Copiar el repositorio
# entero invalidaria esta capa ante cualquier cambio del UI o de los reportes.
COPY Makefile ./
COPY comun/ ./comun/
COPY zonal_cache/ ./zonal_cache/

RUN make zonal_cache

# ---------------------------------------------------------------------------

FROM debian:bookworm-slim

# libssl3    -> el binario enlaza dinamicamente contra libssl.so.3 y
#               libcrypto.so.3 (comprobable con ldd sobre bin/zonal_cache).
#
# ca-certificates -> http_cliente.c llama a SSL_CTX_set_default_verify_paths(),
#               que lee el almacen de certificados del sistema. Una imagen slim
#               no lo trae, y sin el FALLA TODA conexion HTTPS: el API de Vercel
#               y cualquier origen con tls: true. El sintoma es "unable to get
#               local issuer certificate", que parece culpa del servidor remoto
#               y no lo es.
RUN apt-get update && apt-get install -y --no-install-recommends \
        libssl3 \
        ca-certificates \
    && rm -rf /var/lib/apt/lists/*

COPY --from=compilacion /obra/bin/zonal_cache /usr/local/bin/zonal_cache

# Ruta absoluta: el valor por defecto en el codigo es "cache", relativo al
# directorio de trabajo, y eso ya causo errores de mkdir al cambiar de cwd.
ENV CACHE_DIR=/var/cache/zonal

# El chown va ANTES del USER. Si no, el primer mkdir de un dominio falla con
# permission denied y la cache no puede guardar nada.
RUN useradd --system --no-create-home --shell /usr/sbin/nologin zonal \
    && mkdir -p /var/cache/zonal \
    && chown zonal:zonal /var/cache/zonal

# En Kubernetes esto se monta como emptyDir o como PVC. Declararlo deja claro
# que es estado y no parte de la imagen.
VOLUME ["/var/cache/zonal"]

USER zonal

# 9777 es ZONAL_PUERTO_IN por defecto; esta sobre 1024, asi que no hace falta
# root ni la capacidad NET_BIND_SERVICE.
EXPOSE 9777

# Forma de arreglo: en forma de texto el PID 1 seria un shell, que no reenvia
# las senales, y kubectl delete pod esperaria los 30 segundos del plazo de
# gracia antes de matar el proceso de un SIGKILL.
ENTRYPOINT ["/usr/local/bin/zonal_cache"]
