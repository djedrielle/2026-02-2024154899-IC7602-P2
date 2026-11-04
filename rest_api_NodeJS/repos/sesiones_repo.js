import { randomBytes } from 'node:crypto';
import { db } from '../lib/firestore.js';

const TTL_POR_DEFECTO = 3600; // segundos

/* El token se usa como ID del documento, igual que el dominio en 'dominios' y el
 * codigo de pais en 'zonas'. Asi validar una sesion es una lectura directa por
 * clave y no una query, que seria mas lenta y necesitaria indice. */
export async function crear_sesion(usuario, dominios) {
    const token = randomBytes(32).toString('hex');
    const ttl = Number(process.env.SESION_TTL) || TTL_POR_DEFECTO;
    const expiracion = Math.floor(Date.now() / 1000) + ttl;

    await db.collection('sesiones').doc(token).set({
        usuario,
        dominios: Array.isArray(dominios) ? dominios : [],
        expiracion,
        creado: Date.now(),
    });

    return { token, expiracion };
}

/* Valida que la sesion exista, no haya expirado y cubra el dominio pedido.
 * El tercer chequeo importa: sin el, una sesion obtenida para un dominio
 * abriria cualquier otro dominio protegido de la misma cache. */
export async function validar_sesion(token, dominio) {
    const doc = await db.collection('sesiones').doc(token).get();
    if (!doc.exists) return { valida: false, motivo: 'La sesion no existe o fue revocada' };

    const s = doc.data();
    const ahora = Math.floor(Date.now() / 1000);

    if (typeof s.expiracion !== 'number' || s.expiracion <= ahora) {
        return { valida: false, motivo: 'La sesion expiro' };
    }

    if (dominio && !(Array.isArray(s.dominios) && s.dominios.includes(dominio))) {
        /* El arreglo guardado aqui es una foto tomada al iniciar sesion. Si el
         * usuario registro un dominio despues, su sesion no lo cubre aunque sea
         * el dueno, y tendria que volver a entrar sin que nada se lo explique.
         *
         * Antes de rechazar se relee la lista real del usuario. Es una lectura
         * extra, pero solo en el camino del fallo: el caso normal sigue siendo
         * una sola lectura por clave. */
        const usuario = await db.collection('usuarios').doc(s.usuario).get();
        const actuales = usuario.exists && Array.isArray(usuario.data().dominios)
            ? usuario.data().dominios
            : [];

        if (!actuales.includes(dominio)) {
            return { valida: false, motivo: 'La sesion no cubre ese dominio' };
        }

        // Se actualiza la foto para que la proxima peticion tome el atajo.
        await doc.ref.update({ dominios: actuales });
    }

    return { valida: true, usuario: s.usuario, expiracion: s.expiracion };
}

export async function borrar_sesion(token) {
    await db.collection('sesiones').doc(token).delete();
}
