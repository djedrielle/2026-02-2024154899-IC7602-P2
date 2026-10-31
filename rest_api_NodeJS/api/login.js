import { autenticar_usuario } from '../repos/usuarios_repo.js';
import { crear_sesion } from '../repos/sesiones_repo.js';

/* Autenticacion por usuario y contrasenna. Este endpoint lo consume el UI, nunca
 * la cache zonal: la cache no ve credenciales, solo verifica sesiones ya creadas.
 *
 * La respuesta incluye 'dominios' porque el UI lo necesita para validar el destino
 * del parametro ?redirect= antes de navegar con el token. */
export default async function handler(req, res) {
    if (req.method !== 'POST') {
        return res.status(405).json({ error: 'Método no permitido' });
    }

    const { email, contrasenna } = req.body ?? {};

    if (typeof email !== 'string' || typeof contrasenna !== 'string'
        || email.trim() === '' || contrasenna === '') {
        return res.status(400).json({ error: 'Faltan el correo o la contraseña' });
    }

    try {
        const usuario = await autenticar_usuario(email.trim().toLowerCase(), contrasenna);

        /* Un solo mensaje para "no existe" y para "contrasenna incorrecta".
         * Distinguirlos le diria a cualquiera que correos estan registrados. */
        if (usuario === null) {
            return res.status(401).json({ error: 'Correo o contraseña incorrectos' });
        }

        const { token, expiracion } = await crear_sesion(usuario.email, usuario.dominios);

        res.status(200).json({
            token,
            expiracion,
            usuario: usuario.email,
            dominios: usuario.dominios,
        });
    } catch (e) {
        console.error('error en login:', e);
        res.status(500).json({ error: 'Error interno' });
    }
}
