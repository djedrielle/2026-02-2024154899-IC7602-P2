import { leerSesion, borrarSesion } from '../sesion.js';

/* Rutas relativas a proposito.
 *
 * En produccion la UI y el API viven en el mismo proyecto de Vercel y comparten
 * dominio, asi que /api/login resuelve solo. En desarrollo con `vercel dev` pasa
 * lo mismo, porque vercel dev atiende ambas cosas en el 3000.
 *
 * Resultado: ni CORS, ni una URL base quemada que haya que cambiar al desplegar. */

export class ErrorApi extends Error {
    constructor(mensaje, estado) {
        super(mensaje);
        this.name = 'ErrorApi';
        this.estado = estado;
    }
}

async function pedir(ruta, opciones = {}) {
    const sesion = leerSesion();
    const cabeceras = { ...(opciones.headers ?? {}) };

    // El token identifica al usuario en los endpoints de administracion.
    if (sesion?.token) cabeceras['x-session'] = sesion.token;

    let res;
    try {
        res = await fetch(ruta, { ...opciones, headers: cabeceras });
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
        // Un 401 con sesion guardada significa que expiro o fue revocada.
        if (res.status === 401 && sesion) borrarSesion();
        throw new ErrorApi(datos?.error ?? `El servidor respondió ${res.status}`, res.status);
    }

    return datos;
}

const conCuerpo = (metodo) => (ruta, cuerpo) =>
    pedir(ruta, {
        method: metodo,
        headers: { 'Content-Type': 'application/json' },
        body: JSON.stringify(cuerpo ?? {}),
    });

export const apiGet = (ruta) => pedir(ruta, { method: 'GET' });
export const apiPost = conCuerpo('POST');
export const apiPut = conCuerpo('PUT');
export const apiDelete = (ruta) => pedir(ruta, { method: 'DELETE' });
