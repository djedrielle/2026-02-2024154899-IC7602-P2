import { con_sesion } from '../../../lib/sesion_usuario.js';
import { consultar_dominio_de, actualizar_dominio, borrar_dominio } from '../../../repos/dominios_repo.js';
import { valor_txt_esperado } from '../../../lib/verificacion_dns.js';

export default con_sesion(async function handler(req, res, usuario) {
    const { dominio } = req.query;

    try {
        if (req.method === 'GET') {
            const d = await consultar_dominio_de(usuario, dominio);
            if (d === null) return res.status(404).json({ error: 'Dominio no encontrado' });
            return res.status(200).json({
                ...d,
                registro_txt: d.token_verificacion ? valor_txt_esperado(d.token_verificacion) : null,
            });
        }

        if (req.method === 'PUT') {
            const r = await actualizar_dominio(usuario, dominio, req.body ?? {});
            if (!r.actualizado) return res.status(404).json({ error: r.motivo });
            return res.status(200).json({ actualizado: true });
        }

        if (req.method === 'DELETE') {
            const r = await borrar_dominio(usuario, dominio);
            if (!r.borrado) return res.status(404).json({ error: r.motivo });
            return res.status(200).json({ borrado: true });
        }

        res.status(405).json({ error: 'Método no permitido' });
    } catch (e) {
        console.error('error en dominio:', e);
        res.status(500).json({ error: 'Error interno' });
    }
});
