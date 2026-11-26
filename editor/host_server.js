// The files of the builder through tools/dgx_ui_serve.py, for a browser that gives a page no folder to open: the host the
// store reads and writes a project by, as host_browser.js's, its calls carrying the token of the link. A file or folder
// not there comes as 204, no content; a path the server refuses to read or list is one the project does not have: null
// too. A write it refuses says why.
'use strict';

// the calls of the server this page makes, by their number: the server serves a page of its own number alone
// (tools/dgx_ui_serve.py SERVER_API), as it reads it here
const DGX_UI_SERVER_API = 1;

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
        // a picture's PNG, its bytes as they are, beside the file of pictures' needs
        async writeBinary(path, bytes) {
            const response = await call(at('binary', path), { method: 'POST', body: bytes, headers: { 'Content-Type': 'image/png' } });
            if (!response.ok) throw await refused(response);
            return true;
        },
        // a file of DGX made of PNG files by the writer of the project's own DGX: its bytes; refused in its words
        async image(pngs, options) {
            const body = JSON.stringify({ ...options, pngs: pngs.map(base64Of) });
            const response = await call('/project/image', { method: 'POST', body, headers: { 'Content-Type': 'application/json' } });
            if (!response.ok) throw await refused(response);
            const said = await response.json();
            return Uint8Array.from(atob(said.dgxi), (c) => c.charCodeAt(0));
        },
    };
}

// bytes as base64, a piece at a time so that no call takes more arguments than an engine allows
function base64Of(bytes) {
    let text = '';
    for (let at = 0; at < bytes.length; at += 0x8000) text += String.fromCharCode.apply(null, bytes.subarray(at, at + 0x8000));
    return btoa(text);
}

// the token of the link the server printed: #project=<token>; null on a page opened otherwise
function dgxUiServedToken(hash) {
    return new URLSearchParams(hash.replace(/^#/, '')).get('project');
}

if (typeof module !== 'undefined') module.exports = { dgxUiServerHost, dgxUiServedToken, DGX_UI_SERVER_API };
