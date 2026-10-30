import { listar_config_dominios } from '../../repos/dominios_repo.js';
import { config_autorizada } from '../../lib/auth_interna.js';

/* Lista completa de dominios configurados.
 * El Zonal Cache la consulta una sola vez al arrancar y la guarda en memoria,
 * para no pagar una consulta a Firestore por peticion atendida. */
export default async function handler(req, res) {
    if (req.method !== 'GET') {
        return res.status(405).json({ error: 'Método no permitido' });
    }

    if (!config_autorizada(req)) {
        return res.status(401).json({ error: 'No autorizado' });
    }

    try {
        const { dominios, descartados } = await listar_config_dominios();
        /* Invariante que el Zonal Cache asume: "cantidad" es exactamente el largo
         * de "dominios". El cache recorre esa cantidad de objetos, asi que contar
         * los descartados aqui lo haria leer fuera del arreglo. */
        res.status(200).json({ cantidad: dominios.length, dominios, descartados });
    } catch (e) {
        console.error('error en config (lista):', e);
        res.status(500).json({ error: 'Error interno' });
    }
}
