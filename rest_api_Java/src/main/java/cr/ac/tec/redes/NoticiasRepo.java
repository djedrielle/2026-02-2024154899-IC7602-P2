package cr.ac.tec.redes;

import java.util.Comparator;
import java.util.List;
import java.util.Optional;
import java.util.concurrent.ConcurrentHashMap;
import java.util.concurrent.ConcurrentMap;
import java.util.concurrent.atomic.AtomicInteger;

/* Almacenamiento en memoria.
 *
 * El enunciado pide un API simple para probar la cache zonal, no persistencia,
 * asi que no hay base de datos. La consecuencia es que al reiniciar el proceso
 * se vuelve a los datos de ejemplo, y que dos replicas en Kubernetes no
 * comparten estado: cada una responde lo suyo.
 *
 * Es un mapa concurrente porque Jetty atiende cada peticion en un hilo distinto
 * del grupo de trabajadores, de modo que dos escrituras pueden coincidir. */
public class NoticiasRepo {

    private final ConcurrentMap<Integer, Noticia> noticias = new ConcurrentHashMap<>();
    private final AtomicInteger siguienteId = new AtomicInteger(0);

    /* Datos de ejemplo para que un GET recien arrancado el servidor devuelva
     * algo. Sin esto, la primera prueba contra la cache guardaria una lista
     * vacia y habria que recordar hacer un POST antes de cada prueba. */
    public void sembrar() {
        crear(new Noticia.Entrada(
            "Caches zonales reducen la latencia",
            "Un estudio del TEC midio mejoras al acercar el contenido al usuario final.",
            "Redaccion"));
        crear(new Noticia.Entrada(
            "El protocolo TLS 1.3 completa el saludo en un viaje",
            "La version anterior necesitaba dos, lo que se notaba en redes con retardo alto.",
            "Redaccion"));
    }

    /* Ordenadas por id para que dos llamadas seguidas devuelvan lo mismo en el
     * mismo orden. Un ConcurrentHashMap no garantiza orden de iteracion, y una
     * lista que cambia de orden entre peticiones le haria guardar a la cache
     * respuestas distintas para la misma clave. */
    public List<Noticia> listar() {
        return noticias.values().stream()
            .sorted(Comparator.comparingInt(Noticia::id))
            .toList();
    }

    public Optional<Noticia> consultar(int id) {
        return Optional.ofNullable(noticias.get(id));
    }

    public Noticia crear(Noticia.Entrada entrada) {
        int id = siguienteId.incrementAndGet();
        Noticia noticia = Noticia.desde(id, entrada, System.currentTimeMillis());
        noticias.put(id, noticia);
        return noticia;
    }

    /* Devuelve vacio si el id no existia, para que el controlador pueda
     * distinguir entre "actualizada" y "no hay nada que actualizar" sin hacer
     * una consulta previa que otro hilo podria invalidar en el intermedio. */
    public Optional<Noticia> actualizar(int id, Noticia.Entrada entrada) {
        return Optional.ofNullable(
            noticias.computeIfPresent(id, (clave, actual) -> actual.conContenidoDe(entrada))
        );
    }

    public boolean borrar(int id) {
        return noticias.remove(id) != null;
    }

    public int cantidad() {
        return noticias.size();
    }
}
