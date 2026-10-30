/* Los endpoints de configuracion son de consumo interno: solo el Zonal Cache
 * los llama. El API esta publicado en Vercel, asi que sin esta verificacion
 * cualquiera podria leer a que origen apunta cada dominio.
 *
 * El secreto viaja en el encabezado x-config-key y vive en una variable de
 * entorno a los dos lados: en Vercel para el API, y en un Secret de Kubernetes
 * para el cache. */
export function config_autorizada(req) {
    const esperado = process.env.CONFIG_API_KEY;

    // Si no hay secreto configurado se niega todo, en vez de quedar abierto.
    if (!esperado) {
        console.error('Falta la variable de entorno CONFIG_API_KEY');
        return false;
    }

    const recibido = req.headers['x-config-key'];
    return typeof recibido === 'string' && recibido === esperado;
}
