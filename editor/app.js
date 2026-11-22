// The builder's page: the pages of a project and the widgets of the one open, the panel the preview draws with the
// selection outlined over it, the properties of the selected widget, and the texts by hand. No build step: the
// templates are compiled by the Vue of vendor/. dgxUiBuilderMount() mounts it on an element of a page.
'use strict';

const dgxUiBuilderMount = (() => {
    const { computed, ref, watch, nextTick, onMounted, onUnmounted } = Vue;
    const { LOAD_CODES, SCREENS, ZOOMS, DEPTHS } = useDgxUiBuilder;
    // what the loader's codes mean, as dgx_ui_load.h says
    const CODE_WORDS = {
        SYNTAX: 'not JSON, a string not well-formed UTF-8, or a value of the wrong type for its key',
        VERSION: 'no "dgx-ui", or one that is not 1',
        KEY: 'a key unknown, or twice in one object',
        KIND: 'no kind, or one unknown',
        FIELD: 'a key of no meaning for the kind',
        RANGE: 'a number out of its range, a fraction or an exponent',
        NAME_BAD: 'a name of a character not allowed, or longer than 23',
        UNKNOWN: 'a name its table has not',
        NO_LOOK: 'no look said, and the looks name none for the kind',
        NO_STYLE: 'a text, and no style to draw it in',
        DEPTH_MAX: 'nested deeper than the loader goes',
        MEMORY: 'the memory smaller than the need',
        NODES: 'no memory for more widgets',
        ID: 'an id on a second widget, or too many ids',
        MISSING: 'what an appearance must say is not said',
    };
    const NEW_NAME = '\u0000new', NULL_NAME = '\u0000null', ABSENT = '';
    const KEY_NAMES = ['Next', 'Prev', 'Activate', 'Back', 'More', 'Less']; // DGX_UI_KEY_*, in their order
    // the page's longer texts, each its own key of the translations
    const LONG_TEXTS = {
        noFonts: 'No dgx_fonts-files.txt in build/dgx_fonts/ nor in build/*/dgx_fonts/: dgx_fonts_files() in the project\'s CMakeLists ' +
                 'writes it at a build. The fonts the pages name are drawn in stand-ins.',
        start: 'Open a project\'s directory. The builder reads its CMakeLists for dgx_ui_tree(), dgx_ui_names() and dgx_ui_appearance(), ' +
               'finds the other descriptions of pages in it, and takes the fonts DGX writes into build/.',
        local: 'Nothing leaves the page: the preview runs in it, and a file is written only when you save it.',
        asking: 'These descriptions were written by hand. Saving writes them in the builder\'s form: the keys of a widget in one order, ' +
                'two spaces of indent, a short widget on one line. What they hold stays as written, strings and numbers too.',
        opened: 'On the screen: the page "{0}", opened by an action; clicks select nothing in it.',
        unread: 'The text does not read: {0}, at byte {1}.',
    };

    function useWords() {
        const store = useDgxUiBuilder();
        const t = (text, ...values) => dgxUiTranslate(store.language, text, ...values);
        // a verdict of the loader in words: null when taken
        const verdictText = (verdict) => {
            if (!verdict || verdict.code === 0) return null;
            if (verdict.code < 0) return verdict.said;
            const code = LOAD_CODES[verdict.code] ?? String(verdict.code);
            const named = verdict.name ? ` "${verdict.name}"` : '';
            return t('{0} at byte {1}{2}: {3}', `DGX_UI_LOAD_${code}`, verdict.offset, named, t(CODE_WORDS[code] ?? 'refused'));
        };
        return { store, t, verdictText, LONG_TEXTS };
    }

    // ---- the pages of the project and the widgets of the one open ----

    const TreePane = {
        setup() {
            const words = useWords(), store = words.store;
            const kind = ref('label');
            const label = (item) => item.kind + (item.id ? ` #${item.id}` : '') + (item.text !== null ? ` "${item.text}"` : '');
            const onKey = (event) => {
                if (event.key === 'ArrowUp' || event.key === 'ArrowDown') {
                    event.preventDefault();
                    store.selectNext(event.key === 'ArrowUp' ? -1 : 1);
                }
            };
            const isSelected = (item) => item.key === store.selection.join('.');
            const isRefused = (item) => store.refusedPath !== null && item.key === store.refusedPath.join('.');
            return { ...words, kind, label, onKey, isSelected, isRefused };
        },
        template: `
            <nav class="pane tree">
                <details open class="pages">
                    <summary>{{ t('Pages') }} <span class="note">{{ store.project.pages.length }}</span></summary>
                    <button v-for="page in store.project.pages" :key="page.path" class="node" :class="{ here: page.path === store.current }"
                            :title="page.from ? t('made by {0}', page.from) : t('read at run time')" @click="store.choose(page.path)">
                        {{ page.path }}<span v-if="store.dirty.includes(page.path)" class="note"> {{ t('edited') }}</span>
                    </button>
                </details>
                <div v-if="store.current" class="widgets" tabindex="0" @keydown="onKey">
                    <div class="heading">{{ t('Widgets') }}</div>
                    <p v-if="store.readError" class="wrong">{{ t(LONG_TEXTS.unread, t(store.readError.why), store.readError.byte) }}
                        <button @click="store.showText('page')">{{ t('Show the text') }}</button></p>
                    <button v-for="item in store.tree" :key="item.key" class="node" :class="{ here: isSelected(item), refused: isRefused(item) }"
                            :style="{ paddingLeft: (6 + 14 * item.depth) + 'px' }" @click="store.select(item.path)">{{ label(item) }}</button>
                </div>
                <div v-if="store.selected" class="operations">
                    <select v-model="kind" :aria-label="t('Kind')">
                        <option v-for="one in store.format.kinds" :key="one" :value="one">{{ one }}</option></select>
                    <button @click="store.addChild(kind)" :title="t('a child of the selection, or a widget after it')">{{ t('Add') }}</button>
                    <button @click="store.remove()" :disabled="!store.selection.length">{{ t('Delete') }}</button>
                    <button @click="store.move(-1)" :disabled="!store.selection.length" :title="t('Up')">↑</button>
                    <button @click="store.move(1)" :disabled="!store.selection.length" :title="t('Down')">↓</button>
                    <button @click="store.wrap()" :title="t('in a box of its own')">{{ t('Wrap') }}</button>
                </div>
                <details class="project">
                    <summary>{{ t('The project') }}</summary>
                    <p v-if="store.project.board">{{ t('Board of the preset: {0}', store.project.board) }}<span v-if="store.project.boardScreen">,
                        {{ store.project.boardScreen.width }}x{{ store.project.boardScreen.height }} ({{ store.project.boardScreen.from }})</span></p>
                    <div v-if="store.project.fontDirs.length">
                        <label>{{ t('Fonts') }}
                            <select :value="store.project.fontDir" @change="store.setFontDir($event.target.value)">
                                <option v-for="one in store.project.fontDirs" :key="one.dir" :value="one.dir">{{ one.dir }}</option>
                            </select></label>
                        <ul class="said"><li v-for="one in store.verdicts.fonts" :key="one.name" :class="{ wrong: one.verdict.code }">
                            {{ one.name }}<span v-if="one.verdict.code">: {{ one.verdict.said }}</span></li></ul>
                    </div>
                    <p v-else class="note">{{ t(LONG_TEXTS.noFonts) }}</p>
                    <ul class="said">
                        <li v-for="(note, i) in store.project.notes" :key="'n' + i">{{ t(note.text, ...note.values) }}</li>
                    </ul>
                    <div v-if="store.project.notUnderstood.length">
                        <div class="heading">{{ t('Not understood, choose by hand') }}</div>
                        <ul class="said"><li v-for="(one, i) in store.project.notUnderstood" :key="'u' + i">
                            <code>{{ one.file }}:{{ one.line }}</code> <code>{{ one.text }}</code>: {{ t(one.why, ...one.values) }}</li></ul>
                    </div>
                </details>
            </nav>
        `,
    };

    // ---- the panel ----

    const CanvasPane = {
        setup() {
            const words = useWords(), store = words.store;
            const canvas = ref(null);
            let frame = 0, down = false;
            const draw = () => {
                const image = store.image(), element = canvas.value;
                if (!image || !element) return;
                if (element.width !== image.width || element.height !== image.height) element.width = image.width, element.height = image.height;
                element.getContext('2d').putImageData(image, 0, 0);
            };
            const tick = (now) => {
                if (store.paint(now)) draw();
                frame = requestAnimationFrame(tick);
            };
            onMounted(() => (frame = requestAnimationFrame(tick)));
            onUnmounted(() => cancelAnimationFrame(frame));
            const at = (event) => {
                const box = canvas.value.getBoundingClientRect();
                return [Math.floor((event.clientX - box.left) / store.zoom), Math.floor((event.clientY - box.top) / store.zoom)];
            };
            const pressed = (event) => {
                if (!store.trying) return store.selectAt(...at(event));
                down = true;
                canvas.value.setPointerCapture?.(event.pointerId);
                store.pointer(...at(event), true, performance.now());
            };
            const moved = (event) => down && store.pointer(...at(event), true, performance.now());
            const released = (event) => {
                if (!down) return;
                down = false;
                store.pointer(...at(event), false, performance.now());
            };
            const outline = computed(() => {
                const box = store.box, zoom = store.zoom;
                if (!box) return null;
                const pixels = (value) => value + 'px';
                return { left: pixels(box.x * zoom), top: pixels(box.y * zoom),
                         width: pixels(Math.max(1, box.w * zoom)), height: pixels(Math.max(1, box.h * zoom)) };
            });
            const stale = computed(() => !store.verdicts.page || store.verdicts.page.code !== 0);
            return { ...words, canvas, pressed, moved, released, outline, stale, KEY_NAMES };
        },
        template: `
            <section class="pane panel">
                <div class="toolbar">
                    <label><input type="radio" :checked="!store.trying" @change="store.setTrying(false)"> {{ t('Select') }}</label>
                    <label><input type="radio" :checked="store.trying" @change="store.setTrying(true)"> {{ t('Try') }}</label>
                    <span v-if="store.trying" class="keys">
                        <button v-for="(name, k) in KEY_NAMES" :key="name" @click="store.key(k)">{{ t(name) }}</button>
                    </span>
                </div>
                <p v-if="store.shown" class="opened">{{ t(LONG_TEXTS.opened, store.shown) }}
                    <button @click="store.openShown()">{{ t('Open it') }}</button> <button @click="store.back()">{{ t('Back') }}</button></p>
                <div class="stage" :style="{ width: store.screen.width * store.zoom + 'px', height: store.screen.height * store.zoom + 'px' }">
                    <canvas ref="canvas" :class="{ stale }" :width="store.screen.width" :height="store.screen.height"
                            :style="{ width: store.screen.width * store.zoom + 'px', height: store.screen.height * store.zoom + 'px' }"
                            @pointerdown="pressed" @pointermove="moved" @pointerup="released"></canvas>
                    <div v-if="outline && !store.trying" class="outline" :style="outline"></div>
                </div>
                <div v-if="store.events.length" class="events">
                    <div class="heading">{{ t('Events') }}</div>
                    <pre>{{ store.events.slice(-10).join('\\n') }}</pre>
                </div>
            </section>
        `,
    };

    // ---- the texts by hand ----

    const TextPane = {
        setup() {
            const words = useWords(), store = words.store;
            const area = ref(null);
            const file = computed(() => (store.textPath ? store.files[store.textPath] : null));
            const verdict = computed(() => (store.text === 'page' && store.readError
                ? words.t(LONG_TEXTS.unread, words.t(store.readError.why), store.readError.byte)
                : words.verdictText(store.verdicts[store.text])));
            const putCursor = async () => {
                await nextTick();
                const cursor = store.cursor, element = area.value;
                if (!cursor || !element || cursor.text !== store.text) return;
                element.focus();
                element.setSelectionRange(cursor.at, cursor.at);
                const line = element.value.slice(0, cursor.at).split('\n').length;
                element.scrollTop = Math.max(0, (line - 4) * parseFloat(getComputedStyle(element).lineHeight || '16'));
            };
            watch(() => store.cursor, putCursor);
            onMounted(putCursor);
            const texts = computed(() => ['page', 'names', 'appearance'].filter((which) => which === 'page' || store.page?.[which]));
            return { ...words, area, file, verdict, texts };
        },
        template: `
            <section class="pane texts">
                <div class="toolbar">
                    <button v-for="which in texts" :key="which" :class="{ here: store.text === which }" @click="store.showText(which)">
                        {{ which === 'page' ? t('Page') : which === 'names' ? t('Names file') : t('Appearance') }}</button>
                    <span class="path">{{ store.textPath }}</span>
                    <button class="close" @click="store.view = 'page'">{{ t('Back to the panel') }}</button>
                </div>
                <p v-if="verdict" class="wrong verdict" @click="store.putCursor()" :title="t('the cursor to it')">{{ verdict }}</p>
                <p v-else class="good verdict">{{ t('Taken') }}</p>
                <textarea v-if="file" ref="area" spellcheck="false" :value="file.text"
                          @input="store.setText(store.text, $event.target.value)"></textarea>
            </section>
        `,
    };

    // ---- the properties ----

    // a control of a key, as the format gives its type
    const Field = {
        props: ['field'],
        setup(props) {
            const words = useWords(), store = words.store;
            const entry = computed(() => props.field.entry);
            const rawText = (node) => (node && node.t === 'number' ? node.raw : node && node.t === 'string' ? node.value : '');
            const pairText = (k) => (props.field.raw?.t === 'array' ? rawText(props.field.raw.items[k]) : '');
            const fold = () => store.fold(props.field.key);
            const names = computed(() => store.tables[entry.value?.table] ?? []);
            const typingNew = ref(false);
            // the choice of a name field: absent, null, a name of its table, or one typed
            const choice = computed(() => {
                const value = props.field.value;
                if (!props.field.present) return typingNew.value ? NEW_NAME : ABSENT;
                if (value === null) return NULL_NAME;
                return typeof value === 'string' && names.value.includes(value) && !typingNew.value ? value : NEW_NAME;
            });
            const choose = (value) => {
                typingNew.value = value === NEW_NAME;
                if (value === NEW_NAME) return;
                if (value === ABSENT) store.removeKey(props.field.key);
                else store.setKey(props.field.key, value === NULL_NAME ? null : value);
            };
            // a name typed: a number for a field that takes one, as typed; empty, the key removed
            const typeName = (text) => {
                if (entry.value?.min !== undefined && /^-?[0-9]/.test(text.trim())) return store.setNumber(props.field.key, text, fold());
                return text === '' ? store.removeKey(props.field.key) : store.setKey(props.field.key, text, fold());
            };
            const one = (value) => JSON.stringify(value);
            // a number of the field shown as a percent: of the field, or of one of a pair's two
            const isPercent = (k = null) => {
                const node = k === null ? props.field.raw : props.field.raw?.t === 'array' ? props.field.raw.items[k] : null;
                return node?.t === 'string' && node.value.endsWith('%');
            };
            const hint = computed(() => (entry.value ? `${entry.value.min} … ${entry.value.max}` + (entry.value.percent
                ? words.t(', or {0}% … {1}%', entry.value.percent.min, entry.value.percent.max) : '') : ''));
            return { ...words, entry, rawText, pairText, fold, names, choice, choose, typeName, one, isPercent, hint,
                     NEW_NAME, NULL_NAME, ABSENT };
        },
        template: `
            <div class="field" :class="{ foreign: !entry, absent: !field.present }">
                <span class="key" :title="entry ? entry.type : t('a key its kind takes not: the loader refuses it')">{{ field.key }}</span>
                <template v-if="!entry">
                    <code class="value">{{ one(field.value) }}</code>
                </template>
                <select v-else-if="entry.type === 'kind'" :value="field.value" @change="store.setKey('kind', $event.target.value)">
                    <option v-for="kind in store.format.kinds" :key="kind" :value="kind">{{ kind }}</option>
                    <option v-if="!store.format.kinds.includes(field.value)" :value="field.value">{{ field.value }}</option>
                </select>
                <span v-else-if="entry.type === 'words'" class="words">
                    <label v-for="word in entry.words" :key="word">
                        <input type="checkbox" :checked="Array.isArray(field.value) && field.value.includes(word)"
                               @change="store.toggleWord(field.key, word)">
                        {{ word }}</label>
                </span>
                <span v-else-if="entry.type === 'int'" class="pair">
                    <input type="text" :value="rawText(field.raw)" :placeholder="hint"
                           @input="store.setNumber(field.key, $event.target.value, fold())">
                    <button v-if="entry.percent" class="percent" :class="{ here: isPercent() }" :disabled="!field.present"
                            :title="t('in percent')" @click="store.togglePercent(field.key)">%</button>
                </span>
                <span v-else-if="entry.type === 'pair'" class="pair">
                    <template v-for="k in [0, 1]" :key="k">
                        <input type="text" :value="pairText(k)" :placeholder="hint"
                               @input="k === 0 ? store.setPair(field.key, $event.target.value, pairText(1), fold())
                                               : store.setPair(field.key, pairText(0), $event.target.value, fold())">
                        <button v-if="entry.percent" class="percent" :class="{ here: isPercent(k) }" :disabled="!field.present"
                                :title="t('in percent')" @click="store.togglePercent(field.key, k)">%</button>
                    </template>
                </span>
                <span v-else-if="entry.type === 'name'" class="name">
                    <select :value="choice" @change="choose($event.target.value)">
                        <option :value="ABSENT">—</option>
                        <option v-if="entry.null" :value="NULL_NAME">null</option>
                        <option v-for="name in names" :key="name" :value="name">{{ name }}</option>
                        <option :value="NEW_NAME">{{ entry.min !== undefined ? t('a new name or a number…') : t('a new name…') }}</option>
                    </select>
                    <input v-if="choice === NEW_NAME" type="text" @input="typeName($event.target.value)"
                           :value="field.value === null || field.value === undefined ? '' : String(field.value)">
                </span>
                <input v-else-if="entry.type === 'text'" type="text" :value="typeof field.value === 'string' ? field.value : ''"
                       @input="store.setKey(field.key, $event.target.value, fold())">
                <span v-else-if="entry.type === 'id'" class="name">
                    <input type="text" :value="typeof field.value === 'string' ? field.value : ''" :list="'dgx-ui-builder-ids'"
                           @input="$event.target.value === '' ? store.removeKey('id') : store.setKey('id', $event.target.value, fold())">
                    <datalist id="dgx-ui-builder-ids"><option v-for="name in store.tables.id ?? []" :key="name" :value="name"></option></datalist>
                </span>
                <code v-else class="value">{{ field.present ? one(field.value) : '' }}</code>
                <button v-if="field.present && field.key !== 'kind'" class="remove" :title="t('the key removed')"
                        @click="store.removeKey(field.key)">×</button>
            </div>
        `,
    };

    const PropertiesPane = {
        components: { Field },
        setup() {
            const words = useWords();
            return { ...words };
        },
        template: `
            <aside class="pane properties">
                <div v-if="store.page" class="files">
                    <div class="heading">{{ t('The page') }}</div>
                    <p class="path">{{ store.page.path }}<span v-if="store.page.from"> · {{ t('made by {0}', store.page.from) }}</span></p>
                    <template v-if="store.page.from">
                        <p>{{ t('Names file') }}: <code>{{ store.page.names ?? t('none, by the rule') }}</code></p>
                        <p>{{ t('Appearance') }}: <code>{{ store.page.appearance ?? t('none') }}</code></p>
                    </template>
                    <template v-else>
                        <label>{{ t('Names file') }}
                            <select :value="store.page.names ?? ''" @change="store.setPageFiles($event.target.value, store.page.appearance)">
                            <option value="">{{ t('none, by the rule') }}</option>
                            <option v-for="one in store.project.namesFiles" :key="one.from" :value="one.path">{{ one.path }}</option></select></label>
                        <label>{{ t('Appearance') }}
                            <select :value="store.page.appearance ?? ''" @change="store.setPageFiles(store.page.names, $event.target.value)">
                            <option value="">{{ t('none') }}</option>
                            <option v-for="one in store.project.appearances" :key="one.path" :value="one.path">{{ one.path }}</option>
                            </select></label>
                    </template>
                    <p class="buttons">
                        <button @click="store.showText('page')">{{ t('Text of the page') }}</button>
                        <button v-if="store.page.names" @click="store.showText('names')">{{ t('Names file') }}</button>
                        <button v-if="store.page.appearance" @click="store.showText('appearance')">{{ t('Appearance') }}</button>
                    </p>
                </div>
                <div v-if="store.selected">
                    <div class="heading">{{ t('Properties') }}</div>
                    <field v-for="field in store.fields" :key="store.selection.join('.') + '/' + field.key" :field="field"></field>
                </div>
            </aside>
        `,
    };

    // ---- the whole ----

    const DgxUiBuilder = {
        components: { TreePane, CanvasPane, TextPane, PropertiesPane },
        props: { host: { type: Object, default: null } },
        setup(props) {
            const words = useWords(), store = words.store;
            const root = ref(null), picker = ref(null);
            const opening = ref(false);
            const open = async (getHost) => {
                opening.value = true;
                try {
                    const host = await getHost();
                    if (host) await store.open(host);
                } catch (error) {
                    if (error?.name !== 'AbortError') store.say('{0}', [String(error?.message ?? error)], true);
                } finally {
                    opening.value = false;
                }
            };
            const openFolder = () => open(() => props.host.openFolder());
            const openPicked = (event) => open(() => props.host.fromPicked(event.target.files));
            const screenOf = (key, value) => store.setScreen({ ...store.screen, [key]: +value });
            const chooseScreen = (label) => {
                const screen = SCREENS.find((one) => one.label === label);
                if (screen) store.setScreen({ ...screen, unit: store.screen.unit });
            };
            const screenLabel = computed(() => SCREENS.find((one) => one.width === store.screen.width && one.height === store.screen.height)?.label
                                               ?? '');
            const onKey = (event) => {
                if (root.value && event.target !== document.body && !root.value.contains(event.target)) return;
                const key = event.key.toLowerCase(), command = event.ctrlKey || event.metaKey;
                if (command && key === 's') {
                    event.preventDefault();
                    store.save(true);
                    return;
                }
                if (/^(INPUT|TEXTAREA|SELECT)$/.test(event.target.tagName)) return;
                if (command && key === 'z' && !event.shiftKey) event.preventDefault(), store.undo();
                else if (command && (key === 'y' || (key === 'z' && event.shiftKey))) event.preventDefault(), store.redo();
                else if (event.key === 'Delete' && store.view === 'page' && store.current) event.preventDefault(), store.remove();
            };
            onMounted(() => document.addEventListener('keydown', onKey));
            onUnmounted(() => document.removeEventListener('keydown', onKey));
            const pageVerdict = computed(() => (store.readError ? null : words.verdictText(store.verdicts.page)));
            const languages = Object.entries(DGX_UI_LANGUAGES).map(([code, language]) => ({ code, title: language.title }));
            return { ...words, root, picker, opening, openFolder, openPicked, screenOf, chooseScreen, screenLabel, pageVerdict, languages,
                     ZOOMS, DEPTHS, SCREENS };
        },
        template: `
            <div class="dgx-ui-builder" ref="root">
                <header>
                    <h1>{{ t('dgx-ui builder') }}</h1>
                    <button v-if="host && host.canOpenFolder" @click="openFolder" :disabled="opening">{{ t('Open a project') }}</button>
                    <label v-if="host" class="button">{{ host.canOpenFolder ? t('or choose its files') : t('Choose a project\\'s directory') }}
                        <input ref="picker" type="file" webkitdirectory multiple hidden @change="openPicked"></label>
                    <span v-if="store.project" class="path">{{ store.project.name }}</span>
                    <span class="top">
                        <template v-if="store.current">
                            <select :value="screenLabel" @change="chooseScreen($event.target.value)" :aria-label="t('Screen')">
                                <option value="" disabled>{{ t('a screen of its own') }}</option>
                                <option v-for="one in SCREENS" :key="one.label" :value="one.label">{{ one.label }}</option></select>
                            <input type="number" min="1" max="4096" :value="store.screen.width" :aria-label="t('Width')"
                                   @change="screenOf('width', $event.target.value)">
                            ×<input type="number" min="1" max="4096" :value="store.screen.height" :aria-label="t('Height')"
                                    @change="screenOf('height', $event.target.value)">
                            <select :value="store.screen.depth" @change="screenOf('depth', $event.target.value)" :aria-label="t('Depth')">
                                <option v-for="depth in DEPTHS" :key="depth" :value="depth">{{ depth }} {{ t('bits') }}</option></select>
                            <label>{{ t('pixels a point') }}
                                <input type="number" min="1" max="16" :value="store.screen.unit"
                                       @change="screenOf('unit', $event.target.value)"></label>
                            <select :value="store.zoom" @change="store.setZoom($event.target.value)" :aria-label="t('Zoom')">
                                <option v-for="zoom in ZOOMS" :key="zoom" :value="zoom">{{ zoom }}×</option></select>
                            <button @click="store.undo()" :disabled="!store.canUndo" :title="t('Undo') + ' (Ctrl+Z)'">↶</button>
                            <button @click="store.redo()" :disabled="!store.canRedo" :title="t('Redo') + ' (Ctrl+Y)'">↷</button>
                            <button @click="store.save(true)" :disabled="!store.dirty.length" :title="'Ctrl+S'">
                                {{ t('Save') }}<span v-if="store.dirty.length"> ({{ store.dirty.length }})</span></button>
                        </template>
                        <select :value="store.language" @change="store.setLanguage($event.target.value)" :aria-label="t('Language')">
                            <option v-for="one in languages" :key="one.code" :value="one.code">{{ one.title }}</option></select>
                    </span>
                </header>
                <p v-if="store.failed" class="banner">{{ t('The preview did not start: {0}', store.failed) }}</p>
                <div v-else-if="!store.project" class="start">
                    <p>{{ t(LONG_TEXTS.start) }}</p>
                    <p>{{ t(LONG_TEXTS.local) }}</p>
                </div>
                <main v-else>
                    <tree-pane></tree-pane>
                    <text-pane v-if="store.view === 'text'"></text-pane>
                    <canvas-pane v-else-if="store.current"></canvas-pane>
                    <section v-else class="pane"></section>
                    <properties-pane></properties-pane>
                </main>
                <footer v-if="store.project">
                    <span v-if="store.readError" class="wrong">{{ t(LONG_TEXTS.unread, t(store.readError.why), store.readError.byte) }}</span>
                    <span v-else-if="pageVerdict" class="wrong">{{ t('Page') }}: {{ pageVerdict }}</span>
                    <span v-else-if="store.current" class="good">{{ t('Page taken') }}</span>
                    <span v-if="verdictText(store.verdicts.names)" class="wrong">{{ t('Names file') }}: {{ verdictText(store.verdicts.names) }}</span>
                    <span v-if="verdictText(store.verdicts.appearance)" class="wrong">
                        {{ t('Appearance') }}: {{ verdictText(store.verdicts.appearance) }}</span>
                    <span v-for="(note, i) in store.notes" :key="'n' + i" class="note">{{ note }}</span>
                    <span v-for="(one, i) in store.said" :key="'s' + i" :class="one.wrong ? 'wrong' : 'note'">{{ t(one.text, ...one.values) }}</span>
                </footer>
                <div v-if="store.asking" class="asking">
                    <div class="dialog">
                        <p>{{ t(LONG_TEXTS.asking) }}</p>
                        <ul><li v-for="path in store.asking.paths" :key="path"><code>{{ path }}</code></li></ul>
                        <button @click="store.save(store.asking.all, true)">{{ t('Save') }}</button>
                        <button @click="store.cancelSave()">{{ t('Cancel') }}</button>
                    </div>
                </div>
            </div>
        `,
    };

    // the builder on an element of a page: options.host, the files' host; options.start, what the store's start() takes
    return async function mount(element, options = {}) {
        const app = Vue.createApp({ components: { DgxUiBuilder }, data: () => ({ host: options.host ?? null }),
                                    template: '<dgx-ui-builder :host="host"></dgx-ui-builder>' });
        const pinia = Pinia.createPinia();
        app.use(pinia);
        app.mount(element);
        await useDgxUiBuilder(pinia).start(options.start ?? {});
        return { app, store: useDgxUiBuilder(pinia) };
    };
})();
