/* Rutas relativas a proposito.
 *
 * En produccion la UI y el API viven en el mismo proyecto de Vercel y comparten
 * dominio, asi que /api/login resuelve solo. En desarrollo, el proxy declarado en
 * vite.config.js reenvia /api al puerto de `vercel dev`, de modo que el navegador
 * siempre ve un unico origen.
 *
 * Resultado: ni CORS, ni una URL base quemada que haya que cambiar al desplegar. */

export class ErrorApi extends Error {
    constructor(mensaje, estado) {
        super(mensaje);
        this.name = 'ErrorApi';
        this.estado = estado;
    }
}

async function pedir(ruta, opciones) {
    let res;
    try {
        res = await fetch(ruta, opciones);
    } catch {
        // fetch solo rechaza por fallos de red: servidor caido, DNS, sin conexion.
        throw new ErrorApi('No se pudo contactar el servidor', 0);
    }

    /* Un cuerpo vacio o no-JSON no es excepcional: un 500 de la plataforma puede
     * devolver HTML. Se intenta parsear y, si no se puede, se sigue con null. */
    let datos = null;
    try {
        datos = await res.json();
    } catch {
        datos = null;
    }

    /* fetch NO rechaza por codigos de error: un 401 o un 500 resuelven normalmente.
     * Sin este chequeo, el llamador trataria una respuesta de error como exito. */
    if (!res.ok) {
        throw new ErrorApi(datos?.error ?? `El servidor respondió ${res.status}`, res.status);
    }

    return datos;
}

export function apiGet(ruta) {
    return pedir(ruta, { method: 'GET' });
}

export function apiPost(ruta, cuerpo) {
    return pedir(ruta, {
        method: 'POST',
        headers: { 'Content-Type': 'application/json' },
        body: JSON.stringify(cuerpo),
    });
}
