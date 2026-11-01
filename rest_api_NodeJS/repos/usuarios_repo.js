import { db } from '../lib/firestore.js';
import { hashear_contrasenna, verificar_contrasenna } from '../lib/password.js';

/* Hash de relleno para el caso "el usuario no existe". Ver autenticar_usuario. */
const SAL_FICTICIA = 'f'.repeat(32);
const HASH_FICTICIO = '0'.repeat(128);

export async function consultar_usuario(email) {
    const doc = await db.collection('usuarios').doc(email).get();
    if (!doc.exists) return null;
    const u = doc.data();
    return {
        email: doc.id,
        dominios: Array.isArray(u.dominios) ? u.dominios : [],
        creado: u.creado ?? null,
    };
}

/* Devuelve el usuario si las credenciales son correctas, o null si no.
 *
 * Cuando el usuario no existe igual se deriva un hash de relleno antes de
 * responder. Sin eso, "usuario inexistente" respondería en milisegundos y
 * "contrasenna incorrecta" tardaría lo que tarda scrypt: esa diferencia de
 * tiempo permite averiguar que correos estan registrados. */
export async function autenticar_usuario(email, contrasenna) {
    const doc = await db.collection('usuarios').doc(email).get();

    if (!doc.exists) {
        await verificar_contrasenna(contrasenna, SAL_FICTICIA, HASH_FICTICIO);
        return null;
    }

    const u = doc.data();
    const correcta = await verificar_contrasenna(contrasenna, u.sal, u.hash_contrasenna);
    if (!correcta) return null;

    return {
        email: doc.id,
        dominios: Array.isArray(u.dominios) ? u.dominios : [],
    };
}

export async function crear_usuario(email, contrasenna, dominios) {
    const existente = await db.collection('usuarios').doc(email).get();
    if (existente.exists) return { creado: false, motivo: 'Ese correo ya esta registrado' };

    const { sal, hash } = await hashear_contrasenna(contrasenna);

    await db.collection('usuarios').doc(email).set({
        hash_contrasenna: hash,
        sal,
        dominios: Array.isArray(dominios) ? dominios : [],
        creado: Date.now(),
    });

    return { creado: true };
}
