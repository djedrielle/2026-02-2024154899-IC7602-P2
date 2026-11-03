package cr.ac.tec.redes;

/* El recurso que expone el API.
 *
 * Es un record y no una clase mutable a proposito: una noticia que ya esta en
 * el mapa no se modifica en el lugar, se reemplaza por una nueva. Asi dos
 * peticiones concurrentes no pueden ver un objeto a medio actualizar.
 *
 * Jackson sabe construir records desde JSON desde la version 2.12, de modo que
 * sirve igual para leer el cuerpo de un POST que para escribir la respuesta. */
public record Noticia(int id, String titulo, String cuerpo, String autor, long creado) {

    /* Los campos que el cliente puede mandar. El id y la fecha los pone el
     * servidor: si vinieran del cliente, cualquiera podria pisar una noticia
     * ajena mandando su id en un POST. */
    public record Entrada(String titulo, String cuerpo, String autor) {}

    public static Noticia desde(int id, Entrada entrada, long creado) {
        return new Noticia(
            id,
            entrada.titulo().trim(),
            entrada.cuerpo().trim(),
            entrada.autor() == null || entrada.autor().isBlank() ? "anonimo" : entrada.autor().trim(),
            creado
        );
    }

    /* Conserva el id y la fecha de creacion originales: un PUT cambia el
     * contenido, no la identidad del recurso. */
    public Noticia conContenidoDe(Entrada entrada) {
        return Noticia.desde(this.id, entrada, this.creado);
    }
}
