import { consultar_ip_to_country } from "../../repos/ip_to_country_repo.js";
import { consultar_zonal_ip } from "../../repos/zonal_ip_repo.js";

/* Resuelve en un solo viaje lo que antes requeria dos llamadas:
 * la IP del cliente -> su codigo de pais -> la IP del Zonal Cache de esa zona. */
export default async function handler(req, res) {
    if (req.method !== 'GET') {
        return res.status(405).json({ error: 'Método no permitido' });
    }

    const { ip } = req.query;

    try {
        const pais = await consultar_ip_to_country(ip);
        if (pais === null) {
            return res.status(404).json({ error: 'IP no encontrada o inválida', ip });
        }

        const ip_cache = await consultar_zonal_ip(pais);
        if (ip_cache == null) {
            return res.status(404).json({ error: 'No hay Zonal Cache para ese país', ip, pais });
        }

        res.status(200).json({ ip, pais, ip_cache });
    } catch (e) {
        console.error('error en resolver:', e);
        res.status(500).json({ error: 'Error interno' });
    }
}
