import { con_sesion } from '../../../../lib/sesion_usuario.js';
import { actualizar_url, borrar_url } from '../../../../repos/urls_repo.js';

export default con_sesion(async function handler(req, res, usuario) {
    const { dominio, id } = req.query;

    try {
        if (req.method === 'PUT') {
            const r = await actualizar_url(usuario, dominio, id, req.body ?? {});
            if (!r.actualizada) return res.status(400).json({ error: r.motivo });
            return res.status(200).json({ actualizada: true });
        }

        if (req.method === 'DELETE') {
            const ok = await borrar_url(usuario, dominio, id);
            if (!ok) return res.status(404).json({ error: 'Dominio no encontrado' });
            return res.status(200).json({ borrada: true });
        }

        res.status(405).json({ error: 'Método no permitido' });
    } catch (e) {
        console.error('error en url:', e);
        res.status(500).json({ error: 'Error interno' });
    }
});
