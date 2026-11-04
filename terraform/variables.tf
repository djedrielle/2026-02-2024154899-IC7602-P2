variable "proyecto" {
  description = "Id del proyecto de GCP (el mismo de Firestore)"
  type        = string
}

variable "region_principal" {
  description = "Region del cluster de origenes y del registro de imagenes"
  type        = string
  default     = "us-central1"
}

variable "zona_origenes" {
  description = "Zona del cluster que corre interceptor, Apache y API de Java"
  type        = string
  default     = "us-central1-a"
}

# Las tres caches zonales. La clave es el sufijo del nombre; el valor, donde va.
#
# Se usa un mapa con for_each en vez de tres bloques repetidos porque los tres
# clusters son identicos salvo la ubicacion: asi toda la variacion queda a la
# vista en un solo lugar, y agregar una cuarta region es una linea.
variable "caches" {
  description = "Caches zonales: sufijo => zona y region"
  type = map(object({
    zona   = string
    region = string
  }))
  default = {
    cr = { zona = "us-central1-a", region = "us-central1" }
    eu = { zona = "europe-west1-b", region = "europe-west1" }
    br = { zona = "southamerica-east1-a", region = "southamerica-east1" }
  }
}

variable "tipo_maquina" {
  description = "Tipo de maquina de los nodos de las caches zonales"
  type        = string
  default     = "e2-small"
}

# El cluster de origenes necesita mas memoria que los de cache.
#
# Medido sobre un nodo e2-small real: de 2 GB de capacidad, GKE deja 1.36 GiB
# asignables, y sus propios complementos (fluentbit, kube-dns, metrics-server,
# gke-metrics-agent, filestore, pdcsi y demas) reservan ~1.2 GiB. Quedan unos
# 67 MiB libres.
#
# Apache y el interceptor caben ahi porque piden 32Mi cada uno. La JVM no: con
# 128Mi ya no entra, y el pod se queda en Pending con "Insufficient memory".
#
# e2-medium son 4 GB, que dejan cerca de 3 GiB asignables. Cuesta unos $12 mas
# al mes que e2-small, lo mismo que agregar un segundo e2-small, pero sin pagar
# dos veces los DaemonSets de sistema.
variable "tipo_maquina_origenes" {
  description = "Tipo de maquina del cluster de origenes, que corre la JVM"
  type        = string
  default     = "e2-medium"
}

variable "nodos_por_cluster" {
  description = "Nodos por cluster. Con 1 alcanza; 2 da margen si los pods de sistema aprietan"
  type        = number
  default     = 1
}

variable "disco_nodo_gb" {
  description = "Disco por nodo. El valor por omision de GKE son 100 GB, que para esto sobran y se facturan"
  type        = number
  default     = 30
}
