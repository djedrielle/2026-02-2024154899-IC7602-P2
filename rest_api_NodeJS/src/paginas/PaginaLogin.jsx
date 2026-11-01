import { useState } from 'react';
import { useSearchParams } from 'react-router';
import { login } from '../api/auth.js';

/* Interpreta el parametro ?redirect= como URL.
 *
 * La cache lo arma concatenando el encabezado Host con la ruta, asi que puede
 * llegar sin esquema ("127.0.0.1/"). new URL() exige una URL absoluta y lanza
 * con esa forma, de modo que se reintenta anteponiendo http:// antes de darla
 * por invalida. Devuelve null solo si ninguna de las dos formas sirve. */
function interpretarDestino(destino) {
    if (!destino) return null;

    for (const candidato of [destino, `http://${destino}`]) {
        try {
            const url = new URL(candidato);
            if (url.protocol === 'http:' || url.protocol === 'https:') return url;
        } catch {
            // se prueba la forma siguiente
        }
    }
    return null;
}

/* Devuelve una copia de la URL con el token agregado como parametro.
 *
 * searchParams.set decide entre '?' y '&' segun si la URL ya traia query, y
 * escapa el valor. Concatenar a mano falla con /ruta?x=1. */
function conSesion(url, token) {
    const copia = new URL(url);
    copia.searchParams.set('session', token);
    return copia;
}

/* El host del destino tiene que estar entre los dominios del usuario.
 *
 * Sin esta verificacion habria un open redirect: alguien manda un enlace
 * /login?redirect=https://sitio-ajeno.com, la victima se autentica confiando en
 * el dominio del UI, y termina en el sitio del atacante con un token valido en
 * la URL. */
function estaAutorizado(url, dominios) {
    return url !== null && dominios.includes(url.hostname);
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
    const [urlPrueba, setUrlPrueba] = useState(null);

    async function enviar(e) {
        e.preventDefault(); // sin esto el navegador recarga la pagina y aborta el fetch
        setError(null);
        setAviso(null);
        setCargando(true);

        try {
            const s = await login(email, contrasenna);
            const dominios = s.dominios ?? [];
            const destino = interpretarDestino(redirect);

            if (redirect !== null) {
                if (destino === null) {
                    setAviso(`No se pudo interpretar el destino solicitado ("${redirect}"), así que no se redirigió.`);
                } else if (!estaAutorizado(destino, dominios)) {
                    setAviso(`El dominio del destino (${destino.hostname}) no está entre tus dominios autorizados, así que no se redirigió.`);
                } else {
                    window.location.href = conSesion(destino, s.token).toString();
                    return;
                }
            }

            /* Solo se arma una URL de prueba hacia un destino autorizado. Si el
             * destino no lo estaba, se cae al primer dominio del usuario: mostrar
             * el token pegado a un host ajeno anularia la verificacion de arriba. */
            const base = estaAutorizado(destino, dominios)
                ? destino
                : new URL(`http://${dominios[0] ?? 'midominio.com'}/`);

            setUrlPrueba(conSesion(base, s.token).toString());
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
                    <pre>{urlPrueba}</pre>
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
