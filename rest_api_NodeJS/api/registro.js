import { crear_usuario, autenticar_usuario } from '../repos/usuarios_repo.js';
import { crear_sesion } from '../repos/sesiones_repo.js';

const LARGO_MINIMO_CONTRASENNA = 8;

/* Alta de cuenta. Tras crearla se inicia sesion de una vez, para que la persona
 * no tenga que escribir las credenciales dos veces seguidas. */
export default async function handler(req, res) {
    if (req.method !== 'POST') return res.status(405).json({ error: 'Método no permitido' });

    const { email, contrasenna } = req.body ?? {};

    if (typeof email !== 'string' || !email.includes('@') || email.trim() === '') {
        return res.status(400).json({ error: 'El correo no es válido' });
    }
    if (typeof contrasenna !== 'string' || contrasenna.length < LARGO_MINIMO_CONTRASENNA) {
        return res.status(400).json({ error: `La contraseña debe tener al menos ${LARGO_MINIMO_CONTRASENNA} caracteres` });
    }

    try {
        const r = await crear_usuario(email.trim().toLowerCase(), contrasenna, []);
        if (!r.creado) return res.status(409).json({ error: r.motivo });

        const usuario = await autenticar_usuario(email.trim().toLowerCase(), contrasenna);
        const { token, expiracion } = await crear_sesion(usuario.email, usuario.dominios);

        res.status(201).json({ token, expiracion, usuario: usuario.email, dominios: usuario.dominios });
    } catch (e) {
        console.error('error en registro:', e);
        res.status(500).json({ error: 'Error interno' });
    }
}
