# Charts de Helm — Proyecto 2 IC-7602

Un chart por componente. Se repite estructura entre ellos a proposito: cada
chart se lee solo, sin saltar a helpers compartidos.

## Reparto en clusters

Cuatro clusters, todos **zonales** (el credito gratuito de GKE solo cubre
clusters zonales, y un cluster regional replica los nodos en tres zonas,
triplicando el costo sin avisar).

| Cluster | Zona | Charts | TLS |
|---|---|---|---|
| `origenes` | `us-central1-a` | `interceptor`, `apache`, `rest-api-java` | — |
| `cache-cr` | `us-central1-a` | `zonal-cache` con `valores-cr.yaml` | no, puerto 80 |
| `cache-eu` | `europe-west1-b` | `zonal-cache` con `valores-eu.yaml` | **si, puerto 443** |
| `cache-br` | `southamerica-east1-a` | `zonal-cache` con `valores-br.yaml` | no, puerto 80 |

La terminacion TLS se demuestra con la cache de Europa. Las otras dos sirven
HTTP plano para que la cadena completa (DNS -> cache -> origen) se pueda
recorrer con un curl corriente: desde Costa Rica el interceptor resuelve hacia
la cache de CR, que es la que conviene tener sin TLS.

El Secret `zonal-cache-tls` solo hace falta en `cache-eu`.

## Por que los origenes son LoadBalancer

Apache y el API de Java son origenes internos y lo natural seria `ClusterIP`.
No se puede: **un `ClusterIP` solo es enrutable dentro de su propio cluster**, y
las tres caches corren en otros. El campo `origen_host` de cada dominio apunta
entonces a la **IP publica** de esos Services, no al nombre corto.

Quedan expuestos a internet. Para que solo las caches puedan alcanzarlos:

```bash
gcloud compute firewall-rules create origenes-solo-caches \
  --allow=tcp:80,tcp:7070 \
  --source-ranges=IP_CACHE_CR,IP_CACHE_EU,IP_CACHE_BR \
  --target-tags=origen
```

## Orden de instalacion

### 1. Cluster de origenes

```bash
gcloud container clusters get-credentials origenes --zone=us-central1-a

helm install apache ./helm/apache
helm install java   ./helm/rest-api-java
helm install dns    ./helm/interceptor
```

Anotar las tres IPs:

```bash
kubectl get svc -o custom-columns=NOMBRE:.metadata.name,IP:.status.loadBalancer.ingress[0].ip
```

La del interceptor va en el registro NS del dominio en FreeDNS. Las otras dos,
en el campo `origen_host` de los dominios configurados desde el UI.

### 2. Cada cluster de cache

El Secret con el secreto compartido va **antes** del chart. El `printf`
canalizado no es capricho: `$(...)` en bash elimina los saltos de linea
finales, y sin el CRLF la peticion al API sale mal formada y la cache recibe
una respuesta vacia.

```bash
gcloud container clusters get-credentials cache-cr --zone=us-central1-a

printf 'x-config-key: %s\r\n' "$CONFIG_API_KEY" \
  | kubectl create secret generic zonal-cache-config \
      --from-file=extra-headers=/dev/stdin

helm install cache-cr ./helm/zonal-cache -f helm/zonal-cache/valores-cr.yaml
```

Y lo mismo para `cache-eu` y `cache-br`.

### 3. Firestore

Cargar las tres IPs de las caches en la coleccion `zonal_ip`, bajo el codigo de
pais de cada region. **Mientras no se haga, el interceptor sigue devolviendo la
IP anterior** y nada del despliegue se nota desde el cliente.

## Desarrollo sin cluster

`helm template` renderiza e imprime sin instalar nada ni necesitar un cluster.
Es el ciclo de trabajo para editar los charts:

```bash
helm template prueba ./helm/zonal-cache -f helm/zonal-cache/valores-cr.yaml
helm lint ./helm/zonal-cache
```

## Lo que NO esta en los values

`values.yaml` se commitea, asi que no lleva secretos:

| Secret | Lo usa | Se crea con |
|---|---|---|
| `zonal-cache-config` | `zonal-cache` | el `printf` de arriba |
| `zonal-cache-tls` | `zonal-cache`, solo si `tls.habilitado` | `kubectl create secret tls` |

Los `PROYECTO` de los `values.yaml` son marcadores: sustituir por el id real del
proyecto de GCP.
