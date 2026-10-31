import { apiPost } from './client.js';

export function login(email, contrasenna) {
    return apiPost('/api/login', { email, contrasenna });
}
