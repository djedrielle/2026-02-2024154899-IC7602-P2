import { apiGet, apiPost, apiPut, apiDelete } from './client.js';

const base = (dominio) => `/api/dominios/${encodeURIComponent(dominio)}`;

export const listarDominios = () => apiGet('/api/dominios');
export const crearDominio = (datos) => apiPost('/api/dominios', datos);
export const consultarDominio = (d) => apiGet(base(d));
export const actualizarDominio = (d, cambios) => apiPut(base(d), cambios);
export const borrarDominio = (d) => apiDelete(base(d));
export const verificarDominio = (d) => apiPost(`${base(d)}/verificar`);

export const listarUrls = (d) => apiGet(`${base(d)}/urls`);
export const crearUrl = (d, regla) => apiPost(`${base(d)}/urls`, regla);
export const actualizarUrl = (d, id, regla) => apiPut(`${base(d)}/urls/${id}`, regla);
export const borrarUrl = (d, id) => apiDelete(`${base(d)}/urls/${id}`);

export const listarApiKeys = (d) => apiGet(`${base(d)}/apikeys`);
export const crearApiKey = (d, descripcion) => apiPost(`${base(d)}/apikeys`, { descripcion });
export const borrarApiKey = (d, id) => apiDelete(`${base(d)}/apikeys/${id}`);
