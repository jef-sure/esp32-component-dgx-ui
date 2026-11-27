// The builder on this page: by the link a server printed, #project=<token>, its project at once, and with &agent=1 the
// agent's dialog; else a folder the person opens. A file of its own, not a script in the page, so that the page runs
// under a Content-Security-Policy that takes none written in a page.
'use strict';

(() => {
    const start = { language: navigator.language.startsWith('ru') ? 'ru' : 'en' }, served = dgxUiServedToken(location.hash);
    // beside an agent (#...&agent=1): its dialog, and what it asks answered by the store, started by the mount as on a page
    // that hosts the builder
    dgxUiBuilderMount('#app', served ? { open: () => dgxUiServerHost(served), agent: dgxUiAgentAsked(location.hash), start } : { host: dgxUiBrowserHost, start });
})();
