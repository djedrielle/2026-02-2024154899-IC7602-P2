package cr.ac.tec.redes;

import org.junit.jupiter.api.Test;

import java.util.List;
import java.util.Optional;

import static org.junit.jupiter.api.Assertions.*;

class NoticiasRepoTest {

    private Noticia.Entrada entrada(String titulo) {
        return new Noticia.Entrada(titulo, "cuerpo de " + titulo, "Autor");
    }

    @Test
    void creaConIdsCrecientesYSinRepetir() {
        NoticiasRepo repo = new NoticiasRepo();

        Noticia a = repo.crear(entrada("A"));
        Noticia b = repo.crear(entrada("B"));

        assertEquals(1, a.id());
        assertEquals(2, b.id());
        assertEquals(2, repo.cantidad());
    }

    @Test
    void listaOrdenadaPorIdAunqueSeBorreEnElMedio() {
        NoticiasRepo repo = new NoticiasRepo();
        repo.crear(entrada("A"));
        Noticia b = repo.crear(entrada("B"));
        repo.crear(entrada("C"));

        repo.borrar(b.id());

        List<Integer> ids = repo.listar().stream().map(Noticia::id).toList();
        assertEquals(List.of(1, 3), ids);
    }

    @Test
    void actualizarConservaIdYFechaDeCreacion() {
        NoticiasRepo repo = new NoticiasRepo();
        Noticia original = repo.crear(entrada("A"));

        Optional<Noticia> actualizada = repo.actualizar(original.id(), entrada("A corregida"));

        assertTrue(actualizada.isPresent());
        assertEquals(original.id(), actualizada.get().id());
        assertEquals(original.creado(), actualizada.get().creado());
        assertEquals("A corregida", actualizada.get().titulo());
    }

    @Test
    void actualizarUnIdInexistenteNoCreaNada() {
        NoticiasRepo repo = new NoticiasRepo();

        assertTrue(repo.actualizar(99, entrada("fantasma")).isEmpty());
        assertEquals(0, repo.cantidad());
    }

    @Test
    void borrarDosVecesSoloTieneEfectoLaPrimera() {
        NoticiasRepo repo = new NoticiasRepo();
        Noticia a = repo.crear(entrada("A"));

        assertTrue(repo.borrar(a.id()));
        assertFalse(repo.borrar(a.id()));
    }

    @Test
    void elAutorVacioSeSustituyePorAnonimo() {
        NoticiasRepo repo = new NoticiasRepo();

        Noticia sinAutor = repo.crear(new Noticia.Entrada("T", "C", "   "));
        Noticia nulo = repo.crear(new Noticia.Entrada("T", "C", null));

        assertEquals("anonimo", sinAutor.autor());
        assertEquals("anonimo", nulo.autor());
    }

    /* Los ids no se reutilizan: si se borra el ultimo y se crea otro, el nuevo
     * no hereda el id libre. Reutilizarlos haria que una respuesta vieja en la
     * cache zonal apuntara a un recurso distinto con la misma URL. */
    @Test
    void losIdsNoSeReutilizan() {
        NoticiasRepo repo = new NoticiasRepo();
        Noticia a = repo.crear(entrada("A"));
        repo.borrar(a.id());

        assertEquals(2, repo.crear(entrada("B")).id());
    }
}
