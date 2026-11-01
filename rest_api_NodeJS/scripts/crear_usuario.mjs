/* Crea un usuario en la coleccion 'usuarios'. Sirve para sembrar la primera
 * cuenta, antes de que exista la pantalla de registro.
 *
 * Uso:
 *   GOOGLE_APPLICATION_CREDENTIALS=... node scripts/crear_usuario.mjs <correo> <contrasenna> <dominio>[,<dominio>...]
 */
import { crear_usuario } from '../repos/usuarios_repo.js';

const [email, contrasenna, dominios_csv] = process.argv.slice(2);

if (!email || !contrasenna) {
    console.error('Uso: node scripts/crear_usuario.mjs <correo> <contrasenna> [dominios separados por coma]');
    process.exit(1);
}

const dominios = (dominios_csv ?? '').split(',').map((d) => d.trim()).filter(Boolean);

const r = await crear_usuario(email.trim().toLowerCase(), contrasenna, dominios);

if (!r.creado) {
    console.error(`No se creó: ${r.motivo}`);
    process.exit(1);
}

console.log(`Usuario ${email} creado con dominios: ${dominios.join(', ') || '(ninguno)'}`);
