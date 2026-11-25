// The builder on this page: by the link a server printed, #project=<token>, its project at once, and with &agent=1 the
// agent's dialog; else a folder the person opens. A file of its own, not a script in the page, so that the page runs
// under a Content-Security-Policy that takes none written in a page.
'use strict';

(() => {
    const start = { language: navigator.language.startsWith('ru') ? 'ru' : 'en' }, served = dgxUiServedToken(location.hash);
    const mounted = dgxUiBuilderMount('#app', served ? { open: () => dgxUiServerHost(served), start } : { host: dgxUiBrowserHost, start });
    // beside an agent: the dialog, and what it asks answered by the store
    if (served && dgxUiAgentAsked(location.hash)) mounted.then(({ store }) => store.startAgent(dgxUiAgent(store, served)));
})();
