/* Diagnostico minimo para la demostracion.
 *
 * La hora se calcula en el navegador cada vez que la pagina se dibuja, asi que
 * NO sirve para saber si la respuesta vino de la cache: cambia siempre.
 *
 * Lo que si lo dice es el encabezado X-Cache que agrega la cache zonal. Se
 * consulta con una peticion HEAD a la propia pagina, que pasa por el mismo
 * camino que la carga original. */
document.addEventListener('DOMContentLoaded', () => {
    const hora = document.getElementById('hora');
    const origen = document.getElementById('origen');

    hora.textContent = new Date().toLocaleString();

    fetch(window.location.pathname, { method: 'HEAD' })
        .then((respuesta) => {
            const marca = respuesta.headers.get('X-Cache');
            if (marca !== null) {
                origen.textContent = marca;
                return;
            }
            /* Sin el encabezado no se puede afirmar nada: puede que la peticion
             * no haya pasado por la cache, o que la cache no lo agregue. */
            origen.textContent = 'sin encabezado X-Cache';
        })
        .catch(() => {
            origen.textContent = 'no se pudo consultar';
        });
});
