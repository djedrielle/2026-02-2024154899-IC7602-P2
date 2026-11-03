import { db } from '../lib/firestore.js';
import { consultar_dominio_de } from './dominios_repo.js';

const ALGORITMOS_VALIDOS = ['LRU', 'LFU', 'FIFO', 'MRU', 'RANDOM'];
const AUTENTICACIONES_VALIDAS = ['ninguna', 'api_key', 'usuario'];

/* Tipos de archivo que se pueden configurar por separado dentro de una URL.
 * El enunciado pide TTL y politica de reemplazo por tipo, no solo por dominio. */
export const TIPOS_ARCHIVO = ['html', 'css', 'js', 'png', 'jpg', 'gif', 'svg', 'json', 'otros'];

function coleccion(dominio) {
    return db.collection('dominios').doc(dominio).collection('urls');
}

/* Cuenta que tan especifico es un patron, para resolver cual regla gana cuando
 * varias coinciden con la misma URL.
 *
 * Sin un orden definido, la regla aplicada dependeria de como quedaron ordenados
 * los documentos en Firestore, que es imposible de depurar. El criterio: mas
 * caracteres literales (sin comodines) significa mas especifico. */
export function especificidad(patron) {
    return patron.replace(/\*/g, '').length;
}

/* Valida la regla antes de guardarla. Mismo criterio que validar_config: es
 * preferible rechazar en el API que dejar que el cache reciba algo que no sabe
 * interpretar. */
export function validar_url(regla) {
    const problemas = [];

    if (typeof regla.patron !== 'string' || regla.patron.trim() === '') {
        problemas.push('El patrón es obligatorio');
    } else if (!regla.patron.startsWith('/')) {
        problemas.push('El patrón debe empezar con /');
    }

    if (!Number.isFinite(Number(regla.tamanno_cache)) || Number(regla.tamanno_cache) <= 0) {
        problemas.push('El tamaño de caché debe ser un número mayor que cero');
    }

    if (!AUTENTICACIONES_VALIDAS.includes(regla.autenticacion)) {
        problemas.push(`La autenticación debe ser una de: ${AUTENTICACIONES_VALIDAS.join(', ')}`);
    }

    const tipos = regla.tipos_archivo ?? {};
    for (const [tipo, conf] of Object.entries(tipos)) {
        if (!TIPOS_ARCHIVO.includes(tipo)) {
            problemas.push(`Tipo de archivo desconocido: ${tipo}`);
            continue;
        }
        if (!Number.isFinite(Number(conf.ttl)) || Number(conf.ttl) <= 0) {
            problemas.push(`El TTL de ${tipo} debe ser un número mayor que cero`);
        }
        if (!ALGORITMOS_VALIDOS.includes(String(conf.algoritmo).toUpperCase())) {
            problemas.push(`El algoritmo de ${tipo} debe ser uno de: ${ALGORITMOS_VALIDOS.join(', ')}`);
        }
    }

    return problemas;
}

function normalizar_regla(datos) {
    const tipos = {};
    for (const [tipo, conf] of Object.entries(datos.tipos_archivo ?? {})) {
        tipos[tipo] = {
            ttl: Number(conf.ttl),
            algoritmo: String(conf.algoritmo).toUpperCase(),
        };
    }

    return {
        patron: String(datos.patron).trim(),
        tamanno_cache: Number(datos.tamanno_cache),
        autenticacion: datos.autenticacion,
        tipos_archivo: tipos,
    };
}

/* Devuelve las reglas ordenadas de mas a menos especifica, que es el orden en
 * que un consumidor debe evaluarlas: la primera que coincida es la que manda. */
export async function listar_urls(email, dominio) {
    if (await consultar_dominio_de(email, dominio) === null) return null;

    const respuesta = await coleccion(dominio).get();
    return respuesta.docs
        .map((doc) => ({ id: doc.id, ...doc.data() }))
        .sort((a, b) => especificidad(b.patron) - especificidad(a.patron));
}

export async function crear_url(email, dominio, datos) {
    if (await consultar_dominio_de(email, dominio) === null) {
        return { creada: false, motivo: 'Dominio no encontrado' };
    }

    const problemas = validar_url(datos);
    if (problemas.length > 0) return { creada: false, motivo: problemas.join('; ') };

    /* Dos reglas con el mismo patron se pisarian sin que se note cual aplica. */
    const existentes = await coleccion(dominio).where('patron', '==', String(datos.patron).trim()).get();
    if (!existentes.empty) {
        return { creada: false, motivo: 'Ya existe una regla con ese patrón' };
    }

    const ref = await coleccion(dominio).add({ ...normalizar_regla(datos), creado: Date.now() });
    return { creada: true, id: ref.id };
}

export async function actualizar_url(email, dominio, id, datos) {
    if (await consultar_dominio_de(email, dominio) === null) {
        return { actualizada: false, motivo: 'Dominio no encontrado' };
    }

    const doc = await coleccion(dominio).doc(id).get();
    if (!doc.exists) return { actualizada: false, motivo: 'Regla no encontrada' };

    const problemas = validar_url(datos);
    if (problemas.length > 0) return { actualizada: false, motivo: problemas.join('; ') };

    await coleccion(dominio).doc(id).update({ ...normalizar_regla(datos), modificado: Date.now() });
    return { actualizada: true };
}

export async function borrar_url(email, dominio, id) {
    if (await consultar_dominio_de(email, dominio) === null) return false;
    await coleccion(dominio).doc(id).delete();
    return true;
}

/* ───────────────── vista para el Zonal Cache ───────────────── */

/* Devuelve las reglas APLANADAS: una entrada por cada combinacion de regla y
 * tipo de archivo, con el dominio incorporado en cada fila.
 *
 * El formato es deliberadamente plano y redundante. El Zonal Cache parsea el
 * JSON con strstr y delimita cada objeto con el primer '}', asi que un objeto
 * anidado le cortaria el recorrido en el lugar equivocado. Aplanar aqui evita
 * reescribir ese parser.
 *
 * Vienen ordenadas de mas a menos especifica dentro de cada dominio: el cache
 * aplica la primera que coincida y no necesita ordenar nada. */
export async function listar_reglas_planas(dominios) {
    /* Una sola consulta para las reglas de todos los dominios.
     *
     * Antes esto era un await por dominio dentro del ciclo: con cinco dominios
     * eran cinco viajes a Firestore encadenados, y /api/config pasaba de menos
     * de un segundo a 4.5. collectionGroup('urls') recorre todas las
     * subcolecciones con ese nombre de una vez, y no pide indice compuesto
     * porque no lleva filtros.
     *
     * Trae las reglas de dominios que no estan en la lista recibida (los no
     * verificados, los descartados por mal formados), asi que hay que filtrar
     * contra el conjunto permitido. */
    const permitidos = new Set(dominios);
    const por_dominio = new Map();

    const respuesta = await db.collectionGroup('urls').get();
    for (const doc of respuesta.docs) {
        const dominio = doc.ref.parent.parent?.id;
        if (dominio === undefined || !permitidos.has(dominio)) continue;

        if (!por_dominio.has(dominio)) por_dominio.set(dominio, []);
        por_dominio.get(dominio).push({ id: doc.id, ...doc.data() });
    }

    const filas = [];

    /* Se recorre "dominios" y no el Map para que el orden de salida siga el del
     * arreglo de dominios y no el que devuelva Firestore, que no esta definido. */
    for (const dominio of dominios) {
        const reglas = por_dominio.get(dominio) ?? [];
        reglas.sort((a, b) => especificidad(b.patron) - especificidad(a.patron));

        for (const regla of reglas) {
            const tipos = Object.entries(regla.tipos_archivo ?? {});

            /* Una regla sin tipos configurados igual se publica, con tipo
             * "otros": si no, una URL con autenticacion pero sin TTL por tipo
             * desapareceria de la vista del cache y quedaria sin proteger. */
            if (tipos.length === 0) {
                filas.push({
                    id: regla.id, dominio, patron: regla.patron, tipo: 'otros',
                    ttl: 300, algoritmo: 'LRU',
                    tamanno_cache: Number(regla.tamanno_cache),
                    autenticacion: regla.autenticacion,
                });
                continue;
            }

            for (const [tipo, conf] of tipos) {
                filas.push({
                    id: regla.id, dominio, patron: regla.patron, tipo,
                    ttl: Number(conf.ttl),
                    algoritmo: String(conf.algoritmo).toUpperCase(),
                    tamanno_cache: Number(regla.tamanno_cache),
                    autenticacion: regla.autenticacion,
                });
            }
        }
    }

    return filas;
}
