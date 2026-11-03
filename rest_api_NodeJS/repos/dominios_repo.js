import { randomBytes } from 'node:crypto';
import { db } from '../lib/firestore.js';
import { generar_token_verificacion } from '../lib/verificacion_dns.js';

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

/* Una llave de 16 caracteres ya es impracticable de adivinar por fuerza bruta.
 * El limite existe para atajar llaves de prueba tipo "123" que alguien deja
 * puestas y se olvida de cambiar. */
const LARGO_MINIMO_API_KEY = 16;

/* Tiene que coincidir con MAX_API_KEYS en zonal_cache/zonal_cache.c: el cache
 * guarda las llaves en un arreglo de tamanno fijo y descarta las sobrantes.
 * Se recorta aqui tambien para que el recorte quede registrado de este lado,
 * donde se puede ver quien quedo afuera. */
const MAX_API_KEYS_PUBLICADAS = 8;

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

    /* Compatibilidad hacia atras: los documentos creados antes de que existiera
     * la verificacion por TXT no tienen el campo, y se consideran verificados.
     * Si no, al desplegar esto los dominios ya en uso desaparecerian de la
     * configuracion y el cache dejaria de servirlos. */
    config.verificado = datos.verificado === undefined ? true : Boolean(datos.verificado);

    /* El cache lee "api_keys", un arreglo. "api_key" en singular es como se
     * configuraba antes, escribiendo el campo directo en el documento, y se
     * conserva como una entrada mas del arreglo para no dejar sin autenticar a
     * los dominios ya cargados. Se elimina del objeto publicado para que haya
     * una sola fuente de verdad: dos campos con el mismo proposito fue justo lo
     * que dejo las llaves del UI desconectadas del cache. */
    const legado = typeof datos.api_key === 'string' ? datos.api_key.trim() : '';
    config.api_keys = legado === '' ? [] : [legado];
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

    /* Un dominio que exige autenticacion necesita al menos una llave real.
     *
     * El cache ya rechaza por su cuenta cuando no hay llaves (api_key_valida
     * recorre cero elementos y devuelve 0), asi que esto no es la unica defensa.
     * Pero descartar el dominio aqui hace visible el error de configuracion en
     * la respuesta de /api/config, en vez de que el dueno lo descubra porque sus
     * clientes reciben 401 sin explicacion. */
    if (config.requiere_auth) {
        if (config.api_keys.length === 0) {
            problemas.push('requiere_auth es true pero el dominio no tiene ninguna api key');
        } else if (config.api_keys.some((clave) => clave.length < LARGO_MINIMO_API_KEY)) {
            problemas.push(`toda api key debe tener al menos ${LARGO_MINIMO_API_KEY} caracteres`);
        }
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
/* Trae las llaves de TODOS los dominios en una sola consulta.
 *
 * collectionGroup('apikeys') recorre todas las subcolecciones con ese nombre
 * que existan en la base, sin importar de que documento cuelguen. No necesita
 * indice compuesto porque no lleva filtros ni ordenamiento.
 *
 * La alternativa era una consulta por dominio dentro de un ciclo, que es lo que
 * tenia listar_reglas_planas y hacia que /api/config tardara 4.5 segundos.
 *
 * El dominio sale de la ruta del documento: ref.parent es la subcoleccion
 * "apikeys" y ref.parent.parent es el documento del dominio. */
async function llaves_por_dominio() {
    const respuesta = await db.collectionGroup('apikeys').get();
    const mapa = new Map();

    for (const doc of respuesta.docs) {
        const dominio = doc.ref.parent.parent?.id;
        const clave = doc.data().clave;
        if (dominio === undefined || typeof clave !== 'string' || clave.trim() === '') continue;

        if (!mapa.has(dominio)) mapa.set(dominio, []);
        mapa.get(dominio).push(clave.trim());
    }

    return mapa;
}

export async function listar_config_dominios() {
    const [respuesta, llaves] = await Promise.all([
        db.collection('dominios').get(),
        llaves_por_dominio(),
    ]);

    const dominios = [];
    const descartados = [];

    for (const doc of respuesta.docs) {
        const config = normalizar(doc.id, doc.data());

        /* Las llaves de la subcoleccion se suman a la del campo api_key heredado
         * que normalizar ya dejo en el arreglo. */
        config.api_keys = [...config.api_keys, ...(llaves.get(doc.id) ?? [])];

        if (config.api_keys.length > MAX_API_KEYS_PUBLICADAS) {
            console.warn(`dominio ${doc.id}: ${config.api_keys.length} llaves, el cache solo admite ${MAX_API_KEYS_PUBLICADAS}; se publican las primeras y el resto recibira 401`);
            config.api_keys = config.api_keys.slice(0, MAX_API_KEYS_PUBLICADAS);
        }

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
/* ───────────────────────── administracion desde el UI ───────────────────────── */

/* El cache solo debe recibir dominios verificados. Se aplica aqui, no en el C,
 * para no tocar el cache: el filtro va junto al resto de la normalizacion. */
export async function listar_config_dominios_verificados() {
    const { dominios, descartados } = await listar_config_dominios();
    const verificados = dominios.filter((d) => d.verificado);
    const sin_verificar = dominios
        .filter((d) => !d.verificado)
        .map((d) => ({ dominio: d.dominio, problemas: ['El dominio no ha sido verificado por TXT'] }));

    return { dominios: verificados, descartados: [...descartados, ...sin_verificar] };
}

/* Dominios de una persona. El campo propietario es lo que impide que alguien
 * vea o modifique los dominios de otra cuenta. */
export async function listar_dominios_de(email) {
    const respuesta = await db.collection('dominios').where('propietario', '==', email).get();
    return respuesta.docs.map((doc) => {
        const d = doc.data();
        return {
            dominio: doc.id,
            propietario: d.propietario,
            verificado: d.verificado === undefined ? true : Boolean(d.verificado),
            token_verificacion: d.token_verificacion ?? null,
            origen_host: d.origen_host ?? null,
            origen_puerto: Number(d.origen_puerto ?? 80),
            tls: Boolean(d.tls),
            creado: d.creado ?? null,
        };
    });
}

export async function consultar_dominio_de(email, dominio) {
    const doc = await db.collection('dominios').doc(dominio).get();
    if (!doc.exists) return null;
    const d = doc.data();
    if (d.propietario !== email) return null; // ajeno: se responde como inexistente
    return {
        dominio: doc.id,
        ...d,
        verificado: d.verificado === undefined ? true : Boolean(d.verificado),
    };
}

/* Crea el dominio sin verificar y con su token. Queda inutilizable por el cache
 * hasta que el dueno publique el TXT y pase la verificacion. */
export async function crear_dominio(email, dominio, datos) {
    const ref = db.collection('dominios').doc(dominio);
    const existente = await ref.get();
    if (existente.exists) {
        return { creado: false, motivo: 'Ese dominio ya está registrado' };
    }

    const token = generar_token_verificacion();

    await ref.set({
        propietario: email,
        verificado: false,
        token_verificacion: token,
        origen_host: datos.origen_host ?? null,
        origen_puerto: Number(datos.origen_puerto ?? 80),
        tls: Boolean(datos.tls),
        tamanno_maximo: Number(datos.tamanno_maximo ?? POR_DEFECTO.tamanno_maximo),
        ttl: Number(datos.ttl ?? POR_DEFECTO.ttl),
        algoritmo: String(datos.algoritmo ?? POR_DEFECTO.algoritmo).toUpperCase(),
        requiere_auth: Boolean(datos.requiere_auth),
        api_key: typeof datos.api_key === 'string' ? datos.api_key : '',
        creado: Date.now(),
    });

    return { creado: true, token };
}

export async function actualizar_dominio(email, dominio, cambios) {
    const actual = await consultar_dominio_de(email, dominio);
    if (actual === null) return { actualizado: false, motivo: 'Dominio no encontrado' };

    /* Lista blanca de campos modificables: propietario, verificado y
     * token_verificacion no se tocan desde el UI, o cualquiera se auto-verificaria. */
    const permitidos = ['origen_host', 'origen_puerto', 'tls', 'tamanno_maximo', 'ttl', 'algoritmo', 'requiere_auth', 'api_key'];
    const parche = {};
    for (const campo of permitidos) {
        if (cambios[campo] !== undefined) parche[campo] = cambios[campo];
    }
    if (parche.origen_puerto !== undefined) parche.origen_puerto = Number(parche.origen_puerto);
    if (parche.tamanno_maximo !== undefined) parche.tamanno_maximo = Number(parche.tamanno_maximo);
    if (parche.ttl !== undefined) parche.ttl = Number(parche.ttl);
    if (parche.algoritmo !== undefined) parche.algoritmo = String(parche.algoritmo).toUpperCase();
    if (parche.tls !== undefined) parche.tls = Boolean(parche.tls);
    if (parche.requiere_auth !== undefined) parche.requiere_auth = Boolean(parche.requiere_auth);

    await db.collection('dominios').doc(dominio).update(parche);
    return { actualizado: true };
}

/* Borrar el documento no borra sus subcolecciones: Firestore las deja
 * huerfanas y seguirian ocupando espacio invisible desde la consola. */
export async function borrar_dominio(email, dominio) {
    const actual = await consultar_dominio_de(email, dominio);
    if (actual === null) return { borrado: false, motivo: 'Dominio no encontrado' };

    const ref = db.collection('dominios').doc(dominio);
    for (const sub of ['urls', 'apikeys']) {
        const hijos = await ref.collection(sub).get();
        await Promise.all(hijos.docs.map((d) => d.ref.delete()));
    }
    await ref.delete();

    return { borrado: true };
}

export async function marcar_verificado(dominio) {
    await db.collection('dominios').doc(dominio).update({
        verificado: true,
        verificado_en: Date.now(),
    });
}

/* ───────────────────────────────── API keys ───────────────────────────────── */

export async function listar_api_keys(email, dominio) {
    const actual = await consultar_dominio_de(email, dominio);
    if (actual === null) return null;

    const respuesta = await db.collection('dominios').doc(dominio).collection('apikeys').get();
    return respuesta.docs.map((doc) => ({ id: doc.id, ...doc.data() }));
}

export async function crear_api_key(email, dominio, descripcion) {
    const actual = await consultar_dominio_de(email, dominio);
    if (actual === null) return null;

    // 32 bytes en hex: impracticable de adivinar, y supera el minimo de 16 caracteres.
    const clave = randomBytes(32).toString('hex');
    const ref = await db.collection('dominios').doc(dominio).collection('apikeys').add({
        clave,
        descripcion: typeof descripcion === 'string' ? descripcion : '',
        creado: Date.now(),
    });

    return { id: ref.id, clave };
}

export async function borrar_api_key(email, dominio, id) {
    const actual = await consultar_dominio_de(email, dominio);
    if (actual === null) return false;
    await db.collection('dominios').doc(dominio).collection('apikeys').doc(id).delete();
    return true;
}
