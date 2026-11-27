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
        TOO_DEEP: 'nested deeper than the loader goes',
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
        broken: 'As it is now, the appearance is refused, or the pages drawn on it are: these. Save anyway, or cancel and mend it.',
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
            // the words a kind takes: one of some kinds alone ("scroll", a box's) offered there, and where it is said
            const offered = (entry, field) => entry.words.filter((word) => !entry.word_kinds?.[word] || entry.word_kinds[word].kinds.includes(field.kind) ||
                                                                         (Array.isArray(field.value) && field.value.includes(word)));
            const wordHint = (word) => (word === 'scroll' ? words.t('scrolling: the children of a column taller than the box, a scroll bar at its right') : '');
            return { ...words, entry, rawText, pairText, fold, names, choice, choose, typeName, one, isPercent, hint, offered, wordHint,
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
                    <label v-for="word in offered(entry, field)" :key="word" :title="wordHint(word)">
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

    // ---- the appearance by its forms ----

    // a field of the appearance at a path, as the module describes its kind: a colour of the appearance's, null where it
    // may be, a number in its bounds, a word, flags, a list of bands; one the format does not describe is shown as text
    const LookField = {
        props: ['path', 'field', 'node', 'hint'],
        setup(props) {
            const words = useWords(), store = words.store;
            const value = computed(() => (props.node === undefined ? undefined : window.DgxUiDescription.shown(props.node)));
            const colors = computed(() => (store.look ? window.DgxUiAppearance.namesOf(store.look, 'colors') : []));
            const fold = () => store.lookFold(props.path);
            const choose = (choice) => store.setLook(props.path, choice === ABSENT ? undefined : choice === NULL_NAME ? null : choice);
            const numberText = (node) => (node && node.t === 'number' ? node.raw : node && node.t === 'string' ? node.value : '');
            const flag = (word, on) => {
                const now = Array.isArray(value.value) ? value.value.filter((one) => typeof one === 'string' && one !== word) : [];
                const next = on ? [...now, word] : now;
                store.setLook(props.path, next.length ? next : undefined);
            };
            const bands = computed(() => (props.node && props.node.t === 'array' ? props.node.items : []));
            const choice = computed(() => (props.node === undefined ? ABSENT : value.value === null ? NULL_NAME : String(value.value)));
            return { ...words, value, colors, fold, choose, numberText, flag, bands, choice, ABSENT, NULL_NAME,
                     one: (node) => JSON.stringify(window.DgxUiDescription.shown(node)) };
        },
        template: `
            <div class="field" :class="{ absent: node === undefined }">
                <span class="key" :title="hint || field.type">{{ field.name }}</span>
                <select v-if="field.type === 'color' || field.type === 'color_or_under'" :value="choice" @change="choose($event.target.value)">
                    <option :value="ABSENT">—</option>
                    <option v-if="field.type === 'color_or_under'" :value="NULL_NAME">{{ t('null, what lies under') }}</option>
                    <option v-for="name in colors" :key="name" :value="name">{{ name }}</option>
                    <option v-if="typeof value === 'string' && !colors.includes(value)" :value="value">{{ value }}</option>
                </select>
                <input v-else-if="field.type === 'number'" type="text" :value="numberText(node)" :placeholder="field.min + ' … ' + field.max"
                       @input="store.setLookNumber(path, $event.target.value, fold())">
                <input v-else-if="field.type === 'bool' || field.type === 'bit' || field.type === 'part'" type="checkbox" :checked="value === true"
                       @change="store.setLook(path, $event.target.checked ? true : undefined)">
                <select v-else-if="field.type === 'word'" :value="choice" @change="choose($event.target.value)">
                    <option :value="ABSENT">—</option>
                    <option v-for="word in field.words" :key="word" :value="word">{{ word }}</option>
                </select>
                <span v-else-if="field.type === 'flags'" class="words">
                    <label v-for="word in field.words" :key="word">
                        <input type="checkbox" :checked="Array.isArray(value) && value.includes(word)" @change="flag(word, $event.target.checked)">
                        {{ word }}</label>
                </span>
                <span v-else-if="field.type === 'picture'" class="name">
                    <select v-if="!store.lookFormat?.by_rule" :value="choice" @change="choose($event.target.value)">
                        <option :value="ABSENT">—</option>
                        <option v-for="one in store.lookFormat?.pictures ?? []" :key="one.name" :value="one.name">
                            {{ one.name }} ({{ one.count }}{{ one.count < field.min ? ', ' + t('fewer than {0}', field.min) : '' }})</option>
                        <option v-if="typeof value === 'string' && !(store.lookFormat?.pictures ?? []).some((one) => one.name === value)" :value="value">
                            {{ value }}</option>
                    </select>
                    <input v-else type="text" :value="typeof value === 'string' ? value : ''" :placeholder="t('a picture of the program\\'s, {0} at least', field.min)"
                           @input="store.setLook(path, $event.target.value === '' ? undefined : $event.target.value, fold())">
                </span>
                <span v-else-if="field.type === 'font'" class="name">
                    <input type="text" :value="typeof value === 'string' ? value : ''" list="dgx-ui-builder-fonts"
                           @input="store.setLook(path, $event.target.value === '' ? undefined : $event.target.value, fold())">
                </span>
                <select v-else-if="field.type === 'hand'" :value="choice" @change="choose($event.target.value)">
                    <option :value="ABSENT">—</option>
                    <option v-for="hand in store.lookFormat?.hands ?? []" :key="hand" :value="hand">{{ hand }}</option>
                    <option v-if="typeof value === 'string' && !(store.lookFormat?.hands ?? []).includes(value)" :value="value">{{ value }}</option>
                </select>
                <span v-else-if="field.type === 'bands'" class="bands">
                    <span v-for="(band, k) in bands" :key="k" class="band">
                        <input type="text" :value="band.t === 'array' ? numberText(band.items[0]) : ''" :title="t('the values below it')"
                               @input="store.setLookNumber([...path, k, 0], $event.target.value, store.lookFold([...path, k, 0]))">
                        <select :value="band.t === 'array' && band.items[1]?.t === 'string' ? band.items[1].value : ABSENT"
                                @change="store.setLook([...path, k, 1], $event.target.value)">
                            <option v-for="name in colors" :key="name" :value="name">{{ name }}</option></select>
                        <button @click="store.moveLookItem(path, k, -1)" :disabled="k === 0">↑</button>
                        <button @click="store.moveLookItem(path, k, 1)" :disabled="k + 1 === bands.length">↓</button>
                        <button class="remove" @click="store.removeLookItem(path, k)">×</button>
                    </span>
                    <button @click="store.insertLookItem(path, bands.length, [0, colors[0] ?? 'ink'])">{{ t('Add a band') }}</button>
                </span>
                <code v-else class="value">{{ node === undefined ? '' : one(node) }}</code>
                <button v-if="node !== undefined && field.type !== 'bands'" class="remove" :title="t('the key removed')"
                        @click="store.setLook(path, undefined)">×</button>
            </div>
        `,
    };

    const AppearancePane = {
        components: { LookField },
        setup() {
            const words = useWords(), store = words.store;
            const appearance = window.DgxUiAppearance, describe = window.DgxUiDescription;
            const colorNames = window.DgxUiPreview.colorNames();
            const made = computed(() => store.project?.appearances.find((one) => one.path === store.page?.appearance)?.from ?? null);
            const verdict = computed(() => {
                const short = store.appearanceShort;
                if (short) return words.t('{0}: {1} pictures needed, the table\'s "{2}" has {3}', short.field, short.need, short.name, short.count);
                return words.verdictText(store.verdicts.appearance);
            });
            const entriesOf = (section) => {
                const object = store.look ? describe.valueOf(store.look, section) : null;
                return object && object.t === 'object' ? object.entries : [];
            };
            const twice = (node) => node && node.t === 'object' && new Set(node.entries.map(([key]) => key.value)).size !== node.entries.length;
            const colors = computed(() => entriesOf('colors').map(([key, node]) => {
                const text = node.t === 'string' ? node.value : '';
                const hex = appearance.HEX.test(text.toLowerCase()) ? text.toLowerCase() : '#000000';
                return { name: key.value, text, hex, painter: appearance.PAINTERS.includes(key.value),
                         named: appearance.namesOfColor(hex, colorNames), bad: node.t !== 'string' };
            }));
            const styles = computed(() => entriesOf('styles').map(([key]) => key.value));
            const looks = computed(() => entriesOf('looks').map(([key]) => key.value));
            const picked = (section) => (store.pick[0] === section ? store.pick[1] : null);
            const pickedNode = (section) => {
                const name = picked(section);
                return name === null ? null : appearance.nodeAt(store.look, [section, name]) ?? null;
            };
            const style = computed(() => (store.look ? pickedNode('styles') : null));
            const look = computed(() => (store.look ? pickedNode('looks') : null));
            const variants = computed(() => {
                const fonts = style.value && style.value.t === 'object' ? describe.valueOf(style.value, 'fonts') : null;
                return fonts && fonts.t === 'array' ? fonts.items : [];
            });
            const styleFields = computed(() => (store.lookFormat?.style ?? []).filter((field) => field.name !== 'fonts'));
            const draw = computed(() => {
                const node = look.value && look.value.t === 'object' ? describe.valueOf(look.value, 'draw') : null;
                return node && node.t === 'string' ? node.value : null;
            });
            const described = computed(() => store.lookFormat?.draws.find((one) => one.draw === draw.value) ?? null);
            const params = computed(() => (look.value && look.value.t === 'object' ? describe.valueOf(look.value, 'params') : undefined));
            const paramOf = (name) => (params.value && params.value.t === 'object' ? describe.valueOf(params.value, name) : undefined);
            const lookKeysAsText = computed(() => (look.value && look.value.t === 'object'
                ? look.value.entries.map(([key]) => key.value).filter((key) => !['draw', 'fill', 'params'].includes(key)) : []));
            const isAt = (section, name) => store.appearanceAt && store.appearanceAt[0] === section && store.appearanceAt[1] === name;
            const atDepth = (hex) => appearance.colorAtDepth(hex, store.screen.depth);
            // the browser's picker gives a colour at every move: taken once a frame, the last one
            let pending = null;
            const pickColor = (name, hex) => {
                const first = pending === null;
                pending = { name, hex };
                if (!first) return;
                (window.requestAnimationFrame ?? ((call) => setTimeout(call, 16)))(() => {
                    const { name: at, hex: value } = pending;
                    pending = null;
                    store.setColor(at, value, store.lookFold(['colors', at]));
                });
            };
            const fresh = ref(0); // the names typed shown anew after a rename not done
            const plan = ref(null);
            const rename = (section, from, to) => {
                to = to.trim();
                if (!to || to === from) return void ++fresh.value;
                if (section === 'colors') return void (store.renameEntry(section, from, to) || ++fresh.value);
                const said = store.renamePlan(section, from, to);
                if (!said.taken && !said.refused.length && (said.pages.length || said.warnings.length)) plan.value = { section, from, to, ...said };
                else if (!store.renameEntry(section, from, to)) ++fresh.value;
            };
            const confirmRename = () => {
                const { section, from, to } = plan.value;
                plan.value = null;
                if (!store.renameEntry(section, from, to)) ++fresh.value;
            };
            const cancelRename = () => {
                plan.value = null;
                ++fresh.value;
            };
            const widgetCount = (pages) => pages.reduce((sum, page) => sum + page.widgets.length, 0);
            const newColor = ref(''), newStyle = ref(''), newLook = ref(''), newDraw = ref('');
            const addColor = () => {
                if (store.addEntry('colors', newColor.value.trim(), '#808080')) newColor.value = '';
            };
            const addStyle = () => {
                const font = store.lookFormat?.fonts[0] ?? 'body';
                if (store.addEntry('styles', newStyle.value.trim(), { fonts: [font] })) newStyle.value = '';
            };
            const addLook = () => {
                const name = newLook.value.trim() || newDraw.value;
                if (newDraw.value && store.addLook(name, newDraw.value)) newLook.value = '';
            };
            const variantText = (node) => (node.t === 'string' ? node.value : '');
            // the fields of a scrolling box's look, in words
            const fieldHint = (draw, name) => draw === 'scroll'
                ? ({ track: () => words.t('the track of the scroll bar'), knob: () => words.t('the knob of the scroll bar'),
                     focused: () => words.t('the knob while the box has the focus'),
                     arrows: () => words.t('the arrows at the ends of the scroll bar'),
                     bar: () => words.t('the width of the scroll bar, in points') })[name]?.() ?? ''
                : draw === 'input' && name === 'blink_ms' ? words.t('half a period of the caret, in milliseconds; 0 steady') : '';
            return { ...words, colorNames, made, verdict, twice, colors, styles, looks, picked, style, look, variants, styleFields, draw, fieldHint,
                     described, params, paramOf, lookKeysAsText, isAt, atDepth, pickColor, fresh, plan, rename, confirmRename, cancelRename,
                     widgetCount, newColor, newStyle, newLook, newDraw, addColor, addStyle, addLook, variantText,
                     valueOf: describe.valueOf, one: (node) => JSON.stringify(describe.shown(node)) };
        },
        template: `
            <div class="appearance">
                <p class="path">{{ store.page.appearance }}<span v-if="made"> · {{ t('made by {0}', made) }}</span></p>
                <p v-if="store.appearanceError" class="wrong">
                    {{ t(LONG_TEXTS.unread, t(store.appearanceError.why), store.appearanceError.byte) }}
                    <button @click="store.showText('appearance')">{{ t('Show the text') }}</button></p>
                <p v-else-if="verdict" class="wrong verdict" @click="store.showText('appearance')" :title="t('the cursor to it')">{{ verdict }}</p>
                <template v-if="store.look">
                    <div class="heading">{{ t('Colours') }}</div>
                    <div v-for="row in colors" :key="row.name + '/' + fresh" class="color" :class="{ refused: isAt('colors', row.name) }">
                        <input type="text" class="entry-name" :value="row.name" :disabled="row.painter"
                               :title="row.painter ? t('one of the painter\\'s four: its name is the format\\'s') : t('the name: renamed when you leave it')"
                               @change="rename('colors', row.name, $event.target.value)">
                        <span class="swatch" :style="{ background: row.hex }" :title="t('as written')"></span>
                        <span class="swatch" :style="{ background: atDepth(row.hex) }"
                              :title="t('as the panel shows it at {0} bits', store.screen.depth)"></span>
                        <input type="text" class="hex" :value="row.text" list="dgx-ui-builder-colors" :aria-label="t('the colour of {0}', row.name)"
                               @input="store.setColor(row.name, $event.target.value, store.lookFold(['colors', row.name]))">
                        <input type="color" :value="row.hex" :aria-label="t('the colour of {0}', row.name)" @input="pickColor(row.name, $event.target.value)">
                        <span class="named note">{{ row.named }}</span>
                        <button v-if="!row.painter" class="remove" :title="t('Delete')" @click="store.removeEntry('colors', row.name)">×</button>
                    </div>
                    <datalist id="dgx-ui-builder-colors"><option v-for="one in colorNames" :key="one[0]" :value="one[0]">{{ one[1] }}</option></datalist>
                    <datalist id="dgx-ui-builder-fonts"><option v-for="font in store.lookFormat?.fonts ?? []" :key="font" :value="font"></option></datalist>
                    <p class="add"><input type="text" v-model="newColor" :placeholder="t('a new colour\\'s name')">
                        <button @click="addColor" :disabled="!newColor.trim()">{{ t('Add') }}</button></p>

                    <div class="heading">{{ t('Styles') }}</div>
                    <p class="entries">
                        <button v-for="name in styles" :key="name" class="node" :class="{ here: picked('styles') === name, refused: isAt('styles', name) }"
                                @click="store.choosePick(['styles', name])">{{ name }}</button></p>
                    <div v-if="style" class="form">
                        <div class="field"><span class="key">{{ t('name') }}</span>
                            <input type="text" class="entry-name" :key="'s' + fresh" :value="picked('styles')"
                                   @change="rename('styles', picked('styles'), $event.target.value)">
                            <button class="remove" :title="t('Delete')" @click="store.removeEntry('styles', picked('styles'))">×</button></div>
                        <p v-if="style.t !== 'object' || twice(style)" class="note">{{ t('edited as text: not an object of keys said once') }}</p>
                        <template v-else>
                            <div class="field"><span class="key">fonts</span><span class="note">{{ t('the best first') }}</span></div>
                            <div v-for="(variant, k) in variants" :key="k" class="variant">
                                <div class="field">
                                    <span class="key">{{ k + 1 }}</span>
                                    <input v-if="variant.t === 'string'" type="text" :value="variantText(variant)" list="dgx-ui-builder-fonts"
                                           @input="store.setLook(['styles', picked('styles'), 'fonts', k], $event.target.value,
                                                                 store.lookFold(['styles', picked('styles'), 'fonts', k]))">
                                    <span v-else-if="variant.t === 'object'" class="note">{{ t('a variant of its own') }}</span>
                                    <code v-else>{{ one(variant) }}</code>
                                    <button v-if="variant.t === 'string'" :title="t('a variant of its own: a handwritten font, or a font\\'s pixels as points')"
                                            @click="store.setLook(['styles', picked('styles'), 'fonts', k], { font: variant.value })">…</button>
                                    <button @click="store.moveLookItem(['styles', picked('styles'), 'fonts'], k, -1)" :disabled="k === 0">↑</button>
                                    <button @click="store.moveLookItem(['styles', picked('styles'), 'fonts'], k, 1)"
                                            :disabled="k + 1 === variants.length">↓</button>
                                    <button class="remove" @click="store.removeLookItem(['styles', picked('styles'), 'fonts'], k)">×</button>
                                </div>
                                <div v-if="variant.t === 'object'" class="sub">
                                    <look-field v-for="field in store.lookFormat?.variant ?? []" :key="field.name" :field="field"
                                                :path="['styles', picked('styles'), 'fonts', k, field.name]"
                                                :node="valueOf(variant, field.name)"></look-field>
                                </div>
                            </div>
                            <p><button @click="store.insertLookItem(['styles', picked('styles'), 'fonts'], variants.length,
                                                                     store.lookFormat?.fonts[0] ?? 'body')">{{ t('Add a font') }}</button></p>
                            <look-field v-for="field in styleFields" :key="field.name" :field="field" :path="['styles', picked('styles'), field.name]"
                                        :node="valueOf(style, field.name)"></look-field>
                        </template>
                    </div>
                    <p class="add"><input type="text" v-model="newStyle" :placeholder="t('a new style\\'s name')">
                        <button @click="addStyle" :disabled="!newStyle.trim()">{{ t('Add') }}</button></p>

                    <div class="heading">{{ t('Looks') }}</div>
                    <p class="entries">
                        <button v-for="name in looks" :key="name" class="node" :class="{ here: picked('looks') === name, refused: isAt('looks', name) }"
                                @click="store.choosePick(['looks', name])">{{ name }}</button></p>
                    <div v-if="look" class="form">
                        <div class="field"><span class="key">{{ t('name') }}</span>
                            <input type="text" class="entry-name" :key="'l' + fresh" :value="picked('looks')"
                                   @change="rename('looks', picked('looks'), $event.target.value)">
                            <button class="remove" :title="t('Delete')" @click="store.removeEntry('looks', picked('looks'))">×</button></div>
                        <p v-if="look.t !== 'object' || twice(look)" class="note">{{ t('edited as text: not an object of keys said once') }}</p>
                        <template v-else>
                            <div class="field"><span class="key">draw</span>
                                <select :value="draw ?? ''" @change="store.setDraw(picked('looks'), $event.target.value)">
                                    <option v-if="!described" :value="draw ?? ''">{{ draw ?? '—' }}</option>
                                    <option v-for="one in store.lookFormat?.draws ?? []" :key="one.draw" :value="one.draw">
                                        {{ one.draw }}{{ one.kinds.length ? ' (' + one.kinds.join(', ') + ')' : one.own ? ' · ' + t('the program\\'s own') : '' }}</option>
                                </select></div>
                            <look-field :field="{ name: 'fill', type: 'color_or_under' }" :path="['looks', picked('looks'), 'fill']"
                                        :node="valueOf(look, 'fill')"></look-field>
                            <template v-if="described && !described.own && (!params || (params.t === 'object' && !twice(params)))">
                                <div v-if="described.fields.length" class="field"><span class="key">params</span>
                                    <span class="note">{{ t('of the draw {0}', draw) }}</span></div>
                                <div class="sub">
                                    <look-field v-for="field in described.fields" :key="field.name" :field="field" :hint="fieldHint(draw, field.name)"
                                                :path="['looks', picked('looks'), 'params', field.name]" :node="paramOf(field.name)"></look-field>
                                </div>
                            </template>
                            <p v-else-if="params !== undefined" class="note">{{ t('params edited as text: a draw of the program\\'s own, or not an object of keys said once') }}
                                <code>{{ one(params) }}</code></p>
                            <p v-if="lookKeysAsText.length" class="note">{{ t('edited as text: {0}', lookKeysAsText.join(', ')) }}</p>
                        </template>
                    </div>
                    <p class="add"><input type="text" v-model="newLook" :placeholder="t('a new look\\'s name')">
                        <select v-model="newDraw" :aria-label="t('its draw')"><option value="" disabled>{{ t('its draw') }}</option>
                            <option v-for="one in store.lookFormat?.draws ?? []" :key="one.draw" :value="one.draw">{{ one.draw }}</option></select>
                        <button @click="addLook" :disabled="!newDraw">{{ t('Add') }}</button></p>
                </template>
                <div v-if="plan" class="asking">
                    <div class="dialog">
                        <p>{{ t('Renaming "{0}" to "{1}" changes {2} widget(s) of the pages on this appearance:', plan.from, plan.to, widgetCount(plan.pages)) }}</p>
                        <ul><li v-for="page in plan.pages" :key="page.path"><code>{{ page.path }}</code>: {{ page.widgets.length }}</li></ul>
                        <p v-for="(one, i) in plan.warnings" :key="i" class="note">{{ t(one.text, ...one.values) }}</p>
                        <button @click="confirmRename">{{ t('Rename') }}</button>
                        <button @click="cancelRename">{{ t('Cancel') }}</button>
                    </div>
                </div>
            </div>
        `,
    };

    // ---- the names file by its forms ----

    // a line of the names file: its name, its expression, and the word its table takes; the parts of its words, and its
    // words of the parts
    const partsOf = (row) => {
        const [table, name, ...rest] = row.words;
        if (table === 'font') {
            const handwritten = rest[rest.length - 1] === 'handwritten';
            const given = handwritten ? rest.slice(0, -1) : rest;
            return { table, name, expr: given[0] ?? '', extra: handwritten, more: given.slice(1) };
        }
        if (table === 'draw') return { table, name, expr: rest[0] ?? '', extra: rest[1] ?? '', more: rest.slice(2) };
        return { table, name, expr: rest[0] ?? '', extra: rest[1] ?? '', more: rest.slice(2) };
    };
    const wordsOf = (parts) => {
        const words = [parts.table, parts.name];
        if (parts.table === 'font') {
            if (parts.expr) words.push(parts.expr);
            if (parts.extra) words.push('handwritten');
        } else {
            if (parts.expr) words.push(parts.expr);
            if (parts.extra !== '' && parts.extra !== false) words.push(String(parts.extra));
        }
        return [...words, ...parts.more];
    };

    const NamesPane = {
        setup() {
            const words = useWords(), store = words.store;
            const path = computed(() => store.page?.names);
            const missingFile = computed(() => !!(path.value && store.files[path.value]?.missing));
            const verdict = computed(() => words.verdictText(store.verdicts.names));
            const tables = computed(() => [...(store.namesFormat?.tables ?? []), ...(store.namesFormat?.appearance ?? [])]);
            const rowsOf = (table) => store.namesRows.filter((row) => row.known && row.table === table);
            const unknown = computed(() => store.namesRows.filter((row) => !row.known));
            const unused = computed(() => store.namesUnused);
            const bare = (table, name) => (store.namesFormat?.bare?.[table] ?? []).includes(name);
            const setPart = (row, part, value, fold = null) => {
                const parts = { ...partsOf(row), [part]: value };
                return store.setNamesLine(row.number, wordsOf(parts), fold);
            };
            const fresh = ref(0);
            const plan = ref(null);
            const rename = (row, to) => {
                to = to.trim();
                const from = row.words[1];
                if (!to || to === from) return void ++fresh.value;
                const said = store.namesPlan(row.table, from, to);
                if (!said.taken && !said.refused.length && said.files.length) plan.value = { table: row.table, from, to, ...said };
                else if (!store.renameName(row.table, from, to)) ++fresh.value;
            };
            const confirmRename = () => {
                const { table, from, to } = plan.value;
                plan.value = null;
                if (!store.renameName(table, from, to)) ++fresh.value;
            };
            const cancelRename = () => {
                plan.value = null;
                ++fresh.value;
            };
            const adding = ref({ table: 'value', name: '', expr: '' }); // the line the form adds
            const add = () => {
                const { table } = adding.value, name = adding.value.name.trim(), expr = adding.value.expr.trim();
                if (store.addNamesLine(expr && table !== 'hand' ? [table, name, expr] : [table, name])) adding.value = { table, name: '', expr: '' };
            };
            const typed = (part, value) => {
                adding.value = { ...adding.value, [part]: value };
            };
            // PNG files chosen or dropped, given to the import with their bytes
            const imported = async (list) => {
                const files = [];
                for (const file of list) files.push({ name: file.name, bytes: new Uint8Array(await file.arrayBuffer()) });
                await store.importStart(files);
            };
            const chosen = (event) => imported([...event.target.files]).then(() => (event.target.value = ''));
            const dropped = (event) => imported([...event.dataTransfer.files].filter((file) => /\.png$/i.test(file.name)));
            const weight = computed(() => (store.importing ? store.importWeight() : null));
            // the depths the import offers: a mask 4 or 8 bits, a picture with soft edges 16, else any
            const depths = computed(() => (!store.importing?.soft ? [1, 4, 8, 16] : store.importing.mask ? [4, 8] : [16]));
            const users = computed(() => (store.importing ? (store.version, store.importUsers()) : { all: [], other: [] }));
            return { ...words, path, missingFile, verdict, tables, rowsOf, unknown, unused, bare, partsOf, setPart, fresh, plan, rename,
                     confirmRename, cancelRename, adding, add, typed, chosen, dropped, weight, depths, users };
        },
        template: `
            <div class="names">
                <p class="path">{{ path }}<span v-if="missingFile"> · {{ t('not on the disk yet: written at a save') }}</span></p>
                <p v-if="verdict" class="wrong verdict" @click="store.showText('names')" :title="t('the cursor to it')">{{ verdict }}</p>
                <template v-if="store.namesMissing.length">
                    <div class="heading">{{ t('Named by the page, not in the file') }}</div>
                    <div v-for="entry in store.namesMissing" :key="entry.table + ' ' + entry.name" class="field missing">
                        <span class="key">{{ entry.table }}</span><code>{{ entry.name }}</code>
                        <code class="note">{{ entry.bare ? t('the library\\'s') : entry.c ?? t('no C by the rule: one of the program\\'s') }}</code>
                        <button @click="store.addMissing(entry)" :disabled="!entry.bare && !entry.c">{{ t('Add') }}</button>
                    </div>
                    <p><button @click="store.addAllMissing()">{{ t('Add all') }}</button></p>
                </template>
                <template v-for="table in tables.filter((one) => rowsOf(one).length)" :key="table">
                    <div class="heading">{{ table }}</div>
                    <div v-for="row in rowsOf(table)" :key="row.number + '/' + fresh" class="name-row"
                         :class="{ refused: store.namesAt === row.number, here: store.namesPick[0] === table && store.namesPick[1] === row.words[1] }">
                        <input type="text" class="entry-name" :value="row.words[1]" :title="t('the name: renamed when you leave it')"
                               @change="rename(row, $event.target.value)">
                        <input type="text" class="expr" :value="partsOf(row).expr" :aria-label="t('its C expression')"
                               :placeholder="bare(table, row.words[1]) ? t('the library\\'s') : t('the program\\'s C')"
                               @input="setPart(row, 'expr', $event.target.value.trim(), store.namesFold(table, row.words[1], 'expr'))">
                        <input v-if="table === 'picture'" type="text" class="count" :value="partsOf(row).extra" :placeholder="'1'"
                               :aria-label="t('its count of pictures')"
                               @input="setPart(row, 'extra', $event.target.value.trim(), store.namesFold(table, row.words[1], 'count'))">
                        <label v-else-if="table === 'on_event' || table === 'ctx' || table === 'font'" class="word">
                            <input type="checkbox" :checked="table === 'font' ? partsOf(row).extra : partsOf(row).extra === store.namesFormat.words[table]"
                                   @change="setPart(row, 'extra', table === 'font' ? $event.target.checked : $event.target.checked ? store.namesFormat.words[table] : '')">
                            {{ store.namesFormat.words[table] }}</label>
                        <input v-else-if="table === 'draw'" type="text" class="expr" :value="partsOf(row).extra" :aria-label="t('its description')"
                               :placeholder="t('its description')"
                               @input="setPart(row, 'extra', $event.target.value.trim(), store.namesFold(table, row.words[1], 'described'))">
                        <button class="remove" :title="t('Delete')" @click="store.removeName(table, row.words[1])">×</button>
                        <span v-if="row.comment" class="note comment">{{ row.comment }}</span>
                        <span v-if="unused.has(table + ' ' + row.words[1])" class="note">{{ t('used by no page on this file') }}</span>
                    </div>
                </template>
                <p class="add"><select :value="adding.table" @change="typed('table', $event.target.value)" :aria-label="t('its table')">
                        <option v-for="table in tables" :key="table" :value="table">{{ table }}</option></select>
                    <input type="text" :value="adding.name" :placeholder="t('a new name')" @input="typed('name', $event.target.value)">
                    <input v-if="adding.table !== 'hand'" type="text" :value="adding.expr" :placeholder="t('the program\\'s C')"
                           @input="typed('expr', $event.target.value)">
                    <button @click="add()" :disabled="!adding.name.trim()">{{ t('Add') }}</button></p>
                <template v-if="unknown.length">
                    <div class="heading">{{ t('Edited as text') }}</div>
                    <p v-for="row in unknown" :key="row.number" class="note"><code>{{ row.number }}: {{ row.words.join(' ') }}</code></p>
                    <button @click="store.showText('names')">{{ t('Show the text') }}</button>
                </template>
                <p class="note">{{ t('The C expressions are the program\\'s: dgx_ui_c checks them when the program is built.') }}</p>
                <div class="heading">{{ t('A picture of PNG files') }}</div>
                <p v-if="!store.canImport()" class="note">{{ t('DGX\\'s writer of the project makes a picture: open the project through dgx_ui_serve.py or dgx_ui_mcp.py.') }}</p>
                <p v-else-if="!store.importing" class="import" @dragover.prevent @drop.prevent="dropped($event)">
                    <label class="file">{{ t('Choose PNG files, or drop them here') }}
                        <input type="file" accept="image/png" multiple @change="chosen($event)"></label></p>
                <div v-else class="importing">
                    <p>{{ store.importing.files.map((file) => file.name).join(', ') }}: {{ t('{0} colours', store.importing.colors) }}<span
                       v-if="store.importing.transparent">, {{ t('with holes') }}</span></p>
                    <p><label>{{ t('Its name') }} <input type="text" :value="store.importing.name"
                              @change="store.importSet('name', $event.target.value.trim())"></label>
                       <label>{{ t('Bits a pixel') }} <select :value="store.importing.bits" @change="store.importSet('bits', +$event.target.value)">
                           <option v-for="bits in depths" :key="bits" :value="bits">{{ bits }}</option></select></label></p>
                    <p v-if="store.importing.partly"><label><input type="checkbox" :checked="store.importing.soft"
                              @change="store.importSet('soft', $event.target.checked)"> {{ t('soft edges, as the PNG has them') }}</label></p>
                    <p v-if="store.importing.soft && store.importing.mask" class="note">{{ t('A mask: drawn in the ink of the look soft_picture, its edges soft. 8 bits give smoother edges at twice the bytes.') }}</p>
                    <p v-else-if="store.importing.soft" class="note">{{ t('16 bits with the opacity of every pixel: the look soft_picture draws its edges soft.') }}</p>
                    <p v-if="store.importing.soft && users.other.length" class="note">{{ t('{0} of its widgets on this page are drawn by another look.', users.other.length) }}
                       <button @click="store.importDrawSoft()">{{ t('Draw them by soft_picture') }}</button></p>
                    <p><label v-if="(store.importing.bits === 4 || store.importing.bits === 8) && !store.importing.soft"><input type="checkbox" :checked="store.importing.reduce"
                              @change="store.importSet('reduce', $event.target.checked)"> {{ t('fewer colours where they do not fit') }}</label>
                       <label><input type="checkbox" :checked="store.importing.dither" @change="store.importSet('dither', $event.target.checked)">
                           {{ t('dithered') }}</label>
                       <label v-if="store.importing.bits === 1">{{ t('lit pixels') }} <select :value="store.importing.ink ?? ''"
                              @change="store.importSet('ink', $event.target.value || null)">
                           <option value="">{{ t('the light') }}</option><option value="dark">{{ t('the dark') }}</option></select></label>
                       <label v-if="store.importing.bits === 16 && store.importing.transparent && !store.importing.soft">{{ t('the holes in the colour') }} #<input type="text"
                              class="hex" :value="store.importing.background" @change="store.importSet('background', $event.target.value.trim())"></label></p>
                    <p v-if="store.importing.said" class="wrong">{{ store.importing.said }}</p>
                    <p v-else-if="weight" class="note">{{ t('{0} bytes of flash and {1} of RAM; the panel draws the bytes the board reads', weight.flash, weight.ram) }}</p>
                    <p><button @click="store.importAccept()" :disabled="!store.importing.made">{{ t('Take it') }}</button>
                       <button @click="store.importCancel()">{{ t('Cancel') }}</button></p>
                </div>
                <div v-if="plan" class="asking">
                    <div class="dialog">
                        <p>{{ t('Renaming "{0}" to "{1}" changes it in these files on the names file ({2}):', plan.from, plan.to,
                                plan.files.reduce((sum, one) => sum + one.places.length, 0)) }}</p>
                        <ul><li v-for="one in plan.files" :key="one.path"><code>{{ one.path }}</code>: {{ one.places.length }}</li></ul>
                        <p v-for="(one, i) in plan.warnings" :key="i" class="note">{{ t(one.text, ...one.values) }}</p>
                        <button @click="confirmRename">{{ t('Rename') }}</button>
                        <button @click="cancelRename">{{ t('Cancel') }}</button>
                    </div>
                </div>
            </div>
        `,
    };

    // ---- the dialog with an agent beside the builder ----

    const AgentPane = {
        setup() {
            const words = useWords(), store = words.store;
            const typed = ref(''), list = ref(null);
            const send = () => {
                if (store.agentSay(typed.value)) typed.value = '';
            };
            watch(() => store.agentMessages.length, async () => {
                await nextTick();
                if (list.value) list.value.scrollTop = list.value.scrollHeight;
            });
            return { ...words, typed, list, send };
        },
        template: `
            <div class="agent">
                <div class="heading">{{ t('The agent') }}</div>
                <div class="messages" ref="list">
                    <p v-if="!store.agentMessages.length" class="note">{{ t('Write to the agent here; it edits in the builder, and you save.') }}</p>
                    <p v-for="(one, i) in store.agentMessages" :key="i" :class="one.from">{{ one.text }}</p>
                </div>
                <textarea v-model="typed" rows="3" :placeholder="t('a message to the agent')"
                          @keydown.enter.exact.prevent="send()"></textarea>
                <button @click="send()" :disabled="!typed.trim()">{{ t('Send') }}</button>
            </div>
        `,
    };

    const PropertiesPane = {
        components: { Field, AppearancePane, NamesPane, AgentPane },
        setup() {
            const words = useWords();
            return { ...words };
        },
        template: `
            <aside class="pane properties">
                <p v-if="store.page" class="tabs">
                    <button :class="{ here: store.tab === 'properties' }" @click="store.setTab('properties')">{{ t('Properties') }}</button>
                    <button :class="{ here: store.tab === 'appearance' }" :disabled="!store.page.appearance"
                            @click="store.setTab('appearance')">{{ t('Appearance') }}</button>
                    <button :class="{ here: store.tab === 'names' }" :disabled="!store.page.names"
                            @click="store.setTab('names')">{{ t('Names') }}</button></p>
                <appearance-pane v-if="store.page && store.tab === 'appearance' && store.page.appearance"></appearance-pane>
                <names-pane v-else-if="store.page && store.tab === 'names' && store.page.names"></names-pane>
                <template v-else>
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
                </template>
                <agent-pane v-if="store.agent"></agent-pane>
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
                        <template v-if="store.asking.paths.length">
                            <p>{{ t(LONG_TEXTS.asking) }}</p>
                            <ul><li v-for="path in store.asking.paths" :key="path"><code>{{ path }}</code></li></ul>
                        </template>
                        <template v-if="store.asking.broken?.length">
                            <p>{{ t(LONG_TEXTS.broken) }}</p>
                            <ul><li v-for="one in store.asking.broken" :key="one.path"><code>{{ one.path }}</code>: {{ verdictText(one.verdict) }}</li></ul>
                        </template>
                        <button @click="store.save(store.asking.all, true)">{{ t('Save') }}</button>
                        <button @click="store.cancelSave()">{{ t('Cancel') }}</button>
                    </div>
                </div>
            </div>
        `,
    };

    // the builder on an element of a page: options.host, the files' host, whose folder the person opens; options.open, a
    // host of a project opened at once, the server's (host_server.js), with no folder to choose; options.start, what the
    // store's start() takes
    return async function mount(element, options = {}) {
        const app = Vue.createApp({ components: { DgxUiBuilder }, data: () => ({ host: options.host ?? null }),
                                    template: '<dgx-ui-builder :host="host"></dgx-ui-builder>' });
        const pinia = Pinia.createPinia();
        app.use(pinia);
        app.mount(element);
        const store = useDgxUiBuilder(pinia);
        await store.start(options.start ?? {});
        if (options.open) {
            try {
                await store.open(await options.open());
            } catch (error) {
                store.say('{0}', [String(error?.message ?? error)], true);
            }
        }
        return { app, store };
    };
})();
