/* Deja Firestore con los datos exactos de la demostracion.
 *
 * BORRA las colecciones dominios, usuarios y sesiones, y reescribe zonas.
 * NO TOCA ip_to_country: son 357 mil rangos cargados en el Proyecto 1 y
 * recargarlos lleva mucho tiempo.
 *
 * No crea djdrll.privatedny.org a proposito: ese se registra en vivo durante la
 * demostracion, para mostrar el alta y la verificacion por TXT.
 *
 *   export FIREBASE_SERVICE_ACCOUNT="$(sed -n 's/^FIREBASE_SERVICE_ACCOUNT="\(.*\)"$/\1/p' .env.local)"
 *   node scripts/sembrar_demo.mjs            # solo informa
 *   node scripts/sembrar_demo.mjs --aplicar  # borra y escribe
 */
import { randomBytes } from 'node:crypto';
import { writeFileSync } from 'node:fs';
import { db } from '../lib/firestore.js';
import { hashear_contrasenna } from '../lib/password.js';
import { generar_token_verificacion } from '../lib/verificacion_dns.js';

const aplicar = process.argv.includes('--aplicar');

// --- IPs del despliegue (salidas de terraform) ------------------------------

const IP_CACHE = { CR: '34.122.172.37', ES: '34.53.129.191', BR: '34.95.138.204' };
const IP_APACHE = '136.111.140.104';
const IP_JAVA = '35.253.162.152';

// --- Credenciales generadas -------------------------------------------------

const USUARIO = 'demo@zonal.cr';
const CONTRASENNA = randomBytes(9).toString('base64url');
const API_KEY = randomBytes(24).toString('hex');

// --- Lo que va a quedar -----------------------------------------------------

const ZONAS = {
  CR: { ip_cache: IP_CACHE.CR, region: 'us-central1' },
  ES: { ip_cache: IP_CACHE.ES, region: 'europe-west1' },
  BR: { ip_cache: IP_CACHE.BR, region: 'southamerica-east1' },
  /* Comodin para los paises sin documento propio. El resolvedor busca por id
   * exacto, asi que una consulta desde un pais no listado da 404 y el DNS no
   * responde nada. Esto no lo arregla solo (habria que cambiar el repo para
   * que caiga aqui), pero deja el dato listo. */
  ZZ: { ip_cache: IP_CACHE.CR, region: 'us-central1' },
};

/* Cada dominio con sus reglas de URL. Los cuatro cubren un escenario distinto
 * del enunciado, y el quinto (djdrll.privatedns.org) se registra en vivo. */
const DOMINIOS = [
  {
    // 1. REST API en Java, autenticacion por usuario y contrasenna
    id: 'api.demo.cr',
    origen_host: IP_JAVA,
    origen_puerto: 7070,
    tls: false,
    algoritmo: 'LRU',
    urls: [
      {
        patron: '/noticias*',
        tamanno_cache: 2097152,
        autenticacion: 'usuario',
        // El cache deduce el tipo de la extension de la URL. /noticias no
        // tiene ninguna, asi que cae en "otros": sin esta entrada la regla
        // existiria pero no aplicaria TTL a nada.
        tipos_archivo: { otros: { ttl: 60, algoritmo: 'LRU' } },
      },
    ],
  },
  {
    // 2. Apache, autenticacion por x-api-key
    id: 'sitio.demo.cr',
    origen_host: IP_APACHE,
    origen_puerto: 80,
    tls: false,
    algoritmo: 'LRU',
    api_keys: [API_KEY],
    urls: [
      {
        patron: '/*',
        tamanno_cache: 5242880,
        autenticacion: 'api_key',
        tipos_archivo: {
          html: { ttl: 120, algoritmo: 'LRU' },
          css: { ttl: 3600, algoritmo: 'LFU' },
          js: { ttl: 3600, algoritmo: 'LFU' },
          png: { ttl: 86400, algoritmo: 'LFU' },
          json: { ttl: 60, algoritmo: 'FIFO' },
        },
      },
    ],
  },
  {
    // 3. Sitio externo por HTTPS, con configuracion distinta por URL
    id: 'crhoy.com',
    origen_host: 'crhoy.com',
    origen_puerto: 443,
    tls: true,
    algoritmo: 'LRU',
    urls: [
      /* Tres reglas sobre el mismo dominio, de mas a menos especifica. El
       * orden lo decide la cantidad de caracteres literales del patron, asi
       * que /tecnologia/* (12) gana sobre /* (0). */
      {
        patron: '/tecnologia/*',
        tamanno_cache: 10485760,
        autenticacion: 'ninguna',
        tipos_archivo: {
          html: { ttl: 30, algoritmo: 'MRU' },
          png: { ttl: 604800, algoritmo: 'LFU' },
          jpg: { ttl: 604800, algoritmo: 'LFU' },
        },
      },
      {
        patron: '/deportes/*',
        tamanno_cache: 5242880,
        autenticacion: 'ninguna',
        tipos_archivo: {
          html: { ttl: 900, algoritmo: 'FIFO' },
          jpg: { ttl: 172800, algoritmo: 'LRU' },
        },
      },
      {
        patron: '/*',
        tamanno_cache: 20971520,
        autenticacion: 'ninguna',
        tipos_archivo: {
          html: { ttl: 300, algoritmo: 'LRU' },
          css: { ttl: 86400, algoritmo: 'LFU' },
          js: { ttl: 86400, algoritmo: 'LFU' },
          png: { ttl: 86400, algoritmo: 'LFU' },
          jpg: { ttl: 86400, algoritmo: 'LFU' },
          otros: { ttl: 600, algoritmo: 'LRU' },
        },
      },
    ],
  },
  {
    // 4. Sitio externo por HTTP plano.
    //
    // neverssl.com existe justamente para esto: fue hecho para destrabar
    // portales cautivos de wifi, asi que su compromiso es no redirigir nunca a
    // HTTPS. Los demas sitios en HTTP pueden migrar cualquier dia.
    id: 'neverssl.com',
    origen_host: 'neverssl.com',
    origen_puerto: 80,
    tls: false,
    algoritmo: 'FIFO',
    urls: [
      {
        patron: '/*',
        tamanno_cache: 1048576,
        autenticacion: 'ninguna',
        tipos_archivo: { html: { ttl: 600, algoritmo: 'FIFO' } },
      },
    ],
  },
];

// --- Inspeccion de lo que hay hoy -------------------------------------------

const COLECCIONES_A_BORRAR = ['dominios', 'usuarios', 'sesiones'];

console.log('=== SE VA A BORRAR ===');
for (const nombre of COLECCIONES_A_BORRAR) {
  const docs = (await db.collection(nombre).get()).docs;
  console.log(`  ${nombre}: ${docs.length} documento(s)`);
  for (const d of docs) console.log(`      ${d.id}`);
}
const zonasViejas = (await db.collection('zonas').get()).docs;
console.log(`  zonas: ${zonasViejas.length} documento(s), se reemplazan`);
for (const d of zonasViejas) console.log(`      ${d.id} -> ${d.data().ip_cache}`);

console.log('\n=== NO SE TOCA ===');
const n = (await db.collection('ip_to_country').count().get()).data().count;
console.log(`  ip_to_country: ${n} rangos`);

console.log('\n=== SE VA A CREAR ===');
console.log(`  usuarios: ${USUARIO}`);
for (const [pais, z] of Object.entries(ZONAS)) console.log(`  zonas/${pais} -> ${z.ip_cache}`);
for (const d of DOMINIOS) {
  const auth = [...new Set(d.urls.map((u) => u.autenticacion))].join(', ');
  console.log(`  dominios/${d.id}`);
  console.log(`      origen ${d.origen_host}:${d.origen_puerto} tls=${d.tls} | ${d.urls.length} regla(s) | auth: ${auth}`);
}
console.log('  dominios/djdrll.privatedns.org  -> NO, se registra en vivo');

if (!aplicar) {
  console.log('\nSimulacro. Repita con --aplicar para ejecutarlo.');
  process.exit(0);
}

// --- Ejecucion --------------------------------------------------------------

async function borrar_coleccion(nombre) {
  const docs = (await db.collection(nombre).get()).docs;
  for (const d of docs) {
    // Las subcolecciones no se borran con el documento padre: quedarian
    // huerfanas y reaparecerian en una consulta de grupo.
    for (const sub of ['urls', 'apikeys']) {
      const hijos = await d.ref.collection(sub).get();
      await Promise.all(hijos.docs.map((h) => h.ref.delete()));
    }
    await d.ref.delete();
  }
  return docs.length;
}

console.log('\n--- borrando ---');
for (const nombre of COLECCIONES_A_BORRAR) {
  console.log(`  ${nombre}: ${await borrar_coleccion(nombre)} borrado(s)`);
}
for (const d of zonasViejas) await d.ref.delete();
console.log(`  zonas: ${zonasViejas.length} borrado(s)`);

console.log('\n--- creando ---');

for (const [pais, datos] of Object.entries(ZONAS)) {
  await db.collection('zonas').doc(pais).set(datos);
}
console.log(`  zonas: ${Object.keys(ZONAS).length}`);

const { sal, hash } = await hashear_contrasenna(CONTRASENNA);
await db.collection('usuarios').doc(USUARIO).set({
  hash_contrasenna: hash,
  sal,
  dominios: DOMINIOS.map((d) => d.id),
  creado: Date.now(),
});
console.log(`  usuarios: ${USUARIO} con ${DOMINIOS.length} dominios`);

for (const d of DOMINIOS) {
  const ref = db.collection('dominios').doc(d.id);
  await ref.set({
    propietario: USUARIO,
    // Verificados de entrada: la demostracion del TXT se hace en vivo con
    // djdrll.privatedns.org, que es un dominio sobre el que si hay control.
    verificado: true,
    token_verificacion: generar_token_verificacion(),
    origen_host: d.origen_host,
    origen_puerto: d.origen_puerto,
    tls: d.tls,
    tamanno_maximo: 10485760,
    ttl: 300,
    algoritmo: d.algoritmo,
    requiere_auth: false,
    creado: Date.now(),
  });

  for (const u of d.urls) {
    await ref.collection('urls').add({ ...u, creado: Date.now() });
  }
  for (const clave of d.api_keys ?? []) {
    await ref.collection('apikeys').add({ clave, descripcion: 'Demostracion', creado: Date.now() });
  }
  console.log(`  dominios/${d.id}: ${d.urls.length} regla(s), ${(d.api_keys ?? []).length} llave(s)`);
}

// --- Credenciales a un archivo, no a la pantalla ----------------------------

const destino = new URL('./credenciales-demo.txt', import.meta.url).pathname;
writeFileSync(destino, [
  '# Credenciales de la demostracion. Generadas por sembrar_demo.mjs.',
  '# Este archivo esta en .gitignore: no se versiona.',
  '',
  `usuario:     ${USUARIO}`,
  `contrasenna: ${CONTRASENNA}`,
  '',
  `api key de sitio.demo.cr (encabezado x-api-key):`,
  `  ${API_KEY}`,
  '',
].join('\n'));

console.log(`\nCredenciales escritas en ${destino}`);
console.log('Falta reiniciar las tres caches para que recarguen la configuracion.');
process.exit(0);
