# REST API en Java (Javalin). Uno de los origenes con los que el enunciado pide
# probar la cache zonal.
#
#   docker build -f rest_api_Java/Dockerfile -t rest-api-java .
#   docker run --rm -p 7070:7070 rest-api-java

FROM maven:3.9-eclipse-temurin-17 AS compilacion

WORKDIR /obra

# El pom.xml va solo y primero: mientras no cambien las dependencias, Docker
# reutiliza esta capa y no vuelve a descargar nada. Si se copiara junto con
# src/, cada cambio del codigo dispararia la descarga completa otra vez.
#
# Se nombra el plugin con sus coordenadas completas en vez del prefijo corto
# "dependency:go-offline". Resolver el prefijo obliga a Maven a consultar los
# metadatos de todos los grupos de plugins, y eso falla en un contenedor con
# el repositorio local vacio.
COPY rest_api_Java/pom.xml ./
RUN mvn -B org.apache.maven.plugins:maven-dependency-plugin:3.8.1:go-offline

COPY rest_api_Java/src ./src
RUN mvn -B package

# ---------------------------------------------------------------------------

# La variante jre no trae compilador y pesa cerca de la mitad que la jdk.
FROM eclipse-temurin:17-jre

# Usuario sin privilegios. El puerto por defecto es 7070, sobre 1024, asi que
# no hace falta root para escuchar.
RUN useradd --system --create-home --shell /usr/sbin/nologin japi
USER japi

WORKDIR /app

# El shade-plugin dejo las dependencias dentro del JAR, asi que esto es lo
# unico que viaja de la etapa anterior.
COPY --from=compilacion /obra/target/rest-api-java.jar ./rest-api-java.jar

ENV PUERTO=7070
EXPOSE 7070

# -XX:MaxRAMPercentage hace que la JVM calcule la heap sobre la memoria QUE EL
# CONTENEDOR TIENE PERMITIDA, no sobre la de la maquina. Sin esto, en un nodo
# e2-small la JVM pide mas de lo que el limite del pod admite y el contenedor
# muere por OOMKilled sin dejar ni una linea en la bitacora de la aplicacion.
ENTRYPOINT ["java", "-XX:MaxRAMPercentage=60", "-jar", "/app/rest-api-java.jar"]
