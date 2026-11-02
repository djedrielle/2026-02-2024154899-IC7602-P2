import { con_sesion } from '../../../../lib/sesion_usuario.js';
import { listar_api_keys, crear_api_key } from '../../../../repos/dominios_repo.js';

export default con_sesion(async function handler(req, res, usuario) {
    const { dominio } = req.query;

    try {
        if (req.method === 'GET') {
            const claves = await listar_api_keys(usuario, dominio);
            if (claves === null) return res.status(404).json({ error: 'Dominio no encontrado' });
            return res.status(200).json({ apikeys: claves });
        }

        if (req.method === 'POST') {
            const r = await crear_api_key(usuario, dominio, req.body?.descripcion);
            if (r === null) return res.status(404).json({ error: 'Dominio no encontrado' });
            return res.status(201).json(r);
        }

        res.status(405).json({ error: 'Método no permitido' });
    } catch (e) {
        console.error('error en apikeys:', e);
        res.status(500).json({ error: 'Error interno' });
    }
});
