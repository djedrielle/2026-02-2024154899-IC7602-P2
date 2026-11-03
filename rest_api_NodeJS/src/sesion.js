const CLAVE = 'zonal-cache-sesion';

/* La sesion vive en localStorage para sobrevivir a recargas de la pagina.
 * Cada lectura va en try/catch: en modo privado o con el almacenamiento
 * bloqueado, acceder lanza en vez de devolver null. */
export function leerSesion() {
    try {
        const crudo = localStorage.getItem(CLAVE);
        if (!crudo) return null;
        const s = JSON.parse(crudo);

        // Una sesion vencida es como no tener sesion: se descarta sola.
        if (typeof s.expiracion === 'number' && s.expiracion * 1000 < Date.now()) {
            localStorage.removeItem(CLAVE);
            return null;
        }
        return s;
    } catch {
        return null;
    }
}

export function guardarSesion(sesion) {
    try {
        localStorage.setItem(CLAVE, JSON.stringify(sesion));
    } catch {
        // Sin almacenamiento la sesion dura lo que dure la pestana. No es fatal.
    }
}

export function borrarSesion() {
    try {
        localStorage.removeItem(CLAVE);
    } catch {
        /* nada que hacer */
    }
}
