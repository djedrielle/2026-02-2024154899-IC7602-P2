/*
 * Importa el CSV de IP-to-Country a la coleccion `ip_to_country` de Firestore.
 *
 *   node scripts/importar_ip_country.mjs <ruta-al-csv> [--desde N] [--simular]
 *
 *   --desde N    retoma desde la fila N (si una corrida anterior se corto)
 *   --simular    procesa el CSV y reporta, pero NO escribe en Firestore
 *
 * Credencial: exporte GOOGLE_APPLICATION_CREDENTIALS con la ruta al JSON
 * de la cuenta de servicio, o FIREBASE_SERVICE_ACCOUNT con su contenido.
 */
import fs from 'node:fs';
import readline from 'node:readline';
import { initializeApp, cert, applicationDefault } from 'firebase-admin/app';
import { getFirestore } from 'firebase-admin/firestore';

const COLECCION = 'ip_to_country';
const REPORTE_CADA = 25000;

/* --- argumentos --- */
const args = process.argv.slice(2);
const ruta = args.find(a => !a.startsWith('--'));
const simular = args.includes('--simular');
const desde = Number(args[args.indexOf('--desde') + 1]) || 0;

if (!ruta) {
    console.error('Uso: node scripts/importar_ip_country.mjs <ruta-csv> [--desde N] [--simular]');
    process.exit(1);
}

/* --- convierte "190.113.1.1" a 3195535617; null si no es IPv4 --- */
function ipAEntero(texto) {
    const partes = texto.split('.');
    if (partes.length !== 4) return null;          // descarta IPv6
    let n = 0;
    for (const p of partes) {
        const octeto = Number(p);
        if (!Number.isInteger(octeto) || octeto < 0 || octeto > 255) return null;
        n = n * 256 + octeto;
    }
    return n;
}

/* --- Firestore (salvo en modo simulacion) --- */
let db = null, escritor = null;
if (!simular) {
    const credencial = process.env.FIREBASE_SERVICE_ACCOUNT
        ? cert(JSON.parse(process.env.FIREBASE_SERVICE_ACCOUNT))
        : applicationDefault();
    initializeApp({ credential: credencial });
    db = getFirestore();
    escritor = db.bulkWriter();
    escritor.onWriteError(err => {
        if (err.code === 5) {                        // NOT_FOUND: la base no existe
            console.error('\nLa base de datos no existe. Verifique el ID con:');
            console.error('  gcloud firestore databases list --project=<proyecto>\n');
            process.exit(1);
        }
        if (err.failedAttempts < 5) return true;     // reintenta
        console.error('fallo definitivo:', err.documentRef.path, err.message);
        fallidas++;
        return false;
    });
}

/* --- recorrido del CSV --- */
const lector = readline.createInterface({
    input: fs.createReadStream(ruta),
    crlfDelay: Infinity,
});

let fila = 0, encoladas = 0, confirmadas = 0, fallidas = 0, omitidas = 0, saltadas = 0;
const t0 = Date.now();

for await (const linea of lector) {
    fila++;
    if (fila === 1) continue;                        // encabezado
    if (!linea.trim()) continue;

    if (fila <= desde) { saltadas++; continue; }

    const [ipInicio, ipFin, pais] = linea.split(',');
    const inicio = ipAEntero(ipInicio);
    const fin = ipAEntero(ipFin);

    if (inicio === null || fin === null) { omitidas++; continue; }   // IPv6

    if (!simular) {
        escritor.set(db.collection(COLECCION).doc(), { inicio, fin, pais: pais.trim() })
               .then(() => { confirmadas++; }, () => {});
    } else {
        confirmadas++;
    }
    encoladas++;

    if (encoladas % REPORTE_CADA === 0) {
        if (!simular) await escritor.flush();        // espera confirmacion y libera memoria
        const seg = (Date.now() - t0) / 1000;
        console.log(`  ${confirmadas.toLocaleString()} confirmadas  ` +
                    `(fila ${fila.toLocaleString()}, ${Math.round(confirmadas / seg)}/s)`);
    }
}

if (!simular) await escritor.close();

const seg = (Date.now() - t0) / 1000;
console.log(`\n${simular ? '[SIMULACION] ' : ''}Terminado en ${seg.toFixed(1)} s`);
console.log(`  confirmadas: ${confirmadas.toLocaleString()}`);
if (fallidas) console.log(`  fallidas   : ${fallidas.toLocaleString()}`);
console.log(`  omitidas   : ${omitidas.toLocaleString()}  (IPv6 u otras no IPv4)`);
if (saltadas) console.log(`  saltadas   : ${saltadas.toLocaleString()}  (--desde ${desde})`);
