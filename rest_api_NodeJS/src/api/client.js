
export async function apiPost(ruta, cuerpo) {
    const res = await fetch(`${ruta}`, {
        method: 'POST',
        headers: { 'Content-Type': 'application/json' },
        body: JSON.stringify(cuerpo),
    });
    return res.json();
}