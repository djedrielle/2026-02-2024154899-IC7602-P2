import { randomBytes, scrypt, timingSafeEqual } from 'node:crypto';
import { promisify } from 'node:util';

const derivar = promisify(scrypt);

const LARGO_SAL = 16;
const LARGO_HASH = 64;

/* La contrasenna NUNCA se guarda. Se guarda una derivacion con scrypt, que es
 * deliberadamente lenta y consume memoria, para que probar contrasennas por
 * fuerza bruta sea caro incluso si alguien se lleva la coleccion completa.
 *
 * La sal es por usuario: hace que dos personas con la misma contrasenna tengan
 * hashes distintos, e impide precomputar tablas. */
export async function hashear_contrasenna(contrasenna) {
    const sal = randomBytes(LARGO_SAL).toString('hex');
    const derivada = await derivar(contrasenna, sal, LARGO_HASH);
    return { sal, hash: derivada.toString('hex') };
}

export async function verificar_contrasenna(contrasenna, sal, hash_esperado) {
    if (typeof sal !== 'string' || typeof hash_esperado !== 'string') return false;

    const derivada = await derivar(contrasenna, sal, LARGO_HASH);
    const esperado = Buffer.from(hash_esperado, 'hex');

    // timingSafeEqual exige largos iguales; si difieren, el hash guardado esta corrupto.
    if (esperado.length !== derivada.length) return false;

    /* Comparacion en tiempo constante: no corta en el primer byte distinto.
     * Con === el tiempo de respuesta revelaria cuantos bytes acerto un atacante. */
    return timingSafeEqual(derivada, esperado);
}
