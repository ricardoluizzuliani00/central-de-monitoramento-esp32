// ======================================================
// MODELO. Copie este arquivo com o nome  firebase_secrets.h
// e preencha com os seus dados. O firebase_secrets.h de verdade
// NAO vai para o GitHub (esta no .gitignore).
// ======================================================

#ifndef FIREBASE_SECRETS_H
#define FIREBASE_SECRETS_H

// Endereco do Realtime Database (Firebase Console > Realtime Database).
// Sem barra no final. Pode ser ...firebaseio.com ou ...firebasedatabase.app
#define FIREBASE_URL "https://SEU-PROJETO-default-rtdb.firebaseio.com"

// Firebase Console > Configuracoes do projeto > Chave de API da Web
#define FIREBASE_API_KEY "SUA_API_KEY_DO_FIREBASE"

// Usuario (e-mail + senha) criado em Authentication > Metodo de login
// "E-mail/senha". A central usa este usuario para LER os dados.
#define FIREBASE_USER_EMAIL "email-da-central@exemplo.com"
#define FIREBASE_USER_PASSWORD "SENHA_DO_USUARIO_DA_CENTRAL"

// Chave da API da Groq (crie gratis em console.groq.com).
// Usada para transcrever a sua fala (Whisper).
#define GROQ_API_KEY "SUA_CHAVE_DA_GROQ"

#endif
