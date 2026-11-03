# DNS Interceptor. Igual que el Zonal Cache, se construye desde la RAIZ:
#
#   docker build -f interceptor/Dockerfile -t dns-interceptor .
#   docker run --rm -p 5300:5000/udp -e NODE_API_HOST=... dns-interceptor
#   dig @127.0.0.1 -p 5300 ejemplo.com

FROM debian:bookworm-slim AS compilacion

RUN apt-get update && apt-get install -y --no-install-recommends \
        build-essential \
        libssl-dev \
    && rm -rf /var/lib/apt/lists/*

WORKDIR /obra

COPY Makefile ./
COPY comun/ ./comun/
COPY interceptor/ ./interceptor/

RUN make interceptor

# ---------------------------------------------------------------------------

FROM debian:bookworm-slim

# Las mismas dos razones que en el Zonal Cache: el binario enlaza contra
# libssl.so.3, y comparte comun/http_cliente.c, que verifica certificados
# contra el almacen del sistema al consultar el API por HTTPS.
RUN apt-get update && apt-get install -y --no-install-recommends \
        libssl3 \
        ca-certificates \
    && rm -rf /var/lib/apt/lists/*

COPY --from=compilacion /obra/bin/interceptor /usr/local/bin/interceptor

RUN useradd --system --no-create-home --shell /usr/sbin/nologin dnsint
USER dnsint

# El interceptor escucha en DNS_INTERCEPTOR_PORT, 5000 por defecto. Al estar
# sobre 1024 no necesita root ni NET_BIND_SERVICE, que es el dolor de cabeza
# habitual de un servidor DNS.
#
# El 53 externo lo publica el Service de Kubernetes apuntando a targetPort 5000;
# la traduccion la hace Kubernetes y el contenedor sigue sin privilegios.
#
# El sufijo /udp es obligatorio: sin el, EXPOSE asume TCP y aqui no sirve.
EXPOSE 5000/udp

ENTRYPOINT ["/usr/local/bin/interceptor"]
