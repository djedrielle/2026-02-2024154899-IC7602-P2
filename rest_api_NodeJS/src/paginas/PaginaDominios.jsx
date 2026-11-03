import { useEffect, useState } from 'react';
import { Link } from 'react-router';
import { listarDominios, crearDominio, borrarDominio } from '../api/dominios.js';
import { Boton, Campo, Selector, Tarjeta, Aviso, Insignia, Copiable } from '../componentes/Ui.jsx';

const ALGORITMOS = ['LRU', 'LFU', 'FIFO', 'MRU', 'RANDOM'].map((a) => ({ valor: a, texto: a }));

const NUEVO_VACIO = {
    dominio: '', origen_host: '', origen_puerto: 443, tls: true,
    tamanno_maximo: 10485760, ttl: 300, algoritmo: 'LRU',
};

export function PaginaDominios() {
    const [dominios, setDominios] = useState([]);
    const [cargando, setCargando] = useState(true);
    const [error, setError] = useState(null);
    const [nuevo, setNuevo] = useState(NUEVO_VACIO);
    const [recienCreado, setRecienCreado] = useState(null);
    const [mostrarForm, setMostrarForm] = useState(false);

    async function recargar() {
        setError(null);
        try {
            const r = await listarDominios();
            setDominios(r.dominios);
        } catch (err) {
            setError(err.message);
        } finally {
            setCargando(false);
        }
    }

    useEffect(() => { recargar(); }, []);

    async function agregar(e) {
        e.preventDefault();
        setError(null);
        try {
            const r = await crearDominio(nuevo);
            /* Se guarda aparte para mostrar el TXT: es el unico momento en que la
             * persona ve que registro debe crear antes de poder verificar. */
            setRecienCreado(r);
            setNuevo(NUEVO_VACIO);
            setMostrarForm(false);
            await recargar();
        } catch (err) {
            setError(err.message);
        }
    }

    async function eliminar(dominio) {
        if (!confirm(`¿Eliminar ${dominio} y toda su configuración de URLs y API keys?`)) return;
        setError(null);
        try {
            await borrarDominio(dominio);
            await recargar();
        } catch (err) {
            setError(err.message);
        }
    }

    const campo = (k) => (e) => setNuevo({ ...nuevo, [k]: e.target.value });

    return (
        <main className="mx-auto flex max-w-4xl flex-col gap-5 px-4 py-10">
            <div className="flex items-center justify-between">
                <h1 className="text-2xl font-semibold">Mis dominios</h1>
                <Boton onClick={() => setMostrarForm(!mostrarForm)}>
                    {mostrarForm ? 'Cancelar' : 'Registrar dominio'}
                </Boton>
            </div>

            <Aviso tipo="error">{error}</Aviso>

            {recienCreado && (
                <Tarjeta className="flex flex-col gap-3 border-acento/50">
                    <h2 className="font-medium">Falta verificar {recienCreado.dominio}</h2>
                    <p className="text-sm text-tenue">
                        Cree un registro <strong>TXT</strong> en <code>{recienCreado.dominio}</code> con
                        este valor. Solo quien administra la zona DNS puede hacerlo, y por eso sirve
                        como prueba de propiedad.
                    </p>
                    <Copiable valor={recienCreado.registro_txt} />
                    <p className="text-xs text-tenue">
                        La propagación puede tardar desde minutos hasta horas según el TTL de su zona.
                    </p>
                    <Boton variante="secundario" onClick={() => setRecienCreado(null)}>Entendido</Boton>
                </Tarjeta>
            )}

            {mostrarForm && (
                <Tarjeta>
                    <form onSubmit={agregar} className="grid gap-4 sm:grid-cols-2">
                        <Campo
                            etiqueta="Dominio" value={nuevo.dominio} required className="sm:col-span-2"
                            placeholder="midominio.com" onChange={campo('dominio')}
                        />
                        <Campo
                            etiqueta="Host de origen" value={nuevo.origen_host} required
                            placeholder="www.ejemplo.com" onChange={campo('origen_host')}
                            ayuda="A dónde va la caché cuando no tiene el recurso"
                        />
                        <Campo
                            etiqueta="Puerto de origen" type="number" value={nuevo.origen_puerto}
                            required onChange={campo('origen_puerto')} ayuda="443 con TLS, 80 sin TLS"
                        />
                        <Selector
                            etiqueta="Origen por HTTPS" value={String(nuevo.tls)}
                            opciones={[{ valor: 'true', texto: 'Sí' }, { valor: 'false', texto: 'No' }]}
                            onChange={(e) => setNuevo({ ...nuevo, tls: e.target.value === 'true' })}
                        />
                        <Selector
                            etiqueta="Algoritmo de reemplazo" value={nuevo.algoritmo}
                            opciones={ALGORITMOS} onChange={campo('algoritmo')}
                        />
                        <Campo
                            etiqueta="Tamaño máximo (bytes)" type="number" value={nuevo.tamanno_maximo}
                            required onChange={campo('tamanno_maximo')}
                        />
                        <Campo
                            etiqueta="TTL (segundos)" type="number" value={nuevo.ttl}
                            required onChange={campo('ttl')}
                        />
                        <Boton type="submit" className="sm:col-span-2">Registrar</Boton>
                    </form>
                </Tarjeta>
            )}

            {cargando ? (
                <p className="text-sm text-tenue">Cargando...</p>
            ) : dominios.length === 0 ? (
                <Tarjeta>
                    <p className="text-sm text-tenue">
                        Todavía no tiene dominios registrados. Use «Registrar dominio» para empezar.
                    </p>
                </Tarjeta>
            ) : (
                <ul className="flex flex-col gap-3">
                    {dominios.map((d) => (
                        <Tarjeta key={d.dominio} className="flex flex-wrap items-center justify-between gap-3">
                            <div className="flex flex-col gap-1">
                                <div className="flex items-center gap-2">
                                    <Link to={`/dominios/${d.dominio}`} className="font-medium text-acento hover:underline">
                                        {d.dominio}
                                    </Link>
                                    <Insignia ok={d.verificado}>
                                        {d.verificado ? 'verificado' : 'sin verificar'}
                                    </Insignia>
                                </div>
                                <span className="text-xs text-tenue">
                                    origen: {d.origen_host ?? '—'}:{d.origen_puerto} {d.tls && '(TLS)'}
                                </span>
                            </div>
                            <div className="flex gap-2">
                                <Link to={`/dominios/${d.dominio}`}>
                                    <Boton variante="secundario">Administrar</Boton>
                                </Link>
                                <Boton variante="peligro" onClick={() => eliminar(d.dominio)}>Eliminar</Boton>
                            </div>
                        </Tarjeta>
                    ))}
                </ul>
            )}
        </main>
    );
}
