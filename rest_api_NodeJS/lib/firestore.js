import { initializeApp, cert } from 'firebase-admin/app';
import { getFirestore } from 'firebase-admin/firestore';

if (!process.env.FIREBASE_SERVICE_ACCOUNT) {
    throw new Error('Falta la variable de entorno FIREBASE_SERVICE_ACCOUNT');
}
const credenciales = JSON.parse(process.env.FIREBASE_SERVICE_ACCOUNT);
initializeApp({ credential: cert(credenciales) });

export const db = getFirestore();