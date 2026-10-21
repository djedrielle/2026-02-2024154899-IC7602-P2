import { db } from '../lib/firestore.js';

function ip_to_entero(texto) {
    const partes = String(texto).split('.');
    if (partes.length !== 4) return null;
    let n = 0;
    for (const p of partes) {
        const octeto = Number(p);
        if (!Number.isInteger(octeto) || octeto < 0 || octeto > 255) return null;
        n = n * 256 + octeto;
    }
    return n;
}

export async function consultar_ip_to_country(ip) {
    const numero = ip_to_entero(ip);
    if (numero === null) return null;
    const respuesta = await db.collection('ip_to_country').where('inicio', '<=', numero).orderBy('inicio', 'desc').limit(1).get();
    if (respuesta.empty) return null;
    const rango = respuesta.docs[0].data();
    return rango.fin >= numero ? rango.pais : null;
}