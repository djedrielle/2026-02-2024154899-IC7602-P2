import { useState } from 'react';
import { useSearchParams, useNavigate, Link } from 'react-router';
import { login } from '../api/auth.js';
import { guardarSesion } from '../sesion.js';
import { Boton, Campo, Tarjeta, Aviso, Copiable } from '../componentes/Ui.jsx';

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
    const navegar = useNavigate();
    const redirect = searchParams.get('redirect');

    const [email, setEmail] = useState('');
    const [contrasenna, setContrasenna] = useState('');
    const [cargando, setCargando] = useState(false);
    const [error, setError] = useState(null);
    const [aviso, setAviso] = useState(null);
    const [sesion, setSesion] = useState(null);
    const [urlPrueba, setUrlPrueba] = useState(null);

    async function enviar(e) {
        e.preventDefault();
        setError(null);
        setAviso(null);
        setCargando(true);

        try {
            const s = await login(email, contrasenna);
            guardarSesion(s);

            const dominios = s.dominios ?? [];
            const destino = interpretarDestino(redirect);

            if (redirect !== null) {
                if (destino === null) {
                    setAviso(`No se pudo interpretar el destino solicitado ("${redirect}"), así que no se redirigió.`);
                } else if (!estaAutorizado(destino, dominios)) {
                    setAviso(`El dominio del destino (${destino.hostname}) no está entre sus dominios autorizados, así que no se redirigió.`);
                } else {
                    window.location.href = conSesion(destino, s.token).toString();
                    return;
                }

                /* Solo se arma una URL de prueba hacia un destino autorizado. Si el
                 * destino no lo estaba, se cae al primer dominio del usuario: mostrar
                 * el token pegado a un host ajeno anularia la verificacion de arriba.
                 *
                 * El esquema se hereda del destino cuando se pudo interpretar, porque
                 * una cache con terminacion TLS sirve por https y una URL con http://
                 * no funcionaria. Si no hay destino, no hay forma de saberlo desde
                 * aqui: la cache manda el redirect sin esquema. Se asume http, que es
                 * el caso de las caches sin TLS. */
                const esquema = destino !== null ? destino.protocol : 'http:';
                const base = estaAutorizado(destino, dominios)
                    ? destino
                    : new URL(`${esquema}//${dominios[0] ?? 'midominio.com'}/`);

                setUrlPrueba(conSesion(base, s.token).toString());
                setSesion(s);
                return;
            }

            // Inicio de sesion normal desde el panel: va a la administracion.
            navegar('/dominios');
        } catch (err) {
            setError(err.message);
        } finally {
            setCargando(false);
        }
    }

    /* Segundo modo del componente: hay sesion pero no hubo a donde redirigir.
     * Esta es la pantalla de la demostracion, porque el enunciado recomienda
     * probar la cache con curl o Postman, y ahi el token se pasa a mano. */
    if (sesion !== null) {
        const vence = new Date(sesion.expiracion * 1000);
        const dominios = sesion.dominios ?? [];

        return (
            <main className="mx-auto flex max-w-2xl flex-col gap-4 px-4 py-10">
                <h1 className="text-2xl font-semibold">Sesión iniciada</h1>
                <Tarjeta className="flex flex-col gap-4">
                    <dl className="grid grid-cols-[auto_1fr] gap-x-4 gap-y-1 text-sm">
                        <dt className="text-tenue">Usuario</dt><dd>{sesion.usuario}</dd>
                        <dt className="text-tenue">Vence</dt><dd>{vence.toLocaleString()}</dd>
                        <dt className="text-tenue">Dominios</dt>
                        <dd>{dominios.length > 0 ? dominios.join(', ') : 'ninguno'}</dd>
                    </dl>

                    <Aviso tipo="alerta">{aviso}</Aviso>

                    <div className="flex flex-col gap-2">
                        <span className="text-sm font-medium text-tenue">Token de sesión</span>
                        <Copiable valor={sesion.token} />
                    </div>

                    <div className="flex flex-col gap-2">
                        <span className="text-sm font-medium text-tenue">Úselo contra la caché zonal así</span>
                        <Copiable valor={urlPrueba} />
                    </div>
                </Tarjeta>
            </main>
        );
    }

    return (
        <main className="mx-auto flex max-w-md flex-col gap-4 px-4 py-10">
            <h1 className="text-2xl font-semibold">Iniciar sesión</h1>
            {redirect !== null && (
                <Aviso tipo="info">Al autenticarse volverá a: {redirect}</Aviso>
            )}
            <Tarjeta>
                <form onSubmit={enviar} className="flex flex-col gap-4">
                    <Campo
                        etiqueta="Correo" type="email" value={email} required autoComplete="email"
                        onChange={(e) => setEmail(e.target.value)}
                    />
                    <Campo
                        etiqueta="Contraseña" type="password" value={contrasenna} required
                        autoComplete="current-password"
                        onChange={(e) => setContrasenna(e.target.value)}
                    />
                    <Aviso tipo="error">{error}</Aviso>
                    <Boton type="submit" disabled={cargando}>
                        {cargando ? 'Enviando...' : 'Entrar'}
                    </Boton>
                </form>
            </Tarjeta>
            <p className="text-center text-sm text-tenue">
                ¿No tiene cuenta? <Link to="/registro" className="text-acento hover:underline">Cree una</Link>
            </p>
        </main>
    );
}
