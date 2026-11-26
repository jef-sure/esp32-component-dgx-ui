/*
 * node shots.js <font files directory> <directory>: the pictures of the builder for its pages, editor/index.html opened
 * from the disk in a headless Chrome over the DevTools protocol (chrome.js), given the project of test/hook/project in
 * memory under the name "hook", as its host gives one with no File System Access: no path of the disk reaches the page.
 * Six pictures, builder-*.png:
 *
 *     page        the window: the tree, the panel and Properties, the slider of main/pages/a.json selected
 *     appearance  the Appearance tab of main/framed.json, the look "slider" picked
 *     names       the Names tab of main/pages/a.json, a name of the page not in its names file
 *     texts       the text of a page the loader refuses, the refusal marked
 *     agent       the dialog beside an agent, a message each way, the panel as the agent's message says
 *     rename      the question asked before a look of an appearance is renamed in the pages on it
 *
 * Two files are changed in memory, never on the disk, so that a picture has something to show: the names file without
 * the line of volume, and a page with a kind misspelled. The window is 1280 x 800 at one pixel a pixel (960 wide for
 * the agent's picture, so that the panel and the dialog stand close), the builder's zoom 2 (1 for the agent's), with
 * no transitions, animations or caret; the page shows no clock. Every picture is made twice and the two runs held equal
 * byte for byte, the page's text held free of the machine's paths, user and host, and no error may reach the console;
 * only then are the pictures written. The same machine gives the same pixels; another machine's fonts may draw the
 * letters otherwise.
 *
 * Run: make -C test/host builder-shots OUT=<directory> (by default docs/images); SHOTS_DEBUG=<directory> keeps both runs'
 * pictures there when a check fails.
 */
'use strict';
const fs = require('fs');
const os = require('os');
const path = require('path');
const { spawn } = require('child_process');
const { chrome, devtools } = require('./chrome');

const ROOT = path.join(__dirname, '../..');
const PROJECT = path.join(ROOT, 'test', 'hook', 'project');
const [fontDirectory, out] = process.argv.slice(2);
const WIDTH = 1280, HEIGHT = 800, MARGIN = 8, AGENT_WIDTH = 960;
const sleep = (ms) => new Promise((done) => setTimeout(done, ms));
const failures = [];
const check = (held, what) => (held ? true : (failures.push(what), false));

/* the hook project as a host in memory gives it: its files by their paths, the fonts in its build */
function projectFiles() {
    const files = {};
    const walk = (directory) => {
        for (const entry of fs.readdirSync(path.join(PROJECT, directory), { withFileTypes: true })) {
            const where = directory ? `${directory}/${entry.name}` : entry.name;
            if (entry.isDirectory()) {
                if (!['build', 'managed_components'].includes(entry.name)) walk(where);
            } else if (!entry.name.startsWith('sdkconfig') && entry.name !== 'dependencies.lock') {
                files[where] = fs.readFileSync(path.join(PROJECT, where));
            }
        }
    };
    walk('');
    for (const name of fs.readdirSync(fontDirectory)) files[`build/dgx_fonts/${name}`] = fs.readFileSync(path.join(fontDirectory, name));
    return Object.fromEntries(Object.entries(files).map(([where, bytes]) => [where, bytes.toString('base64')]));
}

/* one run: the six pictures as PNG bytes by their names */
async function run(binary) {
    const profile = fs.mkdtempSync(path.join(os.tmpdir(), 'dgx-ui-builder-shots-'));
    const flags = ['--headless=new', `--user-data-dir=${profile}`, '--remote-debugging-port=0', '--no-first-run', '--no-default-browser-check',
                   '--disable-gpu', '--disable-extensions', `--window-size=${WIDTH},${HEIGHT}`, '--force-device-scale-factor=1',
                   '--font-render-hinting=none', '--disable-lcd-text', '--disable-font-subpixel-positioning', '--force-color-profile=srgb',
                   '--hide-scrollbars', 'about:blank'];
    const browser = spawn(binary, flags, { stdio: 'ignore' });
    let protocol = null;
    const shots = {};
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
        await send('Emulation.setDeviceMetricsOverride', { width: WIDTH, height: HEIGHT, deviceScaleFactor: 1, mobile: false });
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
                    if (await evaluate(expression)) return true;
                } catch {
                    /* the page not there yet */
                }
            }
            throw new Error(`${what}: not in ${ms} ms`);
        };
        const store = 'useDgxUiBuilder()';
        await until(`typeof useDgxUiBuilder === 'function' && ${store}.ready`, 'the module started in the page');
        /* no motion and no caret: the same pixels whenever the picture is taken */
        await evaluate(`(() => { const style = document.createElement('style');
            style.textContent = '*, *::before, *::after { transition: none !important; animation: none !important; caret-color: transparent !important; }';
            document.head.appendChild(style); })()`);
        await evaluate(`(async () => {
            const given = ${JSON.stringify(projectFiles())}, files = {};
            for (const [where, text] of Object.entries(given)) files[where] = Uint8Array.from(atob(text), (c) => c.charCodeAt(0));
            await ${store}.open(memoryHost('hook', files, () => true));
        })()`);
        await until(`${store}.page && ${store}.drawn > 0`, 'the project open, a page drawn');
        await evaluate('document.fonts.ready.then(() => true)');
        await sleep(1000); /* the first picture after the page's own controls have settled */

        /* the page as it is drawn, settled: the panel drawn after the last change, a moment for the layout */
        const settled = async (what) => {
            await until(`${store}.drawn > 0`, what);
            await sleep(400);
        };
        /* the text the page shows, held free of the machine */
        const unmarked = async (name) => {
            const text = await evaluate('document.body.innerText');
            for (const mark of ['/home', '/tmp', os.userInfo().username, os.hostname()].filter((one) => one && one.length > 2))
                check(!text.includes(mark), `${name}: the page's text holds "${mark}"`);
        };
        /* a picture: the window, or the box around elements, each scrolled into view, and a margin, in whole pixels within
           the window; at most so many pixels high */
        const shoot = async (name, selectors = [], margin = MARGIN, most = HEIGHT) => {
            await unmarked(name);
            let clip;
            if (selectors.length) {
                const box = await evaluate(`(() => {
                    const elements = ${JSON.stringify(selectors)}.map((one) => document.querySelector(one));
                    if (elements.some((one) => !one)) return null;
                    for (const element of elements) element.scrollIntoView({ block: 'nearest' });
                    const boxes = elements.map((one) => one.getBoundingClientRect());
                    return [Math.min(...boxes.map((r) => r.left)), Math.min(...boxes.map((r) => r.top)),
                            Math.max(...boxes.map((r) => r.right)), Math.max(...boxes.map((r) => r.bottom)), window.innerWidth]; })()`);
                if (!check(box, `${name}: not all of ${selectors.join(', ')} on the page`)) return;
                await sleep(200);
                const left = Math.max(0, Math.floor(box[0]) - margin), top = Math.max(0, Math.floor(box[1]) - margin);
                const right = Math.min(box[4], Math.ceil(box[2]) + margin), bottom = Math.min(HEIGHT, Math.ceil(box[3]) + margin, top + most);
                clip = { x: left, y: top, width: right - left, height: bottom - top, scale: 1 };
            }
            const shot = await send('Page.captureScreenshot', { format: 'png', ...(clip ? { clip } : {}) });
            shots[name] = Buffer.from(shot.data, 'base64');
        };
        const page = (where) => evaluate(`(${store}.choose(${JSON.stringify(where)}), ${store}.setZoom(2), ${store}.view = 'page', true)`);
        const textOf = (where) => evaluate(`${store}.files[${JSON.stringify(where)}].text`);

        /* the page: the slider selected, its properties beside the panel */
        await page('main/pages/a.json');
        await evaluate(`(${store}.setTab('properties'), ${store}.select([1]))`);
        await settled('the page of the sound');
        await shoot('page');

        /* the appearance: its look of the slider picked, its form */
        await page('main/framed.json');
        await evaluate(`(${store}.setTab('appearance'), ${store}.choosePick(['looks', 'slider']))`);
        await settled('the appearance of the frame');
        await shoot('appearance', ['.dgx-ui-builder .appearance']);

        /* the rename of the look of the frame: the question before the pages on the appearance change */
        await evaluate(`${store}.choosePick(['looks', 'pane'])`);
        await sleep(200);
        await evaluate(`(() => { const field = [...document.querySelectorAll('.dgx-ui-builder .appearance .entry-name')].find((one) => one.value === 'pane');
            field.value = 'panel'; field.dispatchEvent(new Event('change', { bubbles: true })); })()`);
        await until(`!!document.querySelector('.dgx-ui-builder .appearance .asking .dialog')`, 'the question of the rename');
        await sleep(200);
        await shoot('rename', ['.dgx-ui-builder .appearance .asking .dialog'], 48);
        await evaluate(`[...document.querySelectorAll('.dgx-ui-builder .appearance .asking button')].find((one) => one.textContent.trim() === 'Cancel').click()`);

        /* the agent: the title changed, as the agent's message says and the panel shows, the dialog both ways; in a window
           narrower by the panel's room, so that the panel and the dialog stand close */
        await page('main/pages/a.json');
        const sound = await textOf('main/pages/a.json');
        check(sound.includes('"text": "Sound"'), 'the page of the sound has its title "Sound"');
        await evaluate(`${store}.setText('page', ${JSON.stringify(sound.replace('"text": "Sound"', '"text": "Volume"'))})`);
        await evaluate(`(${store}.agent = true, ${store}.agentMessages.push({ from: 'person', text: 'Call the title "Volume".' },
            { from: 'agent', text: 'The title says "Volume" now; save when it suits you.' }), ${store}.view = 'page', ${store}.setZoom(1),
            ${store}.select([0]), true)`);
        await send('Emulation.setDeviceMetricsOverride', { width: AGENT_WIDTH, height: HEIGHT, deviceScaleFactor: 1, mobile: false });
        await settled('the page beside the agent');
        check(await evaluate(`${store}.verdicts.page.code === 0 && ${store}.files['main/pages/a.json'].text.includes('"text": "Volume"')`),
              'the page bears out what the agent says: its title "Volume"');
        await evaluate(`document.querySelector('.dgx-ui-builder .agent').scrollIntoView({ block: 'start' })`);
        await shoot('agent', ['.dgx-ui-builder canvas', '.dgx-ui-builder .agent']);
        await send('Emulation.setDeviceMetricsOverride', { width: WIDTH, height: HEIGHT, deviceScaleFactor: 1, mobile: false });
        await evaluate(`(${store}.agent = false, ${store}.agentMessages.splice(0), ${store}.setText('page', ${JSON.stringify(sound)}), true)`);

        /* the names: a name of the page the names file has not */
        const vocab = await textOf('main/vocab.txt');
        const without = vocab.split('\n').filter((line) => !/^value\s+volume\b/.test(line)).join('\n');
        check(without !== vocab, 'the names file had the line of volume');
        await evaluate(`(${store}.setTab('names'), ${store}.setText('names', ${JSON.stringify(without)}), true)`);
        await until(`document.querySelectorAll('.dgx-ui-builder .names .missing').length > 0`, 'the names the page misses listed');
        await sleep(300);
        await shoot('names', ['.dgx-ui-builder .names']);
        await evaluate(`${store}.setText('names', ${JSON.stringify(vocab)})`);

        /* the texts: a kind the loader knows not, its refusal marked, the cursor at it */
        const spoiled = (await textOf('main/pages/a.json')).replace('"kind": "slider"', '"kind": "slidr"');
        await evaluate(`(${store}.setTab('properties'), ${store}.setText('page', ${JSON.stringify(spoiled)}), ${store}.showText('page'), true)`);
        await until(`!!document.querySelector('.dgx-ui-builder .texts .wrong.verdict')`, 'the refusal marked');
        await evaluate(`${store}.putCursor()`);
        await sleep(300);
        await shoot('texts', ['.dgx-ui-builder .texts'], MARGIN, 300);

        check(!errors.length, 'no error on the console: ' + errors.join(' | '));
    } finally {
        protocol?.close();
        browser.kill('SIGKILL');
        await sleep(200);
        fs.rmSync(profile, { recursive: true, force: true, maxRetries: 10, retryDelay: 100 }); /* Chrome's helpers may still write it */
    }
    return shots;
}

(async () => {
    const binary = chrome();
    if (!binary) {
        console.error('shots.js: no Chrome: CHROME=<its path>');
        process.exit(1);
    }
    const first = await run(binary), second = await run(binary);
    const names = ['page', 'appearance', 'names', 'texts', 'agent', 'rename'];
    for (const name of names) {
        check(first[name] && second[name], `builder-${name}.png: not made`);
        if (first[name] && second[name]) check(first[name].equals(second[name]), `builder-${name}.png: the two runs differ`);
    }
    if (first.page) check(first.page.readUInt32BE(16) === WIDTH && first.page.readUInt32BE(20) === HEIGHT, `builder-page.png is not ${WIDTH} x ${HEIGHT}`);
    /* on a failure, the pictures of both runs kept where SHOTS_DEBUG says, for a look at what differs; never in OUT */
    if (failures.length && process.env.SHOTS_DEBUG) {
        fs.mkdirSync(process.env.SHOTS_DEBUG, { recursive: true });
        for (const name of names)
            for (const [k, pictures] of [[1, first], [2, second]])
                if (pictures[name]) fs.writeFileSync(path.join(process.env.SHOTS_DEBUG, `${name}-${k}.png`), pictures[name]);
    }
    if (failures.length) {
        for (const what of failures) console.error('  failed: ' + what);
        console.error(`shots.js: ${failures.length} check(s) failed; nothing written`);
        process.exit(1);
    }
    fs.mkdirSync(out, { recursive: true });
    for (const name of names) fs.writeFileSync(path.join(out, `builder-${name}.png`), first[name]);
    console.log('shots.js: ' + names.map((name) => `builder-${name}.png ${first[name].readUInt32BE(16)}x${first[name].readUInt32BE(20)} ` +
                                                    `${first[name].length} B`).join(', '));
})().catch((error) => {
    console.error('shots.js: ' + (error?.stack ?? error));
    process.exit(1);
});
