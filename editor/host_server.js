// The files of the builder through tools/dgx_ui_serve.py, for a browser that gives a page no folder to open: the host the
// store reads and writes a project by, as host_browser.js's, its calls carrying the token of the link. A file or folder
// not there comes as 204, no content; a path the server refuses to read or list is one the project does not have: null
// too. A write it refuses says why.
'use strict';

async function dgxUiServerHost(token, base = '') {
    const call = (path, init = {}) => fetch(base + path, { ...init, cache: 'no-store', headers: { ...init.headers, Authorization: `Bearer ${token}` } });
    const at = (kind, path) => `/project/${kind}?path=${encodeURIComponent(path.split('/').filter((part) => part !== '').join('/'))}`;
    const refused = async (response) => {
        const said = await response.json().catch(() => null);
        return new Error(said?.error ?? `${response.status} ${response.statusText}`);
    };
    // the token or the address refused: no file of the project can be had, which the builder says
    const unknown = async (response) => {
        if (response.status === 400) return null;
        throw await refused(response);
    };
    const project = await call('/project');
    if (!project.ok) throw await refused(project);
    return {
        name: (await project.json()).name,
        inPlace: true,
        async list(directory) {
            const response = await call(at('list', directory));
            return response.status === 204 ? null : response.ok ? response.json() : unknown(response);
        },
        async read(path) {
            const response = await call(at('file', path));
            return response.status === 204 ? null : response.ok ? new Uint8Array(await response.arrayBuffer()) : unknown(response);
        },
        async write(path, text) {
            const response = await call(at('file', path), { method: 'POST', body: text, headers: { 'Content-Type': 'text/plain; charset=utf-8' } });
            if (!response.ok) throw await refused(response);
            return true;
        },
    };
}

// the token of the link the server printed: #project=<token>; null on a page opened otherwise
function dgxUiServedToken(hash) {
    return new URLSearchParams(hash.replace(/^#/, '')).get('project');
}

if (typeof module !== 'undefined') module.exports = { dgxUiServerHost, dgxUiServedToken };
