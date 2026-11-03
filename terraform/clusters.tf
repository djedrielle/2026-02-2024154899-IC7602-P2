# Cuatro clusters, todos ZONALES.
#
# Que "location" sea una zona (us-central1-a) y no una region (us-central1) es
# la decision que mas plata mueve:
#
#   - Un cluster regional replica el grupo de nodos en las tres zonas de la
#     region. Se pide un nodo y se facturan tres.
#   - El credito gratuito de GKE ($74.40/mes, justo una tarifa de
#     administracion) solo aplica a clusters zonales y Autopilot.
#
# La tarifa de administracion es $0.10/hora por cluster. El credito cubre uno;
# los otros tres se pagan. De ahi la disciplina de destroy entre sesiones.

# ---------------------------------------------------------------------------
# Cluster de origenes: interceptor + Apache + API de Java
# ---------------------------------------------------------------------------

resource "google_container_cluster" "origenes" {
  name     = "origenes"
  location = var.zona_origenes

  # El patron estandar: GKE obliga a crear un grupo de nodos con el cluster,
  # pero ese grupo no se puede configurar del todo. Se crea y se borra, y el
  # bueno se define aparte como google_container_node_pool.
  remove_default_node_pool = true
  initial_node_count       = 1

  # Por omision el proveedor lo pone en true y entonces terraform destroy
  # FALLA. Con la estrategia de crear y destruir alrededor de cada sesion de
  # pruebas, eso dejaria los clusters prendidos facturando.
  deletion_protection = false

  depends_on = [google_project_service.apis]
}

resource "google_container_node_pool" "origenes" {
  name     = "origenes-nodos"
  cluster  = google_container_cluster.origenes.id
  location = var.zona_origenes

  node_count = var.nodos_por_cluster

  node_config {
    machine_type = var.tipo_maquina
    disk_size_gb = var.disco_nodo_gb
    disk_type    = "pd-standard"

    # Necesario para que el kubelet pueda descargar del Artifact Registry.
    oauth_scopes = ["https://www.googleapis.com/auth/cloud-platform"]

    # La etiqueta de red que usa la regla de firewall que limita el acceso a
    # Apache y al API de Java a las tres IPs de las caches.
    tags = ["origen"]
  }
}

# ---------------------------------------------------------------------------
# Tres clusters de cache, uno por region
# ---------------------------------------------------------------------------

resource "google_container_cluster" "cache" {
  for_each = var.caches

  name     = "cache-${each.key}"
  location = each.value.zona

  remove_default_node_pool = true
  initial_node_count       = 1
  deletion_protection      = false

  depends_on = [google_project_service.apis]
}

resource "google_container_node_pool" "cache" {
  for_each = var.caches

  name     = "cache-${each.key}-nodos"
  cluster  = google_container_cluster.cache[each.key].id
  location = each.value.zona

  node_count = var.nodos_por_cluster

  node_config {
    machine_type = var.tipo_maquina
    disk_size_gb = var.disco_nodo_gb
    disk_type    = "pd-standard"
    oauth_scopes = ["https://www.googleapis.com/auth/cloud-platform"]
    tags         = ["cache"]
  }
}
