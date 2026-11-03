package cr.ac.tec.redes;

import io.javalin.Javalin;
import io.javalin.http.Context;
import io.javalin.http.HttpStatus;

import java.util.ArrayList;
import java.util.List;
import java.util.Map;

/* Los cuatro verbos que pide el enunciado, sobre un unico recurso.
 *
 * Las rutas quedan bajo /noticias para que la cache zonal pueda distinguirlas
 * con un patron. Conviene saber que la cache deduce el tipo de archivo de la
 * extension de la URL: /noticias no tiene ninguna, asi que cae en el tipo
 * "otros". Una regla para este origen tiene que configurar ese tipo, o el TTL
 * por tipo no se le aplicaria. */
public class NoticiasControlador {

    private final NoticiasRepo repo;

    public NoticiasControlador(NoticiasRepo repo) {
        this.repo = repo;
    }

    public void registrar(Javalin app) {
        app.get("/noticias", this::listar);
        app.get("/noticias/{id}", this::consultar);
        app.post("/noticias", this::crear);
        app.put("/noticias/{id}", this::actualizar);
        app.delete("/noticias/{id}", this::borrar);
    }

    private void listar(Context ctx) {
        List<Noticia> noticias = repo.listar();
        ctx.json(Map.of("cantidad", noticias.size(), "noticias", noticias));
    }

    private void consultar(Context ctx) {
        Integer id = idDeLaRuta(ctx);
        if (id == null) return;

        repo.consultar(id).ifPresentOrElse(
            ctx::json,
            () -> noEncontrada(ctx, id)
        );
    }

    private void crear(Context ctx) {
        Noticia.Entrada entrada = cuerpoValido(ctx);
        if (entrada == null) return;

        Noticia creada = repo.crear(entrada);

        /* 201 con Location es lo que corresponde a un recurso nuevo: le dice al
         * cliente donde quedo sin obligarlo a adivinar la URL a partir del id. */
        ctx.status(HttpStatus.CREATED);
        ctx.header("Location", "/noticias/" + creada.id());
        ctx.json(creada);
    }

    private void actualizar(Context ctx) {
        Integer id = idDeLaRuta(ctx);
        if (id == null) return;

        Noticia.Entrada entrada = cuerpoValido(ctx);
        if (entrada == null) return;

        repo.actualizar(id, entrada).ifPresentOrElse(
            ctx::json,
            () -> noEncontrada(ctx, id)
        );
    }

    private void borrar(Context ctx) {
        Integer id = idDeLaRuta(ctx);
        if (id == null) return;

        if (repo.borrar(id)) {
            /* 204 y sin cuerpo: no queda nada que describir. */
            ctx.status(HttpStatus.NO_CONTENT);
        } else {
            noEncontrada(ctx, id);
        }
    }

    /* Devuelve null y deja la respuesta lista cuando el id no es un numero.
     * Sin esto, Integer.parseInt lanzaria y el cliente veria un 500 por algo
     * que es culpa suya, no del servidor. */
    private Integer idDeLaRuta(Context ctx) {
        String crudo = ctx.pathParam("id");
        try {
            return Integer.valueOf(crudo);
        } catch (NumberFormatException e) {
            ctx.status(HttpStatus.BAD_REQUEST);
            ctx.json(Map.of("error", "El id debe ser un numero entero", "recibido", crudo));
            return null;
        }
    }

    /* Valida el cuerpo y devuelve null si no sirve, dejando ya escrita la
     * respuesta de error con TODOS los problemas encontrados. Reportarlos de a
     * uno obliga a quien llama a descubrirlos por prueba y error. */
    private Noticia.Entrada cuerpoValido(Context ctx) {
        Noticia.Entrada entrada;
        try {
            entrada = ctx.bodyAsClass(Noticia.Entrada.class);
        } catch (Exception e) {
            ctx.status(HttpStatus.BAD_REQUEST);
            ctx.json(Map.of("error", "El cuerpo debe ser un JSON con los campos titulo, cuerpo y autor"));
            return null;
        }

        List<String> problemas = new ArrayList<>();
        if (entrada == null || entrada.titulo() == null || entrada.titulo().isBlank()) {
            problemas.add("titulo es obligatorio");
        }
        if (entrada == null || entrada.cuerpo() == null || entrada.cuerpo().isBlank()) {
            problemas.add("cuerpo es obligatorio");
        }

        if (!problemas.isEmpty()) {
            ctx.status(HttpStatus.BAD_REQUEST);
            ctx.json(Map.of("error", "Cuerpo invalido", "problemas", problemas));
            return null;
        }

        return entrada;
    }

    private void noEncontrada(Context ctx, int id) {
        ctx.status(HttpStatus.NOT_FOUND);
        ctx.json(Map.of("error", "No existe una noticia con ese id", "id", id));
    }
}
