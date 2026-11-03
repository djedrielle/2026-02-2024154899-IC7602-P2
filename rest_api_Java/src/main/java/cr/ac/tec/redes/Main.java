package cr.ac.tec.redes;

import io.javalin.Javalin;

import java.util.Map;

/* Punto de entrada. Levanta Jetty con las rutas del recurso y nada mas.
 *
 * Este API es uno de los origenes con los que el enunciado pide probar la cache
 * zonal ("Es necesario probar el sistema con: Un REST API"). No lleva
 * autenticacion propia: quien decide si una ruta exige x-api-key o sesion es la
 * cache, a partir de las reglas configuradas desde el UI. */
public class Main {

    private static final int PUERTO_POR_DEFECTO = 7070;

    public static void main(String[] args) {
        NoticiasRepo repo = new NoticiasRepo();
        repo.sembrar();

        Javalin app = Javalin.create(config -> {
            config.showJavalinBanner = false;
        });

        new NoticiasControlador(repo).registrar(app);

        /* Sonda para Kubernetes. Separada de /noticias a proposito: si la
         * sonda pidiera el recurso real, una lista vacia o un error de datos
         * haria que el orquestador reiniciara un proceso que esta sano. */
        app.get("/salud", ctx -> ctx.json(Map.of("estado", "ok", "noticias", repo.cantidad())));

        app.start(puerto());
    }

    /* El puerto viene del entorno porque en Kubernetes lo fija el Deployment,
     * no el codigo. Un valor invalido se avisa y se cae al de siempre, en vez
     * de abortar el arranque por una variable mal escrita. */
    private static int puerto() {
        String crudo = System.getenv("PUERTO");
        if (crudo == null || crudo.isBlank()) return PUERTO_POR_DEFECTO;

        try {
            return Integer.parseInt(crudo.trim());
        } catch (NumberFormatException e) {
            System.err.println("PUERTO='" + crudo + "' no es un numero; se usa " + PUERTO_POR_DEFECTO);
            return PUERTO_POR_DEFECTO;
        }
    }
}
