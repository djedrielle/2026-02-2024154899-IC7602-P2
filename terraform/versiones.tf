terraform {
  required_version = ">= 1.5"

  required_providers {
    google = {
      source = "hashicorp/google"
      # Se fija el rango mayor a proposito: entre versiones mayores del
      # proveedor cambian valores por omision (deletion_protection aparecio
      # asi) y un plan que funcionaba deja de funcionar sin que nada en este
      # codigo haya cambiado.
      version = "~> 6.0"
    }
  }

  # Para trabajar desde una sola maquina, el estado local alcanza.
  #
  # Si en algun momento se trabaja desde dos equipos, descomentar esto y crear
  # antes el bucket. El estado es la memoria de Terraform: sin el no sabe que
  # los clusters son suyos, y perderlo deja recursos existiendo y facturando
  # sin forma comoda de borrarlos.
  #
  # backend "gcs" {
  #   bucket = "zonal-cache-tfstate"
  #   prefix = "redes-p2"
  # }
}

provider "google" {
  project = var.proyecto
  region  = var.region_principal
}
