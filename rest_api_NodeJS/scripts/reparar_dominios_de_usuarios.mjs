/* Sincroniza usuarios/<email>.dominios con el campo propietario de la coleccion
 * dominios.
 *
 * Hace falta porque crear_dominio no actualizaba el arreglo del usuario, de modo
 * que los dominios registrados antes de ese arreglo quedaron con su propietario
 * correcto pero fuera de la lista que consulta validar_sesion. El sintoma es un
 * 401 en el propio sitio del dueno, sin nada en el mensaje que apunte a esto.
 *
 * Es idempotente: correrlo dos veces no cambia nada la segunda vez.
 *
 *   export FIREBASE_SERVICE_ACCOUNT="$(sed -n 's/^FIREBASE_SERVICE_ACCOUNT="\(.*\)"$/\1/p' .env.local)"
 *   node scripts/reparar_dominios_de_usuarios.mjs            # solo informa
 *   node scripts/reparar_dominios_de_usuarios.mjs --aplicar  # escribe
 */
import { db } from '../lib/firestore.js';

const aplicar = process.argv.includes('--aplicar');

const dominios = await db.collection('dominios').get();
const usuarios = await db.collection('usuarios').get();

// propietario -> dominios que realmente le pertenecen
const reales = new Map();
for (const doc of dominios.docs) {
    const duenno = doc.data().propietario;
    if (typeof duenno !== 'string' || duenno === '') {
        console.log(`  aviso: el dominio ${doc.id} no tiene propietario; nadie podra administrarlo desde el UI`);
        continue;
    }
    if (!reales.has(duenno)) reales.set(duenno, []);
    reales.get(duenno).push(doc.id);
}

let cambios = 0;

for (const doc of usuarios.docs) {
    const actual = Array.isArray(doc.data().dominios) ? doc.data().dominios : [];
    const esperado = (reales.get(doc.id) ?? []).sort();

    const faltan = esperado.filter((d) => !actual.includes(d));
    /* Sobran los que estan en el arreglo pero ya no existen como documento, o
     * cuyo propietario es otro. Se quitan: dejarlos haria que una sesion
     * cubriera un dominio ajeno. */
    const sobran = actual.filter((d) => !esperado.includes(d));

    if (faltan.length === 0 && sobran.length === 0) continue;

    cambios++;
    console.log(`\n  ${doc.id}`);
    console.log(`      tiene:  ${JSON.stringify(actual)}`);
    console.log(`      deberia:${JSON.stringify(esperado)}`);
    if (faltan.length > 0) console.log(`      agregar: ${faltan.join(', ')}`);
    if (sobran.length > 0) console.log(`      quitar:  ${sobran.join(', ')}`);

    if (aplicar) await doc.ref.update({ dominios: esperado });
}

if (cambios === 0) {
    console.log('\nTodo consistente, no hay nada que reparar.');
} else if (aplicar) {
    console.log(`\n${cambios} usuario(s) actualizados.`);
} else {
    console.log(`\n${cambios} usuario(s) necesitan cambios. Repita con --aplicar para escribirlos.`);
}

process.exit(0);
