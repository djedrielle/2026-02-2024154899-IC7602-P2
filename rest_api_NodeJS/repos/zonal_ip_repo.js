import { db } from '../lib/firestore.js';

export async function consultar_zonal_ip(codigo_pais) {
    const doc = await db.collection('zonas').doc(codigo_pais).get();
    if (!doc.exists) return null;
    return doc.data().ip_cache;
}