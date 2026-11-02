import { useState } from 'react';
import { useNavigate, Link } from 'react-router';
import { registro } from '../api/auth.js';
import { guardarSesion } from '../sesion.js';
import { Boton, Campo, Tarjeta, Aviso } from '../componentes/Ui.jsx';

export function PaginaRegistro() {
    const navegar = useNavigate();
    const [email, setEmail] = useState('');
    const [contrasenna, setContrasenna] = useState('');
    const [repetida, setRepetida] = useState('');
    const [error, setError] = useState(null);
    const [cargando, setCargando] = useState(false);

    async function enviar(e) {
        e.preventDefault(); // sin esto el navegador recarga y aborta el fetch
        setError(null);

        /* Se compara aqui y no en el servidor porque es un error de tipeo, no una
         * regla de negocio: el API nunca recibe la segunda contrasenna. */
        if (contrasenna !== repetida) {
            setError('Las contraseñas no coinciden');
            return;
        }

        setCargando(true);
        try {
            const s = await registro(email, contrasenna);
            guardarSesion(s);
            navegar('/dominios');
        } catch (err) {
            setError(err.message);
        } finally {
            setCargando(false);
        }
    }

    return (
        <main className="mx-auto flex max-w-md flex-col gap-4 px-4 py-10">
            <h1 className="text-2xl font-semibold">Crear cuenta</h1>
            <Tarjeta>
                <form onSubmit={enviar} className="flex flex-col gap-4">
                    <Campo
                        etiqueta="Correo" type="email" value={email} required autoComplete="email"
                        onChange={(e) => setEmail(e.target.value)}
                    />
                    <Campo
                        etiqueta="Contraseña" type="password" value={contrasenna} required
                        autoComplete="new-password" ayuda="Mínimo 8 caracteres"
                        onChange={(e) => setContrasenna(e.target.value)}
                    />
                    <Campo
                        etiqueta="Repetir contraseña" type="password" value={repetida} required
                        autoComplete="new-password"
                        onChange={(e) => setRepetida(e.target.value)}
                    />
                    <Aviso tipo="error">{error}</Aviso>
                    <Boton type="submit" disabled={cargando}>
                        {cargando ? 'Creando...' : 'Crear cuenta'}
                    </Boton>
                </form>
            </Tarjeta>
            <p className="text-center text-sm text-tenue">
                ¿Ya tiene cuenta? <Link to="/login" className="text-acento hover:underline">Inicie sesión</Link>
            </p>
        </main>
    );
}
