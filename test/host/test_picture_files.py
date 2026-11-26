#!/usr/bin/env python3
"""
test_picture_files.py <dgx_ui_c> <DGX> <directory> <compile command...>: pictures of files of DGX, a names file's lines
picture <name> @<asset>, made C by dgx_ui_c and drawn.

PNG files are written here of colours of an appearance, and DGX's own writer, tools/dgx_images.py, makes of them the files
a build makes, with dgx_assets.S and dgx_assets.h. dgx_ui_c makes C of a page that shows them and of the loader's tables
of the names file; test_picture_files.c, compiled with both and the assembled assets, draws the page added and the page
loaded on those tables, and holds every pixel of every picture to the colour of its PNG: 1 bit with holes and without
(ink and paper of the look), 4 and 8 bits with holes and with a full palette (the value 0 a colour), 16 bits, a picture
at a scale of 2. dgx_ui_c's refusals in words: 16 bits with transparency, a count the file does not hold, an asset that
is none, 4 bits with no appearance and no map, a picture of a depth dgx-ui does not draw, ALPHA, and no -g.
"""
import json
import os
import struct
import subprocess
import sys
import zlib

tool, dgx, work, compile_command = os.path.abspath(sys.argv[1]), os.path.abspath(sys.argv[2]), os.path.abspath(sys.argv[3]), sys.argv[4:]
here = os.path.dirname(os.path.abspath(__file__))
os.makedirs(os.path.join(work, "art"), exist_ok=True)
failures = []
checks = 0


def check(held, what):
    global checks
    checks += 1
    if not held:
        failures.append(what)
        print("  failed: " + what, file=sys.stderr)


def png(name, w, h, rgba):
    raw = b"".join(b"\x00" + bytes(rgba[y * w * 4:(y + 1) * w * 4]) for y in range(h))

    def chunk(kind, data):
        return struct.pack(">I", len(data)) + kind + data + struct.pack(">I", zlib.crc32(kind + data) & 0xFFFFFFFF)
    with open(os.path.join(work, "art", name + ".png"), "wb") as out:
        out.write(b"\x89PNG\r\n\x1a\n" + chunk(b"IHDR", struct.pack(">IIBBBBB", w, h, 8, 6, 0, 0, 0)) + chunk(b"IDAT", zlib.compress(raw)) + chunk(b"IEND", b""))


def rgb16(r, g, b):
    return (r & 0xF8) << 8 | (g & 0xFC) << 3 | (b & 0xF8) >> 3


def level(v, top):  # DGX's rounding of a channel to its depth
    return (v * top * 2 + 255) // 510


# the appearance: 255 colours, all an appearance holds, black the paper (0) and white the ink (1), the rest each other
colors = [(0, 0, 0), (255, 255, 255)]
seen = set(colors)
k = 0
while len(colors) < 255:
    c = ((k * 37 + 11) % 256, (k * 91 + 13) % 256, (k * 53 + 101) % 256)
    k += 1
    if c not in seen:
        seen.add(c), colors.append(c)
table = [rgb16(*c) for c in colors]
HOLE = None


def nearest(c, weights=(2, 4, 3)):  # the weights and the lower index of equals, as dgx_ui_c's
    return min(range(len(colors)), key=lambda i: (sum(w * (colors[i][j] - c[j]) ** 2 for j, w in enumerate(weights)), i))


# the 256th colour of a full palette of 8 bits, none of the appearance's: one whose nearest the weights decide, so that
# a map of other weights is seen
EXTRA = next(c for c in ((r, g, 77) for r in range(0, 256, 3) for g in range(0, 256, 5)) if c not in seen and nearest(c) != nearest(c, (1, 1, 1)))

# the pictures: name, need's words, the PNG's pixels (a colour, or HOLE), the look, the line's extra words, scale
pictures = []


def picture(name, w, h, pixel, bits, look, words="", scale=0, line_words=""):
    px = [pixel(x, y) for y in range(h) for x in range(w)]
    rgba = []
    for p in px:
        rgba += [0, 0, 0, 0] if p is HOLE else [*p, 255]
    png(name, w, h, rgba)
    pictures.append(dict(name=name, w=w, h=h, px=px, bits=bits, look=look, words=words, scale=scale, line=line_words))


picture("one_holes", 9, 6, lambda x, y: HOLE if (x + y) % 3 == 0 else colors[5], 1, "picture")
picture("one_opaque", 8, 5, lambda x, y: colors[1] if (x * y) % 3 else colors[0], 1, "picture")
picture("four_holes", 7, 7, lambda x, y: HOLE if (x + 2 * y) % 5 == 0 else colors[2 + (x * 3 + y) % 15], 4, "indexed")
picture("four_full", 8, 4, lambda x, y: colors[20 + (x + 8 * y) % 16], 4, "indexed")
picture("eight_holes", 12, 10, lambda x, y: HOLE if (x * y) % 7 == 3 else colors[40 + (x + 12 * y) % 100], 8, "indexed")
picture("eight_full", 16, 16, lambda x, y: colors[x * 16 + y] if x * 16 + y < 255 else EXTRA, 8, "indexed")
picture("sixteen", 10, 6, lambda x, y: (x * 25, y * 40, 255 - x * 20), 16, "rgb565")
picture("four_twice", 7, 7, lambda x, y: HOLE if (x + 2 * y) % 5 == 0 else colors[2 + (x * 3 + y) % 15], 4, "indexed", scale=2, line_words="scale 2")

png("half", 4, 4, [v for k in range(16) for v in (*colors[5 + k % 3], 128 if k % 2 else 255)])  # half opaque now and then
with open(os.path.join(work, "images.txt"), "w") as out:
    for p in pictures:
        out.write(f"{p['name']} PNG art/{p['name']}.png BITS {p['bits']}\n")
    out.write("hole_sixteen PNG art/one_holes.png BITS 16\n")
    out.write("twenty_four PNG art/sixteen.png BITS 24\n")
    out.write("with_alpha PNG art/half.png BITS 8 ALPHA\n")
made = subprocess.run([sys.executable, os.path.join(dgx, "tools", "dgx_images.py"), "build", "images.txt", "out"], cwd=work, capture_output=True, text=True)
check(made.returncode == 0, "DGX's writer made the files: " + made.stderr.strip())

with open(os.path.join(work, "look.json"), "w") as out:
    fixed = ["paper", "ink", "fill", "border"]
    json.dump({"dgx-ui": 1, "name": "day", "colors": {(fixed[i] if i < 4 else f"c{i}"): "#%02x%02x%02x" % c for i, c in enumerate(colors)}}, out)
with open(os.path.join(work, "names.txt"), "w") as out:
    out.write("look picture\nlook indexed\nlook rgb565\n")
    for p in pictures:
        asset = "four_holes" if p["name"] == "four_twice" else p["name"]
        out.write(f"picture {p['name']} @{asset} {p['line']}\n")
page = {"dgx-ui": 1, "name": "pf", "tree": {"kind": "box", "flags": ["column"], "gap": 2,
                                           "children": [{"kind": "picture", "picture": p["name"], "look": p["look"]} for p in pictures]}}
with open(os.path.join(work, "page.json"), "w") as out:
    json.dump(page, out)


def tool_run(*options):
    return subprocess.run([tool, "-d", work, *options], cwd=work, capture_output=True, text=True, timeout=60)


run = tool_run("-A", "look.json", "-p", "day")
check(run.returncode == 0, "the appearance made C: " + run.stderr.strip())
run = tool_run("-n", "names.txt", "-g", "out", "-a", "look.json", "-ap", "day", "page.json")
check(run.returncode == 0, "the page made C: " + run.stderr.strip())
run = tool_run("-n", "names.txt", "-g", "out", "-a", "look.json", "-ap", "day", "-t", "pft")
check(run.returncode == 0, "the tables made C: " + run.stderr.strip())

# the expected pixels: a colour of the appearance packed as the table, the paper where a hole is; 1 bit the look's ink
# and paper; 16 bits rounded to the depth as DGX's writer rounds it
values_of = []
with open(os.path.join(work, "pf_expected.h"), "w") as out:
    out.write("/* made by test_picture_files.py */\n#include <stdint.h>\n")
    out.write("static const uint32_t pf_table[%d] = {%s};\n" % (len(table), ", ".join("0x%04x" % v for v in table)))
    for k, p in enumerate(pictures):
        values = []
        for c in p["px"]:
            if p["bits"] == 1:
                values.append(table[1] if c is not HOLE and sum(c) > 0 else table[0])  # light or opaque: 1, the ink
            elif p["bits"] == 16:
                values.append(level(c[0], 31) << 11 | level(c[1], 63) << 5 | level(c[2], 31))
            else:
                values.append(table[0] if c is HOLE else table[nearest(c)])
        out.write("static const uint16_t pf_pixels_%d[%d] = {%s};\n" % (k, len(values), ", ".join("0x%04x" % v for v in values)))
        values_of.append(values)
    out.write("static const struct { int w, h, scale; const uint16_t *px; const char *name; } pf_pictures[%d] = {\n" % len(pictures))
    for k, p in enumerate(pictures):
        out.write('    {%d, %d, %d, pf_pixels_%d, "%s"},\n' % (p["w"], p["h"], p["scale"] or 1, k, p["name"]))
    out.write("};\nstatic const char pf_page[] = %s;\n" % json.dumps(json.dumps(page)))
    out.write("#define PF_PICTURES %d\n" % len(pictures))

assets = os.path.join(work, "out")
program = os.path.join(work, "test_picture_files")
# the assets as a build assembles them: by the assembler alone, none of the C's flags
assembled = subprocess.run(["cc", "-c", "-o", os.path.join(work, "dgx_assets.o"), os.path.join(assets, "dgx_assets.S")], capture_output=True, text=True, timeout=60)
check(assembled.returncode == 0, "the assets assembled: " + assembled.stderr.strip())
built = subprocess.run(compile_command + ["-I" + work, "-I" + assets, "-I" + here, "-o", program, os.path.join(here, "test_picture_files.c"),
                                          os.path.join(work, "pf.c"), os.path.join(work, "pft.c"), os.path.join(work, "day.c"),
                                          os.path.join(work, "dgx_assets.o")], capture_output=True, text=True, timeout=600)
check(built.returncode == 0, "the program built: " + built.stderr.strip()[-2000:])
if built.returncode == 0:
    ran = subprocess.run([program], capture_output=True, text=True, timeout=120)
    sys.stderr.write(ran.stderr)
    check(ran.returncode == 0, "the pictures drawn as their PNGs")

# the preview module: the same files given for their assets, the panel of the same size, every picture's pixels its PNG's
PREVIEW = r"""
const fs = require('fs'), path = require('path');
const [module_, work] = process.argv.slice(1);
const DgxUiPreview = require(module_);
const expected = JSON.parse(fs.readFileSync(path.join(work, 'expected.json'), 'utf8'));
(async () => {
    const preview = await DgxUiPreview.create();
    preview.screen(64, 360, 16, 1);
    preview.names(fs.readFileSync(path.join(work, 'names.txt'), 'utf8'));
    const given = await preview.pictures(fs.readFileSync(path.join(work, 'out', 'dgx_images-files.txt'), 'utf8'), file => fs.readFileSync(path.join(work, 'out', file)));
    const refused = given.filter(one => one.verdict.code !== 0).map(one => one.name + ': ' + one.verdict.said);
    preview.appearance(fs.readFileSync(path.join(work, 'look.json'), 'utf8'));
    preview.page(fs.readFileSync(path.join(work, 'page.json'), 'utf8'));
    preview.paint(0);
    const frame = preview.frame();
    const off = [];
    expected.forEach((picture, k) => {
        const node = preview.widget(k + 1), w = picture.w * picture.scale, h = picture.h * picture.scale;
        const x0 = node.x + Math.floor((node.w - w) / 2), y0 = node.y + Math.floor((node.h - h) / 2);
        let wrong = 0;
        for (let y = 0; y < h; ++y)
            for (let x = 0; x < w; ++x) {
                const at = ((y0 + y) * 64 + x0 + x) * 4;
                const got = (frame[at] >> 3) << 11 | (frame[at + 1] >> 2) << 5 | frame[at + 2] >> 3;
                wrong += got !== picture.px[Math.floor(y / picture.scale) * picture.w + Math.floor(x / picture.scale)];
            }
        if (wrong) off.push(picture.name + ': ' + wrong + ' pixels off');
    });
    console.log(JSON.stringify({refused, off, notes: preview.notes()}));
})();
"""
preview_js = os.environ.get("PREVIEW_JS")
if preview_js:
    with open(os.path.join(work, "expected.json"), "w") as out:
        json.dump([{"name": p["name"], "w": p["w"], "h": p["h"], "scale": p["scale"] or 1, "px": values_of[k]} for k, p in enumerate(pictures)], out)
    ran = subprocess.run(["node", "-e", PREVIEW, os.path.abspath(preview_js), work], capture_output=True, text=True, timeout=300)
    told = json.loads(ran.stdout) if ran.returncode == 0 and ran.stdout.strip() else {}
    check(ran.returncode == 0 and told.get("refused") == [] and told.get("off") == [] and "from its file" in str(told.get("notes")),
          f"the preview draws the pictures of the files as the board: {ran.stderr.strip()[-1500:]} {told}")

# the refusals, in words
REFUSALS = [
    ("picture p @hole_sixteen", "has transparency, which dgx-ui does not mix: BACKGROUND"),
    ("picture p @four_holes 3", "the line says 3 pictures, @four_holes holds 1"),
    ("picture p @nothing", "no asset \"nothing\""),
    ("picture p @twenty_four", "of 24 bits of pixels: dgx-ui draws 1, 4 and 8 bits indexed and 16 of pixels"),
    ("picture p @with_alpha", "keeps the opacity of its PNG (ALPHA)"),
    ("picture p @four", "no asset \"four\""),
    ("picture p @four_holes scale 300", "is not a scale"),
    ("picture p @four_holes map=1x", "map= takes the name of the program's array"),
]
for line, words in REFUSALS:
    with open(os.path.join(work, "refused.txt"), "w") as out:
        out.write("look indexed\n" + line + "\n")
    with open(os.path.join(work, "refused.json"), "w") as out:
        json.dump({"dgx-ui": 1, "name": "rf", "tree": {"kind": "picture", "picture": "p", "look": "indexed"}}, out)
    run = tool_run("-n", "refused.txt", "-g", "out", "-a", "look.json", "-ap", "day", "refused.json")
    check(run.returncode != 0 and words in run.stderr, f"{line}: refused with \"{words}\": {run.stderr.strip()}")
# files DGX's later formats make, by hand: format 2, a kind of 3 (MASK); refused by the names file's line, not drawn
three = open(os.path.join(work, "out", "four_holes.dgxi"), "rb").read()
for name, spoiled, words in (("format_two", three[:4] + bytes([2, 0]) + three[6:], ("of a later format than the DGX dgx-ui is built with reads",
                                                                                 "is of the format 2 of a later DGX: dgx-ui reads the format 1")),
                             ("mask", three[:7] + bytes([3]) + three[8:], ("does not read as a file of DGX's pictures", "of a mask of soft edges (MASK)"))):
    with open(os.path.join(work, "out", name + ".dgxi"), "wb") as out:
        out.write(spoiled)
    with open(os.path.join(work, "out", "dgx_images-files.txt"), "a") as out:
        out.write(f"{name} {name}.dgxi\n")
    with open(os.path.join(work, "refused.txt"), "w") as out:
        out.write(f"look indexed\n# a later file\npicture p @{name}\n")
    run = tool_run("-n", "refused.txt", "-g", "out", "-a", "look.json", "-ap", "day", "refused.json")
    check(run.returncode != 0 and "refused.txt:3: picture p: " in run.stderr and any(one in run.stderr for one in words),
          f"{name}: refused by its line, the DGX given reading it or not: {run.stderr.strip()}")
# a file of a later format DGX's own writer makes, a mask of soft edges, where the DGX given makes one (0.12.11 and later)
masked = subprocess.run([sys.executable, os.path.join(dgx, "tools", "dgx_images.py"), "make", "out/masked.dgxi", "PNG", "art/four_holes.png", "BITS", "4", "MASK"],
                        cwd=work, capture_output=True, text=True)
if masked.returncode == 0:
    with open(os.path.join(work, "out", "dgx_images-files.txt"), "a") as out:
        out.write("masked masked.dgxi\n")
    with open(os.path.join(work, "refused.txt"), "w") as out:
        out.write("look indexed\npicture p @masked\n")
    run = tool_run("-n", "refused.txt", "-g", "out", "-a", "look.json", "-ap", "day", "refused.json")
    check(run.returncode != 0 and "refused.txt:2: picture p: @masked is of the format 2 of a later DGX" in run.stderr,
          "a mask of DGX's writer refused by its line: " + run.stderr.strip())
with open(os.path.join(work, "refused.txt"), "w") as out:
    out.write("look indexed\npicture p @four_holes\n")
run = tool_run("-n", "refused.txt", "-g", "out", "refused.json")
check(run.returncode != 0 and "takes the colours of an appearance: APPEARANCE in the call, or map=<array>" in run.stderr, "4 bits with no appearance: " + run.stderr.strip())
run = tool_run("-n", "refused.txt", "-a", "look.json", "-ap", "day", "refused.json")
check(run.returncode != 0 and "the folder of its files is not given: -g" in run.stderr, "no -g: " + run.stderr.strip())
with open(os.path.join(work, "refused.txt"), "w") as out:
    out.write("look indexed\npicture p @four_holes map=my_map\n")
run = tool_run("-n", "refused.txt", "-g", "out", "refused.json")
check(run.returncode == 0 and "my_map, 1, 4" in open(os.path.join(work, "rf.c")).read(), "a map of the program's taken with no appearance: " + run.stderr.strip())

if failures:
    print(f"test_picture_files.py: {len(failures)} of {checks} checks failed", file=sys.stderr)
    sys.exit(1)
print(f"test_picture_files.py: {checks} checks held: {len(pictures)} pictures of files drawn as their PNGs, added and loaded, {len(REFUSALS) + 2} refusals in words")
