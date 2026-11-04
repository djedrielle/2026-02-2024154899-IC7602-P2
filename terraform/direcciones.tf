# IPs publicas estaticas.
#
# Una direccion de GCP es REGIONAL y tiene que estar en la misma region que el
# balanceador que la use, de ahi que cada cache lleve la suya en su region.
#
# Son estaticas y no efimeras porque cada una esta anotada en algun lado:
#
#   interceptor -> el registro NS del dominio en FreeDNS
#   caches      -> la coleccion zonal_ip de Firestore
#   origenes    -> el campo origen_host de cada dominio, desde el UI
#
# Con IPs efimeras habria que reescribir esas tres cosas en cada despliegue.

resource "google_compute_address" "interceptor" {
  name   = "interceptor"
  region = var.region_principal

  depends_on = [google_project_service.apis]
}

resource "google_compute_address" "apache" {
  name   = "apache-origen"
  region = var.region_principal

  depends_on = [google_project_service.apis]
}

resource "google_compute_address" "rest_api_java" {
  name   = "rest-api-java"
  region = var.region_principal

  depends_on = [google_project_service.apis]
}

resource "google_compute_address" "cache" {
  for_each = var.caches

  name   = "cache-${each.key}"
  region = each.value.region

  depends_on = [google_project_service.apis]
}

# ---------------------------------------------------------------------------
# Firewall: los origenes solo hablan con las caches
# ---------------------------------------------------------------------------

# Apache y el API de Java tienen que ser LoadBalancer porque las caches viven
# en otros clusters y un ClusterIP no es enrutable fuera del suyo. Eso los deja
# expuestos a internet, y entonces se podria pedirles contenido salteandose la
# cache, que es justo lo que la demostracion quiere descartar.
#
# Esta regla cierra ese hueco: solo las tres IPs de las caches entran.
resource "google_compute_firewall" "origenes_solo_caches" {
  name    = "origenes-solo-caches"
  network = "default"

  allow {
    protocol = "tcp"
    ports    = ["80", "7070"]
  }

  source_ranges = [for c in google_compute_address.cache : "${c.address}/32"]
  target_tags   = ["origen"]

  depends_on = [google_project_service.apis]
}
