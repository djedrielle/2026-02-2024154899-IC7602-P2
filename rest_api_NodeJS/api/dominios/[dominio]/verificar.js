import { con_sesion } from '../../../lib/sesion_usuario.js';
import { consultar_dominio_de, marcar_verificado } from '../../../repos/dominios_repo.js';
import { verificar_txt, valor_txt_esperado } from '../../../lib/verificacion_dns.js';

/* Comprueba la propiedad del dominio consultando su registro TXT.
 * Solo quien administra la zona DNS puede publicarlo, asi que encontrarlo
 * prueba que el dominio es de quien lo registro. */
export default con_sesion(async function handler(req, res, usuario) {
    if (req.method !== 'POST') return res.status(405).json({ error: 'Método no permitido' });

    const { dominio } = req.query;

    try {
        const d = await consultar_dominio_de(usuario, dominio);
        if (d === null) return res.status(404).json({ error: 'Dominio no encontrado' });

        if (d.verificado) {
            return res.status(200).json({ verificado: true, motivo: 'El dominio ya estaba verificado' });
        }
        if (!d.token_verificacion) {
            return res.status(409).json({ verificado: false, motivo: 'El dominio no tiene token de verificación' });
        }

        const r = await verificar_txt(dominio, d.token_verificacion);
        if (!r.verificado) {
            return res.status(200).json({
                verificado: false,
                motivo: r.motivo,
                esperado: valor_txt_esperado(d.token_verificacion),
                encontrados: r.encontrados,
            });
        }

        await marcar_verificado(dominio);
        res.status(200).json({ verificado: true, motivo: 'Propiedad confirmada por registro TXT' });
    } catch (e) {
        console.error('error verificando dominio:', e);
        res.status(500).json({ error: 'Error interno' });
    }
});
