/*
 * node test_import.js <DGX>: a picture imported in the builder's store, editor/store.js, in Node as test_store.js drives
 * it: the host of files in memory, with the two calls of the server a picture takes, the file made by DGX's own writer
 * (tools/dgx_images.py of the DGX given, as the server runs it) and a PNG written as bytes.
 *  - the depth a picture takes by its colours, with holes and with none: 2/3, 15/16/17, 255/256/257;
 *  - PNG files given: their colours counted, the name made of the file's, the file made, the panel given its bytes and
 *    the page drawn with it before it is taken, the names file not changed for it; let go, the page refused again for
 *    the name it misses; the weight the file's length and in RAM 40 bytes a picture of 4 bits, 20 one of 16; the options make it again;
 *  - taken: the names file's line picture <name> @<name>, the need's line in the file of needs, the PNG beside it, each
 *    written at a save, the PNG first and as bytes; refused in words: no file of needs, a name of dgx_, a name taken;
 *  - soft edges, pixels partly opaque: of one colour a mask of 4 bits, 8 offered, 16 not; of more 16 bits keeping the
 *    opacity, no colour for the holes; the kind of each file DGX's writer made, the RAM 40 bytes a picture, the need's
 *    words MASK and no BACKGROUND; its widgets of another look given soft_picture, the names file its look's line; the
 *    soft edges let go, the depth by the colours again;
 *  - a host with no writer, as a page opened from the disk: the import not offered, said why.
 */
'use strict';
const fs = require('fs');
const os = require('os');
const path = require('path');
const vm = require('vm');
const zlib = require('zlib');
const { execFileSync } = require('child_process');
const ROOT = path.join(__dirname, '../..');
const EDITOR = path.join(ROOT, 'editor');
const DGX = path.resolve(process.argv[2]);
let failures = 0, checks = 0;
const check = (held, what) => {
    ++checks;
    if (!held) console.error('  failed: ' + what), ++failures;
    return held;
};
const encoder = new TextEncoder(), decoder = new TextDecoder();

function page() {
    const sandbox = { console, TextEncoder, TextDecoder, WebAssembly, atob, btoa, setTimeout, clearTimeout, queueMicrotask, Promise, URL,
                      navigator: { userAgent: '' } };
    sandbox.window = sandbox;
    vm.createContext(sandbox);
    const scripts = [...fs.readFileSync(path.join(EDITOR, 'index.html'), 'utf8').matchAll(/<script src="([^"]+)">/g)].map((match) => match[1]);
    for (const script of scripts.filter((name) => name !== 'app.js' && name !== 'start.js')) {
        const exported = { 'vendor/vue.global.prod.js': 'Vue', 'vendor/pinia.iife.prod.js': 'Pinia' }[script];
        vm.runInContext(fs.readFileSync(path.join(EDITOR, script), 'utf8') + (exported ? `\n;window.${exported} = ${exported};` : ''), sandbox, { filename: script });
    }
    return { sandbox, run: (code) => vm.runInContext(code, sandbox) };
}

/* a PNG of RGBA pixels, and its pixels as a browser decodes them */
function png(width, height, pixel) {
    const rgba = new Uint8Array(width * height * 4);
    for (let y = 0; y < height; ++y)
        for (let x = 0; x < width; ++x) rgba.set(pixel(x, y), (y * width + x) * 4);
    const rows = Buffer.concat([...Array(height).keys()].map((y) => Buffer.concat([Buffer.from([0]), Buffer.from(rgba.subarray(y * width * 4, (y + 1) * width * 4))])));
    const chunk = (kind, data) => {
        const head = Buffer.alloc(4);
        head.writeUInt32BE(data.length);
        const body = Buffer.concat([Buffer.from(kind), data]), crc = Buffer.alloc(4);
        crc.writeUInt32BE(zlib.crc32(body) >>> 0);
        return Buffer.concat([head, body, crc]);
    };
    const header = Buffer.alloc(13);
    header.writeUInt32BE(width, 0), header.writeUInt32BE(height, 4), header.set([8, 6, 0, 0, 0], 8);
    const bytes = Buffer.concat([Buffer.from([0x89, 0x50, 0x4e, 0x47, 0x0d, 0x0a, 0x1a, 0x0a]), chunk('IHDR', header), chunk('IDAT', zlib.deflateSync(rows)),
                                 chunk('IEND', Buffer.alloc(0))]);
    return { bytes: new Uint8Array(bytes), pixels: { width, height, rgba } };
}

/* the server's making of a file, by DGX's writer, as dgx_ui_serve.py runs it */
function made(pngs, options) {
    const work = fs.mkdtempSync(path.join(os.tmpdir(), 'dgx-ui-import-'));
    try {
        const names = pngs.map((bytes, k) => (fs.writeFileSync(path.join(work, `p${k}.png`), bytes), `p${k}.png`));
        const words = ['PNG', ...names, 'BITS', String(options.bits)];
        if (options.mask) words.push('MASK');
        if (options.reduce) words.push('REDUCE');
        if (options.dither) words.push('DITHER');
        if (options.ink) words.push('INK', options.ink.toUpperCase());
        if (options.background) words.push('BACKGROUND', options.background);
        try {
            execFileSync('python3', ['-I', path.join(DGX, 'tools', 'dgx_images.py'), 'make', 'out.dgxi', ...words], { cwd: work, stdio: 'pipe' });
        } catch (error) {
            throw new Error(String(error.stderr).trim() || 'refused');
        }
        return new Uint8Array(fs.readFileSync(path.join(work, 'out.dgxi')));
    } finally {
        fs.rmSync(work, { recursive: true, force: true });
    }
}

const PROJECT = {
    'CMakeLists.txt': 'project(demo)\n',
    'main/CMakeLists.txt': 'idf_component_register()\ndgx_ui_tree(page.json NAMES names.txt APPEARANCE look.json)\ndgx_image_needs_file(images.txt)\n',
    'main/page.json': '{"dgx-ui": 1, "tree": {"kind": "box", "flags": ["column"], "children": [{"kind": "picture", "picture": "logo", "look": "indexed"}]}}\n',
    'main/names.txt': 'look indexed\n',
    'main/look.json': '{"dgx-ui": 1, "name": "day", "colors": {"paper": "#000000", "ink": "#ffffff", "fill": "#ff0000", "border": "#0080ff"}}\n',
    'main/images.txt': '# the pictures\n',
};

async function storeOf(files, withWriter) {
    const { sandbox, run } = page();
    run('Pinia.setActivePinia(Pinia.createPinia())');
    const store = run('dgxUiBuilderStore()');
    await store.start({ preferences: { get: () => null, set: () => {} } });
    check(store.ready && !store.failed, 'the module started: ' + store.failed);
    const written = {}, binary = [];
    sandbox.files = Object.fromEntries(Object.entries(files).map(([where, text]) => [where, encoder.encode(text)]));
    sandbox.save = (where, text) => ((written[where] = text), true);
    const host = run('dgxUiMemoryHost("demo", files, save)');
    if (withWriter) {
        host.image = async (pngs, options) => made(pngs, options);
        host.writeBinary = async (where, bytes) => (binary.push([where, bytes]), true);
    }
    store.raw.pixelsOf = async (bytes) => given.get(bytes);
    await store.open(host);
    return { store, written, binary, run };
}
const given = new Map(); // the pixels of each PNG made here, as a browser would decode it
const pictureBitsOf = (one) => (one.colors <= 2 - (one.transparent ? 1 : 0) ? 1 : one.colors <= 16 - (one.transparent ? 1 : 0) ? 4 : one.colors <= 256 - (one.transparent ? 1 : 0) ? 8 : 16);

(async () => {
    /* the depth by the colours */
    {
        const { run } = page();
        const bits = (colors, holes) => run(`dgxUiBuilderStore.pictureBits(${colors}, ${holes})`);
        const table = [[2, false, 1], [3, false, 4], [1, true, 1], [2, true, 4], [15, true, 4], [16, true, 8], [16, false, 4], [17, false, 8],
                       [255, true, 8], [256, true, 16], [256, false, 8], [257, false, 16], [257, true, 16]];
        for (const [colors, holes, want] of table) check(bits(colors, holes) === want, `${colors} colours${holes ? ' and holes' : ''}: ${want} bits, not ${bits(colors, holes)}`);
        check(run('dgxUiBuilderStore.pictureName("My Logo-2.png")') === 'my_logo_2' && run('dgxUiBuilderStore.pictureName("9.png")') === '_9' &&
              run('dgxUiBuilderStore.pictureName("dgx_x.png")') === 'picture_dgx_x', 'a name of C made of a file\'s');
    }

    /* a picture imported, taken, saved */
    {
        const { store, written, binary } = await storeOf(PROJECT, true);
        check(store.canImport(), 'the import offered with the server\'s writer');
        store.choose('main/page.json');
        check(store.verdicts.page.code > 0 && store.verdicts.page.name === 'logo', 'before the import the page misses its picture: ' + JSON.stringify(store.verdicts.page));
        const logo = png(6, 4, (x, y) => ((x + y) % 4 === 0 ? [0, 0, 0, 0] : [[255, 0, 0, 255], [0, 128, 255, 255], [255, 255, 255, 255]][(x + y) % 3]));
        given.set(logo.bytes, logo.pixels);
        check(await store.importStart([{ name: 'logo.png', bytes: logo.bytes }]), 'PNG files given');
        let one = store.importing;
        check(one && one.name === 'logo' && one.colors === 3 && one.transparent && one.bits === 4 && one.made && one.made[6] === 4 && one.given,
              `counted and made: ${JSON.stringify({ name: one?.name, colors: one?.colors, bits: one?.bits, said: one?.said })}`);
        check(store.verdicts.page.code === 0, 'the page drawn with the picture made, before it is taken: ' + JSON.stringify(store.verdicts.page));
        check(store.files['main/names.txt'].text === 'look indexed\n' && store.namesMissing.some((one) => one.table === 'picture' && one.name === 'logo'),
              'the names file not changed for it, the name still missing from it');
        store.importCancel();
        check(store.verdicts.page.code > 0 && store.verdicts.page.name === 'logo', 'let go: the page misses its picture again');
        given.set(logo.bytes, logo.pixels);
        await store.importStart([{ name: 'logo.png', bytes: logo.bytes }]);
        one = store.importing;
        const weight = store.importWeight();
        check(weight && weight.flash === one.made.length && weight.ram === 40 && weight.count === 1, `the weight: ${JSON.stringify(weight)}`);
        await store.importSet('bits', 8);
        check(store.importing.made[6] === 8, 'made again of another depth');
        await store.importSet('bits', 16);
        check(store.importing.made[6] === 16 && store.importing.made[7] === 2, 'of 16 bits with its holes painted: ' + store.importing.said);
        check(store.importWeight()?.ram === 20, 'of 16 bits: no picture of DGX in RAM, 20 bytes: ' + JSON.stringify(store.importWeight()));
        await store.importSet('bits', 4);
        store.choose('main/page.json');
        check(await store.importAccept(), 'taken: ' + JSON.stringify(store.said));
        check(store.files['main/names.txt'].text.includes('picture logo @logo'), 'the names file\'s line');
        check(store.files['main/images.txt'].text === '# the pictures\nlogo PNG images/logo.png BITS 4\n', 'the need\'s line: ' + store.files['main/images.txt'].text);
        const saved = await store.save(true, true);
        check(binary.length === 1 && binary[0][0] === 'main/images/logo.png' && binary[0][1] === logo.bytes && saved[0] === 'main/images/logo.png',
              'the PNG written first, as bytes: ' + JSON.stringify(saved));
        check(written['main/names.txt']?.includes('picture logo @logo') && written['main/images.txt']?.includes('logo PNG images/logo.png BITS 4'),
              'the names file and the file of needs written at the save');
        // refusals
        given.set(logo.bytes, logo.pixels);
        await store.importStart([{ name: 'logo.png', bytes: logo.bytes }]);
        check(store.importing.name === 'logo_2', 'a name taken made another: ' + store.importing.name);
        await store.importSet('name', 'dgx_logo');
        check(!(await store.importAccept()) && store.said.some((one) => one.wrong && one.text.includes('not of dgx_')), 'a name of dgx_ refused');
        store.importCancel();
        check(store.importing === null, 'the import let go');
    }

    /* soft edges: a mask of one colour, 16 bits of more */
    {
        const { run } = page();
        const kind = (colors, holes, soft) => run(`JSON.stringify(dgxUiBuilderStore.pictureKind(${colors}, ${holes}, ${soft}))`);
        check(kind(1, true, true) === '{"mask":true,"bits":4}' && kind(3, true, true) === '{"mask":false,"bits":16}' && kind(3, true, false) === '{"mask":false,"bits":4}',
              'the kind by colours and soft edges: ' + [kind(1, true, true), kind(3, true, true), kind(3, true, false)]);
        const { store, written } = await storeOf(PROJECT, true);
        const disc = png(8, 8, (x, y) => [255, 200, 0, Math.max(0, Math.min(255, 255 - Math.round(Math.hypot(x - 3.5, y - 3.5) * 60)))]);
        given.set(disc.bytes, disc.pixels);
        store.choose('main/page.json');
        await store.importStart([{ name: 'logo.png', bytes: disc.bytes }]);
        let one = store.importing;
        check(one.partly && one.soft && one.mask && one.bits === 4 && one.made && one.made[7] === 3 && one.made[6] === 4,
              `one colour with soft edges: a mask of 4 bits, made: ${JSON.stringify({ mask: one.mask, bits: one.bits, said: one.said, kind: one.made?.[7] })}`);
        check(store.importWeight().ram === 40, 'a mask: 40 bytes of RAM');
        await store.importSet('bits', 8);
        check(store.importing.bits === 8 && store.importing.made[6] === 8 && store.importing.made[7] === 3, 'a mask of 8 bits');
        await store.importSet('bits', 16);
        check(store.importing.bits === 4 && store.importing.made[6] === 4, 'a mask of 16 bits not taken: 4');
        check(store.importUsers().other.length === 1, 'its widget drawn by another look: ' + JSON.stringify(store.importUsers()));
        check(store.importDrawSoft() && store.importUsers().other.length === 0 && store.files['main/page.json'].text.includes('"look": "soft_picture"') &&
              store.files['main/names.txt'].text.includes('look soft_picture'), 'its widget given soft_picture, the names file its look');
        check(store.verdicts.page.code === 0, 'the page drawn with the mask: ' + JSON.stringify(store.verdicts.page));
        check(await store.importAccept(), 'the mask taken: ' + JSON.stringify(store.said));
        check(store.files['main/images.txt'].text.includes('logo PNG images/logo.png BITS 4 MASK\n'), 'the need of a mask: ' + store.files['main/images.txt'].text);
        // more colours with soft edges: 16 bits that keep their opacity
        const glow = png(6, 6, (x, y) => [x * 40, y * 40, 200, (x + y) * 20]);
        given.set(glow.bytes, glow.pixels);
        await store.importStart([{ name: 'glow.png', bytes: glow.bytes }]);
        one = store.importing;
        check(one.soft && !one.mask && one.bits === 16 && one.made && one.made[6] === 16 && one.made[7] === 2 && store.importWeight().ram === 40,
              `more colours with soft edges: 16 bits with their opacity: ${JSON.stringify({ bits: one.bits, said: one.said })}`);
        await store.importSet('soft', false);
        check(!store.importing.soft && store.importing.bits === pictureBitsOf(store.importing) && store.importWeight().ram === (store.importing.bits === 16 ? 20 : 40),
              'soft edges let go: the depth by the colours again');
        await store.importSet('soft', true);
        check(await store.importAccept(), 'the picture of 16 bits taken');
        check(/^glow PNG images\/glow\.png BITS 16$/m.test(store.files['main/images.txt'].text), 'its need with no BACKGROUND: ' + store.files['main/images.txt'].text);
        void written;
    }

    /* no file of needs: refused in words */
    {
        const files = { ...PROJECT, 'main/CMakeLists.txt': 'idf_component_register()\ndgx_ui_tree(page.json NAMES names.txt APPEARANCE look.json)\n' };
        const { store } = await storeOf(files, true);
        const dot = png(2, 2, () => [255, 255, 255, 255]);
        given.set(dot.bytes, dot.pixels);
        await store.importStart([{ name: 'dot.png', bytes: dot.bytes }]);
        check(store.importing.bits === 1, 'one colour: 1 bit');
        store.choose('main/page.json');
        check(!(await store.importAccept()) && store.said.some((one) => one.wrong && one.text.includes('dgx_image_needs_file(images.txt)')), 'no file of needs: said');
    }

    /* a host with no writer: not offered */
    {
        const { store } = await storeOf(PROJECT, false);
        check(!store.canImport() && !(await store.importStart([{ name: 'x.png', bytes: new Uint8Array(1) }])) &&
              store.said.some((one) => one.text.includes('open the project through dgx_ui_serve.py')), 'no writer: the import not offered, said why');
    }

    if (failures) console.error(`test_import.js: ${failures} of ${checks} checks failed`);
    else console.log(`test_import.js: ${checks} checks held`);
    process.exit(failures ? 1 : 0);
})().catch((error) => {
    console.error(error);
    process.exit(1);
});
