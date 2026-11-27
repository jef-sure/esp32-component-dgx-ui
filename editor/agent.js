// The builder beside an agent (tools/dgx_ui_mcp.py): what the agent asks comes from the server, in order, and the store
// answers it (store.agentAnswer()); the dialog's messages come with it, and what the person writes goes to the server.
// Opened by the link the server gives, #project=<token>&agent=1.
'use strict';

// prefix: the server's paths under it, as dgxUiServerHost() takes it
function dgxUiAgent(store, token, prefix = '') {
    const headers = { Authorization: `Bearer ${token}` };
    const pause = (ms) => new Promise((done) => setTimeout(done, ms));
    const post = (path, body) => fetch(prefix + path, { method: 'POST', cache: 'no-store', body: JSON.stringify(body),
                                                      headers: { ...headers, 'Content-Type': 'application/json' } });
    let messages = 0, requests = 0, stopped = false;
    (async () => {
        while (!stopped) {
            try {
                const response = await fetch(`${prefix}/agent/updates?messages=${messages}&requests=${requests}`, { cache: 'no-store', headers });
                if (!response.ok) {
                    await pause(2000);
                    continue;
                }
                const said = await response.json();
                for (const message of said.messages) store.agentMessages.push(message);
                messages += said.messages.length;
                for (const request of said.requests) {
                    let value;
                    try {
                        value = await store.agentAnswer(request.kind, request.args ?? {});
                    } catch (error) {
                        value = { error: String(error?.message ?? error) };
                    }
                    await post('/agent/answer', { id: request.id, value });
                }
                requests = said.requested; // the server's count: past requests, answered or given up, never asked again
            } catch {
                await pause(2000); // the server stopped, or is not there yet
            }
        }
    })();
    return {
        say: (text) => post('/agent/say', { text }),
        stop: () => (stopped = true),
    };
}

// whether the link is of a server with an agent: #...&agent=1
function dgxUiAgentAsked(hash) {
    return new URLSearchParams(hash.replace(/^#/, '')).get('agent') === '1';
}

if (typeof module !== 'undefined') module.exports = { dgxUiAgent, dgxUiAgentAsked };
