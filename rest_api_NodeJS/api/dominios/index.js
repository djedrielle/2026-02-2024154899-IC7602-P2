import { con_sesion } from '../../lib/sesion_usuario.js';
import { listar_dominios_de, crear_dominio } from '../../repos/dominios_repo.js';
import { valor_txt_esperado } from '../../lib/verificacion_dns.js';

/* GET  -> dominios de la persona autenticada
 * POST -> registra uno nuevo, sin verificar, y devuelve el TXT que debe publicar */
export default con_sesion(async function handler(req, res, usuario) {
    try {
        if (req.method === 'GET') {
            return res.status(200).json({ dominios: await listar_dominios_de(usuario) });
        }

        if (req.method === 'POST') {
            const { dominio, ...datos } = req.body ?? {};

            if (typeof dominio !== 'string' || !/^[a-z0-9.-]+$/i.test(dominio.trim()) || dominio.trim() === '') {
                return res.status(400).json({ error: 'El nombre de dominio no es válido' });
            }

            const nombre = dominio.trim().toLowerCase();
            const r = await crear_dominio(usuario, nombre, datos);
            if (!r.creado) return res.status(409).json({ error: r.motivo });

            return res.status(201).json({
                dominio: nombre,
                token_verificacion: r.token,
                registro_txt: valor_txt_esperado(r.token),
                instruccion: `Cree un registro TXT en ${nombre} con ese valor y luego verifique.`,
            });
        }

        res.status(405).json({ error: 'Método no permitido' });
    } catch (e) {
        console.error('error en dominios:', e);
        res.status(500).json({ error: 'Error interno' });
    }
});
