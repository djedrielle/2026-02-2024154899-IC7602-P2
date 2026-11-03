import { useEffect, useState } from 'react';
import { useParams, Link } from 'react-router';
import {
    consultarDominio, verificarDominio,
    listarUrls, crearUrl, actualizarUrl, borrarUrl,
    listarApiKeys, crearApiKey, borrarApiKey,
} from '../api/dominios.js';
import { Boton, Campo, Selector, Tarjeta, Aviso, Insignia, Copiable } from '../componentes/Ui.jsx';

const ALGORITMOS = ['LRU', 'LFU', 'FIFO', 'MRU', 'RANDOM'].map((a) => ({ valor: a, texto: a }));
const AUTENTICACIONES = [
    { valor: 'ninguna', texto: 'Ninguna' },
    { valor: 'api_key', texto: 'API Key (x-api-key)' },
    { valor: 'usuario', texto: 'Usuario y contraseña' },
];

const REGLA_VACIA = { patron: '/*', tamanno_cache: 1048576, autenticacion: 'ninguna', tipos_archivo: {} };

/* Cuenta los caracteres literales del patron, sin comodines. Es el mismo criterio
 * que usa el API para ordenar: a mas literal, mas especifico. Se muestra para que
 * se entienda por que una regla gana sobre otra. */
function especificidad(patron) {
    return patron.replace(/\*/g, '').length;
}

export function PaginaDominio() {
    const { dominio } = useParams();

    const [info, setInfo] = useState(null);
    const [urls, setUrls] = useState([]);
    const [tipos, setTipos] = useState([]);
    const [apikeys, setApikeys] = useState([]);
    const [error, setError] = useState(null);
    const [aviso, setAviso] = useState(null);
    const [cargando, setCargando] = useState(true);

    const [regla, setRegla] = useState(REGLA_VACIA);
    const [editando, setEditando] = useState(null); // id de la regla en edicion, o null
    const [descripcionClave, setDescripcionClave] = useState('');

    async function recargar() {
        setError(null);
        try {
            const [i, u, k] = await Promise.all([
                consultarDominio(dominio), listarUrls(dominio), listarApiKeys(dominio),
            ]);
            setInfo(i);
            setUrls(u.urls);
            setTipos(u.tipos_archivo);
            setApikeys(k.apikeys);
        } catch (err) {
            setError(err.message);
        } finally {
            setCargando(false);
        }
    }

    useEffect(() => { recargar(); }, [dominio]);

    async function verificar() {
        setError(null);
        setAviso(null);
        try {
            const r = await verificarDominio(dominio);
            if (r.verificado) {
                setAviso({ tipo: 'exito', texto: r.motivo });
                await recargar();
            } else {
                const encontrados = r.encontrados?.length
                    ? ` Se encontraron: ${r.encontrados.join(', ')}`
                    : '';
                setAviso({ tipo: 'alerta', texto: r.motivo + encontrados });
            }
        } catch (err) {
            setError(err.message);
        }
    }

    async function guardarRegla(e) {
        e.preventDefault();
        setError(null);
        try {
            if (editando) await actualizarUrl(dominio, editando, regla);
            else await crearUrl(dominio, regla);
            setRegla(REGLA_VACIA);
            setEditando(null);
            await recargar();
        } catch (err) {
            setError(err.message);
        }
    }

    function editar(u) {
        setEditando(u.id);
        setRegla({
            patron: u.patron,
            tamanno_cache: u.tamanno_cache,
            autenticacion: u.autenticacion,
            tipos_archivo: u.tipos_archivo ?? {},
        });
    }

    async function eliminarRegla(id) {
        if (!confirm('¿Eliminar esta regla de URL?')) return;
        try {
            await borrarUrl(dominio, id);
            if (editando === id) { setEditando(null); setRegla(REGLA_VACIA); }
            await recargar();
        } catch (err) { setError(err.message); }
    }

    /* Activar un tipo lo agrega con valores por defecto; desactivarlo lo quita
     * del objeto, que es como el API distingue "no configurado". */
    function alternarTipo(tipo) {
        const copia = { ...regla.tipos_archivo };
        if (copia[tipo]) delete copia[tipo];
        else copia[tipo] = { ttl: 300, algoritmo: 'LRU' };
        setRegla({ ...regla, tipos_archivo: copia });
    }

    function cambiarTipo(tipo, clave, valor) {
        setRegla({
            ...regla,
            tipos_archivo: { ...regla.tipos_archivo, [tipo]: { ...regla.tipos_archivo[tipo], [clave]: valor } },
        });
    }

    async function nuevaClave(e) {
        e.preventDefault();
        setError(null);
        try {
            await crearApiKey(dominio, descripcionClave);
            setDescripcionClave('');
            await recargar();
        } catch (err) { setError(err.message); }
    }

    if (cargando) return <main className="px-4 py-10 text-sm text-tenue">Cargando...</main>;
    if (info === null) {
        return (
            <main className="mx-auto max-w-3xl px-4 py-10">
                <Aviso tipo="error">{error ?? 'Dominio no encontrado'}</Aviso>
            </main>
        );
    }

    return (
        <main className="mx-auto flex max-w-4xl flex-col gap-6 px-4 py-10">
            <div className="flex flex-col gap-1">
                <Link to="/dominios" className="text-sm text-tenue hover:text-acento">← Mis dominios</Link>
                <div className="flex items-center gap-3">
                    <h1 className="text-2xl font-semibold">{dominio}</h1>
                    <Insignia ok={info.verificado}>
                        {info.verificado ? 'verificado' : 'sin verificar'}
                    </Insignia>
                </div>
            </div>

            <Aviso tipo="error">{error}</Aviso>
            {aviso && <Aviso tipo={aviso.tipo}>{aviso.texto}</Aviso>}

            {!info.verificado && (
                <Tarjeta className="flex flex-col gap-3 border-alerta/50">
                    <h2 className="font-medium">Verificación de propiedad</h2>
                    <p className="text-sm text-tenue">
                        Cree un registro <strong>TXT</strong> en <code>{dominio}</code> con este valor:
                    </p>
                    <Copiable valor={info.registro_txt} />
                    <p className="text-xs text-tenue">
                        Compruébelo con <code>dig +short TXT {dominio} @8.8.8.8</code> antes de verificar.
                        Mientras el caché no lo vea verificado, no servirá este dominio.
                    </p>
                    <Boton onClick={verificar} className="self-start">Verificar ahora</Boton>
                </Tarjeta>
            )}

            <section className="flex flex-col gap-3">
                <h2 className="text-lg font-medium">Reglas de URL</h2>
                <p className="text-sm text-tenue">
                    Se evalúan de más a menos específica: gana la primera que coincida.
                    Use <code>*</code> como comodín, por ejemplo <code>/imagenes/*</code>.
                </p>

                {urls.length === 0 ? (
                    <Tarjeta><p className="text-sm text-tenue">Sin reglas. Agregue una abajo.</p></Tarjeta>
                ) : (
                    <ul className="flex flex-col gap-2">
                        {urls.map((u) => (
                            <Tarjeta key={u.id} className="flex flex-wrap items-center justify-between gap-3">
                                <div className="flex flex-col gap-1">
                                    <code className="font-mono text-sm text-acento">{u.patron}</code>
                                    <span className="text-xs text-tenue">
                                        {u.tamanno_cache.toLocaleString()} bytes · auth: {u.autenticacion} ·
                                        especificidad {especificidad(u.patron)} ·
                                        tipos: {Object.keys(u.tipos_archivo ?? {}).join(', ') || 'ninguno'}
                                    </span>
                                </div>
                                <div className="flex gap-2">
                                    <Boton variante="secundario" onClick={() => editar(u)}>Editar</Boton>
                                    <Boton variante="peligro" onClick={() => eliminarRegla(u.id)}>Eliminar</Boton>
                                </div>
                            </Tarjeta>
                        ))}
                    </ul>
                )}

                <Tarjeta>
                    <form onSubmit={guardarRegla} className="flex flex-col gap-4">
                        <h3 className="font-medium">{editando ? 'Editar regla' : 'Nueva regla'}</h3>
                        <div className="grid gap-4 sm:grid-cols-3">
                            <Campo
                                etiqueta="Patrón" value={regla.patron} required placeholder="/imagenes/*"
                                onChange={(e) => setRegla({ ...regla, patron: e.target.value })}
                            />
                            <Campo
                                etiqueta="Tamaño de caché (bytes)" type="number" required
                                value={regla.tamanno_cache}
                                onChange={(e) => setRegla({ ...regla, tamanno_cache: e.target.value })}
                            />
                            <Selector
                                etiqueta="Autenticación" value={regla.autenticacion} opciones={AUTENTICACIONES}
                                onChange={(e) => setRegla({ ...regla, autenticacion: e.target.value })}
                            />
                        </div>

                        <div className="flex flex-col gap-2">
                            <span className="text-sm font-medium text-tenue">
                                TTL y política por tipo de archivo
                            </span>
                            <div className="flex flex-wrap gap-2">
                                {tipos.map((t) => (
                                    <button
                                        key={t} type="button" onClick={() => alternarTipo(t)}
                                        className={`rounded-full px-3 py-1 text-xs transition-colors ${
                                            regla.tipos_archivo[t]
                                                ? 'bg-acento text-white'
                                                : 'border border-borde text-tenue hover:border-acento/60'
                                        }`}
                                    >
                                        {t}
                                    </button>
                                ))}
                            </div>

                            {Object.entries(regla.tipos_archivo).map(([tipo, conf]) => (
                                <div key={tipo} className="grid items-end gap-3 rounded-md border border-borde p-3 sm:grid-cols-3">
                                    <span className="font-mono text-sm">{tipo}</span>
                                    <Campo
                                        etiqueta="TTL (s)" type="number" value={conf.ttl} required
                                        onChange={(e) => cambiarTipo(tipo, 'ttl', e.target.value)}
                                    />
                                    <Selector
                                        etiqueta="Algoritmo" value={conf.algoritmo} opciones={ALGORITMOS}
                                        onChange={(e) => cambiarTipo(tipo, 'algoritmo', e.target.value)}
                                    />
                                </div>
                            ))}
                        </div>

                        <div className="flex gap-2">
                            <Boton type="submit">{editando ? 'Guardar cambios' : 'Agregar regla'}</Boton>
                            {editando && (
                                <Boton variante="secundario" type="button"
                                    onClick={() => { setEditando(null); setRegla(REGLA_VACIA); }}>
                                    Cancelar
                                </Boton>
                            )}
                        </div>
                    </form>
                </Tarjeta>
            </section>

            <section className="flex flex-col gap-3">
                <h2 className="text-lg font-medium">API Keys</h2>
                <p className="text-sm text-tenue">
                    Se envían en el encabezado <code>x-api-key</code> de las peticiones a la caché.
                </p>

                {apikeys.length === 0 ? (
                    <Tarjeta><p className="text-sm text-tenue">Sin claves creadas.</p></Tarjeta>
                ) : (
                    <ul className="flex flex-col gap-2">
                        {apikeys.map((k) => (
                            <Tarjeta key={k.id} className="flex flex-col gap-2">
                                <div className="flex items-center justify-between gap-3">
                                    <span className="text-sm">{k.descripcion || 'sin descripción'}</span>
                                    <Boton variante="peligro" onClick={async () => {
                                        if (!confirm('¿Eliminar esta API key?')) return;
                                        await borrarApiKey(dominio, k.id);
                                        await recargar();
                                    }}>Eliminar</Boton>
                                </div>
                                <Copiable valor={k.clave} />
                            </Tarjeta>
                        ))}
                    </ul>
                )}

                <Tarjeta>
                    <form onSubmit={nuevaClave} className="flex flex-wrap items-end gap-3">
                        <Campo
                            etiqueta="Descripción" className="flex-1" value={descripcionClave}
                            placeholder="Para el cliente móvil"
                            onChange={(e) => setDescripcionClave(e.target.value)}
                        />
                        <Boton type="submit">Generar API key</Boton>
                    </form>
                </Tarjeta>
            </section>
        </main>
    );
}
