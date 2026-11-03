import { listar_config_dominios } from '../../repos/dominios_repo.js';
import { listar_reglas_planas } from '../../repos/urls_repo.js';
import { peticion_interna_autorizada } from '../../lib/auth_interna.js';

/* Configuracion completa que el Zonal Cache carga al arrancar.
 *
 * Devuelve dos arreglos planos en lugar de anidar las reglas dentro de cada
 * dominio: el parser del cache delimita cada objeto con el primer '}', asi que
 * un objeto anidado le cortaria el recorrido a mitad de camino.
 *
 * Cada arreglo viene precedido por su cantidad, que es lo que el cache usa para
 * saber cuantos objetos recorrer. */
export default async function handler(req, res) {
    if (req.method !== 'GET') {
        return res.status(405).json({ error: 'Método no permitido' });
    }

    if (!peticion_interna_autorizada(req)) {
        return res.status(401).json({ error: 'No autorizado' });
    }

    try {
        const { dominios, descartados } = await listar_config_dominios();
        const reglas = await listar_reglas_planas(dominios.map((d) => d.dominio));

        /* Una regla que pide api_key sobre un dominio sin llaves es una
         * configuracion imposible de cumplir: el cache respondera 401 a todo el
         * mundo, correctamente, pero sin que nadie sepa por que.
         *
         * No se puede validar en validar_config porque ahi todavia no se
         * conocen las reglas, y no se descarta el dominio porque sus otras
         * rutas si funcionan. Queda como aviso en el log del API. */
        const sin_llaves = new Set(dominios.filter((d) => d.api_keys.length === 0).map((d) => d.dominio));
        for (const regla of reglas) {
            if (regla.autenticacion === 'api_key' && sin_llaves.has(regla.dominio)) {
                console.warn(`dominio ${regla.dominio}: la regla ${regla.patron} exige api_key pero el dominio no tiene ninguna; esas rutas responderan 401`);
            }
        }

        /* Invariantes que el Zonal Cache asume:
         *   cantidad        === dominios.length
         *   cantidad_reglas === reglas.length
         * El cache recorre esa cantidad de objetos, asi que contar de mas lo
         * haria leer fuera del arreglo.
         *
         * "cantidad" se mantiene con ese nombre por compatibilidad: el cache ya
         * desplegado lo busca asi. Y buscar la etiqueta "cantidad": no coincide
         * con "cantidad_reglas": porque exige la comilla inmediatamente despues. */
        res.status(200).json({
            cantidad: dominios.length,
            dominios,
            cantidad_reglas: reglas.length,
            reglas,
            descartados,
        });
    } catch (e) {
        console.error('error en config (lista):', e);
        res.status(500).json({ error: 'Error interno' });
    }
}
