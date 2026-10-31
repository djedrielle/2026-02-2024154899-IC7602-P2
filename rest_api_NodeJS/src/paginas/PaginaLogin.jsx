import { useState } from 'react';
import { useSearchParams } from 'react-router';
import { login } from '../api/auth.js';

/* Valida el destino del parametro ?redirect= y le agrega el token.
 *
 * Sin esta verificacion habria un open redirect: alguien manda un enlace
 * /login?redirect=https://sitio-ajeno.com, la victima se autentica confiando en
 * el dominio del UI, y termina en el sitio del atacante con un token valido en
 * la URL. Por eso el host del destino tiene que estar entre los dominios que el
 * API le reconoce a ese usuario. */
function destinoAutorizado(destino, dominios, token) {
    let url;
    try {
        url = new URL(destino);
    } catch {
        return null; // no es una URL absoluta valida
    }

    if (url.protocol !== 'http:' && url.protocol !== 'https:') return null;
    if (!dominios.includes(url.hostname)) return null;

    /* searchParams.set decide entre '?' y '&' segun si la URL ya traia query,
     * y escapa el valor. Concatenar a mano falla con /ruta?x=1. */
    url.searchParams.set('session', token);
    return url;
}

export function PaginaLogin() {
    const [searchParams] = useSearchParams();
    const redirect = searchParams.get('redirect');

    const [email, setEmail] = useState('');
    const [contrasenna, setContrasenna] = useState('');
    const [cargando, setCargando] = useState(false);
    const [error, setError] = useState(null);
    const [sesion, setSesion] = useState(null);
    const [aviso, setAviso] = useState(null);
    const [copiado, setCopiado] = useState(false);

    async function enviar(e) {
        e.preventDefault(); // sin esto el navegador recarga la pagina y aborta el fetch
        setError(null);
        setAviso(null);
        setCargando(true);

        try {
            const s = await login(email, contrasenna);

            if (redirect !== null) {
                const destino = destinoAutorizado(redirect, s.dominios ?? [], s.token);
                if (destino !== null) {
                    window.location.href = destino.toString();
                    return;
                }
                setAviso('El destino solicitado no está entre tus dominios autorizados, así que no se redirigió.');
            }

            setSesion(s);
        } catch (err) {
            setError(err instanceof Error ? err.message : 'No se pudo iniciar sesión');
        } finally {
            setCargando(false);
        }
    }

    async function copiarToken() {
        if (sesion === null) return;
        try {
            await navigator.clipboard.writeText(sesion.token);
            setCopiado(true);
            setTimeout(() => setCopiado(false), 2000);
        } catch {
            setAviso('El navegador no permitió copiar. Seleccione el token manualmente.');
        }
    }

    /* Segundo modo del componente: ya hay sesion y no hubo a donde redirigir.
     * Esta es la pantalla de la demostracion, porque el enunciado recomienda
     * probar la cache con curl o Postman, y ahi el token se pasa a mano. */
    if (sesion !== null) {
        const vence = new Date(sesion.expiracion * 1000);
        const dominios = sesion.dominios ?? [];

        return (
            <main>
                <section>
                    <h2>Sesión iniciada</h2>
                    <p>Usuario: <strong>{sesion.usuario}</strong></p>
                    <p>Vence: {vence.toLocaleString()}</p>
                    <p>Dominios autorizados: {dominios.length > 0 ? dominios.join(', ') : 'ninguno'}</p>

                    {aviso && <p role="alert">{aviso}</p>}

                    <label>
                        Token de sesión
                        <input type="text" value={sesion.token} readOnly />
                    </label>
                    <button type="button" onClick={copiarToken}>
                        {copiado ? 'Copiado' : 'Copiar token'}
                    </button>

                    <p>Úselo contra la caché zonal así:</p>
                    <pre>{`curl -H "x-session: ${sesion.token}" http://${dominios[0] ?? 'midominio.com'}/`}</pre>
                </section>
            </main>
        );
    }

    return (
        <main>
            <section>
                <h2>Iniciar Sesión</h2>
                {redirect !== null && <p>Al autenticarse volverá a: {redirect}</p>}
                <form onSubmit={enviar}>
                    <label>
                        Correo
                        <input
                            type="email"
                            value={email}
                            onChange={(e) => setEmail(e.target.value)}
                            autoComplete="email"
                            required
                        />
                    </label>
                    <label>
                        Contraseña
                        <input
                            type="password"
                            value={contrasenna}
                            onChange={(e) => setContrasenna(e.target.value)}
                            autoComplete="current-password"
                            required
                        />
                    </label>
                    {error && <p role="alert">{error}</p>}
                    <button type="submit" disabled={cargando}>
                        {cargando ? 'Enviando...' : 'Enviar'}
                    </button>
                </form>
            </section>
        </main>
    );
}
