import { consultar_ip_to_country } from "../../repos/ip_to_country_repo.js";

export default async function handler(req, res) {
    const { ip } = req.query;
    try {
        const pais = await consultar_ip_to_country(ip);
        if (pais === null) {
            return res.status(404).json({ error: 'IP no encontrada o inválida', ip });
        }
        res.status(200).json({ ip, pais });
    } catch (e) {
        console.error('error consultando ip_to_country:', e);
        res.status(500).json({ error: 'Error interno' });
    }
}