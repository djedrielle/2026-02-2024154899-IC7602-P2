import { db } from '../lib/firestore.js';

/* Valores que se aplican cuando el documento de Firestore no trae el campo.
 * Asi un documento incompleto no deja al Zonal Cache sin configuracion. */
const POR_DEFECTO = {
    origen_host: null,
    origen_puerto: 80,
    tamanno_maximo: 10485760, // 10 MiB por dominio
    ttl: 300,                 // segundos
    algoritmo: 'LRU',
    requiere_auth: false,
};

const ALGORITMOS_VALIDOS = ['LRU', 'LFU', 'FIFO', 'MRU', 'RANDOM'];

/* Une el documento con los valores por defecto y valida el algoritmo.
 * Si el algoritmo no es uno de los cinco, se cae a LRU en lugar de dejar
 * que el cache reciba un nombre que no sabe interpretar. */
function normalizar(dominio, datos) {
    const config = { dominio, ...POR_DEFECTO, ...datos };

    config.algoritmo = String(config.algoritmo).toUpperCase();
    if (!ALGORITMOS_VALIDOS.includes(config.algoritmo)) {
        console.warn(`algoritmo invalido "${datos.algoritmo}" en el dominio ${dominio}, se usa ${POR_DEFECTO.algoritmo}`);
        config.algoritmo = POR_DEFECTO.algoritmo;
    }

    config.origen_puerto = Number(config.origen_puerto);
    config.tamanno_maximo = Number(config.tamanno_maximo);
    config.ttl = Number(config.ttl);
    config.requiere_auth = Boolean(config.requiere_auth);

    // La api_key nunca sale del repositorio: se compara aqui, no se publica.
    delete config.api_key;

    return config;
}

/* Revisa que el documento tenga lo minimo para que el Zonal Cache pueda operar.
 * Devuelve la lista de problemas encontrados; vacia significa que el documento sirve.
 *
 * Esta validacion vive aqui y no en el cache a proposito: el lado C parsea el JSON
 * con strstr, y un campo nulo o de otro tipo lo obliga a abortar el arranque completo.
 * Filtrando aqui, un documento mal llenado solo deja a su propio dominio sin cache. */
export function validar_config(config) {
    const problemas = [];

    if (typeof config.origen_host !== 'string' || config.origen_host.trim() === '') {
        problemas.push('origen_host debe ser un texto no vacio');
    }

    for (const campo of ['origen_puerto', 'tamanno_maximo', 'ttl']) {
        if (!Number.isFinite(config[campo]) || config[campo] <= 0) {
            problemas.push(`${campo} debe ser un numero mayor que cero`);
        }
    }

    if (Number.isFinite(config.origen_puerto) && config.origen_puerto > 65535) {
        problemas.push('origen_puerto debe ser menor o igual a 65535');
    }

    return problemas;
}

export async function consultar_config_dominio(dominio) {
    const doc = await db.collection('dominios').doc(dominio).get();
    if (!doc.exists) return null;
    return normalizar(doc.id, doc.data());
}

/* Devuelve solo los dominios usables, mas la lista de los descartados con su motivo.
 * 'cantidad' en el endpoint se calcula sobre los usables, que es lo que el cache
 * va a recorrer. */
export async function listar_config_dominios() {
    const respuesta = await db.collection('dominios').get();

    const dominios = [];
    const descartados = [];

    for (const doc of respuesta.docs) {
        const config = normalizar(doc.id, doc.data());
        const problemas = validar_config(config);

        if (problemas.length === 0) {
            dominios.push(config);
        } else {
            console.warn(`dominio descartado: ${doc.id} -> ${problemas.join('; ')}`);
            descartados.push({ dominio: doc.id, problemas });
        }
    }

    return { dominios, descartados };
}

/* Valida una x-api-key contra la guardada en el documento del dominio.
 * Se consulta en el momento de la peticion y no se expone por ningun endpoint. */
export async function validar_api_key(dominio, api_key) {
    if (!api_key) return false;
    const doc = await db.collection('dominios').doc(dominio).get();
    if (!doc.exists) return false;
    const esperada = doc.data().api_key;
    return typeof esperada === 'string' && esperada.length > 0 && esperada === api_key;
}
