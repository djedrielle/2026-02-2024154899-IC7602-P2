import { Routes, Route, NavLink } from 'react-router'
import { PaginaLogin } from './paginas/PaginaLogin.jsx'
import './App.css'

function App() {

  return (
    <>
      <header>
        <span>
          Zonal Cache - Admin Panel
        </span>
        <nav>
          <NavLink to='/'>Inicio</NavLink>
        </nav>
      </header>
      <Routes>
        <Route path="/" element={<h1>Zonal Cache - Admin Panel</h1>} />
        <Route path="/login" element={<PaginaLogin />} />
      </Routes>
    </>
  )
}

export default App
