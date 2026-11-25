/*
 * node test_page.js <font files directory> <screenshot.png>: the builder's page, editor/index.html opened from the disk
 * in a headless Chrome, driven by the DevTools protocol over Node's own WebSocket, with no other package. The page is
 * given a project in memory, as its host gives one with no File System Access; then it is worked as a person works it:
 * a click on the panel selects the widget under it and its outline lies on its box, a property typed, Ctrl+Z, Delete,
 * the arrows of the tree, Ctrl+S and the word the page asks before writing a description in its own form; the
 * appearance by its forms: a colour typed, by a name of DGX's and by the picker, on the panel at once and the time of a
 * pass measured, Ctrl+Z, a look added of a draw, a frame of nine pictures chosen from the names file and refused in the
 * words of its need, a look renamed in the page that names it; the names file by its forms:
 * an expression typed, a name the page misses added, a rename carried into the page and Ctrl+Z, a file not on the disk
 * made of the names its page misses and written by the server; the language changed; a screenshot is kept. The page
 * under the server's Content-Security-Policy, a fetch and an image of another host blocked. Then the same project beside
 * an agent, tools/dgx_ui_mcp.py driven over stdio: the link, the state, an edit and a refusal by the loader's code, the
 * person's Ctrl+Z, the panel as a PNG, the dialog both ways, nothing saved by the agent. Then the same project on the disk through tools/dgx_ui_serve.py, by the link it prints:
 * opened at once, a description saved into its file, a CMakeLists.txt refused. No error may reach the console.
 *
 * Chrome is CHROME, else google-chrome, chromium or chromium-browser on the PATH. With none, the test fails, saying so,
 * unless BUILDER_PAGE_NO_CHROME=skip, for a machine that has none.
 */
'use strict';
const fs = require('fs');
const os = require('os');
const path = require('path');
const { spawn, execFileSync } = require('child_process');
const ROOT = path.join(__dirname, '../..');
const [fontDirectory, screenshot] = process.argv.slice(2);
let failures = 0, checks = 0, passMs = null;
const check = (held, what) => {
    ++checks;
    if (!held) console.error('  failed: ' + what), ++failures;
    return held;
};
const sleep = (ms) => new Promise((done) => setTimeout(done, ms));

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

(async () => {
    const binary = chrome();
    if (!binary) {
        if (process.env.BUILDER_PAGE_NO_CHROME === 'skip') {
            console.log('test_page.js: no Chrome on this machine; passed by, as BUILDER_PAGE_NO_CHROME=skip asks');
            return;
        }
        console.error('test_page.js: no Chrome: CHROME=<its path>, or BUILDER_PAGE_NO_CHROME=skip on a machine that has none');
        process.exit(1);
    }
    const profile = fs.mkdtempSync(path.join(os.tmpdir(), 'dgx-ui-builder-page-'));
    const flags = ['--headless=new', `--user-data-dir=${profile}`, '--remote-debugging-port=0', '--no-first-run', '--no-default-browser-check',
                   '--disable-gpu', '--disable-extensions', '--window-size=1400,900', 'about:blank'];
    const browser = spawn(binary, flags, { stdio: 'ignore' });
    let protocol = null, server = null;
    try {
        let port = null;
        for (let k = 0; k < 200 && !port; ++k) {
            await sleep(100);
            try {
                port = fs.readFileSync(path.join(profile, 'DevToolsActivePort'), 'utf8').split('\n');
            } catch {
                /* not yet */
            }
        }
        if (!port) throw new Error('Chrome told no port of DevTools in 20 s');
        protocol = devtools(`ws://127.0.0.1:${port[0]}${port[1]}`);
        await protocol.opened;
        const { targetId } = await protocol.send('Target.createTarget', { url: 'about:blank' });
        const { sessionId } = await protocol.send('Target.attachToTarget', { targetId, flatten: true });
        const send = (method, params) => protocol.send(method, params, sessionId);
        const errors = [];
        protocol.listen((said) => {
            if (said.sessionId !== sessionId) return;
            const thrown = said.params?.exceptionDetails;
            if (said.method === 'Runtime.exceptionThrown') errors.push(thrown.exception?.description ?? thrown.text);
            if (said.method === 'Runtime.consoleAPICalled' && (said.params.type === 'error' || said.params.type === 'warning'))
                errors.push(said.params.args.map((arg) => arg.value ?? arg.description).join(' '));
            if (said.method === 'Log.entryAdded' && said.params.entry.level === 'error' && !/favicon/.test(said.params.entry.url ?? ''))
                errors.push(said.params.entry.text);
        });
        await send('Runtime.enable'), await send('Page.enable'), await send('Log.enable');
        await send('Emulation.setDeviceMetricsOverride', { width: 1400, height: 900, deviceScaleFactor: 1, mobile: false });
        await send('Page.navigate', { url: 'file://' + path.join(ROOT, 'editor', 'index.html') });
        const evaluate = async (expression) => {
            const said = await send('Runtime.evaluate', { expression, awaitPromise: true, returnByValue: true });
            const thrown = said.exceptionDetails;
            if (thrown) throw new Error(`${expression.slice(0, 80)}: ${thrown.exception?.description ?? thrown.text}`);
            return said.result.value;
        };
        const until = async (expression, what, ms = 10000) => {
            for (const end = Date.now() + ms; Date.now() < end; await sleep(50)) {
                try {
                    if (await evaluate(expression)) return check(true, what);
                } catch {
                    /* the page not there yet */
                }
            }
            return check(false, `${what}: not in ${ms} ms`);
        };
        const store = 'useDgxUiBuilder()';
        await until(`typeof useDgxUiBuilder === 'function' && ${store}.ready`, 'the module started in the page');
        check(await evaluate(`!!document.querySelector('.dgx-ui-builder header h1') && !${store}.failed`), 'the page drawn');

        /* a project in memory, as a host gives one with no File System Access */
        const files = {
            'CMakeLists.txt': 'project(demo)\n',
            'main/CMakeLists.txt': 'dgx_ui_appearance(look.json NAMES names.txt)\ndgx_ui_tree(home.json NAMES names.txt APPEARANCE look.json)\n' +
                                   'dgx_ui_tree(other.json NAMES fresh.txt)\n',
            'main/other.json': '{"dgx-ui": 1, "tree": {"kind": "box", "children": [{"kind": "slider", "value": "level", "max": 5}, ' +
                               '{"kind": "button", "text": "Back", "on_event": "back"}]}}\n',
            'main/names.txt': 'look slider\nlook button\nvalue volume &volume\nfont small TerminusTTFMedium12\ndraw text\ndraw slider\ndraw switch\n' +
                              'draw nine\npicture one one_piece\npicture card card_pieces 9\n',
            'main/look.json': '{"dgx-ui": 1, "name": "day", "colors": {"paper": "#102030", "ink": "#ffffff", "fill": "#304060", "border": "#8090a0", ' +
                              '"accent": "#f0a03c"}, "styles": {"body": {"fonts": ["small"]}}, "looks": {"caption": {"draw": "text", ' +
                              '"params": {"ink": "ink", "paper": null}}, "slider": {"draw": "slider", "params": {"track": "accent", "fill": "accent", ' +
                              '"knob": "ink", "focused": "ink", "disabled": "border", "track_h": 4}}}}\n',
            'main/home.json': '{"dgx-ui": 1, "name": "home", "tree": {"kind": "box", "flags": ["column"], "pad": 6, "gap": 6, "children": [\n' +
                              ' {"kind": "label", "text": "Hello", "look": "caption", "style": "body", "want": [0, 40]},\n' +
                              ' {"kind": "slider", "value": "volume", "min": 0, "max": 100, "want": [0, 24]},\n' +
                              ' {"kind": "button", "text": "Go", "style": "body", "want": [0, 30]}]}}\n',
        };
        const encoded = Object.fromEntries(Object.entries(files).map(([where, text]) => [where, Buffer.from(text).toString('base64')]));
        encoded['build/dgx_fonts/dgx_fonts-files.txt'] = Buffer.from('TerminusTTFMedium12 TerminusTTFMedium12.dgxf\n').toString('base64');
        const font = fs.readFileSync(path.join(fontDirectory, 'TerminusTTFMedium12.dgxf'));
        encoded['build/dgx_fonts/TerminusTTFMedium12.dgxf'] = font.toString('base64');
        await evaluate(`(async () => {
            const given = ${JSON.stringify(encoded)}, files = {};
            for (const [where, text] of Object.entries(given)) files[where] = Uint8Array.from(atob(text), (c) => c.charCodeAt(0));
            window.savedByTest = {};
            await ${store}.open(memoryHost('demo', files, (where, text) => ((window.savedByTest[where] = text), true)));
        })()`);
        check(await evaluate(`${store}.current === 'main/home.json' && ${store}.verdicts.page.code === 0 &&
                              ${store}.verdicts.fonts[0].verdict.code === 0`),
              'the project open, its page and font taken');
        await until(`${store}.drawn > 0`, 'the panel drawn');
        await sleep(300);
        const lit = await evaluate(`(() => {
            const canvas = document.querySelector('.dgx-ui-builder canvas');
            const data = canvas.getContext('2d').getImageData(0, 0, canvas.width, canvas.height).data;
            const seen = new Set();
            const count = new Map();
            for (let k = 0; k < data.length; k += 4) {
                const colour = data[k] << 16 | data[k + 1] << 8 | data[k + 2];
                seen.add(colour), count.set(colour, (count.get(colour) ?? 0) + 1);
            }
            const most = [...count].sort((a, b) => b[1] - a[1])[0][0];
            return [canvas.width, canvas.height, seen.size, [most >> 16, most >> 8 & 255, most & 255]];
        })()`);
        /* the paper the most of it, #102030 as a panel of 16 bits gives it back, the ink and the fill beside it */
        check(lit[0] === 320 && lit[1] === 240 && lit[2] >= 3 && [0x10, 0x20, 0x30].every((value, k) => Math.abs(lit[3][k] - value) < 8),
              'the canvas holds the page, in its colours: ' + JSON.stringify(lit));

        /* a click on the button: selected, its outline on its box; at a zoom of 2 its point read in the panel's pixels is
           far from its point on the page */
        const click = async (x, y) => {
            await send('Input.dispatchMouseEvent', { type: 'mouseMoved', x, y });
            await send('Input.dispatchMouseEvent', { type: 'mousePressed', x, y, button: 'left', clickCount: 1 });
            await send('Input.dispatchMouseEvent', { type: 'mouseReleased', x, y, button: 'left', clickCount: 1 });
        };
        const label = await evaluate(`(() => {
            const store = ${store};
            store.select([2]);
            const box = store.box, canvas = document.querySelector('.dgx-ui-builder canvas').getBoundingClientRect();
            store.select([]);
            return JSON.stringify({box, left: canvas.left, top: canvas.top, zoom: store.zoom});
        })()`).then(JSON.parse);
        check(label.box && label.box.h === 30 && label.zoom === 2, 'the button laid: ' + JSON.stringify(label));
        await click(label.left + (label.box.x + label.box.w / 2) * label.zoom, label.top + (label.box.y + label.box.h / 2) * label.zoom);
        await until(`JSON.stringify(${store}.selection) === '[2]'`, 'a click on the button selects it');
        await sleep(100);
        const outline = await evaluate(`(() => {
            const box = document.querySelector('.dgx-ui-builder .outline').getBoundingClientRect();
            return [box.left, box.top, box.width, box.height];
        })()`);
        const { box, zoom } = label;
        const expected = [label.left + box.x * zoom, label.top + box.y * zoom, box.w * zoom, box.h * zoom];
        check(outline.every((value, k) => Math.abs(value - expected[k]) < 1.5), `the outline on the button's box: ${outline}, ${expected}`);
        check(await evaluate(`document.querySelector('.dgx-ui-builder .widgets .node.here').textContent.includes('button')`), 'the tree marks it');
        await evaluate(`${store}.select([0])`);

        /* its text typed in the properties */
        const original = await evaluate(`${store}.files['main/home.json'].text`);
        await evaluate(`(() => {
            const field = [...document.querySelectorAll('.dgx-ui-builder .field')].find((one) => one.querySelector('.key').textContent === 'text');
            const input = field.querySelector('input'); input.focus(); input.select();
        })()`);
        await send('Input.insertText', { text: 'Hi there' });
        await until(`${store}.files['main/home.json'].text.includes('"text": "Hi there"')`, 'the text typed reaches the description');
        check(await evaluate(`${store}.verdicts.page.code === 0 && ${store}.dirty.includes('main/home.json')`), 'taken, and the file edited');

        /* Ctrl+Z, outside a field */
        const key = (keyName, code, keyCode, modifiers = 0) => [
            send('Input.dispatchKeyEvent', { type: 'rawKeyDown', key: keyName, code, windowsVirtualKeyCode: keyCode, modifiers }),
            send('Input.dispatchKeyEvent', { type: 'keyUp', key: keyName, code, windowsVirtualKeyCode: keyCode, modifiers }),
        ];
        await evaluate('document.activeElement.blur()');
        await Promise.all(key('z', 'KeyZ', 90, 2));
        await until(`${store}.files['main/home.json'].text === ${JSON.stringify(original)}`, 'Ctrl+Z gives the file back');
        /* Delete on the selection, then Ctrl+Y and Ctrl+Z */
        await evaluate(`${store}.select([1])`);
        await Promise.all(key('Delete', 'Delete', 46));
        await until(`${store}.tree.length === 3 && !${store}.files['main/home.json'].text.includes('slider')`, 'Delete removes the selection');
        await Promise.all(key('z', 'KeyZ', 90, 2));
        await until(`${store}.tree.length === 4`, 'Ctrl+Z brings it back');
        await Promise.all(key('y', 'KeyY', 89, 2));
        await until(`${store}.tree.length === 3`, 'Ctrl+Y removes it again');
        await Promise.all(key('z', 'KeyZ', 90, 2));
        await until(`${store}.tree.length === 4`, 'and Ctrl+Z once more');
        /* the arrows of the tree */
        await evaluate(`(${store}.select([0]), document.querySelector('.dgx-ui-builder .widgets').focus())`);
        await Promise.all(key('ArrowDown', 'ArrowDown', 40));
        await until(`JSON.stringify(${store}.selection) === '[1]'`, 'ArrowDown: the next widget');
        await Promise.all(key('ArrowUp', 'ArrowUp', 38));
        await until(`JSON.stringify(${store}.selection) === '[0]'`, 'ArrowUp: the one before');

        /* Ctrl+S: a description by hand, so a word first, then the file */
        await evaluate(`${store}.setNumber('want', '')`);
        await evaluate(`(${store}.select([0]), ${store}.setPair('want', '0', '44'))`);
        await Promise.all(key('s', 'KeyS', 83, 2));
        await until(`!!document.querySelector('.dgx-ui-builder .asking')`, 'Ctrl+S asks before writing in the builder\'s form');
        check(await evaluate('Object.keys(window.savedByTest).length === 0'), 'nothing written before the word');
        await evaluate(`document.querySelector('.dgx-ui-builder .asking button').click()`);
        await until(`(window.savedByTest['main/home.json'] || '').includes('"want": [0, 44]')`, 'saved after it');
        check(await evaluate(`!${store}.dirty.length && !document.querySelector('.dgx-ui-builder .asking')`), 'nothing edited after');

        /* the text of the page, refused: the verdict and the cursor */
        await evaluate(`${store}.setText('page', ${store}.files['main/home.json'].text.replace('"max": 100', '"max": 100, "pad": 1'))`);
        await evaluate(`${store}.showText('page')`);
        await until(`document.activeElement && document.activeElement.tagName === 'TEXTAREA'`, 'the view of the text, the cursor in it');
        const cursor = await evaluate(`(() => {
            const area = document.activeElement;
            return area.value.slice(area.selectionStart, area.selectionStart + 5);
        })()`);
        check(cursor === '"pad"', 'the cursor at the key the loader refuses: ' + JSON.stringify(cursor));
        check(await evaluate(`document.querySelector('.dgx-ui-builder .verdict').textContent.includes('DGX_UI_LOAD_FIELD')`), 'the verdict in words');
        await evaluate(`(${store}.undo(), ${store}.view = 'page')`);

        /* Ctrl+Z within a field is the field's: the builder's history untouched */
        const steps = `${store}.raw.histories.get('main/home.json').past.length`;
        const before = await evaluate(steps);
        await evaluate(`(() => {
            const field = [...document.querySelectorAll('.dgx-ui-builder .field')].find((one) => one.querySelector('.key').textContent === 'text');
            field.querySelector('input').focus();
        })()`);
        await Promise.all(key('z', 'KeyZ', 90, 2));
        await sleep(200);
        check((await evaluate(steps)) === before, 'Ctrl+Z in a field leaves the builder\'s history: ' + before);
        await evaluate('document.activeElement.blur()');

        /* the appearance by its forms: a colour typed reaches the panel at once, as the board's depth shows it; Ctrl+Z */
        const sliderAt = await evaluate(`(() => { const store = ${store}; store.select([1]); const box = store.box; store.select([]);
                                                 return [Math.round(box.x + box.w * 3 / 4), Math.round(box.y + box.h / 2)]; })()`);
        const pixelAt = (x, y) => evaluate(`(() => {
            const canvas = document.querySelector('.dgx-ui-builder canvas');
            return [...canvas.getContext('2d').getImageData(${x}, ${y}, 1, 1).data.slice(0, 3)];
        })()`);
        const canvasNow = () => evaluate(`(() => {
            const canvas = document.querySelector('.dgx-ui-builder canvas');
            return Array.from(canvas.getContext('2d').getImageData(0, 0, canvas.width, canvas.height).data).join();
        })()`);
        const shownAt16 = async (hex) => evaluate(`(() => { const at = DgxUiAppearance.colorAtDepth('${hex}', 16);
                                                          return [1, 3, 5].map((k) => parseInt(at.slice(k, k + 2), 16)); })()`);
        const pixelIs = async (expected, what) => {
            for (const end = Date.now() + 5000; Date.now() < end; await sleep(50))
                if (JSON.stringify(await pixelAt(...sliderAt)) === JSON.stringify(expected)) return check(true, what);
            return check(false, `${what}: ${await pixelAt(...sliderAt)}, not ${expected}`);
        };
        await pixelIs(await shownAt16('#f0a03c'), 'the slider\'s track in the accent');
        await evaluate(`[...document.querySelectorAll('.dgx-ui-builder .tabs button')].find((one) => one.textContent.trim() === 'Appearance').click()`);
        await until(`!!document.querySelector('.dgx-ui-builder .appearance .color')`, 'the Appearance tab open, its colours listed');
        const rowOf = (name) => `[...document.querySelectorAll('.dgx-ui-builder .appearance .color')].find((row) => row.querySelector('.entry-name').value === '${name}')`;
        await evaluate(`(() => { const input = ${rowOf('accent')}.querySelector('.hex'); input.focus(); input.select(); })()`);
        await send('Input.insertText', { text: '#00ff00' });
        await until(`${store}.files['main/look.json'].text.includes('"accent": "#00ff00"')`, 'a hex typed reaches the appearance');
        await pixelIs([0, 255, 0], 'and the panel at once');
        const passes = await evaluate(`(() => {
            const store = ${store}, times = [];
            for (let k = 0; k < 20; ++k) {
                const at = performance.now();
                store.setColor('accent', k % 2 ? '#00ff00' : '#00fe00', store.lookFold(['colors', 'accent']));
                store.paint(performance.now() + 100000 + k);
                times.push(performance.now() - at);
            }
            times.sort((a, b) => a - b);
            return [times[10], times[19]];
        })()`);
        passMs = passes;
        check(passes[0] < 16, `a colour's pass, the appearance and the page loaded again and drawn: ${passes[0].toFixed(2)} ms, at most ${passes[1].toFixed(2)}`);
        await evaluate('document.activeElement.blur()');
        await Promise.all(key('z', 'KeyZ', 90, 2));
        await until(`${store}.files['main/look.json'].text === ${JSON.stringify(files['main/look.json'])}`, 'Ctrl+Z gives the appearance back');
        await pixelIs(await shownAt16('#f0a03c'), 'and the panel');
        check(await evaluate(`${store}.files['main/home.json'].text === ${JSON.stringify(files['main/home.json'])} || !${store}.dirty.includes('main/home.json')`),
              'the page untouched by it');
        await evaluate(`(() => { const input = ${rowOf('border')}.querySelector('.hex'); input.focus(); input.select(); })()`);
        await send('Input.insertText', { text: 'gainsboro' });
        await until(`${store}.files['main/look.json'].text.includes('"border": "#dcdcdc"') &&
                     ${rowOf('border')}.querySelector('.named').textContent.trim() === 'gainsboro'`, 'a name of DGX\'s typed: hex written, the name shown');
        await evaluate(`(() => { const picker = ${rowOf('accent')}.querySelector('input[type=color]'); picker.value = '#0000ff';
                                 picker.dispatchEvent(new Event('input', { bubbles: true })); })()`);
        await until(`${store}.files['main/look.json'].text.includes('"accent": "#0000ff"')`, 'the picker\'s colour taken at the next frame');
        await pixelIs(await shownAt16('#0000ff'), 'and drawn');
        check(await evaluate(`(() => { const swatches = ${rowOf('accent')}.querySelectorAll('.swatch');
                                       return swatches.length === 2 && swatches[1].title.includes('16'); })()`), 'its swatch twice, the second at 16 bits');

        /* a look added from its draw: its fields the module's description of the draw */
        await evaluate(`(() => {
            const add = [...document.querySelectorAll('.dgx-ui-builder .appearance .add')].find((one) => one.querySelector('select'));
            const name = add.querySelector('input'); name.value = 'toggle'; name.dispatchEvent(new Event('input', { bubbles: true }));
            const draw = add.querySelector('select'); draw.value = 'switch'; draw.dispatchEvent(new Event('change', { bubbles: true }));
        })()`);
        await sleep(50);
        await evaluate(`[...document.querySelectorAll('.dgx-ui-builder .appearance .add')].find((one) => one.querySelector('select')).querySelector('button').click()`);
        await until(`JSON.stringify([...document.querySelectorAll('.dgx-ui-builder .appearance .form .field .key')].map((one) => one.textContent.trim())) ===
                     JSON.stringify(['name', 'draw', 'fill', 'params', 'off', 'on', 'knob', 'focused', 'disabled'])`, 'a look of a switch: its fields as the docs list them');
        check(await evaluate(`${store}.verdicts.appearance.code === 0 && JSON.parse(${store}.files['main/look.json'].text).looks.toggle.draw === 'switch'`),
              'taken, written');

        /* a look of the frame of nine pictures under the word "button": the names' pictures to choose, the first of nine
           given; one of fewer refused in the words of the need; the button Go framed by the pieces given */
        const canvasPlain = await canvasNow();
        await evaluate(`(() => {
            const add = [...document.querySelectorAll('.dgx-ui-builder .appearance .add')].find((one) => one.querySelector('select'));
            const name = add.querySelector('input'); name.value = 'button'; name.dispatchEvent(new Event('input', { bubbles: true }));
            const draw = add.querySelector('select'); draw.value = 'nine'; draw.dispatchEvent(new Event('change', { bubbles: true }));
        })()`);
        await sleep(50);
        await evaluate(`[...document.querySelectorAll('.dgx-ui-builder .appearance .add')].find((one) => one.querySelector('select')).querySelector('button').click()`);
        const pieces = `[...document.querySelectorAll('.dgx-ui-builder .appearance .form .field')].find((one) => one.querySelector('.key').textContent.trim() === 'pieces')`;
        await until(`!!${pieces} && ${pieces}.querySelector('select').value === 'card'`, 'a look of the frame: its pieces the first picture of nine');
        check(await evaluate(`JSON.stringify([...${pieces}.querySelectorAll('option')].map((one) => one.textContent.trim())) ===
                              JSON.stringify(['—', 'one (1, fewer than 9)', 'card (9)'])`), 'the names\' pictures to choose, with their counts');
        await sleep(300);
        const canvasFramed = await canvasNow();
        check(canvasFramed !== canvasPlain, 'the button framed by the pieces');
        await evaluate(`(() => { const select = ${pieces}.querySelector('select'); select.value = 'one'; select.dispatchEvent(new Event('change', { bubbles: true })); })()`);
        await until(`(document.querySelector('.dgx-ui-builder .appearance .verdict') || {}).textContent === 'pieces: 9 pictures needed, the table\\'s "one" has 1'`,
                    'one picture for nine: refused in the words of the need');
        await evaluate(`(() => { const select = ${pieces}.querySelector('select'); select.value = 'card'; select.dispatchEvent(new Event('change', { bubbles: true })); })()`);
        await until(`${store}.verdicts.appearance.code === 0 && !document.querySelector('.dgx-ui-builder .appearance .verdict')`, 'card again: taken');
        await sleep(300);
        check((await canvasNow()) === canvasFramed, 'the panel framed again, the same');
        await evaluate(`${store}.removeEntry('looks', 'button')`);
        await sleep(300);
        check((await canvasNow()) === canvasPlain, 'the look deleted: the panel as it was');

        /* a look renamed from its form: the page that names it carried, the panel the same; Ctrl+Z in both */
        await evaluate(`[...document.querySelectorAll('.dgx-ui-builder .appearance .entries .node')].find((one) => one.textContent.trim() === 'caption').click()`);
        await until(`(document.querySelector('.dgx-ui-builder .appearance .form .entry-name') || {}).value === 'caption'`, 'the look picked');
        const canvasBefore = await canvasNow(), homeBefore = await evaluate(`${store}.files['main/home.json'].text`);
        await evaluate(`(() => { const name = document.querySelector('.dgx-ui-builder .appearance .form .entry-name'); name.value = 'title';
                                 name.dispatchEvent(new Event('change', { bubbles: true })); })()`);
        await until(`!!document.querySelector('.dgx-ui-builder .appearance .asking')`, 'the rename says first what it changes');
        check(await evaluate(`document.querySelector('.dgx-ui-builder .appearance .asking').textContent.includes('main/home.json')`), 'the page named');
        await evaluate(`document.querySelector('.dgx-ui-builder .appearance .asking button').click()`);
        await until(`${store}.files['main/home.json'].text.includes('"look": "title"') && ${store}.files['main/look.json'].text.includes('"title": {')`,
                    'renamed in the appearance and in the page');
        await sleep(200);
        check((await canvasNow()) === canvasBefore, 'the panel the same');
        await evaluate('document.activeElement.blur()');
        await Promise.all(key('z', 'KeyZ', 90, 2));
        await until(`${store}.files['main/home.json'].text === ${JSON.stringify(homeBefore)} && ${store}.files['main/look.json'].text.includes('"caption": {')`,
                    'Ctrl+Z gives both back');
        await evaluate(`${store}.setTab('properties')`);

        /* the names file by its forms: an expression typed, a name the page misses added, a rename carried into the
           page and undone in both */
        await evaluate(`[...document.querySelectorAll('.dgx-ui-builder .tabs button')].find((one) => one.textContent.trim() === 'Names').click()`);
        await until(`document.querySelectorAll('.dgx-ui-builder .names .name-row').length === 10`, 'the Names tab, its ten lines');
        const nameRow = (name) => `[...document.querySelectorAll('.dgx-ui-builder .names .name-row')].find((row) => row.querySelector('.entry-name').value === '${name}')`;
        await evaluate(`(() => { const input = ${nameRow('volume')}.querySelector('.expr'); input.focus(); input.select(); })()`);
        await send('Input.insertText', { text: '&room_volume' });
        await until(`${store}.files['main/names.txt'].text.includes('value volume &room_volume\\n') && ${store}.verdicts.names.code === 0`,
                    'an expression typed reaches the names file');
        await evaluate('document.activeElement.blur()');
        await evaluate(`(${store}.setTab('properties'), ${store}.select([1]), ${store}.setKey('value', 'bass'), ${store}.setTab('names'))`);
        await until(`${store}.verdicts.page.code > 0 && !!document.querySelector('.dgx-ui-builder .names .missing')`, 'a value the file has not: missing, listed');
        await evaluate(`document.querySelector('.dgx-ui-builder .names .missing button').click()`);
        await until(`/^value +bass +&bass$/m.test(${store}.files['main/names.txt'].text) && ${store}.verdicts.page.code === 0 &&
                     !document.querySelector('.dgx-ui-builder .names .missing')`, 'added from the list: the page taken');
        const homeNow = await evaluate(`${store}.files['main/home.json'].text`), namesNow = await evaluate(`${store}.files['main/names.txt'].text`);
        await evaluate(`(() => { const name = ${nameRow('bass')}.querySelector('.entry-name'); name.value = 'low';
                                 name.dispatchEvent(new Event('change', { bubbles: true })); })()`);
        await until(`!!document.querySelector('.dgx-ui-builder .names .asking')`, 'the rename says first what it changes');
        await evaluate(`document.querySelector('.dgx-ui-builder .names .asking button').click()`);
        await until(`${store}.files['main/home.json'].text.includes('"value": "low"') && /^value +low +&bass$/m.test(${store}.files['main/names.txt'].text)`,
                    'renamed in the file and the page, its C kept');
        await evaluate('document.activeElement.blur()');
        await Promise.all(key('z', 'KeyZ', 90, 2));
        await until(`${store}.files['main/home.json'].text === ${JSON.stringify(homeNow)} && ${store}.files['main/names.txt'].text === ${JSON.stringify(namesNow)}`,
                    'Ctrl+Z gives both back');
        await evaluate(`${store}.setTab('properties')`);

        /* the language */
        await evaluate(`${store}.setLanguage('ru')`);
        await until(`document.querySelector('.dgx-ui-builder h1').textContent === 'Построитель dgx-ui'`, 'the page in Russian');
        await evaluate(`${store}.setLanguage('en')`);

        await sleep(200);
        const shot = await send('Page.captureScreenshot', { format: 'png' });
        fs.writeFileSync(screenshot, Buffer.from(shot.data, 'base64'));
        check(fs.statSync(screenshot).size > 10000, 'a screenshot kept: ' + screenshot);

        /* the same project on the disk, through tools/dgx_ui_serve.py: the page by the link it prints opens the project at
           once, with nothing to choose; a description saved goes to the file, and a file the build reads is refused,
           saying why */
        const folder = fs.mkdtempSync(path.join(os.tmpdir(), 'dgx-ui-served-')), project = path.join(folder, 'demo');
        for (const [where, text] of Object.entries(encoded)) {
            fs.mkdirSync(path.dirname(path.join(project, where)), { recursive: true });
            fs.writeFileSync(path.join(project, where), Buffer.from(text, 'base64'));
        }
        server = spawn('python3', [path.join(ROOT, 'tools', 'dgx_ui_serve.py'), '--project', project, '--port', '0'], { stdio: ['ignore', 'pipe', 'inherit'] });
        const link = await new Promise((done, fail) => {
            let out = '';
            server.stdout.on('data', (data) => {
                out += data;
                const found = out.match(/^(http:\/\/127\.0\.0\.1:\d+\/editor\/#project=[\w-]+)$/m);
                if (found) done(found[1]);
            });
            server.once('exit', () => fail(new Error('the server stopped: ' + out)));
            setTimeout(() => fail(new Error('no link from the server in 20 s: ' + out)), 20000);
        });
        await send('Page.navigate', { url: link });
        await until(`typeof useDgxUiBuilder === 'function' && ${store}.ready && ${store}.project?.name === 'demo' && ${store}.current === 'main/home.json'`,
                    'the served page opens its project at once', 20000);
        check(await evaluate(`![...document.querySelectorAll('.dgx-ui-builder header button')].some((one) => one.textContent.includes('Open a project')) &&
                              !document.querySelector('.dgx-ui-builder input[type=file]')`), 'no folder to choose on the served page');
        await until(`${store}.drawn > 0`, 'its panel drawn');
        check(await evaluate(`(async () => { const host = ${store}.raw.host;
                                             return (await host.read('main/none.json')) === null && (await host.list('nothing')) === null &&
                                                    (await host.list('main')).some((entry) => entry.name === 'home.json'); })()`),
              'a file and a folder not there: null, as the host of the disk gives them');
        await evaluate(`(${store}.select([0]), ${store}.setPair('want', '0', '48'))`);
        await evaluate(`${store}.setColor('accent', '#123456')`);
        await evaluate(`${store}.save(true)`);
        await until(`!!document.querySelector('.dgx-ui-builder .asking')`, 'the word asked before writing in the builder\'s form');
        await evaluate(`document.querySelector('.dgx-ui-builder .asking button').click()`);
        const home = path.join(project, 'main', 'home.json');
        let written = '';
        for (const end = Date.now() + 10000; Date.now() < end && !written.includes('"want": [0, 48]'); await sleep(100)) written = fs.readFileSync(home, 'utf8');
        check(written.includes('"want": [0, 48]'), 'saved to the file on the disk: ' + written.slice(0, 200));
        check(fs.readFileSync(path.join(project, 'main', 'look.json'), 'utf8') === await evaluate(`${store}.files['main/look.json'].text`) &&
              fs.readFileSync(path.join(project, 'main', 'look.json'), 'utf8').includes('"accent": "#123456"'), 'the appearance saved with it, in its form');
        check(await evaluate(`!${store}.dirty.length && ${store}.raw.host.inPlace`), 'nothing edited after, written in place');
        /* a names file a call names, not on the disk yet: made of the names its page misses, written by the server */
        await evaluate(`(${store}.choose('main/other.json'), ${store}.setTab('names'))`);
        await until(`${store}.files['main/fresh.txt']?.missing && document.querySelectorAll('.dgx-ui-builder .names .missing').length >= 3`,
                    'the Names tab on a file not there, the names its page misses listed');
        await evaluate(`[...document.querySelectorAll('.dgx-ui-builder .names button')].find((one) => one.textContent.trim() === 'Add all').click()`);
        await until(`${store}.verdicts.page.code === 0 && ${store}.verdicts.names.code === 0`, 'all added: the page taken');
        await evaluate(`${store}.save(true)`);
        const fresh = path.join(project, 'main', 'fresh.txt');
        for (const end = Date.now() + 10000; Date.now() < end && !fs.existsSync(fresh); await sleep(100));
        check(fs.existsSync(fresh) && fs.readFileSync(fresh, 'utf8') === await evaluate(`${store}.files['main/fresh.txt'].text`) &&
              fs.readFileSync(fresh, 'utf8').includes('value') && fs.readFileSync(fresh, 'utf8').includes('level'), 'the names file written on the disk');
        /* under the server's Content-Security-Policy a page sends nothing to another host, by fetch nor by an image */
        const blockedBefore = errors.length;
        const out = await evaluate(`(async () => {
            const seen = [];
            document.addEventListener('securitypolicyviolation', (event) => seen.push(event.violatedDirective));
            const fetched = await fetch('http://example.com/leak?x=1').then(() => 'sent', () => 'blocked');
            const image = new Image();
            const loaded = await new Promise((done) => { image.onload = () => done('sent'); image.onerror = () => done('blocked'); image.src = 'http://example.com/leak.png'; });
            await new Promise((done) => setTimeout(done, 200));
            return { fetched, loaded, seen };
        })()`);
        check(out.fetched === 'blocked' && out.loaded === 'blocked' && out.seen.some((one) => one.startsWith('connect-src')) &&
              out.seen.some((one) => one.startsWith('img-src')), 'a fetch and an image of another host blocked by the policy: ' + JSON.stringify(out));
        errors.splice(blockedBefore); // the browser's own lines of the two refusals, the errors expected
        const errorsBefore = errors.length;
        const refusal = await evaluate(`${store}.raw.host.write('main/CMakeLists.txt', 'spoiled').then(() => 'written', (error) => error.message)`);
        await sleep(200);
        /* the browser's own line of the answer refused, the one error expected */
        const refused = errors.splice(errorsBefore).filter((line) => !/status of 400/.test(line));
        errors.push(...refused);
        check(/build's own/.test(refusal) && fs.readFileSync(path.join(project, 'main', 'CMakeLists.txt'), 'utf8') === files['main/CMakeLists.txt'],
              'a CMakeLists.txt refused, saying why: ' + refusal);
        server.kill('SIGKILL');
        server = null;

        /* the same project beside an agent: tools/dgx_ui_mcp.py over stdio, as a client of MCP speaks it */
        server = spawn('python3', [path.join(ROOT, 'tools', 'dgx_ui_mcp.py'), '--project', project, '--port', '0'], { stdio: ['pipe', 'pipe', 'inherit'] });
        let rows = '', ident = 0;
        const waiting = new Map();
        server.stdout.on('data', (data) => {
            rows += data;
            for (let end; (end = rows.indexOf('\n')) >= 0; rows = rows.slice(end + 1)) {
                const answer = JSON.parse(rows.slice(0, end));
                waiting.get(answer.id)?.(answer), waiting.delete(answer.id);
            }
        });
        const mcp = (method, params = {}) => new Promise((done) => {
            waiting.set(++ident, done);
            server.stdin.write(JSON.stringify({ jsonrpc: '2.0', id: ident, method, params }) + '\n');
        });
        const tool = async (name, args = {}) => {
            const answer = await mcp('tools/call', { name, arguments: args });
            return answer.result.content;
        };
        const valueOf = (content) => JSON.parse(content[0].text);
        await mcp('initialize', { protocolVersion: '2025-06-18', capabilities: {} });
        const agentLink = valueOf(await tool('page_link')).link;
        check(/^http:\/\/127\.0\.0\.1:\d+\/editor\/#project=[\w-]+&agent=1$/.test(agentLink), 'the agent gives the link: ' + agentLink);
        await send('Page.navigate', { url: agentLink });
        await until(`typeof useDgxUiBuilder === 'function' && ${store}.agent && ${store}.current === 'main/home.json' && ${store}.drawn > 0`,
                    'the builder beside the agent, its dialog shown', 20000);
        await until(`!!document.querySelector('.dgx-ui-builder .agent textarea')`, 'the dialog on the page');
        let state = valueOf(await tool('page_state'));
        for (let k = 0; k < 20 && state.error; ++k) await sleep(300), (state = valueOf(await tool('page_state')));
        check(state.open === 'main/home.json' && state.verdicts.page.taken && state.pages.some((one) => one.path === 'main/home.json'),
              'page_state: the page open, taken: ' + JSON.stringify(state).slice(0, 200));
        const agentBefore = await evaluate(`${store}.files['main/home.json'].text`);
        const changed = agentBefore.replace('"text": "Hello"', '"text": "Hello from the agent"');
        const edited = valueOf(await tool('edit_file', { path: 'main/home.json', text: changed, why: 'the greeting longer' }));
        check(edited.verdicts.page.taken && (await evaluate(`${store}.files['main/home.json'].text`)) === changed, 'edit_file: in the builder, taken');
        const spoiled = valueOf(await tool('edit_file', { path: 'main/home.json', text: changed.replace('"kind": "slider"', '"kind": "slidr"'), why: 'a try' }));
        check(spoiled.verdicts.page.code === 'DGX_UI_LOAD_KIND' && Array.isArray(spoiled.verdicts.widget), 'a refusal said by the loader\'s code and its widget: ' +
              JSON.stringify(spoiled.verdicts.page));
        await evaluate('document.activeElement && document.activeElement.blur()');
        await Promise.all(key('z', 'KeyZ', 90, 2));
        await until(`${store}.files['main/home.json'].text === ${JSON.stringify(changed)} && ${store}.verdicts.page.code === 0`, 'Ctrl+Z undoes the agent\'s edit');
        check(valueOf(await tool('read_file', { path: 'main/home.json' })).text === changed, 'read_file: the text as the builder holds it');
        check(valueOf(await tool('read_file', { path: 'main/secrets.h' })).error, 'read_file of no file of the builder: said');
        const picture = await tool('page_image');
        const bytes = Buffer.from(picture[0].data, 'base64');
        check(picture[0].type === 'image' && bytes.readUInt32BE(16) === 320 && bytes.readUInt32BE(20) === 240, 'page_image: a PNG of the screen\'s size');
        await tool('say', { text: 'The greeting is longer now.' });
        await until(`[...document.querySelectorAll('.dgx-ui-builder .agent .messages p.agent')].some((one) => one.textContent.includes('The greeting is longer now.'))`,
                    'say shown in the dialog');
        await evaluate(`(() => { const area = document.querySelector('.dgx-ui-builder .agent textarea'); area.focus(); })()`);
        await send('Input.insertText', { text: 'Thank you' });
        await Promise.all(key('Enter', 'Enter', 13));
        const heard = valueOf(await tool('wait_for_person', { seconds: 10 }));
        check(heard.messages.includes('Thank you'), 'the person\'s message to wait_for_person: ' + JSON.stringify(heard));
        check(!fs.readFileSync(path.join(project, 'main', 'home.json'), 'utf8').includes('from the agent'), 'nothing saved by the agent');
        server.stdin.end();
        server.kill('SIGKILL');
        server = null;
        fs.rmSync(folder, { recursive: true, force: true });
        check(!errors.length, 'no error in the console: ' + errors.join(' | '));
    } catch (error) {
        check(false, String(error && error.stack ? error.stack : error));
    } finally {
        server?.kill('SIGKILL');
        protocol?.close();
        browser.kill('SIGKILL');
        await new Promise((done) => (browser.exitCode !== null ? done() : browser.once('exit', done)));
        fs.rmSync(profile, { recursive: true, force: true });
    }
    if (failures) {
        console.error(`test_page.js: ${failures} of ${checks} checks failed`);
        process.exit(1);
    }
    console.log(`test_page.js: ${checks} checks of the page in ${path.basename(binary)}; a colour's pass ${passMs ? passMs[0].toFixed(2) : '?'} ms, ` +
                `at most ${passMs ? passMs[1].toFixed(2) : '?'}; the screenshot in ${screenshot}`);
})();
