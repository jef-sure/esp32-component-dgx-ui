# Vendored for the builder

So the page works with no network and no build step. The same files as jef-sure/boards' editor ships, so that a page
hosting both loads them once.

| File | Package | Licence |
|---|---|---|
| `vue.global.prod.js` | vue 3.5.44, `dist/` | MIT, `LICENSE.vue` |
| `pinia.iife.prod.js` | pinia 4.1.0, `dist/` | MIT, `LICENSE.pinia` |

Taken from the npm registry and compared with its sha512 integrity values:

- vue-3.5.44.tgz `sha512-dvvmaOCG7lAMkJTPqPwWeGMsPS6DIsFsfZDUgBp9V6mHf9b3+/wyVwYR4Es+WV6uJC0FKRxp81Obcq0ZwUcGFQ==`
- pinia-4.1.0.tgz `sha512-+5/HMuVjYQfKlCD5xQ7zfXn1xUzYFEa7pd0DQK5SaMfZCaFRTHlaH/c02N1FoGOdlzSiwf5EOPgJx//1QDw6gw==`
