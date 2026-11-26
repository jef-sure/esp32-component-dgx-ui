/*
 * Chrome for the builder's page, shared by test_page.js and shots.js: the browser found on the machine, and the DevTools
 * protocol over Node's own WebSocket, with no other package.
 *
 * Chrome is CHROME, else google-chrome, google-chrome-stable, chromium or chromium-browser on the PATH.
 */
'use strict';
const { execFileSync } = require('child_process');

function chrome() {
    for (const name of [process.env.CHROME, 'google-chrome', 'google-chrome-stable', 'chromium', 'chromium-browser'].filter(Boolean)) {
        try {
            return execFileSync('which', [name], { encoding: 'utf8' }).trim() || null;
        } catch {
            /* not this one */
        }
    }
    return null;
}

/* the protocol: a command and its answer by id; events to whoever waits for them */
function devtools(url) {
    const socket = new WebSocket(url);
    let next = 0;
    const waiting = new Map(), listeners = [];
    socket.onmessage = (message) => {
        const said = JSON.parse(message.data);
        if (said.id !== undefined && waiting.has(said.id)) {
            const { done, fail } = waiting.get(said.id);
            waiting.delete(said.id);
            said.error ? fail(new Error(`${said.error.message} ${said.error.data ?? ''}`)) : done(said.result);
        } else {
            for (const listener of listeners) listener(said);
        }
    };
    const opened = new Promise((done, fail) => ((socket.onopen = done), (socket.onerror = () => fail(new Error('no connection to Chrome')))));
    return {
        opened,
        listen: (listener) => listeners.push(listener),
        send(method, params = {}, sessionId = undefined) {
            const id = ++next;
            socket.send(JSON.stringify({ id, method, params, ...(sessionId ? { sessionId } : {}) }));
            return new Promise((done, fail) => {
                waiting.set(id, { done, fail });
                setTimeout(() => waiting.has(id) && (waiting.delete(id), fail(new Error(`${method}: no answer in 20 s`))), 20000);
            });
        },
        close: () => socket.close(),
    };
}

module.exports = { chrome, devtools };
