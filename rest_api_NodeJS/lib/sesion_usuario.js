import { validar_sesion } from '../repos/sesiones_repo.js';

/* Autentica una peticion del panel de administracion.
 *
 * Es distinto de peticion_interna_autorizada: aquella usa un secreto compartido
 * y la consume el Zonal Cache; esta usa la sesion del usuario que inicio sesion
 * en el UI, y decide a que datos puede llegar esa persona.
 *
 * Devuelve el correo del usuario, o null si no hay sesion valida. */
export async function usuario_de_la_sesion(req) {
    const token = req.headers['x-session'];
    if (typeof token !== 'string' || token.trim() === '') return null;

    // Sin dominio: estas rutas son de administracion, no de un dominio puntual.
    const r = await validar_sesion(token.trim(), null);
    return r.valida ? r.usuario : null;
}

/* Envoltorio para los handlers que exigen sesion. Evita repetir el 401 en cada
 * endpoint y garantiza que ninguno se olvide de verificar. */
export function con_sesion(handler) {
    return async function (req, res) {
        const usuario = await usuario_de_la_sesion(req);
        if (usuario === null) {
            return res.status(401).json({ error: 'Sesión inválida o expirada' });
        }
        return handler(req, res, usuario);
    };
}
