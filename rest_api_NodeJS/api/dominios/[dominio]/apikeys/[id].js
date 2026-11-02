import { con_sesion } from '../../../../lib/sesion_usuario.js';
import { borrar_api_key } from '../../../../repos/dominios_repo.js';

export default con_sesion(async function handler(req, res, usuario) {
    if (req.method !== 'DELETE') return res.status(405).json({ error: 'Método no permitido' });

    const { dominio, id } = req.query;

    try {
        const ok = await borrar_api_key(usuario, dominio, id);
        if (!ok) return res.status(404).json({ error: 'Dominio no encontrado' });
        res.status(200).json({ borrada: true });
    } catch (e) {
        console.error('error borrando apikey:', e);
        res.status(500).json({ error: 'Error interno' });
    }
});
