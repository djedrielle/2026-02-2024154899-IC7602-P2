import { useState } from 'react';
import { useSearchParams } from 'react-router';
import { login } from '../api/auth.js';

export function PaginaLogin() {
    const [searchParams] = useSearchParams();
    const redirect = searchParams.get('redirect');

    const [email, setEmail] = useState('');
    const [contrasenna, setContrasenna] = useState('');
    const [cargando, setCargando] = useState(false);
    const [error, setError] = useState(null);

    async function enviar(e) {
        e.preventDefault();
        setError(null);
        setCargando(true);
        try {
            await login(email, contrasenna);
            // redirigir al URL original
        } catch {
            setError("Correo o contraseña incorrectos.");
        } finally {
            setCargando(false);
        }
    }

    return (
        <main>
            <section>
                <h2>Iniciar Sesión</h2>
                <form onSubmit={enviar}>
                    <label>
                        Correo
                        <input type="email" value={email} onChange={(e) => setEmail(e.target.value)} required />
                    </label>
                    <label>
                        Contraseña
                        <input type="password" value={contrasenna} onChange={(e) => setContrasenna(e.target.value)} required />
                    </label>
                    {error && <p>{error}</p>}
                    <button disabled={cargando}>
                        {cargando ? "Cargando..." : "Enviar"}
                    </button>
                </form>
            </section>
        </main>
    )
}