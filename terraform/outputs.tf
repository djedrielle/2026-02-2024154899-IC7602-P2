# Lo que hay que copiar a otros lados despues del apply.

output "registro" {
  description = "Prefijo de las imagenes; va en imagen.repositorio de los values de Helm"
  value       = "${var.region_principal}-docker.pkg.dev/${var.proyecto}/${google_artifact_registry_repository.redes.repository_id}"
}

output "ip_interceptor" {
  description = "Va en el registro NS del dominio en FreeDNS"
  value       = google_compute_address.interceptor.address
}

output "ip_apache" {
  description = "Va en origen_host de los dominios servidos por Apache"
  value       = google_compute_address.apache.address
}

output "ip_rest_api_java" {
  description = "Va en origen_host de los dominios servidos por el API de Java"
  value       = google_compute_address.rest_api_java.address
}

output "ips_caches" {
  description = "Van en la coleccion zonal_ip de Firestore, bajo el pais de cada region"
  value       = { for k, v in google_compute_address.cache : k => v.address }
}

# Los comandos exactos, ya rellenos, para no tener que recordar las zonas.
output "credenciales" {
  description = "Comandos para apuntar kubectl a cada cluster"
  value = concat(
    ["gcloud container clusters get-credentials origenes --zone=${var.zona_origenes} --project=${var.proyecto}"],
    [for k, v in var.caches :
      "gcloud container clusters get-credentials cache-${k} --zone=${v.zona} --project=${var.proyecto}"
    ]
  )
}

output "subir_imagenes" {
  description = "Autenticar Docker contra el registro antes del primer push"
  value       = "gcloud auth configure-docker ${var.region_principal}-docker.pkg.dev"
}
