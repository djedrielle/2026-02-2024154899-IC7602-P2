import { consultar_zonal_ip } from "../../repos/zonal_ip_repo.js";

export default async function handler(req, res) {
    const { codigo_pais } = req.query;
    try {
        const ip_cache = await consultar_zonal_ip(codigo_pais);
        if (ip_cache == null) {
            return res.status(404).json({ error: 'Código país no encontrado o inválido', codigo_pais });
        }
        res.status(200).json({ codigo_pais, ip_cache });
    } catch (e) {
        console.error('error consultando zonal_ip');
        res.status(500).json({ error: 'Error interno' });
    }
}