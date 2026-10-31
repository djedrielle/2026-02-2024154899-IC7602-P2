import { validar_sesion } from '../../repos/sesiones_repo.js';
import { peticion_interna_autorizada } from '../../lib/auth_interna.js';

/* Lo consulta la cache zonal para verificar la sesion que viene en una peticion.
 * Es de consumo interno, asi que exige el mismo secreto compartido que /api/config.
 *
 * El dominio llega como query (?dominio=midominio.com) para que la validacion
 * confirme que esa sesion cubre ese dominio en particular. */
export default async function handler(req, res) {
    if (req.method !== 'GET') {
        return res.status(405).json({ error: 'Método no permitido' });
    }

    if (!peticion_interna_autorizada(req)) {
        return res.status(401).json({ error: 'No autorizado' });
    }

    const { token, dominio } = req.query;

    if (typeof token !== 'string' || token.trim() === '') {
        return res.status(400).json({ valida: false, motivo: 'Falta el token' });
    }

    try {
        const r = await validar_sesion(token, typeof dominio === 'string' ? dominio : null);

        /* 401 y no 200 con valida:false, para que el lado C pueda decidir por el
         * codigo de estado sin parsear el cuerpo. */
        if (!r.valida) {
            return res.status(401).json({ valida: false, motivo: r.motivo });
        }

        res.status(200).json({ valida: true, usuario: r.usuario, expiracion: r.expiracion });
    } catch (e) {
        console.error('error validando sesion:', e);
        res.status(500).json({ error: 'Error interno' });
    }
}
