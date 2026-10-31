import { Routes, Route, NavLink } from 'react-router'
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
      </Routes>
    </>
  )
}

export default App
