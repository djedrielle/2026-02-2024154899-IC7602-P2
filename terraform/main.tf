# APIs del proyecto y registro de imagenes.
#
# Los clusters estan en clusters.tf y las IPs publicas en direcciones.tf.
# Terraform lee todos los .tf del directorio como si fueran uno solo y deduce
# el orden a partir de las referencias entre recursos; la division en archivos
# es solo para leerlos.

# ---------------------------------------------------------------------------
# APIs
# ---------------------------------------------------------------------------

# Sin habilitarlas, todo lo demas falla con errores de permisos que no dicen
# que el problema es este.
resource "google_project_service" "apis" {
  for_each = toset([
    "container.googleapis.com",
    "artifactregistry.googleapis.com",
    "compute.googleapis.com",
  ])

  service = each.value

  # CRITICO: este proyecto es el mismo de Firestore, que alimenta al API de
  # Node. Con el valor por omision (true), un terraform destroy deshabilitaria
  # estas APIs y se llevaria por delante servicios que nada tienen que ver con
  # este despliegue.
  disable_on_destroy = false
}

# ---------------------------------------------------------------------------
# Registro de imagenes
# ---------------------------------------------------------------------------

# Un solo repositorio para las cuatro imagenes, en la region principal.
#
# Los clusters de Europa y Suramerica lo leen desde lejos. Eso cuesta trafico de
# salida entre regiones, pero son cuatro imagenes que se descargan una vez por
# nodo: es mucho mas barato que mantener tres repositorios sincronizados.
resource "google_artifact_registry_repository" "redes" {
  location      = var.region_principal
  repository_id = "redes"
  format        = "DOCKER"
  description   = "Imagenes del Proyecto 2 de IC-7602"

  depends_on = [google_project_service.apis]
}
