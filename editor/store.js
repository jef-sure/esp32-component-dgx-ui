// The state of the builder: the project open, the page chosen with its description as a document, the selection, and
// what the preview's module said of the texts and drew. What the format allows is the loader's word: every text goes to
// the module, and its verdicts are shown, none of the format's rules written here again. description.js reads, edits
// and writes a description, appearance.js an appearance, names.js the lines of a names file; history.js keeps undo, a
// history a file; a host (host_browser.js) gives the files.
//
// A project is what its CMakeLists say: main/CMakeLists.txt, components/*/CMakeLists.txt and the top one are read for
// dgx_ui_tree(), dgx_ui_names() and dgx_ui_appearance(), each page with its names file and appearance as the call
// gives them. A call the reading cannot be sure of, a variable in it or a block such as if() around it, is listed as
// not understood with its file and line, never guessed. The other descriptions of the project, files holding "dgx-ui"
// and "tree", are pages too, read at run time, on a names file and an appearance chosen. The fonts are the files DGX
// writes with dgx_fonts-files.txt, in build/dgx_fonts/ or build/<board>/dgx_fonts/.
'use strict';

const dgxUiBuilderStore = (() => {
    const description = window.DgxUiDescription;
    const { read, write, widgets, widgetAt, indexOf, pathOf, samePath, valueOf, shown, builderOf, unitOfByte, illFormedAt } = description;
    const { History } = window.DgxUiHistory;
    const appearance = window.DgxUiAppearance;
    const namesFile = window.DgxUiNames;

    const PREFERENCES = { zoom: 'dgx-ui-builder.zoom', screen: 'dgx-ui-builder.screen', language: 'dgx-ui-builder.language' }; // localStorage
    // the screens offered: the panels of the boards dgx-ui is tried on
    const SCREENS = [
        { label: '320x240 (CYD)', width: 320, height: 240, depth: 16, unit: 1 },
        { label: '480x272', width: 480, height: 272, depth: 16, unit: 1 },
        { label: '480x800', width: 480, height: 800, depth: 16, unit: 1 },
        { label: '1024x600', width: 1024, height: 600, depth: 16, unit: 1 },
        { label: '800x1280', width: 800, height: 1280, depth: 16, unit: 1 },
    ];
    const DEPTHS = [16, 18, 24];
    const ZOOMS = [1, 2, 3, 4];
    // the loader's codes, dgx_ui_load.h; -1 a names file refused, -3 a font, both in words
    const LOAD_CODES = ['OK', 'SYNTAX', 'VERSION', 'KEY', 'KIND', 'FIELD', 'RANGE', 'NAME_BAD', 'UNKNOWN', 'NO_LOOK', 'NO_STYLE', 'TOO_DEEP',
                        'MEMORY', 'NODES', 'ID', 'MISSING'];
    const SYNTAX = 1, NO_LOOK = 9;
    const CALLS = { dgx_ui_tree: ['PREFIX', 'NAMES', 'APPEARANCE'], dgx_ui_names: ['PREFIX', 'APPEARANCE'], dgx_ui_appearance: ['PREFIX', 'NAMES'],
                    dgx_image_needs_file: [] };
    const IMAGES_INDEX = 'dgx_images-files.txt';
    // the bytes a picture of a file costs in RAM on a 32-bit chip, as the made C keeps it: its dgx_ui_picture_t, and for a
    // file of 1, 4 or 8 bits a dgx_image_t of DGX too (include/dgx_ui_picture_file.h)
    const PICTURE_RAM = 20, IMAGE_RAM = 20;

    // the kind and depth an imported picture takes: with soft edges (pixels partly opaque), a mask of 4 bits for one
    // colour, drawn in the ink of the look soft_picture, and 16 bits keeping the opacity for more; else by its colours
    function pictureKind(colors, transparent, soft) {
        if (soft) return colors <= 1 ? { mask: true, bits: 4 } : { mask: false, bits: 16 };
        return { mask: false, bits: pictureBits(colors, transparent) };
    }

    // the depth an imported picture takes by its colours: 1 bit for two (one with holes), 4 for 16 (15), 8 for 256 (255),
    // as DGX's palettes hold them; 16, RGB565, past that
    function pictureBits(colors, transparent) {
        const hole = transparent ? 1 : 0;
        if (colors <= 2 - hole) return 1;
        if (colors <= 16 - hole) return 4;
        if (colors <= 256 - hole) return 8;
        return 16;
    }

    // the pixels of a PNG as the browser decodes it: {width, height, rgba}, for its colours alone (DGX's writer makes the file)
    async function pixelsOfPng(bytes) {
        const image = await createImageBitmap(new Blob([bytes], { type: 'image/png' }));
        const canvas = new OffscreenCanvas(image.width, image.height), context = canvas.getContext('2d');
        context.drawImage(image, 0, 0);
        return { width: image.width, height: image.height, rgba: context.getImageData(0, 0, image.width, image.height).data };
    }

    // a name of C made of a file's name, for an imported picture's asset and its name in the names file
    function pictureName(file) {
        const base = String(file).replace(/\.[^.]*$/, '').replace(/[^A-Za-z0-9_]/g, '_').replace(/^([0-9])/, '_$1').toLowerCase();
        return base.startsWith('dgx_') ? 'picture_' + base : base || 'picture';
    }
    const BLOCKS = /^(if|foreach|while|function|macro|block)$/;
    const BLOCK_ENDS = /^end(if|foreach|while|function|macro|block)$/;
    // what the search for descriptions does not enter
    const SKIPPED = new Set(['.git', '.cache', '.vscode', '.idea', 'node_modules', 'managed_components', 'dist', 'vendor']);
    const WALK_ENTRIES = 4000; // the entries of directories the search looks at, at most
    const WALK_DEPTH = 8;
    const FONTS_INDEX = 'dgx_fonts-files.txt';
    const WAKE_MOST = 1000; // a page with nothing asking to be drawn is still drawn each second: its events read
    const EVENTS_KEPT = 40;
    const JSON_NUMBER = /^-?(0|[1-9][0-9]*)(\.[0-9]+)?([eE][+-]?[0-9]+)?$/;
    const PERCENT = /^-?[0-9]+(\.[0-9]+)?%$/; // "50%": the loader judges its form and range
    // a number typed: a number as JSON writes one, kept as typed; a percent, a string; null for anything else
    const typedNode = (text) => (JSON_NUMBER.test(text) ? { t: 'number', raw: text } : PERCENT.test(text) ? text : null);

    const decoder = new TextDecoder('utf-8', { ignoreBOM: true });
    const textOf = (bytes) => (bytes ? decoder.decode(bytes) : null);
    const kindOf = (widget) => shown(valueOf(widget, 'kind'));
    const stringOf = (node) => (node && node.t === 'string' ? node.value : null);

    // ---- the project's CMakeLists ----

    // a path of a call, relative to the directory of its CMakeLists, as one of the project's; null when it leaves it
    function joinPath(directory, relative) {
        if (relative.startsWith('/') || /^[A-Za-z]:/.test(relative)) return null;
        const parts = directory ? directory.split('/') : [];
        for (const part of relative.split('/')) {
            if (part === '' || part === '.') continue;
            if (part === '..') {
                if (!parts.length) return null;
                parts.pop();
            } else {
                parts.push(part);
            }
        }
        return parts.join('/');
    }

    // the words of a call's arguments: unquoted, or quoted with no escape; null for what the reading does not take, a
    // variable, a list, an escape, a bracket argument
    function cmakeWords(text) {
        const words = [];
        const word = /"([^"\\]*)"|([^\s"()\\]+)|(\S)/g;
        for (let match; (match = word.exec(text));) {
            if (match[3] !== undefined) return null;
            const value = match[1] ?? match[2];
            if (/[$;]/.test(value) || value.startsWith('[')) return null;
            words.push(value);
        }
        return words;
    }

    // the calls of dgx-ui in the text of a CMakeLists: {calls: [{call, file, names, appearance, from}], notUnderstood:
    // [{file, line, text, why, values}]}; file is the project's path of the CMakeLists
    function cmakeCalls(file, text) {
        // the comments made spaces, the positions kept
        let clean = '';
        for (let k = 0, quoted = false; k < text.length; ++k) {
            const c = text[k];
            if (quoted) {
                clean += c;
                if (c === '\\' && k + 1 < text.length) clean += text[++k];
                else if (c === '"') quoted = false;
            } else if (c === '"') {
                quoted = true, clean += c;
            } else if (c === '#') {
                while (k < text.length && text[k] !== '\n') clean += ' ', ++k;
                if (k < text.length) clean += '\n';
            } else {
                clean += c;
            }
        }
        const directory = file.split('/').slice(0, -1).join('/');
        const calls = [], notUnderstood = [];
        const command = /([A-Za-z_][A-Za-z0-9_]*)[ \t]*\(/g;
        let depth = 0;
        for (let match; (match = command.exec(clean));) {
            const name = match[1].toLowerCase(), open = match.index + match[0].length;
            let end = open, level = 1;
            for (let quoted = false; end < clean.length && level; ++end) {
                const c = clean[end];
                if (quoted) {
                    if (c === '\\') ++end;
                    else if (c === '"') quoted = false;
                } else if (c === '"') {
                    quoted = true;
                } else if (c === '(') {
                    ++level;
                } else if (c === ')') {
                    --level;
                }
            }
            command.lastIndex = end;
            if (BLOCKS.test(name)) {
                ++depth;
                continue;
            }
            if (BLOCK_ENDS.test(name)) {
                depth = Math.max(0, depth - 1);
                continue;
            }
            if (!(name in CALLS)) continue;
            const line = clean.slice(0, match.index).split('\n').length;
            const said = (why, values = []) =>
                notUnderstood.push({ file, line, text: text.slice(match.index, end).split('\n')[0].trim(), why, values });
            if (level) {
                said('the call has no end');
                break;
            }
            if (depth) {
                said('within a block, if() or the like: whether it is made is the build\'s to say');
                continue;
            }
            const words = cmakeWords(clean.slice(open, end - 1));
            if (!words || !words.length) {
                said('a variable, a list or a form of argument the builder does not read');
                continue;
            }
            const found = { call: name, file: joinPath(directory, words[0]), from: `${file}:${line}` };
            let wrong = found.file === null ? ['{0}: not a file of the project', [words[0]]] : null;
            for (let k = 1; k < words.length && !wrong; ++k) {
                if (words[k] === 'HEADERS') {
                    while (k + 1 < words.length && !CALLS[name].includes(words[k + 1])) ++k;
                } else if (CALLS[name].includes(words[k]) && k + 1 < words.length) {
                    const value = words[++k];
                    if (words[k - 1] === 'NAMES' || words[k - 1] === 'APPEARANCE') {
                        const path = joinPath(directory, value);
                        if (path === null) wrong = ['{0}: not a file of the project', [value]];
                        else found[words[k - 1] === 'NAMES' ? 'names' : 'appearance'] = path;
                    } else {
                        found.prefix = value;
                    }
                } else {
                    wrong = ['"{0}": a word the call does not take', [words[k]]];
                }
            }
            if (wrong) said(...wrong);
            else calls.push(found);
        }
        return { calls, notUnderstood };
    }

    // ---- the project ----

    async function walk(host, directory, found, budget) {
        const entries = (await host.list(directory)) ?? [];
        for (const entry of entries) {
            if (--budget.left <= 0) return;
            const path = directory ? `${directory}/${entry.name}` : entry.name;
            if (entry.directory) {
                const entered = !SKIPPED.has(entry.name) && !entry.name.startsWith('build') && path.split('/').length < WALK_DEPTH;
                if (entered) await walk(host, path, found, budget);
            } else if (entry.name.endsWith('.json')) {
                found.push(path);
            }
        }
    }

    // the panel of the board a build is of: the configuration showing the display (the build's sdkconfig, else the
    // project's sdkconfig.<BOARD> where a preset of boards keeps it), the display's names of boards first, then the
    // computer's; else the board's entry of jef-sure/boards, local or of the catalog; null when none says it
    async function boardScreen(host, board, binaryDir) {
        const configs = [binaryDir ? `${binaryDir}/sdkconfig` : null, board && /^[a-z0-9_]+$/.test(board) ? `sdkconfig.${board}` : null];
        for (const file of configs.filter((one) => one)) {
            const config = textOf(await host.read(file));
            if (!config) continue;
            for (const kind of ['DISPLAY', 'HOST']) {
                const value = (name) => +(config.match(new RegExp(`^CONFIG_BOARD_${kind}_${name}=([0-9]+)$`, 'm'))?.[1] ?? NaN);
                const depth = DEPTHS.includes(value('COLOR_BITS')) ? value('COLOR_BITS') : 16;
                if (value('WIDTH') > 0 && value('HEIGHT') > 0) return { width: value('WIDTH'), height: value('HEIGHT'), depth, from: file };
            }
        }
        if (!board || !/^[a-z0-9_]+$/.test(board)) return null;
        const entries = [`components/local-boards/boards/${board}/board.json`,
                         `managed_components/jef-sure__boards/catalog/boards/${board}/board.json`];
        for (const file of entries) {
            let entry = null;
            try {
                entry = JSON.parse(textOf(await host.read(file)) ?? 'null');
            } catch {
                continue;
            }
            for (const device of Object.values(entry?.devices ?? {})) {
                const panel = device?.config?.panel;
                const depth = DEPTHS.includes(device.config.depth_bits) ? device.config.depth_bits : 16;
                if (Number.isInteger(panel?.width) && Number.isInteger(panel?.height))
                    return { width: panel.width, height: panel.height, depth, from: file };
            }
        }
        return null;
    }

    // jef-sure/boards in the project, by what the host reads: its copy in managed_components/, a board of the project's
    // own in components/local-boards, or a BOARD in the preset. The copy, when it has tools/boards_mcp.py; null none
    async function boardsOf(host, board) {
        const named = async (folder, name) => ((await host.list(folder)) ?? []).some((entry) => entry.name === name);
        const copy = 'managed_components/jef-sure__boards';
        if (await named('managed_components', 'jef-sure__boards')) return { copy: (await named(`${copy}/tools`, 'boards_mcp.py')) ? copy : null };
        if (board || (await named('components', 'local-boards'))) return { copy: null };
        return null;
    }

    // what the project is: its pages, names files and appearances by its CMakeLists, the other descriptions found, the
    // calls not understood, the directories of fonts and the board of its preset
    async function readProject(host) {
        const project = { name: host.name, pages: [], namesFiles: [], appearances: [], notUnderstood: [], fontDirs: [], fontDir: null, board: null,
                          boardScreen: null, notes: [], needsFiles: [], imageDirs: [], imageDir: null };
        const lists = ['CMakeLists.txt', 'main/CMakeLists.txt'];
        for (const entry of (await host.list('components')) ?? []) if (entry.directory) lists.push(`components/${entry.name}/CMakeLists.txt`);
        const calls = [];
        for (const file of lists) {
            const text = textOf(await host.read(file));
            if (text === null) continue;
            const said = cmakeCalls(file, text);
            calls.push(...said.calls), project.notUnderstood.push(...said.notUnderstood);
        }
        for (const call of calls.filter((one) => one.call === 'dgx_ui_appearance')) {
            if (!project.appearances.some((one) => one.path === call.file))
                project.appearances.push({ path: call.file, names: call.names ?? null, from: call.from, prefix: call.prefix ?? null });
        }
        for (const call of calls.filter((one) => one.call === 'dgx_image_needs_file'))
            if (!project.needsFiles.includes(call.file)) project.needsFiles.push(call.file);
        for (const call of calls.filter((one) => one.call === 'dgx_ui_names'))
            project.namesFiles.push({ path: call.file, appearance: call.appearance ?? null, from: call.from });
        for (const call of calls.filter((one) => one.call === 'dgx_ui_tree')) {
            if (project.pages.some((page) => page.path === call.file)) continue; // one page however many trees are made of it
            project.pages.push({ path: call.file, names: call.names ?? null, appearance: call.appearance ?? null, from: call.from });
        }
        // the other descriptions: pages a program loads at run time
        const found = [], budget = { left: WALK_ENTRIES };
        await walk(host, '', found, budget);
        const known = new Set([...project.pages.map((page) => page.path), ...project.appearances.map((one) => one.path)]);
        const lone = project.namesFiles.length === 1 ? project.namesFiles[0] : null;
        for (const path of found.sort()) {
            if (known.has(path)) continue;
            // one that does not read is a page still when its text holds both keys: it is there to be mended; an appearance
            // loaded at run time holds "colors" where a page holds "tree"
            const data = await host.read(path), said = read(data), text = said.ok ? null : decoder.decode(data);
            const has = (key) => (said.ok ? !!valueOf(said.doc, 'dgx-ui') && !!valueOf(said.doc, key) : new RegExp(`"dgx-ui"[^]*"${key}"`).test(text));
            const page = has('tree');
            if (!page && has('colors')) project.appearances.push({ path, names: null, from: null, prefix: null });
            if (!page) continue;
            project.pages.push({ path, names: lone?.path ?? null, appearance: lone?.appearance ?? null, from: null });
        }
        // the fonts: the index DGX writes, in build/dgx_fonts/ or in a board's build/<board>/dgx_fonts/
        for (const entry of (await host.list('build')) ?? []) {
            if (!entry.directory) continue;
            const dir = entry.name === 'dgx_fonts' ? 'build/dgx_fonts' : `build/${entry.name}/dgx_fonts`;
            if ((await host.read(`${dir}/${FONTS_INDEX}`)) !== null) project.fontDirs.push({ dir, board: entry.name === 'dgx_fonts' ? null : entry.name });
            const images = entry.name === 'dgx_images' ? 'build/dgx_images' : `build/${entry.name}/dgx_images`;
            if ((await host.read(`${images}/${IMAGES_INDEX}`)) !== null) project.imageDirs.push({ dir: images, board: entry.name === 'dgx_images' ? null : entry.name });
        }
        let preset = null;
        try {
            const presets = JSON.parse(textOf(await host.read('CMakePresets.json')) ?? 'null');
            preset = presets?.configurePresets?.find((one) => one?.name === 'default') ?? null;
        } catch {
            project.notes.push({ text: 'CMakePresets.json: not JSON, its board not read', values: [] });
        }
        const binaryDir = typeof preset?.binaryDir === 'string' ? preset.binaryDir.replace(/^\$\{sourceDir\}\//, '') : null;
        project.board = typeof preset?.cacheVariables?.BOARD === 'string' ? preset.cacheVariables.BOARD : null;
        const ofPreset = project.fontDirs.find((one) => binaryDir && one.dir === `${binaryDir}/dgx_fonts`);
        project.fontDir = (ofPreset ?? project.fontDirs[0])?.dir ?? null;
        // the pictures DGX made: of the fonts' build when it made them too, else the preset's, else the first
        const imagesOf = (dir) => dir && project.imageDirs.find((one) => one.dir === dir.replace(/dgx_fonts$/, 'dgx_images'));
        project.imageDir = (imagesOf(project.fontDir) ?? project.imageDirs.find((one) => binaryDir && one.dir === `${binaryDir}/dgx_images`) ??
                            project.imageDirs[0])?.dir ?? null;
        if (project.fontDirs.length > 1 && !ofPreset)
            project.notes.push({ text: 'fonts of several builds: {0} taken, another may be chosen', values: [project.fontDir] });
        if (project.board) project.boardScreen = await boardScreen(host, project.board, binaryDir);
        project.boards = await boardsOf(host, project.board);
        return project;
    }

    // a file as the store holds it: the text as saved and as edited; one not well-formed UTF-8 shown with its bytes
    // replaced, where they are not, until it is edited
    function fileState(bytes, kind) {
        if (bytes === null) return { saved: null, text: '', missing: true, illFormed: null, formed: true, byBuilder: false };
        const bad = illFormedAt(bytes);
        const text = decoder.decode(bytes);
        const state = { saved: text, text, missing: false, illFormed: null, formed: true, byBuilder: false };
        if (bad >= 0) state.illFormed = { byte: bad, at: decoder.decode(bytes.subarray(0, bad)).length };
        else if (kind) {
            const said = read(text);
            state.formed = !said.ok || (kind === 'appearance' ? appearance.write(said.doc) : write(said.doc)).text === text;
        }
        return state;
    }

    const validScreen = (screen) => screen && [screen.width, screen.height].every((n) => Number.isInteger(n) && n >= 1 && n <= 4096) &&
                                    DEPTHS.includes(screen.depth) && Number.isInteger(screen.unit) && screen.unit >= 1 && screen.unit <= 16;

    const store = Pinia.defineStore('dgx-ui-builder', {
        state: () => ({
            ready: false,
            agentAllowed: true, // false: a page that mounts the builder has its own agent, none is started here
            boardsLine: true, // false: a page that mounts the builder is boards', the line that points to it not shown
            dialog: true, // false: the agent's dialog is the page's that mounts the builder, the builder's own not shown
            languageSelector: true, // false: the language is the page's that mounts the builder, given by its setLanguage()
            failed: null, // the module could not start: its words
            language: 'en',
            screens: SCREENS,
            screen: { ...SCREENS[0] },
            zoom: 2,
            project: null,
            files: {}, // path: fileState()
            current: null, // the path of the page open
            version: 0, // a change of the document, which is held raw
            readError: null, // the page's text not read: {at, byte, why}
            verdicts: { names: null, fonts: [], pictures: [], appearance: null, page: null },
            // a picture being imported: {files: [{name, bytes}], colors, transparent, name, bits, reduce, dither, ink, background,
            // made: its bytes as DGX made them, or null; said: why not made; given: the preview has it}
            importing: null,
            refusedPath: null, // the widget the page's verdict stops in
            selection: [],
            shown: null, // the preview's shown(): '' the page, a name a page opened over it, null none
            box: null, // the selection's box on the panel
            notes: [],
            events: [],
            view: 'page', // the canvas; 'text' the texts
            text: 'page', // the text shown in the view of texts: page, names or appearance
            cursor: null, // where the view of texts puts its cursor: {text, at} in UTF-16 units
            asking: null, // a save waiting for a word: {paths, all}
            reformatted: {}, // the files a save was said to write in the builder's form
            said: [], // what the last action says: [{text, values, wrong}]
            trying: false, // the finger works the page, not the selection
            wakeAt: 0,
            drawn: 0,
            tables: {}, // the names of the tables the page is read on, as the module gives them
            tab: 'properties', // the pane beside the panel: the properties of the widget selected, or the appearance's forms
            pick: [], // the entry of the appearance in its form: [section, name]
            namesPick: [], // the line of the names file in its form: [table, name]
            namesMissing: [], // the names the page uses that its names file has not, as the module gives them
            agent: false, // an agent beside the builder: its dialog shown
            agentMessages: [], // the dialog: {from: 'person' | 'agent', text}
            appearanceError: null, // the appearance's text not read: {at, byte, why}
            appearanceAt: null, // the entry the appearance's verdict stops in: [section, name]
            raw: Vue.markRaw({ preview: null, host: null, doc: null, spans: [], histories: new Map(), links: new Map(), linkCount: 0, format: null,
                               preferences: null, look: null, lookSpans: [], lookFormat: null, namesFormat: null, agentLink: null,
                               pending: {}, pixelsOf: null }),
        }),

        getters: {
            format: (state) => state.raw.format,
            page: (state) => state.project?.pages.find((page) => page.path === state.current) ?? null,
            holding: (state) => state.raw.format?.keys.find((entry) => entry.key === 'children')?.kinds ?? [],
            dirty: (state) => Object.keys(state.files).filter((path) => state.files[path].text !== (state.files[path].saved ?? '')),
            tree() {
                this.version;
                if (!this.raw.doc) return [];
                return widgets(this.raw.doc).map(({ path, widget }) => ({
                    path, key: path.join('.'), depth: path.length, kind: kindOf(widget),
                    id: stringOf(valueOf(widget, 'id')), text: stringOf(valueOf(widget, 'text')),
                }));
            },
            selected() {
                this.version;
                return this.raw.doc ? widgetAt(this.raw.doc, this.selection) ?? null : null;
            },
            // the properties of the selected widget: every key the format gives its kind, in the order a save writes them,
            // then the keys it holds that its kind takes not
            fields() {
                this.version;
                const widget = this.selected, format = this.raw.format;
                if (!widget || !format) return [];
                const kind = kindOf(widget);
                const present = new Map(widget.entries.map(([key, value]) => [key.value, value]));
                // a key a kind takes only with flags beside it (a box's offset and step: a scrolling box's) offered with them
                const flags = shown(present.get('flags'));
                const ofKind = format.keys.filter((entry) => entry.key !== 'children' && entry.kinds.includes(kind) &&
                                                  (entry.kind_flags?.[kind] ?? []).every((flag) => Array.isArray(flags) && flags.includes(flag)));
                ofKind.sort((a, b) => description.WIDGET_ORDER.indexOf(a.key) - description.WIDGET_ORDER.indexOf(b.key));
                const fields = ofKind.map((entry) => ({ key: entry.key, entry, kind, present: present.has(entry.key), value: shown(present.get(entry.key)),
                                                       raw: present.get(entry.key) }));
                const taken = new Set(ofKind.map((entry) => entry.key).concat('children'));
                for (const [key, value] of present) {
                    if (!taken.has(key)) fields.push({ key, entry: null, present: true, value: shown(value), raw: value });
                }
                return fields;
            },
            // the file an undo goes to: the text shown, the appearance while its forms are open, else the page
            undoPath() {
                if (this.view === 'text') return this.textPath;
                if (this.tab === 'appearance' && this.page?.appearance) return this.page.appearance;
                return this.tab === 'names' && this.page?.names ? this.page.names : this.current;
            },
            canUndo() {
                this.version;
                return !!this.raw.histories.get(this.undoPath)?.canUndo;
            },
            canRedo() {
                this.version;
                return !!this.raw.histories.get(this.undoPath)?.canRedo;
            },
            // the appearance of the page open as a document, null when it does not read; its format, what the module says
            // it may say; its prefix, the call's PREFIX or its "name", which dgx_ui_c names its C by
            look() {
                this.version;
                return this.raw.look;
            },
            lookFormat() {
                this.version;
                return this.raw.lookFormat;
            },
            lookPrefix() {
                this.version;
                const made = this.project?.appearances.find((one) => one.path === this.page?.appearance);
                return made?.prefix ?? stringOf(this.raw.look ? valueOf(this.raw.look, 'name') : null);
            },
            // the names file of the page open as lines that name something, its format, the line its verdict stops at
            namesFormat() {
                return this.raw.namesFormat;
            },
            namesRows() {
                this.version;
                const path = this.page?.names;
                return path && this.files[path] ? namesFile.rows(this.files[path].text, this.raw.namesFormat) : [];
            },
            namesAt() {
                return this.verdicts.names && this.verdicts.names.code !== 0 ? namesFile.lineOfVerdict(this.verdicts.names.said) : null;
            },
            // the pages read on the names file of the page open, and the appearances read on it: made with it as their
            // NAMES, or loaded at run time by a page on it
            pagesOnNames() {
                const path = this.page?.names;
                return path ? this.project.pages.filter((page) => page.names === path).map((page) => page.path) : [];
            },
            appearancesOnNames() {
                const path = this.page?.names;
                if (!path) return [];
                const made = this.project.appearances.filter((one) => one.from && one.names === path).map((one) => one.path);
                const loaded = this.project.pages.filter((page) => page.names === path && page.appearance &&
                                                               this.project.appearances.some((one) => one.path === page.appearance && !one.from))
                                                 .map((page) => page.appearance);
                return [...new Set([...made, ...loaded])];
            },
            // the names of the file no page and no appearance on it names: "table name", a note on their lines
            // the words of the appearance's refusal at a field of pictures short of its need: "pieces: 9 pictures needed,
            // the table's "card" has 1"; null for any other verdict
            appearanceShort() {
                const verdict = this.verdicts.appearance, at = this.appearanceAt, doc = this.raw.look;
                if (!verdict || verdict.code !== 6 || !at || at[0] !== 'looks' || !doc) return null;
                const look = appearance.nodeAt(doc, at), draw = stringOf(valueOf(look, 'draw'));
                const field = this.raw.lookFormat?.draws.find((one) => one.draw === draw)?.fields.find((one) => one.name === verdict.name);
                if (!field || field.type !== 'picture') return null;
                const named = stringOf(valueOf(valueOf(look, 'params'), field.name));
                const picture = this.raw.lookFormat.pictures.find((one) => one.name === named);
                return { field: field.name, need: field.min, name: named, count: picture?.count ?? 0 };
            },

            namesUnused() {
                this.version;
                return this.namesUsers(null).unused;
            },
            // the pages drawn on the appearance of the page open
            pagesOn() {
                const path = this.page?.appearance;
                return path ? this.project.pages.filter((page) => page.appearance === path).map((page) => page.path) : [];
            },
            // the text shown in the view of texts, and its file
            textPath() {
                return this.text === 'page' ? this.current : this.page?.[this.text] ?? null;
            },
        },

        actions: {
            // the module started: options.preferences, an object of get(key) and set(key, value), localStorage's by default
            async start(options = {}) {
                this.raw.preferences = options.preferences ?? {
                    get: (key) => window.localStorage?.getItem(key) ?? null,
                    set: (key, value) => window.localStorage?.setItem(key, value),
                };
                const preferences = this.raw.preferences;
                this.language = preferences.get(PREFERENCES.language) ?? options.language ?? 'en';
                const zoom = +preferences.get(PREFERENCES.zoom);
                if (ZOOMS.includes(zoom)) this.zoom = zoom;
                try {
                    const screen = JSON.parse(preferences.get(PREFERENCES.screen) ?? 'null');
                    if (validScreen(screen)) this.screen = { ...this.screen, ...screen };
                } catch {
                    /* a preference spoiled: the first screen */
                }
                try {
                    const preview = await window.DgxUiPreview.create({ pageText: (name) => this.pageTextOf(name), log: options.log ?? (() => {}) });
                    this.raw.preview = Vue.markRaw(preview);
                    this.raw.format = Vue.markRaw(this.raw.preview.format());
                    this.raw.namesFormat = Vue.markRaw(this.raw.preview.namesFormat());
                    this.ready = true;
                } catch (error) {
                    this.failed = String(error && error.message ? error.message : error);
                }
            },

            say(text, values = [], wrong = false) {
                this.said.push({ text, values, wrong });
                if (this.said.length > 6) this.said.shift();
            },

            async open(host) {
                if (!this.ready) return;
                this.raw.host = Vue.markRaw(host);
                const project = await readProject(host);
                const files = {};
                const pages = new Set(project.pages.map((page) => page.path));
                const appearances = new Set([...project.appearances.map((one) => one.path), ...project.pages.map((page) => page.appearance)]);
                const paths = new Set([...pages, ...project.namesFiles.map((one) => one.path),
                                       ...project.appearances.flatMap((one) => [one.path, one.names]).filter((path) => path),
                                       ...project.pages.flatMap((page) => [page.names, page.appearance]).filter((path) => path)]);
                for (const path of paths) {
                    files[path] = fileState(await host.read(path), pages.has(path) ? 'page' : appearances.has(path) ? 'appearance' : null);
                    if (files[path].missing) project.notes.push({ text: '{0}: no such file', values: [path] });
                }
                this.files = files, this.project = project, this.current = null, this.reformatted = {}, this.said = [], this.asking = null;
                this.raw.histories = new Map(), this.raw.links = new Map(), this.raw.doc = null, this.pick = [], this.version++;
                this.raw.pending = {}, this.importing = null;
                await this.giveFonts();
                await this.givePictures();
                if (project.pages.length) this.choose(project.pages[0].path);
                else this.say('no description of a page found: no dgx_ui_tree() in the CMakeLists, no file holding "dgx-ui" and "tree"');
            },

            async giveFonts() {
                const preview = this.raw.preview;
                for (const one of this.verdicts.fonts) if (one.verdict.code === 0) preview.font(one.name, null);
                this.verdicts.fonts = [];
                const dir = this.project?.fontDir;
                if (!dir) return;
                const host = this.raw.host;
                const index = textOf(await host.read(`${dir}/${FONTS_INDEX}`)) ?? '';
                this.verdicts.fonts = await preview.fonts(index, (file) => host.read(`${dir}/${file}`));
            },

            // the pictures DGX made by the project's needs, by their index, as the fonts: the panel reads the board's bytes
            async givePictures() {
                const preview = this.raw.preview;
                for (const one of this.verdicts.pictures) if (one.verdict.code === 0) preview.picture(one.name, null);
                this.verdicts.pictures = [];
                const dir = this.project?.imageDir;
                if (!dir) return;
                const host = this.raw.host;
                const index = textOf(await host.read(`${dir}/${IMAGES_INDEX}`)) ?? '';
                this.verdicts.pictures = await preview.pictures(index, (file) => host.read(`${dir}/${file}`));
            },

            // ---- a picture imported: PNG files made a file of DGX by the project's own DGX, through the server ----

            // the import offered: the server makes the file with DGX's writer of the project; a page opened from the disk
            // has no writer
            canImport() {
                return typeof this.raw.host?.image === 'function';
            },

            // PNG files chosen or dropped: [{name, bytes}]; their colours counted, the depth they take, the file made
            async importStart(files) {
                if (!this.canImport()) {
                    this.say('DGX\'s writer of the project makes a picture: open the project through dgx_ui_serve.py or dgx_ui_mcp.py.', [], true);
                    return false;
                }
                if (!files.length) return false;
                this.importCancel();
                const pixelsOf = this.raw.pixelsOf ?? pixelsOfPng;
                const colors = new Set();
                let transparent = false, partly = false;
                for (const file of files) {
                    let pixels;
                    try {
                        pixels = await pixelsOf(file.bytes);
                    } catch (error) {
                        this.say('{0}: not a picture the browser reads: {1}', [file.name, error.message ?? String(error)], true);
                        return false;
                    }
                    for (let at = 0; at < pixels.rgba.length; at += 4) {
                        const alpha = pixels.rgba[at + 3];
                        partly ||= alpha > 0 && alpha < 255;
                        if (alpha < 128) transparent = true;
                        else if (colors.size <= 257) colors.add(pixels.rgba[at] << 16 | pixels.rgba[at + 1] << 8 | pixels.rgba[at + 2]);
                    }
                }
                const taken = new Set(this.namesRows.filter((row) => row.table === 'picture').map((row) => row.words[1]));
                let name = pictureName(files[0].name);
                for (let k = 2; taken.has(name); ++k) name = `${pictureName(files[0].name)}_${k}`;
                const colorsOf = this.raw.look ? valueOf(this.raw.look, 'colors') : null;
                const paper = colorsOf ? stringOf(valueOf(colorsOf, 'paper')) : null;
                const kind = pictureKind(colors.size, transparent, partly);
                this.importing = { files, colors: colors.size, transparent, partly, soft: partly, mask: kind.mask, name, bits: kind.bits, reduce: false,
                                   dither: false, ink: null, background: typeof paper === 'string' && /^#[0-9a-fA-F]{6}$/.test(paper) ? paper.slice(1) : '000000',
                                   made: null, said: null, given: false };
                await this.importMake();
                return true;
            },

            // the file made again by DGX's writer with the options as they are; the panel given its bytes
            async importMake() {
                const one = this.importing;
                if (!one) return;
                const background = one.bits === 16 && one.transparent && !one.soft ? one.background : null;
                try {
                    const made = await this.raw.host.image(one.files.map((file) => file.bytes),
                                                           { bits: one.bits, reduce: one.reduce && !one.soft, dither: one.dither, ink: one.bits === 1 ? one.ink : null,
                                                             background, mask: one.soft && one.mask });
                    if (this.importing !== one) return;
                    one.made = Vue.markRaw(made), one.said = null;
                    const verdict = this.raw.preview.picture(one.name, made);
                    one.given = verdict.code === 0;
                    if (!one.given) one.said = verdict.said;
                } catch (error) {
                    if (this.importing !== one) return;
                    one.made = null, one.said = error.message ?? String(error);
                }
                if (this.current) this.giveAll();
            },

            async importSet(key, value) {
                const one = this.importing;
                if (!one || !['name', 'bits', 'reduce', 'dither', 'ink', 'background', 'soft'].includes(key)) return;
                if (key === 'soft' && !one.partly) return;
                if (key === 'name' && one.given) this.raw.preview.picture(one.name, null), one.given = false;
                one[key] = value;
                if (key === 'soft') Object.assign(one, pictureKind(one.colors, one.transparent, value)); /* the kind and depth its edges take */
                if (key === 'bits' && one.soft && (one.mask ? value !== 4 && value !== 8 : value !== 16)) one.bits = one.mask ? 4 : 16;
                await this.importMake();
            },

            // the widgets of the page that show the picture being imported, and those of them a look other than the
            // soft pictures' draws: a mask and 16 bits with soft edges are drawn by soft_picture alone
            importUsers() {
                const one = this.importing, doc = this.raw.doc;
                if (!one || !doc) return { all: [], other: [] };
                const all = widgets(doc).filter(({ widget }) => stringOf(valueOf(widget, 'picture')) === one.name);
                const other = all.filter(({ widget }) => (stringOf(valueOf(widget, 'look')) ?? kindOf(widget)) !== 'soft_picture');
                return { all: all.map((user) => user.path), other: other.map((user) => user.path) };
            },

            // those widgets given the look soft_picture, and the names file its line when it has none
            importDrawSoft() {
                const paths = this.importUsers().other;
                if (!paths.length) return false;
                if (!this.namesRows.some((row) => row.table === 'look' && row.words[1] === 'soft_picture') && !this.addNamesLine(['look', 'soft_picture'])) return false;
                return this.edit((doc) => {
                    for (const path of paths) description.setKey(doc, path, 'look', 'soft_picture');
                    return [...this.selection];
                });
            },

            // the weight the picture takes: its file in flash, its tables in RAM
            importWeight() {
                const one = this.importing;
                if (!one?.made) return null;
                const count = new DataView(one.made.buffer, one.made.byteOffset).getUint16(8, true);
                return { flash: one.made.length, ram: count * (PICTURE_RAM + (one.made[6] <= 8 || one.soft ? IMAGE_RAM : 0)), count };
            },

            // the import taken: the PNG files beside the file of needs, its line there, the names file's line; written at a
            // save. Refused in words: no file of needs, no names file, a name of no C or taken
            async importAccept() {
                const one = this.importing;
                if (!one?.made) return false;
                const needs = this.project.needsFiles[0];
                if (!needs) {
                    this.say('no file of pictures\' needs: add dgx_image_needs_file(images.txt) to main/CMakeLists.txt, then open the project again', [], true);
                    return false;
                }
                if (!this.page?.names) {
                    this.say('the page has no names file: its line picture {0} @{0} has nowhere to go', [one.name], true);
                    return false;
                }
                if (!/^[A-Za-z_][A-Za-z0-9_]*$/.test(one.name) || one.name.startsWith('dgx_')) {
                    this.say('"{0}": a picture\'s asset is a name of C, not of dgx_', [one.name], true);
                    return false;
                }
                if (!this.files[needs]) this.files[needs] = fileState(await this.raw.host.read(needs), null);
                const file = this.files[needs], folder = needs.split('/').slice(0, -1).join('/');
                const lines = (file.missing ? '' : file.text).split('\n');
                if (lines.some((line) => line.trim().split(/\s+/)[0] === one.name)) {
                    this.say('"{0}": the file of needs has a need of that name already', [one.name], true);
                    return false;
                }
                const quote = (path) => (/\s/.test(path) ? `"${path}"` : path);
                const pngs = one.files.map((_, k) => `images/${one.name}${one.files.length > 1 ? `_${k}` : ''}.png`);
                const words = [one.name, 'PNG', ...pngs.map(quote), 'BITS', String(one.bits)];
                if (one.soft && one.mask) words.push('MASK');
                if (one.reduce && !one.soft) words.push('REDUCE');
                if (one.dither) words.push('DITHER');
                if (one.bits === 1 && one.ink) words.push('INK', one.ink.toUpperCase());
                if (one.bits === 16 && one.transparent && !one.soft) words.push('BACKGROUND', one.background);
                if (!this.addNamesLine(['picture', one.name, '@' + one.name])) return false;
                const before = file.missing ? '' : file.text;
                file.text = before + (before && !before.endsWith('\n') ? '\n' : '') + words.join(' ') + '\n', file.missing = false;
                one.files.forEach((png, k) => (this.raw.pending[joinPath(folder, pngs[k])] = png.bytes));
                this.say('picture {0}: its PNG, its need and its name written at a save', [one.name]);
                this.importing = null; // the preview keeps its bytes until the next build gives the board's
                return true;
            },

            importCancel() {
                if (this.importing?.given) this.raw.preview.picture(this.importing.name, null);
                this.importing = null;
                if (this.current) this.giveAll();
            },

            async setFontDir(dir) {
                if (!this.project.fontDirs.some((one) => one.dir === dir)) return;
                this.project.fontDir = dir;
                await this.giveFonts();
                if (this.current) this.giveAll();
            },

            // the screen a page is drawn on: the one its description was made for, the board's of the project's preset,
            // the last chosen
            screenFor() {
                const own = builderOf(this.raw.doc ?? { t: 'object', entries: [] }).screen;
                if (validScreen(own)) return { ...own };
                const board = this.project?.boardScreen;
                if (board) return { width: board.width, height: board.height, depth: board.depth, unit: this.screen.unit };
                return { ...this.screen };
            },

            choose(path) {
                const page = this.project?.pages.find((one) => one.path === path);
                if (!page) return false;
                if (page.appearance !== this.page?.appearance) this.pick = [];
                if (page.names !== this.page?.names) this.namesPick = [];
                this.current = path, this.view = 'page', this.text = 'page', this.cursor = null, this.trying = false, this.events = [];
                this.loadText();
                this.selection = [];
                const screen = this.screenFor();
                this.screen = { width: screen.width, height: screen.height, depth: screen.depth, unit: screen.unit };
                this.giveScreen();
                return true;
            },

            // a page read at run time: the names file and appearance it is tried on
            setPageFiles(names, appearance) {
                const page = this.page;
                if (!page || page.from) return;
                page.names = names || null, page.appearance = appearance || null;
                for (const path of [page.names, page.appearance]) if (path && !this.files[path]) this.files[path] = fileState(null, false);
                this.giveAll();
            },

            historyOf(path = this.current) {
                if (!this.raw.histories.has(path)) this.raw.histories.set(path, new History());
                return this.raw.histories.get(path);
            },

            // the page's text read into the document; its spans when it is the builder's writing
            loadText() {
                const file = this.files[this.current];
                this.raw.spans = [];
                if (file.illFormed) {
                    this.raw.doc = null, this.readError = { at: file.illFormed.at, byte: file.illFormed.byte, why: 'not UTF-8' };
                } else if (file.missing && file.text === '') {
                    this.raw.doc = null, this.readError = { at: 0, byte: 0, why: 'no such file' };
                } else {
                    const said = read(file.text);
                    if (said.ok) {
                        this.raw.doc = Vue.markRaw(said.doc), this.readError = null;
                        const written = write(said.doc);
                        if (written.text === file.text) this.raw.spans = written.spans;
                    } else {
                        this.raw.doc = null, this.readError = { at: said.at, byte: said.byte, why: said.why };
                    }
                }
                this.version++;
            },

            giveScreen() {
                const { width, height, depth, unit } = this.screen;
                if (this.raw.preview.screen(width, height, depth, unit) !== 0)
                    this.say('the screen {0}x{1}, depth {2}, {3} a point, not taken', [width, height, depth, unit], true);
                this.giveAll();
            },

            // the names, the appearance and the page given again, in that order; the fonts stay. An appearance made by
            // dgx_ui_appearance() is read on its own names file, or by the rule with none, as dgx_ui_c -a reads it
            giveAll() {
                const page = this.page;
                if (!page) return;
                const said = this.giveFiles(page);
                this.verdicts.names = said.names, this.verdicts.appearance = said.appearance;
                this.loadAppearance();
                this.givePage();
            },

            // the names file and the appearance of a page given to the module; their verdicts. A picture being imported
            // is given with the line it takes, picture <name> @<name>, when the file has no picture of the name: the panel
            // draws it before it is taken; the file itself is not changed
            giveFiles(page) {
                const preview = this.raw.preview;
                const textAt = (path) => (path && this.files[path] && !this.files[path].missing ? this.files[path].text : null);
                const one = this.importing, namesText = textAt(page.names) ?? (page.names ? '' : null);
                const pending = one?.given && namesText !== null && /^[A-Za-z_][A-Za-z0-9_]*$/.test(one.name) &&
                                !namesText.split('\n').some((line) => { const words = line.trim().split(/\s+/); return words[0] === 'picture' && words[1] === one.name; });
                const names = page.names ? preview.names(pending ? `${namesText.replace(/\n?$/, '\n')}picture ${one.name} @${one.name}\n` : textAt(page.names)) : null;
                if (!page.names) preview.names(null);
                const made = this.project.appearances.find((one) => one.path === page.appearance);
                const itsNames = made && made.from ? (made.names ? textAt(made.names) : null) : undefined;
                const verdict = page.appearance ? preview.appearance(textAt(page.appearance), itsNames) : (preview.appearance(null), null);
                return { names, appearance: verdict };
            },

            // the appearance of the page open read into its document, what the module says it may say, and the entry its
            // verdict stops in when its text is the builder's writing
            loadAppearance() {
                const path = this.page?.appearance, file = path ? this.files[path] : null;
                this.raw.look = null, this.raw.lookSpans = [], this.appearanceError = null, this.appearanceAt = null;
                this.raw.lookFormat = Vue.markRaw(this.raw.preview.appearanceFormat());
                if (file && file.illFormed) {
                    this.appearanceError = { at: file.illFormed.at, byte: file.illFormed.byte, why: 'not UTF-8' };
                } else if (file && !file.missing) {
                    const said = read(file.text);
                    if (said.ok) {
                        this.raw.look = Vue.markRaw(said.doc);
                        const written = appearance.write(said.doc);
                        if (written.text === file.text) this.raw.lookSpans = written.spans;
                        const verdict = this.verdicts.appearance;
                        if (verdict && verdict.code > 0) this.appearanceAt = appearance.entryAtByte(this.raw.lookSpans, verdict.offset);
                    } else {
                        this.appearanceError = { at: said.at, byte: said.byte, why: said.why };
                    }
                }
                this.version++;
            },

            givePage() {
                const preview = this.raw.preview, file = this.files[this.current];
                if (file.illFormed) {
                    preview.page(null);
                    this.verdicts.page = { code: SYNTAX, offset: file.illFormed.byte, name: '', said: 'not UTF-8' };
                } else {
                    this.verdicts.page = preview.page(file.text);
                }
                this.tables = preview.tables();
                const names = this.page?.names ? this.files[this.page.names] : null;
                this.namesMissing = names ? preview.namesMissing(names.missing ? '' : names.text) : [];
                this.notes = preview.notes();
                this.shown = preview.shown();
                this.refusedPath = this.verdicts.page.code > 0 && !file.illFormed ? this.locate(this.verdicts.page) : null;
                this.wakeAt = 0, this.box = null;
            },

            // the widget a refusal stops in: by the spans of the text as the builder writes it, the text given again
            // in that form when it is in another, as the loader judges both alike
            locate(verdict) {
                const doc = this.raw.doc;
                if (!doc) return null;
                if (this.raw.spans.length) return description.pathAtByte(this.raw.spans, verdict.offset);
                const written = write(doc), preview = this.raw.preview;
                const again = preview.page(written.text);
                preview.page(this.files[this.current].text);
                return again.code === verdict.code ? description.pathAtByte(written.spans, again.offset) : null;
            },

            // an edit of the document: the operation gives the path to select after it, null when it changed nothing
            edit(operation, fold = null) {
                const doc = this.raw.doc;
                if (!doc || !this.current) return false;
                const before = { text: this.files[this.current].text, selection: [...this.selection] };
                let selection;
                try {
                    selection = operation(doc);
                } catch (error) {
                    this.say('{0}', [error.message], true);
                    this.loadText();
                    return false;
                }
                if (!selection) return false;
                this.historyOf().record(before, fold);
                this.selection = selection;
                this.rewrite();
                return true;
            },

            rewrite() {
                const file = this.files[this.current];
                const written = write(this.raw.doc);
                file.text = written.text, file.byBuilder = true, file.illFormed = null;
                this.raw.spans = written.spans, this.readError = null;
                this.version++;
                this.givePage();
            },

            select(path) {
                if (!this.raw.doc || !widgetAt(this.raw.doc, path)) return false;
                this.selection = [...path];
                this.place();
                return true;
            },

            // the widget before or after the selection in the tree's order
            selectNext(by) {
                const tree = this.tree, at = tree.findIndex((one) => samePath(one.path, this.selection));
                const next = tree[Math.max(0, Math.min(tree.length - 1, at + by))];
                return next ? this.select(next.path) : false;
            },

            // the deepest widget drawn under a pixel of the panel, by the boxes of the layout: every widget, not only
            // those that answer to a finger; none while a page an action opened is on the screen
            selectAt(x, y) {
                const preview = this.raw.preview, doc = this.raw.doc;
                if (!doc || this.shown !== '' || this.verdicts.page?.code !== 0) return false;
                const count = preview.count();
                if (count !== widgets(doc).length) return false;
                const boxes = [];
                const inside = (box) => x >= box.x && y >= box.y && x < box.x + box.w && y < box.y + box.h;
                let found = -1;
                for (let i = 0; i < count; ++i) {
                    const one = preview.widget(i);
                    boxes.push(one);
                    let holds = one.shown && inside(one);
                    for (let parent = one.parent; holds && parent >= 0 && parent < i; parent = boxes[parent].parent) holds = inside(boxes[parent]);
                    if (holds) found = i;
                }
                return found >= 0 ? this.select(pathOf(doc, found)) : false;
            },

            // the selection's box on the panel, as the last layout laid it
            place() {
                const preview = this.raw.preview, doc = this.raw.doc;
                this.box = null;
                if (!doc || this.shown !== '' || this.verdicts.page?.code !== 0) return;
                const index = indexOf(doc, this.selection);
                if (index < 0 || preview.count() !== widgets(doc).length) return;
                const one = preview.widget(index);
                if (one) this.box = { x: one.x, y: one.y, w: one.w, h: one.h, shown: one.shown };
            },

            // the time given: the page drawn where it changed, its events read; true when pixels were written
            paint(now) {
                const preview = this.raw.preview;
                if (!preview || !this.current || now < this.wakeAt) return false;
                const drew = preview.paint(now);
                this.wakeAt = now + Math.min(preview.wake(now), WAKE_MOST);
                const events = preview.events();
                if (events.length) {
                    this.events.push(...events);
                    if (this.events.length > EVENTS_KEPT) this.events.splice(0, this.events.length - EVENTS_KEPT);
                }
                const shownNow = preview.shown();
                if (shownNow !== this.shown) this.shown = shownNow, this.notes = preview.notes();
                this.place();
                if (drew) ++this.drawn;
                return drew;
            },

            image() {
                return this.raw.preview?.image() ?? null;
            },

            // ---- the properties ----

            fold(key) {
                return `${this.current}|${this.selection.join('.')}|${key}`;
            },

            setKey(key, value, fold = null) {
                const path = this.selection;
                return this.edit((doc) => description.setKey(doc, path, key, value), fold);
            },

            removeKey(key) {
                const path = this.selection, widget = this.selected;
                if (!widget || !widget.entries.some(([name]) => name.value === key)) return false;
                return this.edit((doc) => description.removeKey(doc, path, key));
            },

            // a number field: empty, the key removed; a number as JSON writes one, set as typed, or a percent, "50%", a
            // string; anything else waits, nothing set, so that no NaN reaches the document
            setNumber(key, typed, fold = null) {
                const text = String(typed).trim();
                if (text === '') return this.removeKey(key);
                const node = typedNode(text);
                return node === null ? false : this.setKey(key, node, fold);
            },

            // a pair of numbers or percents: both empty, the key removed; one empty or of neither, it waits
            setPair(key, first, second, fold = null) {
                const one = String(first).trim(), two = String(second).trim();
                if (one === '' && two === '') return this.removeKey(key);
                const nodes = [typedNode(one), typedNode(two)];
                return nodes.includes(null) ? false : this.setKey(key, nodes, fold);
            },

            // a number made a percent, or a percent its number: of a field, or of one of a pair's two by index
            togglePercent(key, index = null) {
                const field = this.fields.find((one) => one.key === key);
                if (!field || !field.present) return false;
                const flip = (node) => {
                    if (node?.t === 'number') return `${node.raw}%`;
                    if (node?.t === 'string' && PERCENT.test(node.value)) return typedNode(node.value.slice(0, -1));
                    return null;
                };
                if (index === null) {
                    const flipped = flip(field.raw);
                    return flipped === null ? false : this.setKey(key, flipped);
                }
                if (field.raw?.t !== 'array' || field.raw.items.length !== 2) return false;
                const items = [...field.raw.items], flipped = flip(items[index]);
                if (flipped === null) return false;
                items[index] = flipped;
                return this.setKey(key, items);
            },

            toggleWord(key, word) {
                const now = this.fields.find((field) => field.key === key)?.value;
                const words = Array.isArray(now) ? now.filter((one) => typeof one === 'string') : [];
                const next = words.includes(word) ? words.filter((one) => one !== word) : [...words, word];
                return next.length ? this.setKey(key, next) : this.removeKey(key);
            },

            // ---- the operations of the tree ----

            // a widget of a kind: a child of the selection when it holds children, else after it in its parent; a kind
            // that takes a text is given its word; one the loader finds no look for is given "look": null in the same
            // step, by the loader's own verdict
            addChild(kind) {
                const doc = this.raw.doc, format = this.raw.format;
                if (!doc || !format.kinds.includes(kind)) return false;
                let parent = this.selection, at;
                if (!this.holding.includes(kindOf(this.selected))) {
                    if (!this.selection.length) return false;
                    parent = this.selection.slice(0, -1), at = this.selection[this.selection.length - 1] + 1;
                }
                const extra = format.keys.find((entry) => entry.key === 'text')?.kinds.includes(kind) ? { text: kind } : {};
                if (!this.edit((doc) => description.addChild(doc, parent, kind, extra, at))) return false;
                if (this.verdicts.page.code === NO_LOOK && samePath(this.refusedPath, this.selection)) {
                    description.setKey(doc, this.selection, 'look', null);
                    this.rewrite();
                }
                return true;
            },

            remove() {
                const path = this.selection;
                return path.length ? this.edit((doc) => description.remove(doc, path)) : false;
            },

            move(by) {
                const path = this.selection;
                return path.length ? this.edit((doc) => description.move(doc, path, by)) : false;
            },

            wrap() {
                const path = this.selection;
                return this.edit((doc) => description.wrap(doc, path));
            },

            undo() {
                const history = this.raw.histories.get(this.current);
                const back = history?.undo({ text: this.files[this.current].text, selection: [...this.selection] });
                return back ? (this.restore(back), true) : false;
            },

            redo() {
                const history = this.raw.histories.get(this.current);
                const ahead = history?.redo({ text: this.files[this.current].text, selection: [...this.selection] });
                return ahead ? (this.restore(ahead), true) : false;
            },

            // the state of a file now, as its history keeps one
            snapshot(path) {
                const text = this.files[path].text;
                if (path === this.current) return { text, selection: [...this.selection] };
                if (path === this.page?.appearance) return { text, selection: [...this.pick] };
                if (path === this.page?.names) return { text, selection: [...this.namesPick] };
                return { text, selection: [] };
            },

            // an undo or a redo of the file the undo goes to; a step of several files in all of them, or in none while
            // one of them was changed after it
            step(way) {
                const path = this.undoPath, history = path ? this.raw.histories.get(path) : null;
                if (!history) return false;
                const link = way === 'undo' ? history.undoLink : history.redoLink;
                if (link === null) {
                    const state = history[way](this.snapshot(path));
                    return state ? (this.restore(path, state), true) : false;
                }
                const paths = this.raw.links.get(link) ?? [path];
                const stale = paths.find((one) => {
                    const its = this.raw.histories.get(one);
                    return (way === 'undo' ? its?.undoLink : its?.redoLink) !== link;
                });
                if (stale !== undefined) {
                    if (way === 'undo') this.say('the step is undone in all its files together, and {0} was edited after it: undo there first', [stale], true);
                    else this.say('the step is done again in all its files together, and {0} was edited after its undo: undo there first', [stale], true);
                    return false;
                }
                for (const one of paths) this.restore(one, this.raw.histories.get(one)[way]({ ...this.snapshot(one), link }), false);
                this.giveAll();
                return true;
            },

            undo() {
                return this.step('undo');
            },

            redo() {
                return this.step('redo');
            },

            restore(path, state, give = true) {
                const file = this.files[path];
                file.text = state.text, file.illFormed = null;
                const said = read(state.text);
                if (path === this.current) {
                    this.loadText();
                    file.byBuilder = this.raw.spans.length > 0;
                    this.selection = this.raw.doc && widgetAt(this.raw.doc, state.selection) ? [...state.selection] : [];
                } else if (this.project.appearances.some((one) => one.path === path) || this.project.pages.some((page) => page.appearance === path)) {
                    file.byBuilder = said.ok && appearance.write(said.doc).text === state.text;
                    if (path === this.page?.appearance) this.pick = [...state.selection];
                } else if (this.project.pages.some((page) => page.path === path)) {
                    file.byBuilder = said.ok && write(said.doc).text === state.text;
                } else if (path === this.page?.names) {
                    this.namesPick = [...state.selection];
                }
                if (give) this.giveAll();
            },

            // ---- the texts, by hand ----

            showText(which) {
                const path = which === 'page' ? this.current : this.page?.[which];
                if (!path) return;
                this.view = 'text', this.text = which;
                this.putCursor();
            },

            // the cursor at the byte a verdict or the reader stops at
            putCursor() {
                const which = this.text, path = this.textPath;
                if (!path) return;
                const text = this.files[path].text;
                if (which === 'page' && this.readError) this.cursor = { text: which, at: this.readError.at };
                else {
                    const verdict = this.verdicts[which];
                    this.cursor = verdict && verdict.code > 0 ? { text: which, at: unitOfByte(text, verdict.offset) } : null;
                }
            },

            setText(which, text) {
                const path = which === 'page' ? this.current : this.page?.[which];
                if (!path) return;
                const file = this.files[path];
                this.historyOf(path).record(this.snapshot(path), `${path}|text`);
                file.text = text, file.illFormed = null, file.missing = false, file.byBuilder = false;
                if (which === 'page') {
                    this.loadText();
                    if (!this.raw.doc || !widgetAt(this.raw.doc, this.selection)) this.selection = [];
                    this.givePage();
                } else {
                    this.giveAll();
                }
            },

            // ---- the appearance by its forms ----

            setTab(tab) {
                this.tab = tab === 'appearance' || tab === 'names' ? tab : 'properties';
            },

            choosePick(pick) {
                this.pick = Array.isArray(pick) ? [...pick] : [];
            },

            // an edit of the appearance's document: the operation gives the entry to show after it, null when it changed
            // nothing; written back in the builder's form and given to the module, the page drawn on it again
            editAppearance(operation, fold = null) {
                const path = this.page?.appearance, doc = this.raw.look;
                if (!path || !doc) return false;
                const file = this.files[path], before = this.snapshot(path);
                let pick;
                try {
                    pick = operation(doc);
                } catch (error) {
                    this.say('{0}', [error.message], true);
                    pick = null;
                }
                if (!pick) {
                    this.loadAppearance();
                    return false;
                }
                this.historyOf(path).record(before, fold);
                this.pick = pick;
                file.text = appearance.write(doc).text, file.byBuilder = true, file.illFormed = null, file.missing = false;
                this.giveAll();
                return true;
            },

            lookFold(path) {
                return `${this.page?.appearance}|${path.join('.')}`;
            },

            // a value at a path of the appearance, such as ['looks', 'slider', 'params', 'track']; undefined removes it
            setLook(path, value, fold = null) {
                if (value === undefined) return this.editAppearance((doc) => (appearance.removeAt(doc, path) ? path.slice(0, 2) : null));
                return this.editAppearance((doc) => (appearance.setAt(doc, path, value) ? path.slice(0, 2) : null), fold);
            },

            // a number of a field: empty removes it; anything not a number waits
            setLookNumber(path, typed, fold = null) {
                const text = String(typed).trim();
                if (text === '') return this.setLook(path, undefined);
                return JSON_NUMBER.test(text) ? this.setLook(path, { t: 'number', raw: text }, fold) : false;
            },

            insertLookItem(path, index, value) {
                return this.editAppearance((doc) => (appearance.insertAt(doc, path, index, value) ? path.slice(0, 2) : null));
            },

            moveLookItem(path, index, by) {
                return this.editAppearance((doc) => (appearance.moveAt(doc, path, index, by) ? path.slice(0, 2) : null));
            },

            removeLookItem(path, index) {
                return this.editAppearance((doc) => (appearance.removeAt(doc, [...path, index]) ? path.slice(0, 2) : null));
            },

            // a colour: "#rrggbb", "rrggbb" or a name of DGX's colours, written as "#rrggbb"; anything else waits
            setColor(name, typed, fold = null) {
                const hex = appearance.colorOf(typed, window.DgxUiPreview.colorNames());
                return hex ? this.setLook(['colors', name], hex, fold) : false;
            },

            // a name the program's C may hold of a colour renamed or deleted
            sayColorOutside(name) {
                const prefix = this.lookPrefix;
                if (prefix && appearance.C_NAME.test(prefix) && appearance.C_NAME.test(name))
                    this.say('the program may name the colour {1} as {0}_color_{1}, and dgx_ui_appearance_color() finds it by "{1}"', [prefix, name]);
                else this.say('the program may find the colour {0} by dgx_ui_appearance_color(): it finds none of that name now', [name]);
            },

            addEntry(section, name, value) {
                const doc = this.raw.look;
                if (!doc || !name) return false;
                if (appearance.namesOf(doc, section).includes(name)) {
                    this.say('"{0}": the appearance has one of that name already', [name], true);
                    return false;
                }
                if (!this.editAppearance((doc) => (appearance.setAt(doc, [section, name], value) ? [section, name] : null))) return false;
                if (section === 'looks' && this.raw.format.kinds.includes(name))
                    this.say('"{0}" is the word of a kind: every widget of the kind that names no look takes it', [name]);
                return true;
            },

            // a new look of a draw: its parameters the library's look's of the draw's word, as the module gives them
            addLook(name, draw) {
                const described = this.raw.lookFormat?.draws.find((one) => one.draw === draw);
                if (!described) return false;
                const look = { draw };
                if (described.fields.length) look.params = this.defaultsOf(described);
                return this.addEntry('looks', name, look);
            },

            // a draw's library defaults as a look's parameters: a field of pictures, which the library gives none of, the
            // first of the names' pictures of as many as it needs, else left out for the loader to say so at it
            defaultsOf(described) {
                const params = { ...(described.defaults ?? {}) };
                for (const field of described.fields.filter((one) => one.type === 'picture')) {
                    const fits = (this.raw.lookFormat?.pictures ?? []).find((picture) => picture.count >= field.min);
                    if (fits) params[field.name] = fits.name;
                    else delete params[field.name];
                }
                return params;
            },

            // a look given another draw: each field of the same name and kind kept, the others the new draw's defaults; a
            // draw the module does not describe keeps the parameters as they are
            setDraw(name, draw) {
                const format = this.raw.lookFormat;
                const after = format?.draws.find((one) => one.draw === draw);
                if (!after) return false;
                return this.editAppearance((doc) => {
                    const look = appearance.nodeAt(doc, ['looks', name]);
                    if (!look || look.t !== 'object') return null;
                    const beforeDraw = stringOf(valueOf(look, 'draw'));
                    const before = format.draws.find((one) => one.draw === beforeDraw);
                    appearance.setAt(doc, ['looks', name, 'draw'], draw);
                    if (after.own) return ['looks', name];
                    const params = valueOf(look, 'params'), kept = { t: 'object', entries: [] };
                    for (const field of after.fields) {
                        const old = before && !before.own && before.fields.find((one) => one.name === field.name && one.type === field.type);
                        const value = old && params && params.t === 'object' ? valueOf(params, field.name) : undefined;
                        if (value !== undefined) kept.entries.push([{ t: 'string', value: field.name }, value]);
                        else if (after.defaults && field.name in this.defaultsOf(after))
                            kept.entries.push([{ t: 'string', value: field.name }, description.plain(this.defaultsOf(after)[field.name])]);
                    }
                    if (kept.entries.length) appearance.setAt(doc, ['looks', name, 'params'], kept);
                    else appearance.removeAt(doc, ['looks', name, 'params']);
                    return ['looks', name];
                });
            },

            // what a rename of a look or a style would change: the widgets of the pages on the appearance that name it,
            // the pages that do not read, the words to say first
            renamePlan(section, from, to) {
                const key = section === 'looks' ? 'look' : 'style', doc = this.raw.look;
                const plan = { pages: [], refused: [], warnings: [], taken: !!doc && appearance.namesOf(doc, section).includes(to) };
                if (section === 'colors') return plan;
                for (const path of this.pagesOn) {
                    const file = this.files[path], said = file && !file.missing && !file.illFormed ? read(file.text) : { ok: false };
                    if (!said.ok) {
                        plan.refused.push(path);
                        continue;
                    }
                    const named = widgets(said.doc).filter(({ widget }) => stringOf(valueOf(widget, key)) === from).map((one) => one.path);
                    if (named.length) plan.pages.push({ path, widgets: named });
                }
                const kinds = this.raw.format.kinds;
                if (section === 'looks' && kinds.includes(from))
                    plan.warnings.push({ text: '"{0}" is the word of a kind: the widgets of the kind that name no look take the library\'s look after it', values: [from] });
                if (section === 'looks' && kinds.includes(to))
                    plan.warnings.push({ text: '"{0}" is the word of a kind: every widget of the kind that names no look takes it', values: [to] });
                if (section === 'styles' && doc && appearance.namesOf(doc, 'styles')[0] === from)
                    plan.warnings.push({ text: '"{0}" is the first style: the texts that name none take it, under its new name too', values: [from] });
                return plan;
            },

            // a colour renamed in the appearance, every look that names it with it; a look or a style renamed in the
            // appearance and in every widget of the pages on it that names it, one step of all those files
            renameEntry(section, from, to) {
                const path = this.page?.appearance, doc = this.raw.look;
                if (!path || !doc || !to || from === to) return false;
                const plan = this.renamePlan(section, from, to);
                if (plan.taken) {
                    this.say('"{0}": the appearance has one of that name already', [to], true);
                    return false;
                }
                if (section === 'colors') {
                    let renamed = null;
                    const done = this.editAppearance((doc) => {
                        renamed = appearance.renameColor(doc, from, to, this.raw.lookFormat);
                        return renamed ? ['colors', to] : null;
                    });
                    if (!done) return false;
                    if (renamed.own.length) this.say('the parameters of {0} are of a draw of the program\'s own: a colour named in them is not renamed', [renamed.own.join(', ')]);
                    this.sayColorOutside(from);
                    return true;
                }
                if (plan.refused.length) {
                    this.say('"{0}" not renamed: these pages on the appearance do not read: {1}', [from, plan.refused.join(', ')], true);
                    return false;
                }
                const key = section === 'looks' ? 'look' : 'style', link = ++this.raw.linkCount, now = Date.now();
                const file = this.files[path];
                this.historyOf(path).record(this.snapshot(path), null, now, link);
                if (!appearance.renameKey(doc, [section], from, to)) return false;
                file.text = appearance.write(doc).text, file.byBuilder = true;
                this.pick = [section, to];
                for (const page of plan.pages) {
                    const its = this.files[page.path], said = read(its.text);
                    this.historyOf(page.path).record(this.snapshot(page.path), null, now, link);
                    for (const widget of page.widgets) description.setKey(said.doc, widget, key, to);
                    its.text = write(said.doc).text, its.byBuilder = true;
                    if (page.path === this.current) this.loadText();
                }
                this.raw.links.set(link, [path, ...plan.pages.map((page) => page.path)]);
                for (const { text, values } of plan.warnings) this.say(text, values);
                if (plan.pages.length) this.say('renamed in {0} widget(s) of {1}', [plan.pages.reduce((sum, page) => sum + page.widgets.length, 0),
                                                                                    plan.pages.map((page) => page.path).join(', ')]);
                this.giveAll();
                return true;
            },

            // an entry deleted: a look or a style a widget of a page on the appearance names is not, and the widgets are
            // said; a colour is, the loader judging the looks that name it
            removeEntry(section, name) {
                if (section !== 'colors') {
                    const plan = this.renamePlan(section, name, name);
                    if (plan.pages.length) {
                        const named = plan.pages.map((page) => `${page.path}: ${page.widgets.map((widget) => widget.join('.') || 'tree').join(', ')}`);
                        this.say('"{0}" not deleted: widgets name it: {1}', [name, named.join('; ')], true);
                        return false;
                    }
                }
                if (!this.editAppearance((doc) => (appearance.removeAt(doc, [section, name]) ? [section] : null))) return false;
                if (section === 'colors') this.sayColorOutside(name);
                else if (section === 'looks' && this.raw.format.kinds.includes(name))
                    this.say('"{0}" is the word of a kind: the widgets of the kind that name no look take the library\'s look after it', [name]);
                else if (section === 'styles' && this.raw.look && appearance.namesOf(this.raw.look, 'styles').length &&
                         this.verdicts.appearance?.code === 0)
                    this.say('the texts that name no style take the first style now, "{0}"', [appearance.namesOf(this.raw.look, 'styles')[0]]);
                return true;
            },

            // ---- the names file by its forms ----

            // an edit of the names file: the change gives its new text and the line to show, null when it changes nothing;
            // given to the module, the page read on it again. A names file a call names and the disk has not yet is
            // edited from nothing, and written at a save
            editNames(change, fold = null) {
                const path = this.page?.names;
                if (!path) return false;
                const file = this.files[path], before = this.snapshot(path);
                const done = change(file.missing ? '' : file.text);
                if (!done || done.text === (file.missing ? '' : file.text)) return false;
                this.historyOf(path).record(before, fold);
                file.text = done.text, file.missing = false, file.illFormed = null;
                if (done.pick) this.namesPick = done.pick;
                this.giveAll();
                return true;
            },

            namesFold(table, name, k) {
                return `${this.page?.names}|${table}.${name}.${k}`;
            },

            // the words of a line set: a word of white space or '#' waits, as it would split the line
            setNamesLine(number, words, fold = null) {
                if (words.some((word) => !word || /[\s#]/.test(word))) return false;
                return this.editNames((text) => {
                    const changed = namesFile.setLine(text, number, words);
                    return changed === null ? null : { text: changed, pick: [words[0], words[1]] };
                }, fold);
            },

            // a line added after the last of its table; refused when its table has the name already
            addNamesLine(words) {
                if (words.length < 2 || words.some((word) => !word || /[\s#]/.test(word))) return false;
                if (this.namesRows.some((row) => row.table === words[0] && row.words[1] === words[1])) {
                    this.say('"{0}": the names file has a {1} of that name already', [words[1], words[0]], true);
                    return false;
                }
                return this.editNames((text) => ({ text: namesFile.addLine(text, words).text, pick: [words[0], words[1]] }));
            },

            // the line of a name the page uses and the file has not, with the expression the rule gives it
            missingLine(entry) {
                const words = [entry.table, entry.name];
                if (!entry.bare) {
                    if (!entry.c) return null;
                    words.push(entry.c);
                    if (entry.count) words.push(String(entry.count));
                    if (entry.number) words.push('number');
                    if (entry.page) words.push('page');
                }
                return words;
            },

            addMissing(entry) {
                const words = this.missingLine(entry);
                if (!words) {
                    this.say('"{0}": the rule makes no C of it; add it with an expression of the program\'s', [entry.name], true);
                    return false;
                }
                return this.addNamesLine(words);
            },

            // every name missing that the rule makes C of, in one step
            addAllMissing() {
                const lines = this.namesMissing.map((entry) => this.missingLine(entry)).filter(Boolean);
                if (!lines.length) return false;
                return this.editNames((text) => ({ text: namesFile.addLines(text, lines), pick: [] }));
            },

            // who names a name of the names file: the widgets of the pages on it by the key of its table, a look of a
            // kind's word by the widgets of the kind that name none, a draw, a font or a hand by the appearances on it;
            // with name null, the names of the file no one names ("table name")
            namesUsers(table, name = null) {
                const format = this.raw.namesFormat, kinds = this.raw.format?.kinds ?? [];
                const users = [], seen = new Set();
                const note = (one, what) => seen.add(`${one} ${what}`);
                const firstStyle = this.page?.names && this.files[this.page.names]
                    ? namesFile.rows(this.files[this.page.names].text, format).find((row) => row.table === 'style')?.words[1] ?? null : null;
                for (const path of this.pagesOnNames) {
                    const file = this.files[path], said = file && !file.missing && !file.illFormed ? read(file.text) : { ok: false };
                    if (!said.ok) continue;
                    // a text that names no style takes the first: the appearance's when it has styles, else the file's
                    const its = this.project.pages.find((page) => page.path === path)?.appearance;
                    const look = its && this.files[its] && !this.files[its].missing ? read(this.files[its].text) : { ok: false };
                    const appearanceStyles = look.ok ? appearance.namesOf(look.doc, 'styles').length : 0;
                    for (const { path: at, widget } of widgets(said.doc)) {
                        if ((valueOf(widget, 'text') || valueOf(widget, 'text_size')) && !valueOf(widget, 'style') && !appearanceStyles && firstStyle)
                            note('style', firstStyle);
                        format.tables.forEach((one, k) => {
                            const value = stringOf(valueOf(widget, format.keys[k]));
                            if (value === null) return;
                            note(one, value);
                            if (one === table && value === name) users.push({ path, at, key: format.keys[k] });
                        });
                        const kind = kindOf(widget);
                        if (typeof kind === 'string' && kinds.includes(kind) && !valueOf(widget, 'look')) note('look', kind);
                    }
                }
                for (const path of this.appearancesOnNames) {
                    const file = this.files[path], said = file && !file.missing && !file.illFormed ? read(file.text) : { ok: false };
                    if (!said.ok) continue;
                    const looks = valueOf(said.doc, 'looks'), styles = valueOf(said.doc, 'styles');
                    for (const [key, look] of looks?.t === 'object' ? looks.entries : []) {
                        const draw = stringOf(valueOf(look, 'draw'));
                        if (draw === null) continue;
                        note('draw', draw);
                        if (table === 'draw' && draw === name) users.push({ path, at: ['looks', key.value, 'draw'] });
                        // a field of pictures of the draw names a picture of the file, as a page's widget does
                        const fields = this.raw.lookFormat?.draws.find((one) => one.draw === draw)?.fields ?? [];
                        const params = valueOf(look, 'params');
                        for (const field of fields.filter((one) => one.type === 'picture')) {
                            const picture = params?.t === 'object' ? stringOf(valueOf(params, field.name)) : null;
                            if (picture === null) continue;
                            note('picture', picture);
                            if (table === 'picture' && picture === name) users.push({ path, at: ['looks', key.value, 'params', field.name] });
                        }
                    }
                    for (const [key, style] of styles?.t === 'object' ? styles.entries : []) {
                        const fonts = valueOf(style, 'fonts');
                        (fonts?.t === 'array' ? fonts.items : []).forEach((variant, k) => {
                            const font = variant.t === 'string' ? variant.value : stringOf(valueOf(variant, 'font'));
                            const hand = variant.t === 'object' ? stringOf(valueOf(variant, 'hand')) : null;
                            if (font !== null) note('font', font);
                            if (hand !== null) note('hand', hand);
                            if (table === 'font' && font === name)
                                users.push({ path, at: variant.t === 'string' ? ['styles', key.value, 'fonts', k] : ['styles', key.value, 'fonts', k, 'font'] });
                            if (table === 'hand' && hand === name) users.push({ path, at: ['styles', key.value, 'fonts', k, 'hand'] });
                        });
                    }
                }
                const unused = new Set();
                const path = this.page?.names;
                if (path && this.files[path] && (this.pagesOnNames.length || this.appearancesOnNames.length))
                    for (const row of namesFile.rows(this.files[path].text, format))
                        if (row.known && row.words.length > 1 && !seen.has(`${row.table} ${row.words[1]}`)) unused.add(`${row.table} ${row.words[1]}`);
                return { users, unused };
            },

            // what a rename of a name would change: the pages and appearances on the file that name it, those that do not
            // read, the words to say first
            namesPlan(table, from, to) {
                const plan = { files: [], refused: [], warnings: [], taken: this.namesRows.some((row) => row.table === table && row.words[1] === to) };
                const users = this.namesUsers(table, from).users;
                for (const path of [...this.pagesOnNames, ...this.appearancesOnNames]) {
                    const file = this.files[path];
                    if (!file || file.missing || file.illFormed || !read(file.text).ok) plan.refused.push(path);
                }
                for (const user of users) {
                    let one = plan.files.find((each) => each.path === user.path);
                    if (!one) plan.files.push((one = { path: user.path, places: [] }));
                    one.places.push(user);
                }
                const kinds = this.raw.format.kinds;
                if (table === 'look' && kinds.includes(from))
                    plan.warnings.push({ text: '"{0}" is the word of a kind: the widgets of the kind that name no look are refused after it, for want of one', values: [from] });
                if (table === 'look' && kinds.includes(to))
                    plan.warnings.push({ text: '"{0}" is the word of a kind: every widget of the kind that names no look takes it', values: [to] });
                if (['value', 'picture', 'on_event', 'take', 'ctx', 'id', 'style', 'look', 'font', 'draw'].includes(table))
                    plan.warnings.push({ text: 'the C expression stays as it is, the program\'s: only the name the pages and appearances use changes', values: [] });
                return plan;
            },

            // a name renamed in the names file and in every page and appearance on it that names it, one step of all
            // those files
            renameName(table, from, to) {
                const path = this.page?.names;
                if (!path || !to || from === to || /[\s#]/.test(to)) return false;
                const plan = this.namesPlan(table, from, to);
                if (plan.taken) {
                    this.say('"{0}": the names file has a {1} of that name already', [to, table], true);
                    return false;
                }
                if (plan.refused.length) {
                    this.say('"{0}" not renamed: these files on the names file do not read: {1}', [from, plan.refused.join(', ')], true);
                    return false;
                }
                const row = this.namesRows.find((one) => one.table === table && one.words[1] === from);
                if (!row) return false;
                const words = this.renamedWords(row.words, to);
                if (!words) {
                    this.say('a hand is the library\'s, by its word alone: not renamed', [], true);
                    return false;
                }
                const link = ++this.raw.linkCount, now = Date.now(), file = this.files[path];
                this.historyOf(path).record(this.snapshot(path), null, now, link);
                file.text = namesFile.setLine(file.text, row.number, words);
                this.namesPick = [table, to];
                for (const one of plan.files) {
                    const its = this.files[one.path], said = read(its.text);
                    this.historyOf(one.path).record(this.snapshot(one.path), null, now, link);
                    const isPage = this.project.pages.some((page) => page.path === one.path);
                    for (const place of one.places) {
                        if (isPage) description.setKey(said.doc, place.at, place.key, to);
                        else appearance.setAt(said.doc, place.at, to);
                    }
                    its.text = isPage ? write(said.doc).text : appearance.write(said.doc).text, its.byBuilder = true;
                    if (one.path === this.current) this.loadText();
                }
                this.raw.links.set(link, [path, ...plan.files.map((one) => one.path)]);
                for (const { text, values } of plan.warnings) this.say(text, values);
                if (plan.files.length) this.say('renamed in {0} place(s) of {1}', [plan.files.reduce((sum, one) => sum + one.places.length, 0),
                                                                                  plan.files.map((one) => one.path).join(', ')]);
                this.giveAll();
                return true;
            },

            // the words of a line renamed, its C kept: a name that went with no expression, whose C its name gave, given
            // that C now, as dgx_ui_c gives it ("look button" is &dgx_ui_look_button, "font small" &dgx_font_small); a
            // hand, the library's word alone, none
            renamedWords(words, to) {
                const [table, from, ...rest] = words;
                if (table === 'hand') return null;
                if (table === 'font' && (!rest.length || rest[0] === 'handwritten')) return [table, to, `dgx_font_${from}`, ...rest];
                if (rest.length) return [table, to, ...rest];
                if (table === 'look') return [table, to, `&dgx_ui_look_${from}`];
                if (table === 'on_event') return [table, to, `dgx_ui_action_${from}`];
                if (table === 'draw') return [table, to, `dgx_ui_draw_${from}`, `&dgx_ui_params_${from}`];
                return [table, to];
            },

            // a line deleted: a name a widget or an appearance names is not, and they are said
            removeName(table, name) {
                const users = this.namesUsers(table, name).users;
                if (users.length) {
                    const said = [...new Set(users.map((user) => user.path))].map((path) =>
                        `${path}: ${users.filter((user) => user.path === path).map((user) => user.at.join('.') || 'tree').join(', ')}`);
                    this.say('"{0}" not deleted: these name it: {1}', [name, said.join('; ')], true);
                    return false;
                }
                const row = this.namesRows.find((one) => one.table === table && one.words[1] === name);
                if (!row) return false;
                return this.editNames((text) => ({ text: namesFile.removeLine(text, row.number), pick: [] }));
            },

            // the pages on an appearance or a names file among the files a save writes, loaded on them by the module, one
            // pass each: the appearance or the names file where it is refused itself, and the pages refused, {path,
            // verdict}; the page open given again after
            brokenPages(paths) {
                const looks = new Set(paths.filter((path) => this.project.appearances.some((one) => one.path === path) ||
                                                              this.project.pages.some((page) => page.appearance === path)));
                const vocabularies = new Set(paths.filter((path) => this.project.pages.some((page) => page.names === path)));
                if (!looks.size && !vocabularies.size) return [];
                const broken = [], judged = new Set();
                for (const page of this.project.pages.filter((one) => looks.has(one.appearance) || vocabularies.has(one.names))) {
                    const said = this.giveFiles(page);
                    if (page.names && !judged.has(page.names) && said.names && said.names.code !== 0)
                        broken.push({ path: page.names, verdict: { ...said.names } });
                    if (page.appearance && !judged.has(page.appearance) && said.appearance && said.appearance.code !== 0)
                        broken.push({ path: page.appearance, verdict: { ...said.appearance } });
                    judged.add(page.appearance), judged.add(page.names);
                    const file = this.files[page.path];
                    const verdict = file.illFormed || file.missing ? { code: SYNTAX, offset: file.illFormed?.byte ?? 0, name: '', said: '' }
                                                                   : this.raw.preview.page(file.text);
                    if (verdict.code !== 0) broken.push({ path: page.path, verdict: { ...verdict } });
                }
                this.giveAll();
                return broken;
            },

            // ---- an agent beside the builder (tools/dgx_ui_mcp.py) ----

            startAgent(link) {
                if (!this.agentAllowed) return link?.stop?.(); /* the page's own agent: nothing polls /agent/* */
                this.raw.agentLink = Vue.markRaw(link);
                this.agent = true;
            },

            agentSay(text) {
                if (!text.trim() || !this.raw.agentLink) return false;
                this.raw.agentLink.say(text);
                return true;
            },

            // a verdict as the agent reads it: the loader's code by its name, the byte, the name; null none
            verdictOf(verdict) {
                if (!verdict) return null;
                if (verdict.code === 0) return { taken: true };
                return { code: verdict.code > 0 ? `DGX_UI_LOAD_${LOAD_CODES[verdict.code] ?? verdict.code}` : verdict.code, byte: verdict.offset,
                         name: verdict.name || undefined, said: verdict.said || undefined };
            },

            agentVerdicts() {
                return { page: this.readError ? { code: 'not read', byte: this.readError.byte, said: this.readError.why } : this.verdictOf(this.verdicts.page),
                         names: this.verdictOf(this.verdicts.names), appearance: this.verdictOf(this.verdicts.appearance),
                         fonts: this.verdicts.fonts.filter((one) => one.verdict.code !== 0).map((one) => ({ name: one.name, said: one.verdict.said })),
                         widget: this.refusedPath, appearance_entry: this.appearanceAt, names_line: this.namesAt };
            },

            // what the agent asks, answered: the builder now, the panel, a file's text, an edit, a page and widget shown, the
            // formats
            async agentAnswer(kind, args) {
                if (kind === 'state') {
                    return { pages: this.project?.pages.map((page) => ({ path: page.path, names: page.names, appearance: page.appearance, made_by: page.from })) ?? [],
                             names_files: this.project?.namesFiles.map((one) => one.path) ?? [], appearances: this.project?.appearances.map((one) => one.path) ?? [],
                             open: this.current, selected: this.selection, verdicts: this.agentVerdicts(), notes: this.notes, missing_names: this.namesMissing,
                             edited: this.dirty, screen: this.screen, tab: this.tab };
                }
                if (kind === 'image') {
                    const image = typeof document === 'undefined' ? null : this.image(); // a page's, not Node's
                    if (!image) return { error: 'no panel drawn' };
                    const canvas = document.createElement('canvas');
                    canvas.width = image.width, canvas.height = image.height;
                    canvas.getContext('2d').putImageData(image, 0, 0);
                    return { png: canvas.toDataURL('image/png').replace(/^data:image\/png;base64,/, ''), width: image.width, height: image.height };
                }
                if (kind === 'read') {
                    const file = this.files[args.path];
                    return file ? { path: args.path, text: file.missing ? null : file.text, edited: this.dirty.includes(args.path) }
                                : { error: `${args.path}: not a file the builder holds: ${Object.keys(this.files).join(', ')}` };
                }
                if (kind === 'edit') return this.agentEdit(args.path, args.text);
                if (kind === 'select') {
                    if (!this.choose(args.path)) return { error: `${args.path}: no page of the project` };
                    if (args.widget && !this.select(args.widget)) return { error: `no widget at ${JSON.stringify(args.widget)}`, open: this.current };
                    return { open: this.current, selected: this.selection };
                }
                if (kind === 'formats') return { page: this.raw.format, appearance: this.raw.lookFormat ?? this.raw.preview.appearanceFormat(), names: this.raw.namesFormat };
                return { error: `no such request: ${kind}` };
            },

            // the whole text of a file of the project put in as one step of its history, the page it is of or that uses it
            // opened and the tab of its kind shown, as the person would undo it; the verdicts after
            agentEdit(path, text) {
                const file = this.files[path];
                if (!file || typeof text !== 'string')
                    return { error: `${path}: not a description, names file or appearance of the project: ${Object.keys(this.files).join(', ')}` };
                const page = this.project.pages.find((one) => one.path === path) ??
                             (this.page && (this.page.names === path || this.page.appearance === path) ? this.page : null) ??
                             this.project.pages.find((one) => one.names === path || one.appearance === path);
                if (!page) return { error: `${path}: no page of the project uses it` };
                if (page.path !== this.current) this.choose(page.path);
                const kind = path === page.path ? 'page' : path === page.names ? 'names' : 'appearance';
                this.setTab(kind === 'page' ? 'properties' : kind);
                this.historyOf(path).record(this.snapshot(path));
                file.text = text, file.missing = false, file.illFormed = null, file.byBuilder = false;
                if (kind === 'page') {
                    this.loadText();
                    if (!this.raw.doc || !widgetAt(this.raw.doc, this.selection)) this.selection = [];
                }
                this.giveAll();
                return { path, verdicts: this.agentVerdicts() };
            },

            // ---- the screen and the view ----

            // the screen chosen: written into the description's "editor" entry, the page made for it
            setScreen(screen) {
                const chosen = { width: +screen.width, height: +screen.height, depth: +screen.depth, unit: +screen.unit };
                if (!validScreen(chosen)) return false;
                this.raw.preferences.set(PREFERENCES.screen, JSON.stringify(chosen));
                const own = this.raw.doc ? builderOf(this.raw.doc).screen : null;
                const same = own && ['width', 'height', 'depth', 'unit'].every((key) => own[key] === chosen[key]);
                this.screen = chosen;
                if (this.raw.doc && !same) {
                    const selection = this.selection;
                    this.edit((doc) => (description.setBuilder(doc, { ...builderOf(doc), screen: chosen }), selection));
                }
                this.giveScreen();
                return true;
            },

            setZoom(zoom) {
                if (!ZOOMS.includes(+zoom)) return;
                this.zoom = +zoom;
                this.raw.preferences.set(PREFERENCES.zoom, String(this.zoom));
            },

            setLanguage(language) {
                this.language = language;
                this.raw.preferences.set(PREFERENCES.language, language);
            },

            // ---- trying the page ----

            setTrying(on) {
                this.trying = !!on;
            },

            pointer(x, y, down, now) {
                if (!this.trying) return;
                this.raw.preview.pointer(x, y, down, now);
                this.wakeAt = 0;
            },

            key(key) {
                this.raw.preview.key(key);
                this.wakeAt = 0;
            },

            // the page an action opened closed: the page given again
            back() {
                this.givePage();
            },

            // the description of a page by its name, as the action page finds it: the one whose "name" it is, else the
            // one whose file is <name>.json
            pageOfName(name) {
                for (const page of this.project?.pages ?? []) {
                    const said = read(this.files[page.path]?.text ?? '');
                    if (said.ok && stringOf(valueOf(said.doc, 'name')) === name) return page.path;
                }
                return this.project?.pages.find((page) => page.path.split('/').pop() === `${name}.json`)?.path ?? null;
            },

            pageTextOf(name) {
                const path = this.pageOfName(name);
                return path && !this.files[path].illFormed ? this.files[path].text : null;
            },

            openShown() {
                const path = this.shown ? this.pageOfName(this.shown) : null;
                return path ? this.choose(path) : false;
            },

            // ---- saving ----

            // the files edited, of the page open (its names file and appearance with it) or all; a description first
            // written in the builder's form waits for a word, once a file
            async save(all = false, confirmed = false) {
                const own = [this.current, this.page?.names, this.page?.appearance, ...(this.project?.needsFiles ?? [])];
                const paths = this.dirty.filter((path) => all || own.includes(path));
                if (!paths.length && !Object.keys(this.raw.pending).length) return [];
                const reformatting = paths.filter((path) => !this.files[path].formed && this.files[path].byBuilder && !this.reformatted[path]);
                const broken = confirmed ? [] : this.brokenPages(paths);
                if ((reformatting.length || broken.length) && !confirmed) {
                    this.asking = { paths: reformatting, broken, all };
                    return null;
                }
                for (const path of reformatting) this.reformatted[path] = true;
                this.asking = null;
                const done = [];
                // the pictures imported first: a need that names a PNG not yet there would stop the build
                for (const [path, bytes] of Object.entries(this.raw.pending)) {
                    try {
                        await this.raw.host.writeBinary(path, bytes);
                        delete this.raw.pending[path];
                        done.push(path);
                        this.say('saved {0}', [path]);
                    } catch (error) {
                        this.say('{0} not saved: {1}', [path, error.message], true);
                    }
                }
                for (const path of paths) {
                    const text = this.files[path].text;
                    try {
                        const inPlace = await this.raw.host.write(path, text);
                        this.files[path].saved = text, this.files[path].missing = false;
                        done.push(path);
                        if (inPlace) this.say('saved {0}', [path]);
                        else this.say('downloaded {0}: put it in place of the file in the project', [path]);
                    } catch (error) {
                        this.say('{0} not saved: {1}', [path, error.message], true);
                    }
                }
                return done;
            },

            cancelSave() {
                this.asking = null;
            },
        },
    });
    return Object.assign(store, { SCREENS, ZOOMS, DEPTHS, LOAD_CODES, cmakeCalls, readProject, joinPath, pictureBits, pictureKind, pictureName });
})();
