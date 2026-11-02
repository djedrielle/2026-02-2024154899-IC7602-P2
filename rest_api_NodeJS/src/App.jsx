import { Routes, Route, NavLink, Navigate, useNavigate } from 'react-router'
import { PaginaLogin } from './paginas/PaginaLogin.jsx'
import { PaginaRegistro } from './paginas/PaginaRegistro.jsx'
import { PaginaDominios } from './paginas/PaginaDominios.jsx'
import { PaginaDominio } from './paginas/PaginaDominio.jsx'
import { leerSesion, borrarSesion } from './sesion.js'
import { Boton } from './componentes/Ui.jsx'

/* Protege las rutas de administracion. Es una comodidad del UI, no una medida
 * de seguridad: quien manipule el navegador puede saltarla. La proteccion real
 * esta en el API, que valida la sesion en cada endpoint. */
function Privada({ children }) {
    return leerSesion() ? children : <Navigate to="/login" replace />
}

function Encabezado() {
    const navegar = useNavigate()
    const sesion = leerSesion()

    const enlace = ({ isActive }) =>
        `text-sm transition-colors ${isActive ? 'text-acento' : 'text-tenue hover:text-texto'}`

    return (
        <header className="border-b border-borde bg-panel">
            <div className="mx-auto flex max-w-4xl flex-wrap items-center justify-between gap-3 px-4 py-3">
                <NavLink to="/" className="font-semibold">Zonal Cache</NavLink>
                <nav className="flex items-center gap-4">
                    {sesion ? (
                        <>
                            <NavLink to="/dominios" className={enlace}>Mis dominios</NavLink>
                            <span className="text-sm text-tenue">{sesion.usuario}</span>
                            <Boton variante="secundario" onClick={() => { borrarSesion(); navegar('/login') }}>
                                Salir
                            </Boton>
                        </>
                    ) : (
                        <>
                            <NavLink to="/login" className={enlace}>Entrar</NavLink>
                            <NavLink to="/registro" className={enlace}>Crear cuenta</NavLink>
                        </>
                    )}
                </nav>
            </div>
        </header>
    )
}

function Inicio() {
    return (
        <main className="mx-auto flex max-w-2xl flex-col gap-4 px-4 py-16 text-center">
            <h1 className="text-3xl font-semibold">Panel de administración</h1>
            <p className="text-tenue">
                Registre sus dominios, verifíquelos por registro TXT y configure las reglas
                de caché por URL que aplicarán las cachés zonales.
            </p>
            <div className="flex justify-center gap-3 pt-2">
                <NavLink to="/dominios"><Boton>Mis dominios</Boton></NavLink>
                <NavLink to="/registro"><Boton variante="secundario">Crear cuenta</Boton></NavLink>
            </div>
        </main>
    )
}

export default function App() {
    return (
        <div className="min-h-screen">
            <Encabezado />
            <Routes>
                <Route path="/" element={<Inicio />} />
                <Route path="/login" element={<PaginaLogin />} />
                <Route path="/registro" element={<PaginaRegistro />} />
                <Route path="/dominios" element={<Privada><PaginaDominios /></Privada>} />
                <Route path="/dominios/:dominio" element={<Privada><PaginaDominio /></Privada>} />
                <Route path="*" element={<main className="px-4 py-16 text-center text-tenue">Página no encontrada</main>} />
            </Routes>
        </div>
    )
}
