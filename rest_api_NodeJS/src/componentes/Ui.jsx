/* Piezas visuales compartidas. Centralizarlas evita repetir cadenas largas de
 * clases de Tailwind y mantiene el aspecto coherente entre pantallas. */

export function Boton({ variante = 'primario', className = '', ...props }) {
    const base = 'rounded-md px-3 py-2 text-sm font-medium transition-colors disabled:opacity-50 disabled:cursor-not-allowed';
    const variantes = {
        primario: 'bg-acento text-white hover:bg-acento/85',
        secundario: 'border border-borde bg-panel text-texto hover:border-acento/60',
        peligro: 'border border-error/40 text-error hover:bg-error/10',
    };
    return <button className={`${base} ${variantes[variante]} ${className}`} {...props} />;
}

export function Campo({ etiqueta, ayuda, className = '', ...props }) {
    return (
        <label className={`block ${className}`}>
            <span className="mb-1 block text-sm font-medium text-tenue">{etiqueta}</span>
            <input
                className="w-full rounded-md border border-borde bg-fondo px-3 py-2 text-sm
                           outline-none placeholder:text-tenue/50 focus:border-acento"
                {...props}
            />
            {ayuda && <span className="mt-1 block text-xs text-tenue">{ayuda}</span>}
        </label>
    );
}

export function Selector({ etiqueta, opciones, className = '', ...props }) {
    return (
        <label className={`block ${className}`}>
            <span className="mb-1 block text-sm font-medium text-tenue">{etiqueta}</span>
            <select
                className="w-full rounded-md border border-borde bg-fondo px-3 py-2 text-sm
                           outline-none focus:border-acento"
                {...props}
            >
                {opciones.map((o) => (
                    <option key={o.valor} value={o.valor}>{o.texto}</option>
                ))}
            </select>
        </label>
    );
}

export function Tarjeta({ className = '', ...props }) {
    return <div className={`rounded-lg border border-borde bg-panel p-5 ${className}`} {...props} />;
}

export function Aviso({ tipo = 'info', children }) {
    if (!children) return null;
    const estilos = {
        info: 'border-acento/40 bg-acento-suave text-texto',
        exito: 'border-exito/40 bg-exito/10 text-exito',
        error: 'border-error/40 bg-error/10 text-error',
        alerta: 'border-alerta/40 bg-alerta/10 text-alerta',
    };
    return (
        <p role="alert" className={`rounded-md border px-3 py-2 text-sm ${estilos[tipo]}`}>
            {children}
        </p>
    );
}

export function Insignia({ ok, children }) {
    return (
        <span className={`rounded-full px-2 py-0.5 text-xs font-medium ${
            ok ? 'bg-exito/15 text-exito' : 'bg-alerta/15 text-alerta'
        }`}>
            {children}
        </span>
    );
}

/* Muestra un valor que hay que copiar (token, API key) con su boton. */
export function Copiable({ valor }) {
    return (
        <div className="flex items-center gap-2">
            <code className="flex-1 overflow-x-auto rounded border border-borde bg-fondo px-2 py-1.5 font-mono text-xs">
                {valor}
            </code>
            <Boton
                variante="secundario"
                type="button"
                onClick={() => navigator.clipboard?.writeText(valor)}
            >
                Copiar
            </Boton>
        </div>
    );
}
