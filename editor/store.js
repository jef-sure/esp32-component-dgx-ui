// The state of the builder: the project open, the page chosen with its description as a document, the selection, and
// what the preview's module said of the texts and drew. What the format allows is the loader's word: every text goes to
// the module, and its verdicts are shown, none of the format's rules written here again. description.js reads, edits
// and writes a description; history.js keeps undo; a host (host_browser.js) gives the files.
//
// A project is what its CMakeLists say: main/CMakeLists.txt, components/*/CMakeLists.txt and the top one are read for
// dgx_ui_tree(), dgx_ui_names() and dgx_ui_appearance(), each page with its names file and appearance as the call
// gives them. A call the reading cannot be sure of, a variable in it or a block such as if() around it, is listed as
// not understood with its file and line, never guessed. The other descriptions of the project, files holding "dgx-ui"
// and "tree", are pages too, read at run time, on a names file and an appearance chosen. The fonts are the files DGX
// writes with dgx_fonts-files.txt, in build/dgx_fonts/ or build/<board>/dgx_fonts/.
'use strict';

const useDgxUiBuilder = (() => {
    const description = window.DgxUiDescription;
    const { read, write, widgets, widgetAt, indexOf, pathOf, samePath, valueOf, shown, builderOf, unitOfByte, illFormedAt } = description;
    const { History } = window.DgxUiHistory;

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
    const LOAD_CODES = ['OK', 'SYNTAX', 'VERSION', 'KEY', 'KIND', 'FIELD', 'RANGE', 'NAME_BAD', 'UNKNOWN', 'NO_LOOK', 'NO_STYLE', 'DEPTH_MAX',
                        'MEMORY', 'NODES', 'ID', 'MISSING'];
    const SYNTAX = 1, NO_LOOK = 9;
    const CALLS = { dgx_ui_tree: ['PREFIX', 'NAMES', 'APPEARANCE'], dgx_ui_names: ['PREFIX', 'APPEARANCE'], dgx_ui_appearance: ['PREFIX', 'NAMES'] };
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

    // the panel of the board a build is of: the computer's sdkconfig showing a board, or the board's entry of
    // jef-sure/boards, local or of the catalog; null when neither says it
    async function boardScreen(host, board, binaryDir) {
        const config = binaryDir ? textOf(await host.read(`${binaryDir}/sdkconfig`)) : null;
        if (config) {
            const value = (name) => +(config.match(new RegExp(`^CONFIG_BOARD_HOST_${name}=([0-9]+)$`, 'm'))?.[1] ?? NaN);
            const depth = DEPTHS.includes(value('COLOR_BITS')) ? value('COLOR_BITS') : 16;
            if (value('WIDTH') > 0 && value('HEIGHT') > 0)
                return { width: value('WIDTH'), height: value('HEIGHT'), depth, from: `${binaryDir}/sdkconfig` };
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

    // what the project is: its pages, names files and appearances by its CMakeLists, the other descriptions found, the
    // calls not understood, the directories of fonts and the board of its preset
    async function readProject(host) {
        const project = { name: host.name, pages: [], namesFiles: [], appearances: [], notUnderstood: [], fontDirs: [], fontDir: null, board: null,
                          boardScreen: null, notes: [] };
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
                project.appearances.push({ path: call.file, names: call.names ?? null, from: call.from });
        }
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
            // one that does not read is a page still when its text holds both keys: it is there to be mended
            const data = await host.read(path), said = read(data);
            const page = said.ok ? valueOf(said.doc, 'dgx-ui') && valueOf(said.doc, 'tree') : /"dgx-ui"[^]*"tree"/.test(decoder.decode(data));
            if (!page) continue;
            project.pages.push({ path, names: lone?.path ?? null, appearance: lone?.appearance ?? null, from: null });
        }
        // the fonts: the index DGX writes, in build/dgx_fonts/ or in a board's build/<board>/dgx_fonts/
        for (const entry of (await host.list('build')) ?? []) {
            if (!entry.directory) continue;
            const dir = entry.name === 'dgx_fonts' ? 'build/dgx_fonts' : `build/${entry.name}/dgx_fonts`;
            if ((await host.read(`${dir}/${FONTS_INDEX}`)) === null) continue;
            project.fontDirs.push({ dir, board: entry.name === 'dgx_fonts' ? null : entry.name });
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
        if (project.fontDirs.length > 1 && !ofPreset)
            project.notes.push({ text: 'fonts of several builds: {0} taken, another may be chosen', values: [project.fontDir] });
        if (project.board) project.boardScreen = await boardScreen(host, project.board, binaryDir);
        return project;
    }

    // a file as the store holds it: the text as saved and as edited; one not well-formed UTF-8 shown with its bytes
    // replaced, where they are not, until it is edited
    function fileState(bytes, isPage) {
        if (bytes === null) return { saved: null, text: '', missing: true, illFormed: null, formed: true, byBuilder: false };
        const bad = illFormedAt(bytes);
        const text = decoder.decode(bytes);
        const state = { saved: text, text, missing: false, illFormed: null, formed: true, byBuilder: false };
        if (bad >= 0) state.illFormed = { byte: bad, at: decoder.decode(bytes.subarray(0, bad)).length };
        else if (isPage) {
            const said = read(text);
            state.formed = !said.ok || write(said.doc).text === text;
        }
        return state;
    }

    const validScreen = (screen) => screen && [screen.width, screen.height].every((n) => Number.isInteger(n) && n >= 1 && n <= 4096) &&
                                    DEPTHS.includes(screen.depth) && Number.isInteger(screen.unit) && screen.unit >= 1 && screen.unit <= 16;

    const store = Pinia.defineStore('dgx-ui-builder', {
        state: () => ({
            ready: false,
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
            verdicts: { names: null, fonts: [], appearance: null, page: null },
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
            raw: Vue.markRaw({ preview: null, host: null, doc: null, spans: [], histories: new Map(), format: null, preferences: null }),
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
                const ofKind = format.keys.filter((entry) => entry.key !== 'children' && entry.kinds.includes(kind));
                ofKind.sort((a, b) => description.WIDGET_ORDER.indexOf(a.key) - description.WIDGET_ORDER.indexOf(b.key));
                const fields = ofKind.map((entry) => ({ key: entry.key, entry, present: present.has(entry.key), value: shown(present.get(entry.key)),
                                                       raw: present.get(entry.key) }));
                const taken = new Set(ofKind.map((entry) => entry.key).concat('children'));
                for (const [key, value] of present) {
                    if (!taken.has(key)) fields.push({ key, entry: null, present: true, value: shown(value), raw: value });
                }
                return fields;
            },
            canUndo() {
                this.version;
                return !!this.raw.histories.get(this.current)?.canUndo;
            },
            canRedo() {
                this.version;
                return !!this.raw.histories.get(this.current)?.canRedo;
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
                const paths = new Set([...pages, ...project.namesFiles.map((one) => one.path),
                                       ...project.appearances.flatMap((one) => [one.path, one.names]).filter((path) => path),
                                       ...project.pages.flatMap((page) => [page.names, page.appearance]).filter((path) => path)]);
                for (const path of paths) {
                    files[path] = fileState(await host.read(path), pages.has(path));
                    if (files[path].missing) project.notes.push({ text: '{0}: no such file', values: [path] });
                }
                this.files = files, this.project = project, this.current = null, this.reformatted = {}, this.said = [], this.asking = null;
                this.raw.histories = new Map(), this.raw.doc = null, this.version++;
                await this.giveFonts();
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

            historyOf() {
                if (!this.raw.histories.has(this.current)) this.raw.histories.set(this.current, new History());
                return this.raw.histories.get(this.current);
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
                const preview = this.raw.preview, page = this.page;
                if (!page) return;
                const textAt = (path) => (path && this.files[path] && !this.files[path].missing ? this.files[path].text : null);
                this.verdicts.names = page.names ? preview.names(textAt(page.names)) : null;
                if (!page.names) preview.names(null);
                const made = this.project.appearances.find((one) => one.path === page.appearance);
                const itsNames = made ? (made.names ? textAt(made.names) : null) : undefined;
                this.verdicts.appearance = page.appearance ? preview.appearance(textAt(page.appearance), itsNames) : (preview.appearance(null), null);
                this.givePage();
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

            restore(state) {
                const file = this.files[this.current];
                file.text = state.text, file.illFormed = null;
                this.loadText();
                file.byBuilder = this.raw.spans.length > 0;
                this.selection = this.raw.doc && widgetAt(this.raw.doc, state.selection) ? [...state.selection] : [];
                this.givePage();
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
                if (which === 'page') this.historyOf().record({ text: file.text, selection: [...this.selection] }, `${this.current}|text`);
                file.text = text, file.illFormed = null, file.missing = false, file.byBuilder = false;
                if (which === 'page') {
                    this.loadText();
                    if (!this.raw.doc || !widgetAt(this.raw.doc, this.selection)) this.selection = [];
                    this.givePage();
                } else {
                    this.giveAll();
                }
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
                const own = [this.current, this.page?.names, this.page?.appearance];
                const paths = this.dirty.filter((path) => all || own.includes(path));
                if (!paths.length) return [];
                const reformatting = paths.filter((path) => !this.files[path].formed && this.files[path].byBuilder && !this.reformatted[path]);
                if (reformatting.length && !confirmed) {
                    this.asking = { paths: reformatting, all };
                    return null;
                }
                for (const path of reformatting) this.reformatted[path] = true;
                this.asking = null;
                const done = [];
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
    return Object.assign(store, { SCREENS, ZOOMS, DEPTHS, LOAD_CODES, cmakeCalls, readProject, joinPath });
})();
