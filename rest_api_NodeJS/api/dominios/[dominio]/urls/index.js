import { con_sesion } from '../../../../lib/sesion_usuario.js';
import { listar_urls, crear_url, TIPOS_ARCHIVO } from '../../../../repos/urls_repo.js';

/* GET  -> reglas de URL del dominio, de mas a menos especifica
 * POST -> crea una regla nueva */
export default con_sesion(async function handler(req, res, usuario) {
    const { dominio } = req.query;

    try {
        if (req.method === 'GET') {
            const urls = await listar_urls(usuario, dominio);
            if (urls === null) return res.status(404).json({ error: 'Dominio no encontrado' });
            return res.status(200).json({ urls, tipos_archivo: TIPOS_ARCHIVO });
        }

        if (req.method === 'POST') {
            const r = await crear_url(usuario, dominio, req.body ?? {});
            if (!r.creada) return res.status(400).json({ error: r.motivo });
            return res.status(201).json({ id: r.id });
        }

        res.status(405).json({ error: 'Método no permitido' });
    } catch (e) {
        console.error('error en urls:', e);
        res.status(500).json({ error: 'Error interno' });
    }
});
