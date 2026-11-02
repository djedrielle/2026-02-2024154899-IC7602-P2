import { promises as dns } from 'node:dns';
import { randomBytes } from 'node:crypto';

/* Prefijo del registro TXT que prueba la propiedad de un dominio.
 * Va fijo para poder distinguir nuestro token de los de Google, Microsoft y
 * demas servicios que suelen convivir en el mismo dominio. */
export const PREFIJO_TXT = 'zonal-cache-verification';

export function generar_token_verificacion() {
    return randomBytes(16).toString('hex');
}

export function valor_txt_esperado(token) {
    return `${PREFIJO_TXT}=${token}`;
}

/* Consulta los registros TXT del dominio y busca el valor esperado.
 *
 * Solo quien administra la zona DNS puede crear registros en ella, asi que
 * encontrar el token ahi prueba que quien registro el dominio lo controla.
 * Sin esto, cualquiera podria registrar un dominio ajeno en el sistema.
 *
 * Devuelve { verificado, motivo, encontrados }. */
export async function verificar_txt(dominio, token) {
    const esperado = valor_txt_esperado(token);

    let registros;
    try {
        registros = await dns.resolveTxt(dominio);
    } catch (e) {
        /* ENOTFOUND y ENODATA no son fallas del sistema: significan que el
         * dominio no existe o todavia no tiene registros TXT. Es el caso normal
         * cuando el usuario aun no creo el registro. */
        if (e.code === 'ENOTFOUND' || e.code === 'ENODATA') {
            return {
                verificado: false,
                motivo: 'El dominio no tiene registros TXT todavía. Si acaba de crearlo, espere a que propague.',
                encontrados: [],
            };
        }
        return { verificado: false, motivo: `No se pudo consultar el DNS: ${e.code ?? 'error'}`, encontrados: [] };
    }

    /* resolveTxt devuelve un arreglo de arreglos: un registro TXT puede venir
     * partido en varias cadenas de 255 bytes, que hay que concatenar. */
    const valores = registros.map((partes) => partes.join(''));

    if (valores.includes(esperado)) {
        return { verificado: true, motivo: null, encontrados: valores };
    }

    return {
        verificado: false,
        motivo: `No se encontró el registro TXT esperado (${esperado}).`,
        encontrados: valores,
    };
}
