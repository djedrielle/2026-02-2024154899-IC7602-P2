import { consultar_config_dominio, validar_config } from '../../repos/dominios_repo.js';
import { config_autorizada } from '../../lib/auth_interna.js';

/* Configuracion de un solo dominio. Sirve para depurar con curl y para el caso
 * en que el cache quiera recargar un dominio sin reiniciarse. */
export default async function handler(req, res) {
    if (req.method !== 'GET') {
        return res.status(405).json({ error: 'Método no permitido' });
    }

    if (!config_autorizada(req)) {
        return res.status(401).json({ error: 'No autorizado' });
    }

    const { dominio } = req.query;

    try {
        const config = await consultar_config_dominio(dominio);
        if (config === null) {
            return res.status(404).json({ error: 'Dominio no configurado', dominio });
        }

        /* A diferencia de la lista, aqui no se descarta en silencio: si preguntan
         * por un dominio puntual es para depurarlo, asi que se dice que le falta. */
        const problemas = validar_config(config);
        if (problemas.length > 0) {
            return res.status(422).json({ error: 'Documento mal formado', dominio, problemas });
        }

        res.status(200).json(config);
    } catch (e) {
        console.error('error en config:', e);
        res.status(500).json({ error: 'Error interno' });
    }
}
